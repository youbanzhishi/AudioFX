// VC-MultiBand CLI - JUCE-based Command Line Tool
// Requires JUCE library for WAV I/O
// 4-band LR4 Linkwitz-Riley crossover with per-band gain & compression

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <sstream>
#include <algorithm>

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

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
};

//==============================================================================
// Help text
//==============================================================================
void printHelp(const char* progName) {
    std::cout << "VC-MultiBand CLI (JUCE Version)\n";
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
}

//==============================================================================
// Parse comma-separated float values
//==============================================================================
std::vector<float> parseFloatList(const std::string& s) {
    std::vector<float> result;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
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

    std::cout << "VC-MultiBand CLI (JUCE Version)\n";
    std::cout << "Input: " << inFile << "\n";
    std::cout << "Output: " << outFile << "\n";

    //============================================================================
    // Read audio file using JUCE
    //============================================================================
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(
        fm.createReaderFor(juce::File(inFile)));

    if (!reader) {
        std::cerr << "Error: Cannot read file: " << inFile << "\n";
        return 1;
    }

    std::cout << "Sample rate: " << reader->sampleRate << " Hz\n";
    std::cout << "Channels: " << reader->numChannels << "\n";
    std::cout << "Duration: " << reader->lengthInSamples / reader->sampleRate << " seconds\n";

    // Read audio data
    juce::AudioBuffer<float> buffer(
        static_cast<int>(reader->numChannels),
        static_cast<int>(reader->lengthInSamples));

    reader->read(
        buffer.getArrayOfWritePointers(),
        buffer.getNumChannels(),
        0,
        buffer.getNumSamples());

    //============================================================================
    // Initialize DSP
    //============================================================================
    VCPluginDSP dsp;
    dsp.prepare(reader->sampleRate, 4096);

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

    // Override crossover frequencies
    if (args.count("--xover1")) { params.xover1 = std::stof(args["--xover1"]); }
    if (args.count("--xover2")) { params.xover2 = std::stof(args["--xover2"]); }
    if (args.count("--xover3")) { params.xover3 = std::stof(args["--xover3"]); }

    // Override per-band gain
    if (args.count("--band-gain")) {
        auto values = parseFloatList(args["--band-gain"]);
        if (values.size() != 4) {
            std::cerr << "Error: --band-gain requires 4 values\n";
            return 1;
        }
        for (int i = 0; i < 4; ++i) params.bandGain[i] = values[i];
    }

    // Override per-band threshold
    if (args.count("--band-threshold")) {
        auto values = parseFloatList(args["--band-threshold"]);
        if (values.size() != 4) {
            std::cerr << "Error: --band-threshold requires 4 values\n";
            return 1;
        }
        for (int i = 0; i < 4; ++i) params.bandThreshold[i] = values[i];
    }

    // Override per-band ratio
    if (args.count("--band-ratio")) {
        auto values = parseFloatList(args["--band-ratio"]);
        if (values.size() != 4) {
            std::cerr << "Error: --band-ratio requires 4 values\n";
            return 1;
        }
        for (int i = 0; i < 4; ++i) params.bandRatio[i] = values[i];
    }

    // Solo band
    if (args.count("--solo-band")) {
        params.soloBand = std::stoi(args["--solo-band"]);
    }

    // Mute bands
    int muteBands = parseMuteBands(argc, argv);
    if (muteBands != 0) {
        params.muteBands = muteBands;
    }

    // Bypass
    if (args.count("--bypass")) {
        params.enabled = (args["--bypass"] == "1");
        dsp.setEnabled(params.enabled);
    }

    // Apply all parameters
    dsp.setParams(params);

    //============================================================================
    // Process audio
    //============================================================================
    std::cout << "Processing...\n";

    if (buffer.getNumChannels() >= 2) {
        dsp.process(buffer.getWritePointer(0), buffer.getWritePointer(1),
                   buffer.getNumSamples());
    } else if (buffer.getNumChannels() == 1) {
        auto* data = buffer.getWritePointer(0);
        dsp.process(data, data, buffer.getNumSamples());
    }

    //============================================================================
    // Write output file using JUCE
    //============================================================================
    juce::WavAudioFormat wavFmt;
    std::unique_ptr<juce::AudioFormatWriter> writer(
        wavFmt.createWriterFor(
            new juce::FileOutputStream(juce::File(outFile)),
            reader->sampleRate,
            static_cast<unsigned int>(buffer.getNumChannels()),
            32,
            {},
            0
        )
    );

    if (!writer) {
        std::cerr << "Error: Cannot write file: " << outFile << "\n";
        return 1;
    }

    writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());

    std::cout << "Done! Output: " << outFile << "\n";
    return 0;
}
