// VC-DynamicEQ CLI - JUCE-based Command Line Tool
// Dynamic Equalizer: EQ + Compressor on specific frequency band

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cmath>

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include "../DSP/VCPluginDSP.h"

//==============================================================================
// Dynamic EQ Presets
//==============================================================================
//==============================================================================
// Help text
//==============================================================================
void printHelp(const char* progName) {
    std::cout << "VC-DynamicEQ CLI (JUCE Version)\n";
    std::cout << "Dynamic Equalizer - EQ + Compressor on specific frequency band\n\n";
    std::cout << "Usage: " << progName << " <input.wav> <output.wav> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --help, -h            Show this help\n";
    std::cout << "  --frequency <Hz>     Center frequency (20~20000)\n";
    std::cout << "  --q <value>           Q value (0.1~10)\n";
    std::cout << "  --threshold <dB>      Dynamic threshold (-48~0)\n";
    std::cout << "  --range <dB>          Dynamic range (-24~+24, negative=attenuate)\n";
    std::cout << "  --attack <ms>         Attack time (0.1~50)\n";
    std::cout << "  --release <ms>        Release time (10~500)\n";
    std::cout << "  --mix <0-100>         Dry/Wet mix percentage\n";
    std::cout << "  --bypass <0|1>       Bypass processing\n\n";
    std::cout << "Examples:\n";
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

    std::cout << "VC-DynamicEQ CLI (JUCE Version)\n";
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

    // Override with command line parameters
    if (args.count("--frequency")) {
        params.frequency = std::stof(args["--frequency"]);
        dsp.setParams(params);
        std::cout << "Frequency: " << params.frequency << " Hz\n";
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
            32,  // bits per sample (32-bit float)
            {},  // metadata
            0    // quality
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
