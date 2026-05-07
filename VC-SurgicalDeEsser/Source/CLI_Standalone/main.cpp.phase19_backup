// VC-SurgicalDeEsser Standalone CLI
// Two-pass surgical de-esser with detection report and clip export
// Uses dr_wav for WAV I/O

#define DR_WAV_IMPLEMENTATION
#include <dr_wav.h>

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <cmath>
#include <fstream>

#include "../DSP/VCSurgicalDeEsserDSP.h"

//==============================================================================
// Presets
//==============================================================================
struct Preset {
    const char* name;
    VCPluginDSP::Params params;
};

static const Preset presets[] = {
    {"gentle",       {-25.0f, 3.0f,  20.0f, 5.0f,  5000.0f, 9000.0f, 0, true}},
    {"moderate",     {-30.0f, 6.0f,  20.0f, 5.0f,  5000.0f, 9000.0f, 0, true}},
    {"aggressive",   {-35.0f, 12.0f, 15.0f, 3.0f,  4000.0f, 10000.0f, 0, true}},
    {"broadcast",    {-28.0f, 8.0f,  15.0f, 3.0f,  5000.0f, 12000.0f, 0, true}},
    {"high-freq",    {-30.0f, 6.0f,  20.0f, 5.0f,  7000.0f, 12000.0f, 0, true}},
    {"dyneq",        {-30.0f, 6.0f,  20.0f, 5.0f,  5000.0f, 9000.0f, 1, true}},
};

//==============================================================================
// Help text
//==============================================================================
void printHelp(const char* progName) {
    std::cout << "VC-SurgicalDeEsser Standalone CLI\n\n";
    std::cout << "Usage: " << progName << " <input.wav> <output.wav> [options]\n\n";
    std::cout << "Modes: 0=gain (recommended), 1=dynEQ\n\n";
    std::cout << "Options:\n";
    std::cout << "  --help              Show this help\n";
    std::cout << "  --preset <name>     Preset name\n";
    std::cout << "  --threshold <dB>    Detection threshold -60~0 dBFS (default: -30)\n";
    std::cout << "  --reduction <dB>    Max attenuation 0~20 dB (default: 6)\n";
    std::cout << "  --min-duration <ms> Min sibilance duration 5~100ms (default: 20)\n";
    std::cout << "  --fade <ms>         Crossfade time 0.5~10ms (default: 5)\n";
    std::cout << "  --freq-low <Hz>     Detection band low 2000~8000Hz (default: 5000)\n";
    std::cout << "  --freq-high <Hz>    Detection band high 5000~14000Hz (default: 9000)\n";
    std::cout << "  --mode <0|1>        0=gain 1=dynEQ (default: 0)\n";
    std::cout << "  --report            Print detection report to stdout\n";
    std::cout << "  --report-file <f>   Save report to file\n";
    std::cout << "  --export-sibilances Export sibilance clips to <output>_clips/\n\n";
    std::cout << "Presets:\n";
    for (const auto& p : presets) {
        std::cout << "  " << p.name << "\n";
    }
    std::cout << "\nExamples:\n";
    std::cout << "  " << progName << " in.wav out.wav --threshold -30 --reduction 6 --report\n";
    std::cout << "  " << progName << " in.wav out.wav --preset aggressive --export-sibilances\n";
}

//==============================================================================
// Parse command line arguments
//==============================================================================
std::map<std::string, std::string> parseArgs(int argc, char** argv) {
    std::map<std::string, std::string> args;
    std::set<std::string> noValueFlags = {"--help", "-h", "--report", "--export-sibilances"};
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            args["--help"] = "";
        } else if (arg.substr(0, 2) == "--") {
            std::string key = arg;
            std::string value;
            if (noValueFlags.count(key) == 0 && i + 1 < argc) {
                value = argv[++i];
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
// Export sibilance clips
//==============================================================================
void exportSibilanceClips(const std::string& baseName,
                          const float* origLeft, const float* origRight,
                          int totalFrames, int channels, unsigned int sampleRate,
                          const std::vector<SibilanceRegion>& regions)
{
    std::string clipDir = baseName + "_clips";
    // Create directory
    std::string mkdirCmd = "mkdir -p " + clipDir;
    system(mkdirCmd.c_str());

    for (size_t idx = 0; idx < regions.size(); ++idx) {
        const auto& reg = regions[idx];
        int start = reg.startSample;
        int end = reg.endSample;
        int length = end - start;

        if (length <= 0) continue;

        char filename[512];
        float startSec = (float)start / (float)sampleRate;
        snprintf(filename, sizeof(filename), "%s/sib_%03d_%.3fs.wav",
                 clipDir.c_str(), (int)idx, startSec);

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

    std::cout << "Exported " << regions.size() << " clips to " << clipDir << "/\n";
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
    std::vector<std::string> files;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] != '-') {
            files.push_back(argv[i]);
        }
    }

    if (files.size() < 2) {
        std::cerr << "Error: Need input and output files\n\n";
        printHelp(argv[0]);
        return 1;
    }

    std::string inFile = files[0];
    std::string outFile = files[1];

    std::cout << "VC-SurgicalDeEsser Standalone CLI\n";
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
    std::cout << "Total frames: " << totalFrames << " (" << (float)totalFrames / sampleRate << "s)\n";

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
        dsp.setParams(params);
    }

    // Override with command line parameters
    if (args.count("--threshold")) {
        params.threshold = std::stof(args["--threshold"]);
        params.threshold = std::clamp(params.threshold, -60.0f, 0.0f);
        dsp.setParams(params);
        std::cout << "Threshold: " << params.threshold << " dBFS\n";
    }

    if (args.count("--reduction")) {
        params.reduction = std::stof(args["--reduction"]);
        params.reduction = std::clamp(params.reduction, 0.0f, 20.0f);
        dsp.setParams(params);
        std::cout << "Reduction: " << params.reduction << " dB\n";
    }

    if (args.count("--min-duration")) {
        params.minDuration = std::stof(args["--min-duration"]);
        params.minDuration = std::clamp(params.minDuration, 5.0f, 100.0f);
        dsp.setParams(params);
        std::cout << "Min duration: " << params.minDuration << " ms\n";
    }

    if (args.count("--fade")) {
        params.fadeTime = std::stof(args["--fade"]);
        params.fadeTime = std::clamp(params.fadeTime, 0.5f, 10.0f);
        dsp.setParams(params);
        std::cout << "Fade time: " << params.fadeTime << " ms\n";
    }

    if (args.count("--freq-low")) {
        params.freqLow = std::stof(args["--freq-low"]);
        params.freqLow = std::clamp(params.freqLow, 2000.0f, 8000.0f);
        dsp.setParams(params);
        std::cout << "Freq low: " << params.freqLow << " Hz\n";
    }

    if (args.count("--freq-high")) {
        params.freqHigh = std::stof(args["--freq-high"]);
        params.freqHigh = std::clamp(params.freqHigh, 5000.0f, 14000.0f);
        dsp.setParams(params);
        std::cout << "Freq high: " << params.freqHigh << " Hz\n";
    }

    if (args.count("--mode")) {
        params.mode = std::stoi(args["--mode"]);
        params.mode = std::clamp(params.mode, 0, 1);
        dsp.setParams(params);
        std::cout << "Mode: " << (params.mode == 0 ? "gain" : "dynEQ") << "\n";
    }

    //==========================================================================
    // Two-pass processing: detect then process
    //==========================================================================
    std::cout << "Pass 1: Detecting sibilance...\n";
    dsp.detectSibilance(left.data(), right.data(), static_cast<int>(totalFrames));

    auto regions = dsp.getSibilanceRegions();
    std::cout << "Detected " << regions.size() << " sibilance regions\n";

    if (!regions.empty()) {
        std::cout << "Pass 2: Processing sibilance...\n";
        dsp.processSibilance(left.data(), right.data(), static_cast<int>(totalFrames));
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
    // Export sibilance clips
    //==========================================================================
    if (args.count("--export-sibilances") && !regions.empty()) {
        std::string baseName = outFile.substr(0, outFile.rfind('.'));
        exportSibilanceClips(baseName,
                             origLeft.data(), origRight.data(),
                             static_cast<int>(totalFrames), channels, sampleRate,
                             regions);
    }

    std::cout << "Done! Output: " << outFile << "\n";
    return 0;
}
