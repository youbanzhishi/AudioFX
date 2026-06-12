// VC-SurgicalDeEsser CLI - JUCE-based Command Line Tool
// Requires JUCE library for WAV I/O

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cmath>

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include "../DSP/VCSurgicalDeEsserDSP.h"

//==============================================================================
// Help text
//==============================================================================
void printHelp(const char* progName) {
    std::cout << "VC-SurgicalDeEsser CLI - Surgical De-Esser Tool\n\n";
    std::cout << "Usage: " << progName << " <input.wav> <output.wav> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --help, -h           Show this help\n";
    std::cout << "  --threshold <-60-0>  Detection threshold dBFS (default: -30)\n";
    std::cout << "  --reduction <0-20>   Max attenuation dB (default: 6)\n";
    std::cout << "  --freq-low <2000-8000>  Detection band low Hz (default: 5000)\n";
    std::cout << "  --freq-high <5000-14000> Detection band high Hz (default: 9000)\n";
    std::cout << "  --bypass <0|1>       Bypass processing (default: 0)\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << progName << " in.wav out.wav --threshold -25 --reduction 8\n";
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

    std::cout << "VC-SurgicalDeEsser CLI (JUCE Version)\n";
    std::cout << "Input: " << inFile << "\n";
    std::cout << "Output: " << outFile << "\n";

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
    dsp.prepare(reader->sampleRate, buffer.getNumSamples());

    VCPluginDSP::Params params;

    if (args.count("--threshold")) {
        params.threshold = std::stof(args["--threshold"]);
        std::cout << "Threshold: " << params.threshold << " dBFS\n";
    }

    if (args.count("--reduction")) {
        params.reduction = std::stof(args["--reduction"]);
        std::cout << "Reduction: " << params.reduction << " dB\n";
    }

    if (args.count("--freq-low")) {
        params.freqLow = std::stof(args["--freq-low"]);
        std::cout << "Freq Low: " << params.freqLow << " Hz\n";
    }

    if (args.count("--freq-high")) {
        params.freqHigh = std::stof(args["--freq-high"]);
        std::cout << "Freq High: " << params.freqHigh << " Hz\n";
    }

    if (args.count("--bypass")) {
        params.enabled = (args["--bypass"] != "1");
        std::cout << "Bypass: " << (!params.enabled ? "on" : "off") << "\n";
    }

    dsp.setParams(params);

    //============================================================================
    // Process audio (two-pass: detect then process)
    //============================================================================
    std::cout << "Processing...\n";

    if (buffer.getNumChannels() >= 2) {
        dsp.detectSibilance(buffer.getWritePointer(0), buffer.getWritePointer(1),
                           buffer.getNumSamples());
        dsp.processSibilance(buffer.getWritePointer(0), buffer.getWritePointer(1),
                            buffer.getNumSamples());
    } else if (buffer.getNumChannels() == 1) {
        auto* data = buffer.getWritePointer(0);
        dsp.detectSibilance(data, data, buffer.getNumSamples());
        dsp.processSibilance(data, data, buffer.getNumSamples());
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
