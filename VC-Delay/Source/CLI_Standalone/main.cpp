// VC-Delay Gen2 Standalone CLI - No JUCE Dependency
// Multi-tap delay with BPM sync, feedback filtering, ping-pong
// Uses dr_wav for WAV I/O

//==============================================================================
// MUST define VC_STANDALONE before including DSP header
// MUST include dr_wav implementation before DSP header
//==============================================================================
#define DR_WAV_IMPLEMENTATION

#include <dr_wav.h>

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <set>
#include <sstream>

// Include DSP header AFTER DR_WAV_IMPLEMENTATION and VC_STANDALONE
#include "../DSP/VCPluginDSP.h"

//==============================================================================
// Gen2 Presets
//==============================================================================
struct Preset {
    const char* name;
    VCPluginDSP::Params params;
};

static VCPluginDSP::Params makePreset(
    float delayTime, float feedback, float mix, bool enabled,
    float syncBpm = 0, int noteVal = 4, bool trip = false, bool dot = false,
    int taps = 1, bool pp = false, float hpf = 80.0f, float lpf = 8000.0f,
    float t0 = 250, float t1 = 500, float t2 = 750, float t3 = 1000,
    float g0 = 0, float g1 = -3, float g2 = -6, float g3 = -9)
{
    VCPluginDSP::Params p;
    p.delayTime = delayTime; p.feedback = feedback; p.mix = mix; p.enabled = enabled;
    p.syncBpm = syncBpm; p.noteValue = noteVal; p.triplet = trip; p.dotted = dot;
    p.taps = taps; p.pingPong = pp;
    p.feedbackHpf = hpf; p.feedbackLpf = lpf;
    p.tapTime[0] = t0; p.tapTime[1] = t1; p.tapTime[2] = t2; p.tapTime[3] = t3;
    p.tapGain[0] = g0; p.tapGain[1] = g1; p.tapGain[2] = g2; p.tapGain[3] = g3;
    return p;
}

static const Preset presets[] = {
    {"bypass",         makePreset(250, 30, 50, false)},
    {"quarter-note",   makePreset(250, 30, 50, true, 120, 4, false, false)},
    {"eighth-note",    makePreset(125, 30, 50, true, 120, 8, false, false)},
    {"slapback",       makePreset(80, 10, 40, true)},
    {"ping-pong",      makePreset(250, 40, 55, true, 0, 4, false, false, 1, true, 80, 8000,
                                  250, 500, 750, 1000, 0, -3, -6, -9)},
    {"ambient",        makePreset(500, 55, 45, true, 0, 4, false, false, 2, false, 60, 6000,
                                  500, 700, 750, 1000, 0, -6, -6, -9)},
    {"multi-tap",      makePreset(250, 35, 50, true, 0, 4, false, false, 4, false, 80, 8000,
                                  125, 250, 375, 500, 0, -3, -6, -9)},
};

//==============================================================================
// Helper: parse comma-separated float list
//==============================================================================
std::vector<float> parseFloatList(const std::string& s) {
    std::vector<float> result;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
        try { result.push_back(std::stof(item)); }
        catch (...) {}
    }
    return result;
}

//==============================================================================
// Help text
//==============================================================================
void printHelp(const char* progName) {
    std::cout << "VC-Delay Gen2 Standalone CLI - Multi-tap Delay (No JUCE)\n\n";
    std::cout << "Usage: " << progName << " <input.wav> <output.wav> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --help, -h            Show this help\n";
    std::cout << "  --preset <name>       Preset (bypass, quarter-note, eighth-note, slapback,\n";
    std::cout << "                         ping-pong, ambient, multi-tap)\n";
    std::cout << "  --time <ms>           Delay time in ms (default: 250, used if --sync-bpm=0)\n";
    std::cout << "  --feedback <%>        Feedback percentage (0~90, default: 30)\n";
    std::cout << "  --mix <0-100>         Dry/Wet mix percentage (default: 50)\n";
    std::cout << "  --bypass <0|1>        Bypass processing (default: 0)\n";
    std::cout << "  --sync-bpm <bpm>      BPM sync (0=free time, default: 0)\n";
    std::cout << "  --note <value>        Note value: 1,2,4,8,16,32 (default: 4)\n";
    std::cout << "  --triplet <0|1>       Triplet mode (default: 0)\n";
    std::cout << "  --dotted <0|1>        Dotted mode (default: 0)\n";
    std::cout << "  --taps <1-4>          Number of delay taps (default: 1)\n";
    std::cout << "  --tap-time <ms,...>   Comma-separated delay times per tap\n";
    std::cout << "  --tap-gain <dB,...>   Comma-separated gain per tap in dB\n";
    std::cout << "  --ping-pong <0|1>     Ping-pong stereo mode (default: 0)\n";
    std::cout << "  --feedback-hpf <Hz>   Feedback HPF frequency (default: 80)\n";
    std::cout << "  --feedback-lpf <Hz>   Feedback LPF frequency (default: 8000)\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << progName << " in.wav out.wav --preset ping-pong\n";
    std::cout << "  " << progName << " in.wav out.wav --sync-bpm 120 --note 8 --taps 3\n";
    std::cout << "  " << progName << " in.wav out.wav --time 250 --feedback 40 --ping-pong 1\n";
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

float getFloatArg(const std::map<std::string, std::string>& args, const std::string& key, float defaultVal) {
    auto it = args.find(key);
    if (it != args.end() && !it->second.empty()) {
        try { return std::stof(it->second); } catch (...) { return defaultVal; }
    }
    return defaultVal;
}

int getIntArg(const std::map<std::string, std::string>& args, const std::string& key, int defaultVal) {
    auto it = args.find(key);
    if (it != args.end() && !it->second.empty()) {
        try { return std::stoi(it->second); } catch (...) { return defaultVal; }
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

    std::cout << "VC-Delay Gen2 Standalone CLI\n";
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
    if (args.count("--time")) {
        params.delayTime = getFloatArg(args, "--time", params.delayTime);
        std::cout << "Delay: " << params.delayTime << " ms\n";
    }

    if (args.count("--feedback")) {
        params.feedback = getFloatArg(args, "--feedback", params.feedback);
        std::cout << "Feedback: " << params.feedback << " %\n";
    }

    if (args.count("--mix")) {
        params.mix = getFloatArg(args, "--mix", params.mix);
        std::cout << "Mix: " << params.mix << "%\n";
    }

    if (args.count("--sync-bpm")) {
        params.syncBpm = getFloatArg(args, "--sync-bpm", params.syncBpm);
        std::cout << "Sync BPM: " << params.syncBpm << "\n";
    }

    if (args.count("--note")) {
        params.noteValue = getIntArg(args, "--note", params.noteValue);
        std::cout << "Note: 1/" << params.noteValue << "\n";
    }

    if (args.count("--triplet")) {
        params.triplet = (args["--triplet"] == "1");
        std::cout << "Triplet: " << (params.triplet ? "on" : "off") << "\n";
    }

    if (args.count("--dotted")) {
        params.dotted = (args["--dotted"] == "1");
        std::cout << "Dotted: " << (params.dotted ? "on" : "off") << "\n";
    }

    if (args.count("--taps")) {
        params.taps = getIntArg(args, "--taps", params.taps);
        params.taps = std::clamp(params.taps, 1, 4);
        std::cout << "Taps: " << params.taps << "\n";
    }

    if (args.count("--tap-time")) {
        auto times = parseFloatList(args["--tap-time"]);
        for (int i = 0; i < (int)times.size() && i < VC_DELAY_MAX_TAPS; ++i) {
            params.tapTime[i] = times[i];
        }
        std::cout << "Tap times set\n";
    }

    if (args.count("--tap-gain")) {
        auto gains = parseFloatList(args["--tap-gain"]);
        for (int i = 0; i < (int)gains.size() && i < VC_DELAY_MAX_TAPS; ++i) {
            params.tapGain[i] = gains[i];
        }
        std::cout << "Tap gains set\n";
    }

    if (args.count("--ping-pong")) {
        params.pingPong = (args["--ping-pong"] == "1");
        std::cout << "Ping-pong: " << (params.pingPong ? "on" : "off") << "\n";
    }

    if (args.count("--feedback-hpf")) {
        params.feedbackHpf = getFloatArg(args, "--feedback-hpf", params.feedbackHpf);
        std::cout << "Feedback HPF: " << params.feedbackHpf << " Hz\n";
    }

    if (args.count("--feedback-lpf")) {
        params.feedbackLpf = getFloatArg(args, "--feedback-lpf", params.feedbackLpf);
        std::cout << "Feedback LPF: " << params.feedbackLpf << " Hz\n";
    }

    if (args.count("--bypass")) {
        params.enabled = (args["--bypass"] != "1");
        dsp.setEnabled(params.enabled);
        std::cout << "Bypass: " << (params.enabled ? "off" : "on") << "\n";
    }

    // Apply all params at once
    dsp.setParams(params);

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
