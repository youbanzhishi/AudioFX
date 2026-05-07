// VC-Distortion Standalone CLI - No JUCE Dependency
// Uses dr_wav for WAV I/O

#define DR_WAV_IMPLEMENTATION
#include <dr_wav.h>

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <cmath>

#include "../DSP/VCDistortionDSP.h"

//==============================================================================
// Presets
//==============================================================================
struct Preset {
    const char* name;
    VCPluginDSP::Params params;
};

static const Preset presets[] = {
    {"tube-light",    {0, 0.3f, 1.0f, 0.5f, 0.0f, true}},
    {"tube-drive",    {0, 0.8f, 1.0f, 0.5f, 0.0f, true}},
    {"tape-saturate", {1, 0.5f, 1.0f, 0.6f, 0.0f, true}},
    {"transistor",    {2, 0.6f, 1.0f, 0.5f, 0.0f, true}},
    {"fuzz-heavy",    {3, 0.7f, 1.0f, 0.3f, -6.0f, true}},
    {"bitcrush-lofi", {4, 0.7f, 1.0f, 0.4f, 0.0f, true}},
};

//==============================================================================
// Help text
//==============================================================================
void printHelp(const char* progName) {
    std::cout << "VC-Distortion Standalone CLI\n\n";
    std::cout << "Usage: " << progName << " <input.wav> <output.wav> [options]\n\n";
    std::cout << "Distortion types:\n";
    std::cout << "  0=Tube  1=Tape  2=Transistor  3=Fuzz  4=BitCrush\n\n";
    std::cout << "Options:\n";
    std::cout << "  --help              Show this help\n";
    std::cout << "  --preset <name>     Preset name\n";
    std::cout << "  --type <0-4>        Distortion type (default: 0)\n";
    std::cout << "  --drive <0-100>     Drive amount (default: 50)\n";
    std::cout << "  --mix <0-100>       Dry/Wet mix % (default: 100)\n";
    std::cout << "  --tone <0-100>      Tone: 0=dark 100=bright (default: 50)\n";
    std::cout << "  --makeup <-30-30>   Makeup gain in dB (default: 0)\n\n";
    std::cout << "Presets:\n";
    for (const auto& p : presets) {
        std::cout << "  " << p.name << "\n";
    }
    std::cout << "\nExamples:\n";
    std::cout << "  " << progName << " in.wav out.wav --type 0 --drive 80 --mix 75\n";
    std::cout << "  " << progName << " in.wav out.wav --preset fuzz-heavy\n";
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

    std::cout << "VC-Distortion Standalone CLI\n";
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
    if (args.count("--type")) {
        params.type = std::stoi(args["--type"]);
        params.type = std::clamp(params.type, 0, 4);
        dsp.setParams(params);
        const char* typeNames[] = {"Tube", "Tape", "Transistor", "Fuzz", "BitCrush"};
        std::cout << "Type: " << typeNames[params.type] << "\n";
    }

    if (args.count("--drive")) {
        params.drive = std::stof(args["--drive"]) / 100.0f;
        params.drive = std::clamp(params.drive, 0.0f, 1.0f);
        dsp.setParams(params);
        std::cout << "Drive: " << (int)(params.drive * 100) << "%\n";
    }

    if (args.count("--mix")) {
        params.mix = std::stof(args["--mix"]) / 100.0f;
        params.mix = std::clamp(params.mix, 0.0f, 1.0f);
        dsp.setParams(params);
        std::cout << "Mix: " << (int)(params.mix * 100) << "%\n";
    }

    if (args.count("--tone")) {
        params.tone = std::stof(args["--tone"]) / 100.0f;
        params.tone = std::clamp(params.tone, 0.0f, 1.0f);
        dsp.setParams(params);
        std::cout << "Tone: " << (int)(params.tone * 100) << "%\n";
    }

    if (args.count("--makeup")) {
        params.makeup = std::stof(args["--makeup"]);
        params.makeup = std::clamp(params.makeup, -30.0f, 30.0f);
        dsp.setParams(params);
        std::cout << "Makeup: " << params.makeup << " dB\n";
    }

    //==========================================================================
    // Process audio
    //==========================================================================
    std::cout << "Processing...\n";
    dsp.process(left.data(), right.data(), static_cast<int>(totalFrames));

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

    std::cout << "Done! Output: " << outFile << "\n";
    return 0;
}
