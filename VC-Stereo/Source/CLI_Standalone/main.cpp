//==============================================================================
// VC-Stereo Standalone CLI - Stereo Width / MS Codec / Pan / Mono Bass
// No JUCE dependency - Uses dr_wav for WAV I/O
//
// Parameters:
//   --width      0~200 %       Stereo width (0=mono, 100=original, 200=extra-wide, default: 100)
//   --pan       -100~100       Stereo pan (-100=full left, 0=center, 100=full right, default: 0)
//   --mono-bass  0|1           Collapse bass to mono (default: 0)
//   --bass-freq  50~300 Hz     Mono bass crossover frequency (default: 150)
//   --bypass     0|1           Bypass processing (default: 0)
//
// Presets: bypass, mono, wide, extra-wide, bass-mono, center-pan
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
    // name,          width,  pan,    monoBass,  bassFreq, enabled
    {"bypass",        {100.0f, 0.0f,   false,    150.0f,  false}},
    {"mono",          {  0.0f, 0.0f,   false,    150.0f,  true }},
    {"wide",          {150.0f, 0.0f,   false,    150.0f,  true }},
    {"extra-wide",    {200.0f, 0.0f,   false,    150.0f,  true }},
    {"bass-mono",     {100.0f, 0.0f,   true,     150.0f,  true }},
    {"center-pan",    {100.0f, 0.0f,   false,    150.0f,  true }},
};

//==============================================================================
// Help text
//==============================================================================
void printHelp(const char* progName) {
    std::cout << "VC-Stereo Standalone CLI - Stereo Width / MS Codec / Pan / Mono Bass (No JUCE)\n\n";
    std::cout << "Usage: " << progName << " <input.wav> <output.wav> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --help, -h           Show this help\n";
    std::cout << "  --preset <name>      Preset (bypass, mono, wide, extra-wide, bass-mono, center-pan)\n";
    std::cout << "  --width <%>          Stereo width (0 ~ 200), default: 100\n";
    std::cout << "               0 = mono, 100 = original, 200 = extra-wide\n";
    std::cout << "  --pan <-100~100>     Stereo pan, default: 0 (center)\n";
    std::cout << "               -100 = full left, 0 = center, 100 = full right\n";
    std::cout << "  --mono-bass <0|1>    Collapse bass to mono, default: 0\n";
    std::cout << "  --bass-freq <Hz>     Mono bass crossover freq (50 ~ 300), default: 150\n";
    std::cout << "  --bypass <0|1>       Bypass processing, default: 0\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << progName << " in.wav out.wav --preset wide\n";
    std::cout << "  " << progName << " in.wav out.wav --width 150 --pan -20 --mono-bass 1\n";
    std::cout << "  " << progName << " in.wav out.wav --preset mono --bass-freq 200\n";
}

//==============================================================================
// Parse command line arguments
//==============================================================================
std::map<std::string, std::string> parseArgs(int argc, char** argv) {
    std::map<std::string, std::string> args;
    std::set<std::string> noValueFlags = {"--help", "-h"};
    // Phase 19: isOption() distinguishes flags from negative numbers
    auto isOption = [](const std::string& s) -> bool {
        if (s.size() < 2) return false;
        if (s.substr(0, 2) == "--") return true;       // long option: --flag
        if (s == "-h") return true;                     // short help
        if (s[0] == '-' && s.size() > 1 && !std::isdigit(static_cast<unsigned char>(s[1]))) return true; // -x
        return false;  // -20, -3.5 etc. are negative numbers, not options
    };
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--") break;  // Phase 19: stop option parsing on "--"
        if (arg == "--help" || arg == "-h") {
            args["--help"] = "";
        } else if (arg.substr(0, 2) == "--") {
            std::string key = arg;
            std::string value;
            if (noValueFlags.count(key) == 0 && i + 1 < argc) {
                // Phase 19 fix: only consume next arg as value if it's not another option
                if (!isOption(argv[i + 1])) {
                    value = argv[++i];
                }
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
    // Phase 19: smarter file extraction using parseArgs results
    std::set<std::string> consumedValues;
    for (const auto& kv : args) { if (!kv.second.empty()) consumedValues.insert(kv.second); }
    std::vector<std::string> files;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a.substr(0, 2) == "--" || a == "-h") continue;  // skip options
        if (args.count(a) || consumedValues.count(a)) continue;  // skip consumed args
        files.push_back(a);
    }

    if (files.size() < 2) {
        std::cerr << "Error: Need input and output files\n\n";
        printHelp(argv[0]);
        return 1;
    }

    std::string inFile = files[0];
    std::string outFile = files[1];

    std::cout << "VC-Stereo Standalone CLI - Stereo Width / MS Codec / Pan / Mono Bass\n";
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
    params.width     = getFloatArg(args, "--width",     params.width);
    params.pan       = getFloatArg(args, "--pan",       params.pan);
    params.bassFreq  = getFloatArg(args, "--bass-freq", params.bassFreq);

    if (args.count("--mono-bass")) {
        params.monoBass = (args["--mono-bass"] == "1");
    }

    if (args.count("--bypass")) {
        params.enabled = (args["--bypass"] != "1");
    }

    // Print settings
    std::cout << "\nParameters:\n";
    std::cout << "  Width: " << params.width << " %\n";
    std::cout << "  Pan: " << params.pan << "\n";
    std::cout << "  Mono bass: " << (params.monoBass ? "on" : "off") << "\n";
    std::cout << "  Bass freq: " << params.bassFreq << " Hz\n";
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
