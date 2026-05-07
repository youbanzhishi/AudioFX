//==============================================================================
// VC-PitchShift Standalone CLI - High-Quality Pitch Shifting (Phase Vocoder)
// No JUCE dependency - Uses dr_wav for WAV I/O
//
// Parameters:
//   --semitones  -12~+12       Pitch shift in semitones (default: 0)
//   --cents      -100~+100     Micro-tuning in cents (default: 0)
//   --formant    0|1           Formant preservation (default: 0)
//   --bypass     0|1           Bypass processing (default: 0)
//
// Presets: bypass, up1, down1, up3, down3, octave-up, octave-down, formant-shift
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
// Plugin-specific presets
//==============================================================================
struct Preset {
    const char* name;
    VCPluginDSP::Params params;
};

static const Preset presets[] = {
    // name,            semitones, cents,  formant, enabled
    {"bypass",          { 0,        0.0f,   false,  false}},
    {"up1",             { 1,        0.0f,   false,  true }},
    {"down1",           {-1,        0.0f,   false,  true }},
    {"up3",             { 3,        0.0f,   false,  true }},
    {"down3",           {-3,        0.0f,   false,  true }},
    {"octave-up",       {12,        0.0f,   false,  true }},
    {"octave-down",     {-12,       0.0f,   false,  true }},
    {"formant-shift",   { 0,        0.0f,   true,   true }},
};

//==============================================================================
// Help text
//==============================================================================
void printHelp(const char* progName) {
    std::cout << "VC-PitchShift Standalone CLI - Phase Vocoder Pitch Shifting (No JUCE)\n\n";
    std::cout << "Usage: " << progName << " <input.wav> <output.wav> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --help, -h           Show this help\n";
    std::cout << "  --preset <name>      Preset (bypass, up1, down1, up3, down3,\n";
    std::cout << "                       octave-up, octave-down, formant-shift)\n";
    std::cout << "  --semitones <n>      Pitch shift in semitones (-12 ~ +12), default: 0\n";
    std::cout << "  --cents <n>          Micro-tuning in cents (-100 ~ +100), default: 0\n";
    std::cout << "  --formant <0|1>      Formant preservation, default: 0\n";
    std::cout << "  --bypass <0|1>       Bypass processing, default: 0\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << progName << " in.wav out.wav --preset up3\n";
    std::cout << "  " << progName << " in.wav out.wav --semitones 5 --cents 30\n";
    std::cout << "  " << progName << " in.wav out.wav --semitones -7 --formant 1\n";
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
// Safe float argument parser
//==============================================================================
float getFloatArg(const std::map<std::string, std::string>& args,
                  const std::string& key, float defaultVal) {
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
// Safe int argument parser
//==============================================================================
int getIntArg(const std::map<std::string, std::string>& args,
              const std::string& key, int defaultVal) {
    auto it = args.find(key);
    if (it != args.end() && !it->second.empty()) {
        try {
            return std::stoi(it->second);
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

    std::cout << "VC-PitchShift Standalone CLI - Phase Vocoder Pitch Shifting\n";
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
    }

    // Override with command line parameters
    params.semitones = getIntArg(args, "--semitones", params.semitones);
    params.cents     = getFloatArg(args, "--cents", params.cents);

    if (args.count("--formant")) {
        params.formant = (args["--formant"] == "1");
    }

    if (args.count("--bypass")) {
        params.enabled = (args["--bypass"] != "1");
    }

    // Compute pitch ratio for display
    float pitchRatio = std::pow(2.0f, (static_cast<float>(params.semitones) + params.cents / 100.0f) / 12.0f);

    // Print settings
    std::cout << "\nParameters:\n";
    std::cout << "  Semitones: " << params.semitones << "\n";
    std::cout << "  Cents: " << params.cents << "\n";
    std::cout << "  Pitch ratio: " << pitchRatio << "x\n";
    std::cout << "  Formant: " << (params.formant ? "on" : "off") << "\n";
    std::cout << "  Bypass: " << (params.enabled ? "off" : "on") << "\n";

    dsp.setParams(params);
    dsp.setEnabled(params.enabled);

    //==========================================================================
    // Process audio
    //==========================================================================
    std::cout << "\nProcessing...\n";
    dsp.process(left.data(), right.data(), static_cast<int>(totalFrames));

    //==========================================================================
    // Write output file using dr_wav
    //==========================================================================
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
