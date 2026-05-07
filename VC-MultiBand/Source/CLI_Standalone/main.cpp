// VC-MultiBand Standalone CLI - No JUCE Dependency
// Uses dr_wav for WAV I/O
// 4-band LR4 Linkwitz-Riley crossover with per-band gain & compression

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
#include <sstream>
#include <algorithm>

// Include DSP header AFTER DR_WAV_IMPLEMENTATION and VC_STANDALONE
#include "../DSP/VCPluginDSP.h"

//==============================================================================
// Plugin-specific presets
//==============================================================================
struct Preset {
    const char* name;
    const char* description;
    VCPluginDSP::Params params;
};

static const Preset presets[] = {
    {"bypass", "No processing - pass through",
     {120.0f, 1000.0f, 8000.0f,
      {0.0f, 0.0f, 0.0f, 0.0f},
      {0.0f, 0.0f, 0.0f, 0.0f},
      {1.0f, 1.0f, 1.0f, 1.0f},
      0, 0, true}},
    {"de-ess", "De-esser: compress high band",
     {120.0f, 1000.0f, 6000.0f,
      {0.0f, 0.0f, 0.0f, -2.0f},
      {0.0f, 0.0f, 0.0f, -20.0f},
      {1.0f, 1.0f, 1.0f, 4.0f},
      0, 0, true}},
    {"loudness-plus", "Boost lows and highs (loudness contour)",
     {100.0f, 800.0f, 6000.0f,
      {4.0f, 0.0f, 0.0f, 3.0f},
      {0.0f, 0.0f, 0.0f, 0.0f},
      {1.0f, 1.0f, 1.0f, 1.0f},
      0, 0, true}},
    {"vocal-balance", "Vocal: boost mid-high, tame low",
     {120.0f, 500.0f, 4000.0f,
      {-3.0f, 0.0f, 2.0f, 0.0f},
      {-10.0f, 0.0f, 0.0f, 0.0f},
      {2.0f, 1.0f, 1.0f, 1.0f},
      0, 0, true}},
    {"solo-low", "Listen to low band only",
     {120.0f, 1000.0f, 8000.0f,
      {0.0f, 0.0f, 0.0f, 0.0f},
      {0.0f, 0.0f, 0.0f, 0.0f},
      {1.0f, 1.0f, 1.0f, 1.0f},
      1, 0, true}},
    {"mute-high", "Remove high frequency band",
     {120.0f, 1000.0f, 8000.0f,
      {0.0f, 0.0f, 0.0f, 0.0f},
      {0.0f, 0.0f, 0.0f, 0.0f},
      {1.0f, 1.0f, 1.0f, 1.0f},
      0, 0x8, true}},
};

//==============================================================================
// Help text
//==============================================================================
void printHelp(const char* progName) {
    std::cout << "VC-MultiBand Standalone CLI (No JUCE)\n";
    std::cout << "4-band LR4 Linkwitz-Riley crossover with per-band gain & compression\n\n";
    std::cout << "Usage: " << progName << " <input.wav> <output.wav> [options]\n\n";
    std::cout << "Crossover Options:\n";
    std::cout << "  --xover1 <Hz>        Low/Mid-Low crossover (default: 120)\n";
    std::cout << "  --xover2 <Hz>        Mid-Low/Mid-High crossover (default: 1000)\n";
    std::cout << "  --xover3 <Hz>        Mid-High/High crossover (default: 8000)\n\n";
    std::cout << "Per-Band Options (comma-separated, 4 values: Low,Mid-Low,Mid-High,High):\n";
    std::cout << "  --band-gain <dB,...>     Per-band gain (default: 0,0,0,0)\n";
    std::cout << "  --band-threshold <dB,...> Per-band compressor threshold (default: 0,0,0,0)\n";
    std::cout << "  --band-ratio <r,...>    Per-band compressor ratio (default: 1,1,1,1)\n\n";
    std::cout << "Solo/Mute Options:\n";
    std::cout << "  --solo-band <1-4>    Solo a specific band (0=none)\n";
    std::cout << "  --mute-band <1-4>    Mute a specific band (can repeat)\n\n";
    std::cout << "General Options:\n";
    std::cout << "  --help, -h           Show this help\n";
    std::cout << "  --preset <name>      Load preset\n";
    std::cout << "  --bypass <0|1>       Bypass processing (default: 0)\n\n";
    std::cout << "Available Presets:\n";
    for (const auto& p : presets) {
        std::cout << "  " << p.name << " - " << p.description << "\n";
    }
    std::cout << "\nExamples:\n";
    std::cout << "  " << progName << " in.wav out.wav --preset de-ess\n";
    std::cout << "  " << progName << " in.wav out.wav --band-gain -3,0,+2,0\n";
    std::cout << "  " << progName << " in.wav out.wav --solo-band 2\n";
    std::cout << "  " << progName << " in.wav out.wav --mute-band 4 --xover3 6000\n";
    std::cout << "  " << progName << " in.wav out.wav --band-threshold 0,0,-10,-20 --band-ratio 1,1,3,4\n";
}

//==============================================================================
// Parse comma-separated float values
//==============================================================================
std::vector<float> parseFloatList(const std::string& s) {
    std::vector<float> result;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
        // Trim whitespace
        item.erase(0, item.find_first_not_of(" \t"));
        item.erase(item.find_last_not_of(" \t") + 1);
        if (!item.empty()) {
            result.push_back(std::stof(item));
        }
    }
    return result;
}

//==============================================================================
// Parse command line arguments
//==============================================================================
std::map<std::string, std::string> parseArgs(int argc, char** argv) {
    std::map<std::string, std::string> args;
    std::set<std::string> noValueFlags = {"--help", "-h"};
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
// Parse mute-band arguments (can appear multiple times)
//==============================================================================
int parseMuteBands(int argc, char** argv) {
    int muteBands = 0;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--mute-band" && i + 1 < argc) {
            int band = std::stoi(argv[++i]);
            if (band >= 1 && band <= 4) {
                muteBands |= (1 << (band - 1));
            }
        }
    }
    return muteBands;
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

    std::cout << "VC-MultiBand Standalone CLI (No JUCE)\n";
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
            std::cerr << "Error: Unknown preset '" << presetName << "'\n";
            std::cerr << "Available: bypass, de-ess, loudness-plus, vocal-balance, solo-low, mute-high\n";
            return 1;
        }
        dsp.setParams(params);
    }

    // Override crossover frequencies
    if (args.count("--xover1")) {
        params.xover1 = std::stof(args["--xover1"]);
        std::cout << "Xover1: " << params.xover1 << " Hz\n";
    }
    if (args.count("--xover2")) {
        params.xover2 = std::stof(args["--xover2"]);
        std::cout << "Xover2: " << params.xover2 << " Hz\n";
    }
    if (args.count("--xover3")) {
        params.xover3 = std::stof(args["--xover3"]);
        std::cout << "Xover3: " << params.xover3 << " Hz\n";
    }

    // Override per-band gain
    if (args.count("--band-gain")) {
        auto values = parseFloatList(args["--band-gain"]);
        if (values.size() != 4) {
            std::cerr << "Error: --band-gain requires 4 comma-separated values\n";
            return 1;
        }
        for (int i = 0; i < 4; ++i) {
            params.bandGain[i] = values[i];
        }
        std::cout << "Band gain: " << params.bandGain[0] << ", "
                  << params.bandGain[1] << ", " << params.bandGain[2] << ", "
                  << params.bandGain[3] << " dB\n";
    }

    // Override per-band threshold
    if (args.count("--band-threshold")) {
        auto values = parseFloatList(args["--band-threshold"]);
        if (values.size() != 4) {
            std::cerr << "Error: --band-threshold requires 4 comma-separated values\n";
            return 1;
        }
        for (int i = 0; i < 4; ++i) {
            params.bandThreshold[i] = values[i];
        }
        std::cout << "Band threshold: " << params.bandThreshold[0] << ", "
                  << params.bandThreshold[1] << ", " << params.bandThreshold[2] << ", "
                  << params.bandThreshold[3] << " dB\n";
    }

    // Override per-band ratio
    if (args.count("--band-ratio")) {
        auto values = parseFloatList(args["--band-ratio"]);
        if (values.size() != 4) {
            std::cerr << "Error: --band-ratio requires 4 comma-separated values\n";
            return 1;
        }
        for (int i = 0; i < 4; ++i) {
            params.bandRatio[i] = values[i];
        }
        std::cout << "Band ratio: " << params.bandRatio[0] << ", "
                  << params.bandRatio[1] << ", " << params.bandRatio[2] << ", "
                  << params.bandRatio[3] << ":1\n";
    }

    // Solo band
    if (args.count("--solo-band")) {
        int band = std::stoi(args["--solo-band"]);
        if (band < 0 || band > 4) {
            std::cerr << "Error: --solo-band must be 0-4\n";
            return 1;
        }
        params.soloBand = band;
        if (band > 0) {
            const char* names[] = {"Low", "Mid-Low", "Mid-High", "High"};
            std::cout << "Solo band: " << band << " (" << names[band-1] << ")\n";
        }
    }

    // Mute bands (can appear multiple times)
    int muteBands = parseMuteBands(argc, argv);
    if (muteBands != 0) {
        params.muteBands = muteBands;
        const char* names[] = {"Low", "Mid-Low", "Mid-High", "High"};
        std::cout << "Muted bands:";
        for (int b = 0; b < 4; ++b) {
            if (muteBands & (1 << b)) {
                std::cout << " " << (b+1) << "(" << names[b] << ")";
            }
        }
        std::cout << "\n";
    }

    // Bypass
    if (args.count("--bypass")) {
        params.enabled = (args["--bypass"] == "1");
        dsp.setEnabled(params.enabled);
        std::cout << "Bypass: " << (params.enabled ? "off" : "on") << "\n";
    }

    // Apply all parameters
    dsp.setParams(params);

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
