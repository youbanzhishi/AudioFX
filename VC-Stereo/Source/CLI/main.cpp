// VC-Stereo CLI - JUCE-based Command Line Tool
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

#include "../DSP/VCPluginDSP.h"

//==============================================================================
// Presets
//==============================================================================
struct Preset {
    const char* name;
    VCPluginDSP::Params params;
};

static const Preset presets[] = {
    {"bypass",        {100.0f, 0.0f, false, 150.0f, false}},
    {"mono",          {  0.0f, 0.0f, false, 150.0f, true }},
    {"wide",          {150.0f, 0.0f, false, 150.0f, true }},
    {"extra-wide",    {200.0f, 0.0f, false, 150.0f, true }},
    {"bass-mono",     {100.0f, 0.0f, true,  150.0f, true }},
    {"center-pan",    {100.0f, 0.0f, false, 150.0f, true }},
};

//==============================================================================
// Help / Arg parsing (same pattern as CLI_Standalone)
//==============================================================================
void printHelp(const char* progName) {
    std::cout << "VC-Stereo CLI (JUCE Version)\n\n";
    std::cout << "Usage: " << progName << " <input.wav> <output.wav> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --help, -h           Show this help\n";
    std::cout << "  --preset <name>      Preset\n";
    std::cout << "  --width <%>          Stereo width (0 ~ 200), default: 100\n";
    std::cout << "  --pan <-100~100>     Stereo pan, default: 0\n";
    std::cout << "  --mono-bass <0|1>    Mono bass, default: 0\n";
    std::cout << "  --bass-freq <Hz>     Bass crossover freq (50 ~ 300), default: 150\n";
    std::cout << "  --bypass <0|1>       Bypass, default: 0\n";
}

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

bool loadPreset(const std::string& name, VCPluginDSP::Params& p) {
    for (const auto& preset : presets) {
        if (name == preset.name) {
            p = preset.params;
            return true;
        }
    }
    return false;
}

float getFloatArg(const std::map<std::string, std::string>& args,
                  const std::string& key, float defaultVal) {
    auto it = args.find(key);
    if (it != args.end() && !it->second.empty()) {
        try { return std::stof(it->second); } catch (...) { return defaultVal; }
    }
    return defaultVal;
}

//==============================================================================
// Main
//==============================================================================
int main(int argc, char** argv) {
    if (argc < 2) { printHelp(argv[0]); return 1; }

    auto args = parseArgs(argc, argv);
    if (args.count("--help")) { printHelp(argv[0]); return 0; }

    std::vector<std::string> files;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] != '-') files.push_back(argv[i]);
    }
    if (files.size() < 2) {
        std::cerr << "Error: Need input and output files\n";
        return 1;
    }

    std::string inFile = files[0];
    std::string outFile = files[1];

    std::cout << "VC-Stereo CLI (JUCE Version)\n";

    // Read audio
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(juce::File(inFile)));
    if (!reader) {
        std::cerr << "Error: Cannot read file: " << inFile << "\n";
        return 1;
    }

    juce::AudioBuffer<float> buffer(
        static_cast<int>(reader->numChannels),
        static_cast<int>(reader->lengthInSamples));
    reader->read(buffer.getArrayOfWritePointers(), buffer.getNumChannels(),
                 0, buffer.getNumSamples());

    // Init DSP
    VCPluginDSP dsp;
    dsp.prepare(reader->sampleRate, 4096);
    VCPluginDSP::Params params;

    if (args.count("--preset")) {
        std::string presetName = args["--preset"];
        if (!loadPreset(presetName, params)) {
            std::cerr << "Error: Unknown preset\n"; return 1;
        }
    }

    params.width    = getFloatArg(args, "--width",    params.width);
    params.pan      = getFloatArg(args, "--pan",      params.pan);
    params.bassFreq = getFloatArg(args, "--bass-freq", params.bassFreq);

    if (args.count("--mono-bass")) params.monoBass = (args["--mono-bass"] == "1");
    if (args.count("--bypass"))    params.enabled   = (args["--bypass"] != "1");

    dsp.setParams(params);
    dsp.setEnabled(params.enabled);

    // Process
    std::cout << "Processing...\n";
    if (buffer.getNumChannels() >= 2) {
        dsp.process(buffer.getWritePointer(0), buffer.getWritePointer(1),
                   buffer.getNumSamples());
    }

    // Write output
    juce::WavAudioFormat wavFmt;
    std::unique_ptr<juce::AudioFormatWriter> writer(
        wavFmt.createWriterFor(
            new juce::FileOutputStream(juce::File(outFile)),
            reader->sampleRate,
            static_cast<unsigned int>(buffer.getNumChannels()),
            32, {}, 0));

    if (!writer) {
        std::cerr << "Error: Cannot write file: " << outFile << "\n";
        return 1;
    }
    writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());

    std::cout << "Done! Output: " << outFile << "\n";
    return 0;
}
