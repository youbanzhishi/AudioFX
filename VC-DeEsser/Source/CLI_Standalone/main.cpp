// VC-DeEsser Standalone CLI - No JUCE Dependency
// De-esser / Sibilance Reduction
// Uses dr_wav for WAV I/O

//==============================================================================
// MUST define VC_STANDALONE before including DSP header
// MUST include dr_wav implementation before DSP header
//==============================================================================
#define DR_WAV_IMPLEMENTATION

#include <dr_wav.h>

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <set>

// Include DSP header AFTER DR_WAV_IMPLEMENTATION and VC_STANDALONE
#include "../DSP/VCPluginDSP.h"

//==============================================================================
// Plugin-specific presets and parameters
//==============================================================================
struct Preset {
    const char* name;
    VCDeEsserDSP::Params params;
};

static const Preset presets[] = {
    {"bypass", {-20.0f, 6000.0f, -10.0f, 100.0f, false}},
    {"mild", {-18.0f, 6000.0f, -6.0f, 100.0f, true}},
    {"moderate", {-20.0f, 7000.0f, -12.0f, 100.0f, true}},
    {"heavy", {-24.0f, 8000.0f, -20.0f, 100.0f, true}},
};

//==============================================================================
// Help text
//==============================================================================
void printHelp(const char* progName) {
    std::cout << "VC-DeEsser Standalone CLI (No JUCE)\n\n";
    std::cout << "Usage: " << progName << " <input.wav> <output.wav> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --help, -h           Show this help\n";
    std::cout << "  --preset <name>      Preset (bypass, mild, moderate, heavy)\n";
    std::cout << "  --threshold <dB>     Trigger threshold dB (default: -20)\n";
    std::cout << "  --frequency <Hz>     Sibilance detection frequency Hz (default: 6000)\n";
    std::cout << "  --reduction <dB>     Maximum attenuation dB (default: -10)\n";
    std::cout << "  --mix <0-100>        Dry/Wet mix percentage (default: 100)\n";
    std::cout << "  --bypass <0|1>       Bypass processing (default: 0)\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << progName << " in.wav out.wav --preset moderate\n";
    std::cout << "  " << progName << " in.wav out.wav --threshold -24 --frequency 7000\n";
}

//==============================================================================
// Parse command line arguments
//==============================================================================
std::map<std::string, std::string> parseArgs(int argc, char** argv) {
    std::map<std::string, std::string> args;
    std::set<std::string> noValueFlags = {"--help", "-h"};
    // Phase 19: isOption() distinguishes flags from negative numbers
    auto isOption = [](const std::string& s) -> bool {
        if (s.size() < 2) return false;
        if (s.substr(0, 2) == "--") return true;       // long option: --flag
        if (s == "-h") return true;                     // short help
        if (s[0] == '-' && s.size() > 1 && !std::isdigit(static_cast<unsigned char>(s[1]))) return true; // -x
        return false;  // -20, -3.5 etc. are negative numbers, not options
    };
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--") break;  // Phase 19: stop option parsing on "--"
        if (arg == "--help" || arg == "-h") {
            args["--help"] = "";
        } else if (arg.substr(0, 2) == "--") {
            std::string key = arg;
            std::string value;
            if (noValueFlags.count(key) == 0 && i + 1 < argc) {
                // Phase 19 fix: only consume next arg as value if it's not another option
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
bool loadPreset(const std::string& name, VCDeEsserDSP::Params& p) {
    for (const auto& preset : presets) {
        if (name == preset.name) {
            p = preset.params;
            return true;
        }
    }
    return false;
}


float getFloatArg(const std::map<std::string, std::string>& args, const std::string& key, float defaultVal) {
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
    // Phase 19: smarter file extraction using parseArgs results
    std::set<std::string> consumedValues;
    for (const auto& kv : args) { if (!kv.second.empty()) consumedValues.insert(kv.second); }
    std::vector<std::string> files;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a.substr(0, 2) == "--" || a == "-h") continue;  // skip options
        if (args.count(a) || consumedValues.count(a)) continue;  // skip consumed args
        files.push_back(a);
    }

    if (files.size() < 2) {
        std::cerr << "Error: Need input and output files\n\n";
        printHelp(argv[0]);
        return 1;
    }

    std::string inFile = files[0];
    std::string outFile = files[1];

    std::cout << "VC-DeEsser Standalone CLI (No JUCE)\n";
    std::cout << "Input: " << inFile << "\n";
    std::cout << "Output: " << outFile << "\n";

    //============================================================================
    // Read audio file using dr_wav
    //============================================================================
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
    std::cout << "Total frames: " << totalFrames << "\n";

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

    //============================================================================
    // Initialize DSP
    //============================================================================
    VCDeEsserDSP dsp;
    dsp.prepare(sampleRate, 4096);

    VCDeEsserDSP::Params params;

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
    params.threshold = getFloatArg(args, "--threshold", params.threshold);
    if (args.count("--threshold")) { dsp.setParams(params); std::cout << "Threshold: " << params.threshold << " dB\n"; }

    params.frequency = getFloatArg(args, "--frequency", params.frequency);
    if (args.count("--frequency")) { dsp.setParams(params); std::cout << "Frequency: " << params.frequency << " Hz\n"; }

    params.reduction = getFloatArg(args, "--reduction", params.reduction);
    if (args.count("--reduction")) { dsp.setParams(params); std::cout << "Reduction: " << params.reduction << " dB\n"; }

    params.mix = getFloatArg(args, "--mix", params.mix);
    if (args.count("--mix")) { dsp.setParams(params); std::cout << "Mix: " << params.mix << "%\n"; }

    if (args.count("--bypass")) {
        params.enabled = (args["--bypass"] == "1");
        dsp.setEnabled(params.enabled);
        std::cout << "Bypass: " << (params.enabled ? "off" : "on") << "\n";
    }

    //============================================================================
    // Process audio
    //============================================================================
    std::cout << "Processing...\n";
    dsp.process(left.data(), right.data(), static_cast<int>(totalFrames));

    //============================================================================
    // Write output file using dr_wav
    //============================================================================
    drwav_data_format format;
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
    format.channels = channels >= 2 ? 2 : 1;
    format.sampleRate = sampleRate;
    format.bitsPerSample = 32;  // 32-bit float

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

    std::cout << "Done! Output: " << outFile << "\n";
    return 0;
}
