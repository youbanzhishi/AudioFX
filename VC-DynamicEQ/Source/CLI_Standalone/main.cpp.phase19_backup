// VC-DynamicEQ Gen2 Standalone CLI - No JUCE Dependency
// Multi-band Dynamic EQ with sidechain support, band types, attack/release
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

std::vector<std::string> parseStringList(const std::string& s) {
    std::vector<std::string> result;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
        result.push_back(item);
    }
    return result;
}

VCBandType parseBandType(const std::string& s) {
    if (s == "ls" || s == "lowshelf") return VCBandType::LowShelf;
    if (s == "hs" || s == "highshelf") return VCBandType::HighShelf;
    if (s == "notch") return VCBandType::Notch;
    return VCBandType::Bell;
}

//==============================================================================
// Helper to set band params
//==============================================================================
void setBand(VCPluginDSP::BandParams& bp, float freq, float q, VCBandType type,
             float threshold, float ratio, float attack, float release, float gain) {
    bp.frequency = freq; bp.q = q; bp.type = type;
    bp.threshold = threshold; bp.ratio = ratio;
    bp.attack = attack; bp.release = release; bp.gain = gain;
}

//==============================================================================
// Gen2 Presets
//==============================================================================
struct Preset {
    const char* name;
    VCPluginDSP::Params params;
};

static const Preset presets[] = {
    {"bypass", []() {
        VCPluginDSP::Params p; p.bands = 1; p.enabled = false; p.mix = 100.0f;
        setBand(p.band[0], 200, 1.0f, VCBandType::Bell, -12, 3, 10, 100, -6);
        return p;
    }()},
    {"de-ess", []() {
        VCPluginDSP::Params p; p.bands = 1; p.enabled = true; p.mix = 100.0f;
        setBand(p.band[0], 6000, 2.0f, VCBandType::Bell, -15, 4, 1, 50, 0);
        return p;
    }()},
    {"bass-control", []() {
        VCPluginDSP::Params p; p.bands = 1; p.enabled = true; p.mix = 100.0f;
        setBand(p.band[0], 120, 0.7f, VCBandType::LowShelf, -18, 3, 10, 150, 0);
        return p;
    }()},
    {"presence-boost", []() {
        VCPluginDSP::Params p; p.bands = 1; p.enabled = true; p.mix = 100.0f;
        setBand(p.band[0], 4000, 1.0f, VCBandType::HighShelf, -10, 2, 15, 120, 3);
        return p;
    }()},
    {"vocal-tamer", []() {
        VCPluginDSP::Params p; p.bands = 2; p.enabled = true; p.mix = 100.0f;
        setBand(p.band[0], 2500, 2.0f, VCBandType::Bell, -12, 3, 5, 80, 0);
        setBand(p.band[1], 5000, 1.5f, VCBandType::Bell, -10, 3, 5, 80, 0);
        return p;
    }()},
    {"multi-band", []() {
        VCPluginDSP::Params p; p.bands = 4; p.enabled = true; p.mix = 100.0f;
        setBand(p.band[0], 100, 0.7f, VCBandType::LowShelf, -18, 3, 10, 150, 0);
        setBand(p.band[1], 500, 1.5f, VCBandType::Bell, -12, 3, 8, 100, 0);
        setBand(p.band[2], 3000, 1.0f, VCBandType::Bell, -10, 3, 5, 80, 0);
        setBand(p.band[3], 10000, 0.7f, VCBandType::HighShelf, -15, 2, 8, 120, 0);
        return p;
    }()},
};

//==============================================================================
// Help text
//==============================================================================
void printHelp(const char* progName) {
    std::cout << "VC-DynamicEQ Gen2 Standalone CLI (No JUCE)\n";
    std::cout << "Multi-band Dynamic EQ with sidechain, band types\n\n";
    std::cout << "Usage: " << progName << " <input.wav> <output.wav> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --help, -h              Show this help\n";
    std::cout << "  --preset <name>         Preset (bypass, de-ess, bass-control,\n";
    std::cout << "                           presence-boost, vocal-tamer, multi-band)\n";
    std::cout << "  --bands <1-4>           Number of dynamic bands (default: 1)\n";
    std::cout << "  --band-freq <Hz,...>    Comma-separated center frequency per band\n";
    std::cout << "  --band-q <Q,...>        Comma-separated Q value per band\n";
    std::cout << "  --band-type <t,...>     Comma-separated band type per band\n";
    std::cout << "                           (bell, ls, hs, notch)\n";
    std::cout << "  --band-threshold <dB,..> Comma-separated threshold per band\n";
    std::cout << "  --band-ratio <r,...>    Comma-separated ratio per band\n";
    std::cout << "  --band-attack <ms,...>  Comma-separated attack per band\n";
    std::cout << "  --band-release <ms,...> Comma-separated release per band\n";
    std::cout << "  --sidechain <int|ext>   Sidechain mode (default: internal)\n";
    std::cout << "  --frequency <Hz>        Center frequency (Gen1 compat, sets band 0)\n";
    std::cout << "  --gain <dB>             Static gain (Gen1 compat, sets band 0)\n";
    std::cout << "  --q <value>             Q value (Gen1 compat, sets band 0)\n";
    std::cout << "  --threshold <dB>        Threshold (Gen1 compat, sets band 0)\n";
    std::cout << "  --attack <ms>           Attack (Gen1 compat, sets band 0)\n";
    std::cout << "  --release <ms>          Release (Gen1 compat, sets band 0)\n";
    std::cout << "  --mix <0-100>           Dry/Wet mix percentage\n";
    std::cout << "  --bypass <0|1>          Bypass processing\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << progName << " in.wav out.wav --preset de-ess\n";
    std::cout << "  " << progName << " in.wav out.wav --bands 2 --band-freq 200,3000 --band-type bell,bell\n";
    std::cout << "  " << progName << " in.wav out.wav --frequency 200 --threshold -12 --ratio 4\n";
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

    std::cout << "VC-DynamicEQ Gen2 Standalone CLI\n";
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

    if (args.count("--preset")) {
        std::string presetName = args["--preset"];
        std::cout << "Preset: " << presetName << "\n";
        if (!loadPreset(presetName, params)) {
            std::cerr << "Error: Unknown preset\n";
            return 1;
        }
        dsp.setParams(params);
    }

    // Gen2: --bands
    if (args.count("--bands")) {
        params.bands = getIntArg(args, "--bands", params.bands);
        params.bands = std::clamp(params.bands, 1, VC_DYN_EQ_MAX_BANDS);
        std::cout << "Bands: " << params.bands << "\n";
    }

    // Gen2: per-band parameters (comma-separated lists)
    if (args.count("--band-freq")) {
        auto vals = parseFloatList(args["--band-freq"]);
        for (int i = 0; i < (int)vals.size() && i < VC_DYN_EQ_MAX_BANDS; ++i)
            params.band[i].frequency = vals[i];
        std::cout << "Band frequencies set\n";
    }

    if (args.count("--band-q")) {
        auto vals = parseFloatList(args["--band-q"]);
        for (int i = 0; i < (int)vals.size() && i < VC_DYN_EQ_MAX_BANDS; ++i)
            params.band[i].q = vals[i];
        std::cout << "Band Q values set\n";
    }

    if (args.count("--band-type")) {
        auto vals = parseStringList(args["--band-type"]);
        for (int i = 0; i < (int)vals.size() && i < VC_DYN_EQ_MAX_BANDS; ++i)
            params.band[i].type = parseBandType(vals[i]);
        std::cout << "Band types set\n";
    }

    if (args.count("--band-threshold")) {
        auto vals = parseFloatList(args["--band-threshold"]);
        for (int i = 0; i < (int)vals.size() && i < VC_DYN_EQ_MAX_BANDS; ++i)
            params.band[i].threshold = vals[i];
        std::cout << "Band thresholds set\n";
    }

    if (args.count("--band-ratio")) {
        auto vals = parseFloatList(args["--band-ratio"]);
        for (int i = 0; i < (int)vals.size() && i < VC_DYN_EQ_MAX_BANDS; ++i)
            params.band[i].ratio = vals[i];
        std::cout << "Band ratios set\n";
    }

    if (args.count("--band-attack")) {
        auto vals = parseFloatList(args["--band-attack"]);
        for (int i = 0; i < (int)vals.size() && i < VC_DYN_EQ_MAX_BANDS; ++i)
            params.band[i].attack = vals[i];
        std::cout << "Band attacks set\n";
    }

    if (args.count("--band-release")) {
        auto vals = parseFloatList(args["--band-release"]);
        for (int i = 0; i < (int)vals.size() && i < VC_DYN_EQ_MAX_BANDS; ++i)
            params.band[i].release = vals[i];
        std::cout << "Band releases set\n";
    }

    if (args.count("--sidechain")) {
        params.sidechain = (args["--sidechain"] == "external") ? 1 : 0;
        std::cout << "Sidechain: " << (params.sidechain ? "external" : "internal") << "\n";
    }

    // Gen1 compat overrides (sets band 0)
    if (args.count("--frequency")) {
        float v = getFloatArg(args, "--frequency", params.frequency);
        params.frequency = v;
        params.band[0].frequency = v;
        std::cout << "Frequency: " << v << " Hz\n";
    }

    if (args.count("--gain")) {
        float v = getFloatArg(args, "--gain", params.gain);
        params.gain = v;
        params.band[0].gain = v;
        std::cout << "Gain: " << v << " dB\n";
    }

    if (args.count("--q")) {
        float v = getFloatArg(args, "--q", params.q);
        params.q = v;
        params.band[0].q = v;
        std::cout << "Q: " << v << "\n";
    }

    if (args.count("--threshold")) {
        float v = getFloatArg(args, "--threshold", params.threshold);
        params.threshold = v;
        params.band[0].threshold = v;
        std::cout << "Threshold: " << v << " dB\n";
    }

    if (args.count("--attack")) {
        float v = getFloatArg(args, "--attack", params.attack);
        params.attack = v;
        params.band[0].attack = v;
        std::cout << "Attack: " << v << " ms\n";
    }

    if (args.count("--release")) {
        float v = getFloatArg(args, "--release", params.release);
        params.release = v;
        params.band[0].release = v;
        std::cout << "Release: " << v << " ms\n";
    }

    if (args.count("--mix")) {
        params.mix = getFloatArg(args, "--mix", params.mix);
        std::cout << "Mix: " << params.mix << "%\n";
    }

    if (args.count("--bypass")) {
        params.enabled = (args["--bypass"] != "1");
        dsp.setEnabled(params.enabled);
        std::cout << "Bypass: " << (params.enabled ? "off" : "on") << "\n";
    }

    // Apply all params
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
