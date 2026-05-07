// VC-DynamicEQ Standalone CLI - No JUCE Dependency
// Dynamic Equalizer: EQ + Compressor on specific frequency band
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

// Include DSP header AFTER DR_WAV_IMPLEMENTATION and VC_STANDALONE
#include "../DSP/VCPluginDSP.h"

//==============================================================================
// Dynamic EQ Presets
//==============================================================================
struct Preset {
    const char* name;
    VCPluginDSP::Params params;
};

static const Preset presets[] = {
    {"bypass", {200.0f, -6.0f, 1.0f, -12.0f, -12.0f, 10.0f, 100.0f, 100.0f, false}},
    {"de-boom", {150.0f, 0.0f, 2.0f, -18.0f, -12.0f, 10.0f, 150.0f, 100.0f, true}},
    {"de-harsh", {3500.0f, 0.0f, 1.5f, -15.0f, -8.0f, 5.0f, 80.0f, 100.0f, true}},
    {"presence-boost", {4000.0f, 3.0f, 1.0f, -10.0f, 6.0f, 15.0f, 120.0f, 100.0f, true}},
};

//==============================================================================
// Help text
//==============================================================================
void printHelp(const char* progName) {
    std::cout << "VC-DynamicEQ Standalone CLI (No JUCE)\n";
    std::cout << "Dynamic Equalizer - EQ + Compressor on specific frequency band\n\n";
    std::cout << "Usage: " << progName << " <input.wav> <output.wav> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --help, -h            Show this help\n";
    std::cout << "  --preset <name>       Preset (bypass, de-boom, de-harsh, presence-boost)\n";
    std::cout << "  --frequency <Hz>      Center frequency (20~20000)\n";
    std::cout << "  --gain <dB>           Static gain (-18~+18)\n";
    std::cout << "  --q <value>           Q value (0.1~10)\n";
    std::cout << "  --threshold <dB>      Dynamic threshold (-48~0)\n";
    std::cout << "  --range <dB>          Dynamic range (-24~+24, negative=attenuate)\n";
    std::cout << "  --attack <ms>         Attack time (0.1~50)\n";
    std::cout << "  --release <ms>        Release time (10~500)\n";
    std::cout << "  --mix <0-100>         Dry/Wet mix percentage\n";
    std::cout << "  --bypass <0|1>        Bypass processing\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << progName << " in.wav out.wav --preset de-boom\n";
    std::cout << "  " << progName << " in.wav out.wav --frequency 200 --gain -6 --threshold -12\n";
}

//==============================================================================
// Parse command line arguments
//==============================================================================
std::map<std::string, std::string> parseArgs(int argc, char** argv) {
    std::map<std::string, std::string> args;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            args["--help"] = "";
        } else if (arg.substr(0, 2) == "--") {
            std::string key = arg;
            std::string value;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
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

    std::cout << "VC-DynamicEQ Standalone CLI (No JUCE)\n";
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
    if (args.count("--frequency")) {
        params.frequency = std::stof(args["--frequency"]);
        dsp.setParams(params);
        std::cout << "Frequency: " << params.frequency << " Hz\n";
    }

    if (args.count("--gain")) {
        params.gain = std::stof(args["--gain"]);
        dsp.setParams(params);
        std::cout << "Gain: " << params.gain << " dB\n";
    }

    if (args.count("--q")) {
        params.q = std::stof(args["--q"]);
        dsp.setParams(params);
        std::cout << "Q: " << params.q << "\n";
    }

    if (args.count("--threshold")) {
        params.threshold = std::stof(args["--threshold"]);
        dsp.setParams(params);
        std::cout << "Threshold: " << params.threshold << " dB\n";
    }

    if (args.count("--range")) {
        params.range = std::stof(args["--range"]);
        dsp.setParams(params);
        std::cout << "Range: " << params.range << " dB\n";
    }

    if (args.count("--attack")) {
        params.attack = std::stof(args["--attack"]);
        dsp.setParams(params);
        std::cout << "Attack: " << params.attack << " ms\n";
    }

    if (args.count("--release")) {
        params.release = std::stof(args["--release"]);
        dsp.setParams(params);
        std::cout << "Release: " << params.release << " ms\n";
    }

    if (args.count("--mix")) {
        params.mix = std::stof(args["--mix"]);
        dsp.setParams(params);
        std::cout << "Mix: " << params.mix << "%\n";
    }

    if (args.count("--bypass")) {
        params.enabled = (args["--bypass"] != "1");
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
