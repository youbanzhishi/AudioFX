//==============================================================================
// VC-Chorus Standalone CLI - Multi-Voice Chorus Effect (Gen2)
// No JUCE dependency - Uses dr_wav for WAV I/O
//
// Gen2 Parameters:
//   --rate           0.1~10 Hz      LFO modulation rate (default: 1.5)
//   --depth          0~100 %        LFO modulation depth (default: 50)
//   --voices         2~8            Number of chorus voices (default: 3)
//   --mix            0~100 %        Dry/wet mix (default: 50)
//   --delay          5~40 ms        Base delay time (default: 15)
//   --width          0~100 %        Stereo width (default: 80)
//   --feedback       0~0.9          Feedback amount (default: 0.2)
//   --lfo-waveform   sine|triangle|random  LFO waveform (default: sine)
//   --stereo-phase   0~180 deg      L/R LFO phase offset (default: 90)
//   --bypass         0|1            Bypass (default: 0)
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
// Plugin-specific presets (Gen2)
//==============================================================================
struct Preset {
    const char* name;
    VCPluginDSP::Params params;
};

static const Preset presets[] = {
    // name,          rate, depth, voices, mix,   delay, width, feedback, lfoWaveform,          stereoPhase, enabled
    {"bypass",        {1.5f, 50.0f, 3, 50.0f, 15.0f, 80.0f, 0.2f, LFOWaveform::Sine,     90.0f, false}},
    {"subtle",        {0.8f, 30.0f, 2, 30.0f, 12.0f, 50.0f, 0.1f, LFOWaveform::Sine,     60.0f, true }},
    {"rich",          {1.2f, 60.0f, 4, 55.0f, 18.0f, 85.0f, 0.3f, LFOWaveform::Sine,     90.0f, true }},
    {"stereo-wide",   {1.5f, 50.0f, 4, 50.0f, 15.0f, 100.0f, 0.2f, LFOWaveform::Triangle, 120.0f, true }},
    {"ensemble",      {2.0f, 55.0f, 6, 60.0f, 20.0f, 90.0f, 0.25f, LFOWaveform::Random,   90.0f, true }},
    {"leslie",        {5.5f, 70.0f, 2, 65.0f, 10.0f, 95.0f, 0.4f, LFOWaveform::Sine,     180.0f, true }},
};

//==============================================================================
// Help text
//==============================================================================
void printHelp(const char* progName) {
    std::cout << "VC-Chorus Standalone CLI - Multi-Voice Chorus Effect (Gen2)\n\n";
    std::cout << "Usage: " << progName << " <input.wav> <output.wav> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --help, -h                Show this help\n";
    std::cout << "  --preset <name>           Preset (bypass, subtle, rich, stereo-wide, ensemble, leslie)\n";
    std::cout << "  --rate <Hz>               LFO rate (0.1 ~ 10), default: 1.5\n";
    std::cout << "  --depth <%>               Modulation depth (0 ~ 100), default: 50\n";
    std::cout << "  --voices <n>              Number of voices (2 ~ 8), default: 3\n";
    std::cout << "  --mix <%>                 Dry/wet mix (0 ~ 100), default: 50\n";
    std::cout << "  --delay <ms>              Base delay time (5 ~ 40), default: 15\n";
    std::cout << "  --width <%>               Stereo width (0 ~ 100), default: 80\n";
    std::cout << "  --feedback <0~0.9>        Feedback amount (0 ~ 0.9), default: 0.2\n";
    std::cout << "  --lfo-waveform <type>     LFO waveform (sine, triangle, random), default: sine\n";
    std::cout << "  --stereo-phase <deg>      L/R LFO phase offset (0 ~ 180), default: 90\n";
    std::cout << "  --bypass <0|1>            Bypass processing (default: 0)\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << progName << " in.wav out.wav --preset ensemble\n";
    std::cout << "  " << progName << " in.wav out.wav --rate 0.8 --depth 40 --voices 5 --lfo-waveform triangle\n";
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
// Parse LFO waveform string
//==============================================================================
LFOWaveform parseLFOWaveform(const std::string& str, LFOWaveform defaultVal) {
    if (str == "sine") return LFOWaveform::Sine;
    if (str == "triangle") return LFOWaveform::Triangle;
    if (str == "random") return LFOWaveform::Random;
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

    std::cout << "VC-Chorus Standalone CLI - Multi-Voice Chorus Effect (Gen2)\n";
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
    params.rate         = getFloatArg(args, "--rate",  params.rate);
    params.depth        = getFloatArg(args, "--depth", params.depth);
    params.voices       = getIntArg(args,   "--voices", params.voices);
    params.mix          = getFloatArg(args, "--mix",   params.mix);
    params.delay        = getFloatArg(args, "--delay", params.delay);
    params.width        = getFloatArg(args, "--width", params.width);
    params.feedback     = getFloatArg(args, "--feedback", params.feedback);
    params.stereoPhase  = getFloatArg(args, "--stereo-phase", params.stereoPhase);

    // Parse LFO waveform
    if (args.count("--lfo-waveform")) {
        params.lfoWaveform = parseLFOWaveform(args["--lfo-waveform"], params.lfoWaveform);
    }

    if (args.count("--bypass")) {
        params.enabled = (args["--bypass"] != "1");
    }

    // Waveform name for display
    const char* waveformNames[] = {"sine", "triangle", "random"};

    // Print settings
    std::cout << "\nParameters:\n";
    std::cout << "  Rate:          " << params.rate << " Hz\n";
    std::cout << "  Depth:         " << params.depth << " %\n";
    std::cout << "  Voices:        " << params.voices << "\n";
    std::cout << "  Mix:           " << params.mix << " %\n";
    std::cout << "  Delay:         " << params.delay << " ms\n";
    std::cout << "  Width:         " << params.width << " %\n";
    std::cout << "  Feedback:      " << params.feedback << "\n";
    std::cout << "  LFO Waveform:  " << waveformNames[static_cast<int>(params.lfoWaveform)] << "\n";
    std::cout << "  Stereo Phase:  " << params.stereoPhase << " deg\n";
    std::cout << "  Bypass:        " << (params.enabled ? "off" : "on") << "\n";

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
