//==============================================================================
// VC-Gate Standalone CLI - Noise Gate / Downward Expander (Gen2)
// No JUCE dependency - Uses dr_wav for WAV I/O
//
// Gen2 Parameters:
//   --threshold      -80~0 dB     Gate threshold (default: -40)
//   --ratio          1~20         Expansion ratio (default: 10)
//   --attack         0.1~50 ms    Attack time (default: 1)
//   --hold           0~500 ms     Hold time (default: 50)
//   --release        10~2000 ms   Release time (default: 100)
//   --range          -80~0 dB     Max attenuation when closed (default: -80)
//   --hysteresis     0~20 dB      Open/close threshold difference (default: 6)
//   --sidechain-hpf  0~500 Hz     Sidechain HPF cutoff (default: 80)
//   --attack-hold    0~100 ms     Hold after attack (default: 10)
//   --lookahead      0~5 ms       Lookahead time (default: 2)
//   --bypass         0|1          Bypass (default: 0)
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
    // name,          threshold, ratio, attack, hold,  release, range,  hysteresis, sidechainHpf, attackHold, lookahead, enabled
    {"bypass",        {-40.0f, 10.0f,  1.0f,  50.0f, 100.0f, -80.0f,  0.0f,   0.0f,   0.0f,  0.0f, false}},
    {"gentle-gate",   {-40.0f,  3.0f,  5.0f, 100.0f, 200.0f, -30.0f,  3.0f,  80.0f,  20.0f,  1.0f, true }},
    {"hard-gate",     {-50.0f, 20.0f,  0.5f,  10.0f,  50.0f, -80.0f,  8.0f, 120.0f,   5.0f,  3.0f, true }},
    {"drum-gate",     {-35.0f, 15.0f,  0.1f,  20.0f,  80.0f, -80.0f,  4.0f, 200.0f,   5.0f,  4.0f, true }},
    {"vocal-gate",    {-35.0f, 10.0f,  1.0f,  50.0f, 100.0f, -60.0f,  6.0f, 100.0f,  10.0f,  2.0f, true }},
    {"de-breath",     {-45.0f,  5.0f,  2.0f,  30.0f, 150.0f, -20.0f,  4.0f, 300.0f,  15.0f,  1.0f, true }},
};

//==============================================================================
// Help text
//==============================================================================
void printHelp(const char* progName) {
    std::cout << "VC-Gate Standalone CLI - Noise Gate / Downward Expander (Gen2)\n\n";
    std::cout << "Usage: " << progName << " <input.wav> <output.wav> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --help, -h              Show this help\n";
    std::cout << "  --preset <name>         Preset (bypass, gentle-gate, hard-gate, drum-gate, vocal-gate, de-breath)\n";
    std::cout << "  --threshold <dB>        Gate threshold (-80 ~ 0), default: -40\n";
    std::cout << "  --ratio <n>             Expansion ratio (1 ~ 20), default: 10\n";
    std::cout << "  --attack <ms>           Attack time (0.1 ~ 50), default: 1\n";
    std::cout << "  --hold <ms>             Hold time (0 ~ 500), default: 50\n";
    std::cout << "  --release <ms>          Release time (10 ~ 2000), default: 100\n";
    std::cout << "  --range <dB>            Max attenuation when closed (-80 ~ 0), default: -80\n";
    std::cout << "  --hysteresis <dB>       Open/close threshold difference (0 ~ 20), default: 6\n";
    std::cout << "  --sidechain-hpf <Hz>    Sidechain HPF cutoff (0 ~ 500), default: 80\n";
    std::cout << "  --attack-hold <ms>      Hold after attack (0 ~ 100), default: 10\n";
    std::cout << "  --lookahead <ms>        Lookahead time (0 ~ 5), default: 2\n";
    std::cout << "  --bypass <0|1>          Bypass processing (default: 0)\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << progName << " in.wav out.wav --preset vocal-gate\n";
    std::cout << "  " << progName << " in.wav out.wav --threshold -30 --hysteresis 8 --sidechain-hpf 120\n";
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

    std::cout << "VC-Gate Standalone CLI - Noise Gate / Downward Expander (Gen2)\n";
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
    params.threshold    = getFloatArg(args, "--threshold",    params.threshold);
    params.ratio        = getFloatArg(args, "--ratio",        params.ratio);
    params.attack       = getFloatArg(args, "--attack",       params.attack);
    params.hold         = getFloatArg(args, "--hold",         params.hold);
    params.release      = getFloatArg(args, "--release",      params.release);
    params.range        = getFloatArg(args, "--range",        params.range);
    params.hysteresis   = getFloatArg(args, "--hysteresis",   params.hysteresis);
    params.sidechainHpf = getFloatArg(args, "--sidechain-hpf", params.sidechainHpf);
    params.attackHold   = getFloatArg(args, "--attack-hold",  params.attackHold);
    params.lookahead    = getFloatArg(args, "--lookahead",    params.lookahead);

    if (args.count("--bypass")) {
        params.enabled = (args["--bypass"] != "1");
    }

    // Print settings
    std::cout << "\nParameters:\n";
    std::cout << "  Threshold:     " << params.threshold << " dB\n";
    std::cout << "  Ratio:         " << params.ratio << " :1\n";
    std::cout << "  Attack:        " << params.attack << " ms\n";
    std::cout << "  Hold:          " << params.hold << " ms\n";
    std::cout << "  Release:       " << params.release << " ms\n";
    std::cout << "  Range:         " << params.range << " dB\n";
    std::cout << "  Hysteresis:    " << params.hysteresis << " dB\n";
    std::cout << "  Sidechain HPF: " << params.sidechainHpf << " Hz\n";
    std::cout << "  Attack-Hold:   " << params.attackHold << " ms\n";
    std::cout << "  Lookahead:     " << params.lookahead << " ms\n";
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
