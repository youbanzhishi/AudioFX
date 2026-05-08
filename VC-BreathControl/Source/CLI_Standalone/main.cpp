//==============================================================================
// VC-BreathControl Standalone CLI
// Automatic breath detection and gain control
// Two-pass processing: detect then process
// Uses dr_wav for WAV I/O
//==============================================================================

#define DR_WAV_IMPLEMENTATION
#include <dr_wav.h>

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <cmath>
#include <fstream>

#include "../DSP/VCPluginDSP.h"

//==============================================================================
// Presets
//==============================================================================
struct Preset {
    const char* name;
    VCPluginDSP::Params params;
};

static const Preset presets[] = {
    // name,        threshold, reduction, attack, release, autoSmooth, fadeIn, fadeOut, minBreathDur, sensitivity, lookahead, freqLow, freqHigh, sfThreshold, enabled
    {"gentle",      {-35.0f, -4.0f,  15.0f, 60.0f, true,  2.0f,  5.0f, 80.0f,  0.5f, 5.0f, 200.0f, 4000.0f, 0.6f, true}},
    {"moderate",    {-40.0f, -8.0f,  10.0f, 50.0f, true,  2.0f,  5.0f, 50.0f,  0.5f, 5.0f, 200.0f, 4000.0f, 0.6f, true}},
    {"aggressive",  {-45.0f, -14.0f,  5.0f, 40.0f, true,  2.0f,  5.0f, 30.0f,  0.5f, 5.0f, 200.0f, 4000.0f, 0.6f, true}},
    {"broadcast",   {-38.0f, -10.0f,  8.0f, 50.0f, true,  2.0f,  5.0f, 60.0f,  0.5f, 5.0f, 200.0f, 4000.0f, 0.6f, true}},
    {"solo-vocal",  {-40.0f, -6.0f,  12.0f, 60.0f, true,  2.0f,  5.0f, 50.0f,  0.5f, 5.0f, 200.0f, 4000.0f, 0.6f, true}},
    {"enhance",     {-40.0f,  6.0f,  10.0f, 50.0f, true,  2.0f,  5.0f, 50.0f,  0.5f, 5.0f, 200.0f, 4000.0f, 0.6f, true}},
    {"detect-only", {-40.0f,  0.0f,  10.0f, 50.0f, true,  2.0f,  5.0f, 50.0f,  0.5f, 5.0f, 200.0f, 4000.0f, 0.6f, true}},
    {"step-fade",   {-40.0f, -8.0f,  10.0f, 50.0f, false, 2.0f,  5.0f, 50.0f,  0.5f, 5.0f, 200.0f, 4000.0f, 0.6f, true}},
};

//==============================================================================
// Help text
//==============================================================================
void printHelp(const char* progName) {
    std::cout << "VC-BreathControl Standalone CLI\n\n";
    std::cout << "Usage: " << progName << " <input.wav> <output.wav> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --help                Show this help\n";
    std::cout << "  --preset <name>       Preset name\n";
    std::cout << "  --threshold <dB>      Detection threshold -60~-10 dBFS (default: -40)\n";
    std::cout << "  --reduction <dB>      Gain adjustment -18~+12 dB (default: -8)\n";
    std::cout << "  --attack <ms>         Gain decrease time 1~100ms (default: 10)\n";
    std::cout << "  --release <ms>        Gain recovery time 10~500ms (default: 50)\n";
    std::cout << "  --auto-smooth <0|1>   Auto smooth mode (default: 1)\n";
    std::cout << "  --fade-in <ms>        Micro-fade at breath start 0~20ms (default: 2)\n";
    std::cout << "  --fade-out <ms>       Micro-fade at breath end 0~20ms (default: 5)\n";
    std::cout << "  --min-duration <ms>   Minimum breath duration 10~500ms (default: 50)\n";
    std::cout << "  --sensitivity <0.1-1> Spectral flatness weight (default: 0.5)\n";
    std::cout << "  --lookahead <ms>      Lookahead time 0~10ms (default: 5)\n";
    std::cout << "  --freq-low <Hz>       Detection band low 100~1000Hz (default: 200)\n";
    std::cout << "  --freq-high <Hz>      Detection band high 2000~8000Hz (default: 4000)\n";
    std::cout << "  --sf-threshold <0.3-0.9> Spectral flatness threshold (default: 0.6)\n";
    std::cout << "  --report              Print detection report to stdout\n";
    std::cout << "  --report-file <f>     Save report to file\n";
    std::cout << "  --detect-only         Output only breath regions (non-breath = silence)\n";
    std::cout << "  --export-breaths      Export breath clips to <output>_clips/\n\n";
    std::cout << "Presets:\n";
    for (const auto& p : presets) {
        std::cout << "  " << p.name << "\n";
    }
    std::cout << "\nExamples:\n";
    std::cout << "  " << progName << " in.wav out.wav --threshold -40 --reduction -8 --report\n";
    std::cout << "  " << progName << " in.wav out.wav --preset aggressive --export-breaths\n";
    std::cout << "  " << progName << " in.wav out.wav --detect-only --report  (hear detected breaths only)\n";
    std::cout << "  " << progName << " in.wav out.wav --reduction +6  (enhance breaths)\n";
}

//==============================================================================
// Parse command line arguments
//==============================================================================
std::map<std::string, std::string> parseArgs(int argc, char** argv) {
    std::map<std::string, std::string> args;
    std::set<std::string> noValueFlags = {"--help", "-h", "--detect-only", "--export-breaths", "--report"};
    auto isOption = [](const std::string& s) -> bool {
        if (s.size() < 2) return false;
        if (s.substr(0, 2) == "--") return true;
        if (s == "-h") return true;
        if (s[0] == '-' && s.size() > 1 && !std::isdigit(static_cast<unsigned char>(s[1]))) return true;
        return false;
    };
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--") break;
        if (arg == "--help" || arg == "-h") {
            args["--help"] = "";
        } else if (arg.substr(0, 2) == "--") {
            std::string key = arg;
            std::string value;
            if (noValueFlags.count(key) == 0 && i + 1 < argc) {
                if (!isOption(argv[i + 1])) {
                    value = argv[++i];
                }
            }
            args[key] = value;
        }
    }
    return args;
}

//==============================================================================
// Load preset by name
//==============================================================================
bool loadPreset(const std::string& name, VCPluginDSP::Params& p) {
    for (const auto& preset : presets) {
        if (name == preset.name) {
            p = preset.params;
            return true;
        }
    }
    return false;
}

//==============================================================================
// Safe float argument parser
//==============================================================================
float getFloatArg(const std::map<std::string, std::string>& args,
                  const std::string& key, float defaultVal) {
    auto it = args.find(key);
    if (it != args.end() && !it->second.empty()) {
        try {
            return std::stof(it->second);
        } catch (...) {
            return defaultVal;
        }
    }
    return defaultVal;
}

//==============================================================================
// Export breath clips
//==============================================================================
void exportBreathClips(const std::string& baseName,
                       const float* origLeft, const float* origRight,
                       int totalFrames, int channels, unsigned int sampleRate,
                       const std::vector<BreathRegion>& regions)
{
    std::string clipDir = baseName + "_clips";
    std::string mkdirCmd = "mkdir -p " + clipDir;
    system(mkdirCmd.c_str());

    for (size_t idx = 0; idx < regions.size(); ++idx) {
        const auto& reg = regions[idx];
        int start = reg.startSample;
        int end = reg.endSample;
        int length = end - start;

        if (length <= 0) continue;

        char filename[512];
        float startSec = static_cast<float>(start) / static_cast<float>(sampleRate);
        snprintf(filename, sizeof(filename), "%s/breath_%03d_%.3fs.wav",
                 clipDir.c_str(), static_cast<int>(idx), startSec);

        drwav_data_format format;
        format.container = drwav_container_riff;
        format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
        format.channels = channels >= 2 ? 2 : 1;
        format.sampleRate = sampleRate;
        format.bitsPerSample = 32;

        drwav wav;
        if (!drwav_init_file_write(&wav, filename, &format, NULL)) continue;

        std::vector<float> clipData(length * format.channels);
        if (format.channels >= 2) {
            for (int i = 0; i < length; ++i) {
                int si = start + i;
                if (si < totalFrames) {
                    clipData[i * 2] = origLeft[si];
                    clipData[i * 2 + 1] = origRight[si];
                }
            }
        } else {
            for (int i = 0; i < length; ++i) {
                int si = start + i;
                if (si < totalFrames) {
                    clipData[i] = (origLeft[si] + origRight[si]) * 0.5f;
                }
            }
        }

        drwav_write_pcm_frames(&wav, length, clipData.data());
        drwav_uninit(&wav);
    }

    std::cout << "Exported " << regions.size() << " breath clips to " << clipDir << "/\n";
}

//==============================================================================
// Main entry point
//==============================================================================
int main(int argc, char** argv) {
    if (argc < 2) {
        printHelp(argv[0]);
        return 1;
    }

    auto args = parseArgs(argc, argv);
    if (args.count("--help")) {
        printHelp(argv[0]);
        return 0;
    }

    // Parse input/output files
    std::set<std::string> consumedValues;
    for (const auto& kv : args) { if (!kv.second.empty()) consumedValues.insert(kv.second); }
    std::vector<std::string> files;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a.substr(0, 2) == "--" || a == "-h") continue;
        if (args.count(a) || consumedValues.count(a)) continue;
        files.push_back(a);
    }

    if (files.size() < 2) {
        std::cerr << "Error: Need input and output files\n\n";
        printHelp(argv[0]);
        return 1;
    }

    std::string inFile = files[0];
    std::string outFile = files[1];

    std::cout << "VC-BreathControl Standalone CLI\n";
    std::cout << "Input: " << inFile << "\n";
    std::cout << "Output: " << outFile << "\n";

    //==========================================================================
    // Read audio file using dr_wav
    //==========================================================================
    unsigned int channels = 0;
    unsigned int sampleRate = 0;
    drwav_uint64 totalFrames = 0;

    float* pSampleData = drwav_open_file_and_read_pcm_frames_f32(
        inFile.c_str(), &channels, &sampleRate, &totalFrames, NULL);

    if (pSampleData == NULL) {
        std::cerr << "Error: Cannot read file: " << inFile << "\n";
        return 1;
    }

    std::cout << "Sample rate: " << sampleRate << " Hz\n";
    std::cout << "Channels: " << channels << "\n";
    std::cout << "Total frames: " << totalFrames << " (" << static_cast<float>(totalFrames) / sampleRate << "s)\n";

    // Convert interleaved to L/R arrays
    std::vector<float> left(totalFrames);
    std::vector<float> right(totalFrames);

    if (channels >= 2) {
        for (drwav_uint64 i = 0; i < totalFrames; ++i) {
            left[i] = pSampleData[i * 2];
            right[i] = pSampleData[i * 2 + 1];
        }
    } else {
        for (drwav_uint64 i = 0; i < totalFrames; ++i) {
            left[i] = right[i] = pSampleData[i];
        }
    }

    drwav_free(pSampleData, NULL);

    // Keep original for clip export
    std::vector<float> origLeft = left;
    std::vector<float> origRight = right;

    //==========================================================================
    // Initialize DSP
    //==========================================================================
    VCPluginDSP dsp;
    dsp.prepare(sampleRate, 4096);

    VCPluginDSP::Params params;

    // Load preset if specified
    if (args.count("--preset")) {
        std::string presetName = args["--preset"];
        std::cout << "Preset: " << presetName << "\n";
        if (!loadPreset(presetName, params)) {
            std::cerr << "Error: Unknown preset\n";
            return 1;
        }
    }

    // Override with command line parameters
    params.threshold        = getFloatArg(args, "--threshold",     params.threshold);
    params.reduction        = getFloatArg(args, "--reduction",     params.reduction);
    params.attack           = getFloatArg(args, "--attack",        params.attack);
    params.release          = getFloatArg(args, "--release",       params.release);
    params.fadeIn           = getFloatArg(args, "--fade-in",       params.fadeIn);
    params.fadeOut          = getFloatArg(args, "--fade-out",      params.fadeOut);
    params.minBreathDuration = getFloatArg(args, "--min-duration", params.minBreathDuration);
    params.sensitivity      = getFloatArg(args, "--sensitivity",   params.sensitivity);
    params.lookahead        = getFloatArg(args, "--lookahead",     params.lookahead);
    params.freqLow          = getFloatArg(args, "--freq-low",      params.freqLow);
    params.freqHigh         = getFloatArg(args, "--freq-high",     params.freqHigh);
    params.sfThreshold      = getFloatArg(args, "--sf-threshold",  params.sfThreshold);

    if (args.count("--auto-smooth")) {
        params.autoSmooth = (args["--auto-smooth"] != "0");
    }

    // Clamp parameters
    params.threshold        = std::clamp(params.threshold, -60.0f, -10.0f);
    params.reduction        = std::clamp(params.reduction, -18.0f, 12.0f);
    params.attack           = std::clamp(params.attack, 1.0f, 100.0f);
    params.release          = std::clamp(params.release, 10.0f, 500.0f);
    params.fadeIn           = std::clamp(params.fadeIn, 0.0f, 20.0f);
    params.fadeOut          = std::clamp(params.fadeOut, 0.0f, 20.0f);
    params.minBreathDuration = std::clamp(params.minBreathDuration, 10.0f, 500.0f);
    params.sensitivity      = std::clamp(params.sensitivity, 0.1f, 1.0f);
    params.lookahead        = std::clamp(params.lookahead, 0.0f, 10.0f);
    params.freqLow          = std::clamp(params.freqLow, 100.0f, 1000.0f);
    params.freqHigh         = std::clamp(params.freqHigh, 2000.0f, 8000.0f);
    params.sfThreshold      = std::clamp(params.sfThreshold, 0.3f, 0.9f);

    // Print settings
    std::cout << "\nParameters:\n";
    std::cout << "  Threshold:     " << params.threshold << " dBFS\n";
    std::cout << "  Reduction:     " << params.reduction << " dB\n";
    std::cout << "  Attack:        " << params.attack << " ms\n";
    std::cout << "  Release:       " << params.release << " ms\n";
    std::cout << "  Auto Smooth:   " << (params.autoSmooth ? "true" : "false") << "\n";
    if (!params.autoSmooth) {
        std::cout << "  Fade In:       " << params.fadeIn << " ms\n";
        std::cout << "  Fade Out:      " << params.fadeOut << " ms\n";
    }
    std::cout << "  Min Duration:  " << params.minBreathDuration << " ms\n";
    std::cout << "  Sensitivity:   " << params.sensitivity << "\n";
    std::cout << "  Detection Band:" << params.freqLow << " - " << params.freqHigh << " Hz\n";
    std::cout << "  SF Threshold:  " << params.sfThreshold << "\n";
    std::cout << "  Lookahead:     " << params.lookahead << " ms\n";

    dsp.setParams(params);
    dsp.setEnabled(params.enabled);

    //==========================================================================
    // Two-pass processing: detect then process
    //==========================================================================
    std::cout << "\nPass 1: Detecting breaths...\n";
    dsp.detectBreaths(left.data(), right.data(), static_cast<int>(totalFrames));

    auto regions = dsp.getBreathRegions();
    std::cout << "Detected " << regions.size() << " breath regions\n";

    if (!regions.empty()) {
        std::cout << "Pass 2: Processing breaths...\n";
        dsp.processBreaths(left.data(), right.data(), static_cast<int>(totalFrames));
    }

    //==========================================================================
    // Detect-only mode: silence non-breath regions
    //==========================================================================
    bool detectOnly = args.count("--detect-only") > 0;
    if (detectOnly && !regions.empty()) {
        std::cout << "Detect-only mode: silencing non-breath regions\n";
        // Build a mask: 1.0 for breath, 0.0 for non-breath
        std::vector<float> breathMask(totalFrames, 0.0f);
        for (const auto& reg : regions) {
            for (drwav_uint64 i = reg.startSample; i < reg.endSample && i < totalFrames; ++i) {
                breathMask[i] = 1.0f;
            }
        }
        // Apply short fade (5ms) at boundaries to avoid clicks
        int fadeSamples = static_cast<int>(5.0f * sampleRate / 1000.0f);
        for (size_t i = 0; i < totalFrames; ++i) {
            if (breathMask[i] > 0.0f) {
                // Check if near a rising edge
                if (i > 0 && breathMask[i-1] == 0.0f) {
                    // Fade in
                    int start = static_cast<int>(i) - fadeSamples;
                    if (start < 0) start = 0;
                    for (int j = start; j <= static_cast<int>(i); ++j) {
                        float t = static_cast<float>(j - start) / fadeSamples;
                        breathMask[j] = t;
                    }
                }
                // Check if near a falling edge
                if (i + 1 < totalFrames && breathMask[i+1] == 0.0f) {
                    // Fade out
                    int end = static_cast<int>(i) + fadeSamples;
                    if (end >= static_cast<int>(totalFrames)) end = static_cast<int>(totalFrames) - 1;
                    for (int j = static_cast<int>(i); j <= end; ++j) {
                        float t = 1.0f - static_cast<float>(j - static_cast<int>(i)) / fadeSamples;
                        breathMask[j] = t;
                    }
                }
            }
        }
        // Apply mask using ORIGINAL audio (before processBreaths modified it)
        for (drwav_uint64 i = 0; i < totalFrames; ++i) {
            left[i] = origLeft[i] * breathMask[i];
            right[i] = origRight[i] * breathMask[i];
        }
    }

    //==========================================================================
    // Write output file using dr_wav
    //==========================================================================
    drwav_data_format format;
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
    format.channels = channels >= 2 ? 2 : 1;
    format.sampleRate = sampleRate;
    format.bitsPerSample = 32;

    drwav wav;
    if (!drwav_init_file_write(&wav, outFile.c_str(), &format, NULL)) {
        std::cerr << "Error: Cannot write file: " << outFile << "\n";
        return 1;
    }

    // Interleave output
    std::vector<float> output(totalFrames * format.channels);
    if (format.channels >= 2) {
        for (drwav_uint64 i = 0; i < totalFrames; ++i) {
            output[i * 2] = left[i];
            output[i * 2 + 1] = right[i];
        }
    } else {
        for (drwav_uint64 i = 0; i < totalFrames; ++i) {
            output[i] = (left[i] + right[i]) * 0.5f;
        }
    }

    drwav_write_pcm_frames(&wav, totalFrames, output.data());
    drwav_uninit(&wav);

    //==========================================================================
    // Report
    //==========================================================================
    if (args.count("--report")) {
        std::cout << "\n" << dsp.generateReport();
    }

    if (args.count("--report-file")) {
        std::string reportFile = args["--report-file"];
        std::ofstream rf(reportFile);
        if (rf.is_open()) {
            rf << dsp.generateReport();
            rf.close();
            std::cout << "Report saved to: " << reportFile << "\n";
        }
    }

    //==========================================================================
    // Export breath clips
    //==========================================================================
    if (args.count("--export-breaths") && !regions.empty()) {
        std::string baseName = outFile.substr(0, outFile.rfind('.'));
        exportBreathClips(baseName,
                          origLeft.data(), origRight.data(),
                          static_cast<int>(totalFrames), channels, sampleRate,
                          regions);
    }

    std::cout << "Done! Output: " << outFile << "\n";
    return 0;
}
