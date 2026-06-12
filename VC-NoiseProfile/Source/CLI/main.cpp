// VC-NoiseProfile CLI - JUCE-based Command Line Tool
// Noise Profile Analysis + Adaptive Spectral Subtraction + Noise Gate
// Requires JUCE library for WAV I/O

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <cmath>
#include <algorithm>
#include <iomanip>

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include "../DSP/VCPluginDSP.h"

//==============================================================================
// Presets
//==============================================================================
//==============================================================================
// Help text
//==============================================================================
void printHelp(const char* progName) {
    std::cout << "VC-NoiseProfile CLI - Noise Analysis & Reduction\n\n";
    std::cout << "Usage: " << progName << " <input.wav> <output.wav> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --help, -h             Show this help\n";
    std::cout << "                         gate-only, denoise-gate, analyze-only)\n";
    std::cout << "  --mode <0-3>           0=denoise 1=gate 2=both 3=analyze (default: 2)\n";
    std::cout << "  --reduction <dB>       Spectral subtraction 0-30 dB (default: 10)\n";
    std::cout << "  --floor <1-20>         Spectral floor ratio (default: 5)\n";
    std::cout << "  --learn-ms <ms>        Learn noise from first N ms 100-5000 (default: 500)\n";
    std::cout << "  --threshold <dB>       Gate threshold -80~0 dB (default: -40)\n";
    std::cout << "  --attack <ms>          Gate attack 0.1-100 ms (default: 5)\n";
    std::cout << "  --release <ms>         Gate release 1-1000 ms (default: 50)\n";
    std::cout << "  --bypass <0|1>         Bypass processing (default: 0)\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << progName << " noisy.wav clean.wav --mode 0 --reduction 12 --learn-ms 1000\n";
    std::cout << "  " << progName << " noisy.wav analysis.wav --mode 3 --learn-ms 2000\n";
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
// Load preset
//==============================================================================
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

    std::cout << "VC-NoiseProfile CLI (JUCE Version)\n";
    std::cout << "Input:  " << inFile << "\n";
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

    // Override with command line parameters
    if (args.count("--mode")) {
        params.mode = std::clamp(std::stoi(args["--mode"]), 0, 3);
        dsp.setParams(params);
        const char* modeNames[] = {"Denoise", "Gate", "Both", "Analyze"};
        std::cout << "Mode: " << modeNames[params.mode] << "\n";
    }

    if (args.count("--reduction")) {
        params.reduction = std::clamp(std::stof(args["--reduction"]), 0.0f, 30.0f);
        dsp.setParams(params);
        std::cout << "Reduction: " << params.reduction << " dB\n";
    }

    if (args.count("--floor")) {
        params.floor = std::clamp(std::stof(args["--floor"]), 1.0f, 20.0f);
        dsp.setParams(params);
        std::cout << "Floor: " << params.floor << "\n";
    }

    if (args.count("--learn-ms")) {
        params.learnMs = std::clamp(std::stof(args["--learn-ms"]), 100.0f, 5000.0f);
        dsp.setParams(params);
        std::cout << "Learn time: " << params.learnMs << " ms\n";
    }

    if (args.count("--threshold")) {
        params.threshold = std::clamp(std::stof(args["--threshold"]), -80.0f, 0.0f);
        dsp.setParams(params);
        std::cout << "Threshold: " << params.threshold << " dB\n";
    }

    if (args.count("--attack")) {
        params.attack = std::clamp(std::stof(args["--attack"]), 0.1f, 100.0f);
        dsp.setParams(params);
        std::cout << "Attack: " << params.attack << " ms\n";
    }

    if (args.count("--release")) {
        params.release = std::clamp(std::stof(args["--release"]), 1.0f, 1000.0f);
        dsp.setParams(params);
        std::cout << "Release: " << params.release << " ms\n";
    }

    if (args.count("--bypass")) {
        bool bypass = (args["--bypass"] == "1");
        dsp.setEnabled(!bypass);
        std::cout << "Bypass: " << (bypass ? "on" : "off") << "\n";
    }

    //============================================================================
    // Learn noise profile from first N ms, then process
    //============================================================================
    std::cout << "\nProcessing...\n";

    // Step 1: Learn noise profile
    int learnSamples = static_cast<int>(params.learnMs * 0.001f * reader->sampleRate);
    learnSamples = std::min(learnSamples, buffer.getNumSamples());

    // Use mono mix for profile learning
    std::vector<float> monoMix(learnSamples);
    if (buffer.getNumChannels() >= 2) {
        for (int i = 0; i < learnSamples; ++i) {
            monoMix[i] = 0.5f * (buffer.getSample(0, i) + buffer.getSample(1, i));
        }
    } else {
        for (int i = 0; i < learnSamples; ++i) {
            monoMix[i] = buffer.getSample(0, i);
        }
    }

    dsp.learnNoiseProfile(monoMix.data(), learnSamples, 1);

    if (dsp.hasNoiseProfile()) {
        std::cout << "Noise profile learned successfully.\n";
    } else {
        std::cout << "Warning: Noise profile learning may have insufficient data.\n";
    }

    // Step 2: Process audio
    if (buffer.getNumChannels() >= 2) {
        dsp.processWithProfile(buffer.getWritePointer(0), buffer.getWritePointer(1),
                               buffer.getNumSamples());
    } else if (buffer.getNumChannels() == 1) {
        auto* data = buffer.getWritePointer(0);
        dsp.processWithProfile(data, data, buffer.getNumSamples());
    }

    //============================================================================
    // Write output file using JUCE
    // IMPORTANT: createWriterFor takes 6 parameters!
    // - Parameter 5: metadata (MUST be StringPairArray, NOT StringArray!)
    // - Parameter 6: quality (0 for PCM formats)
    //============================================================================
    juce::WavAudioFormat wavFmt;
    std::unique_ptr<juce::AudioFormatWriter> writer(
        wavFmt.createWriterFor(
            new juce::FileOutputStream(juce::File(outFile)),
            reader->sampleRate,
            static_cast<unsigned int>(buffer.getNumChannels()),
            32,  // bits per sample (32-bit float)
            {},  // metadata - MUST be StringPairArray, not StringArray!
            0    // quality - 0 for PCM formats
        )
    );

    if (!writer) {
        std::cerr << "Error: Cannot write file: " << outFile << "\n";
        return 1;
    }

    writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());

    std::cout << "\nDone! Output: " << outFile << "\n";
    return 0;
}
