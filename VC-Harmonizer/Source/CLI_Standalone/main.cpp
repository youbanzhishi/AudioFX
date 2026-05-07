// VC-Harmonizer Standalone CLI - Intelligent Harmony Generator
// No JUCE dependency. Uses dr_wav for WAV I/O.

#define DR_WAV_IMPLEMENTATION
#include <dr_wav.h>

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <sstream>

#include "../DSP/VCPluginDSP.h"

//==============================================================================
// Presets
//==============================================================================
struct Preset {
    const char* name;
    const char* description;
    VCPluginDSP::Params params;
};

static VCPluginDSP::Params makeDefaultParams() {
    VCPluginDSP::Params p;
    p.numVoices = 2;
    p.intervals[0] = 3; p.intervals[1] = 7; p.intervals[2] = 12; p.intervals[3] = -5;
    p.voiceGain[0] = 0; p.voiceGain[1] = 0; p.voiceGain[2] = 0; p.voiceGain[3] = 0;
    p.voicePan[0] = -0.5f; p.voicePan[1] = 0.5f; p.voicePan[2] = 0.7f; p.voicePan[3] = -0.7f;
    p.formantPreserve = 100.0f;
    p.autoKey = false;
    p.scale = 0;
    p.direction = 0;
    p.bypass = false;
    p.midiTrack = -1;
    return p;
}

static VCPluginDSP::Params makePreset3rd5th() {
    auto p = makeDefaultParams();
    p.numVoices = 2;
    p.intervals[0] = 4;  // major 3rd
    p.intervals[1] = 7;  // 5th
    p.voiceGain[0] = -3; p.voiceGain[1] = -6;
    p.voicePan[0] = -0.4f; p.voicePan[1] = 0.4f;
    return p;
}

static VCPluginDSP::Params makePresetChoir() {
    auto p = makeDefaultParams();
    p.numVoices = 4;
    p.intervals[0] = 3; p.intervals[1] = 7; p.intervals[2] = 12; p.intervals[3] = -5;
    p.voiceGain[0] = -3; p.voiceGain[1] = -6; p.voiceGain[2] = -9; p.voiceGain[3] = -6;
    p.voicePan[0] = -0.5f; p.voicePan[1] = 0.5f; p.voicePan[2] = 0.7f; p.voicePan[3] = -0.7f;
    p.formantPreserve = 100.0f;
    return p;
}

static VCPluginDSP::Params makePresetOctave() {
    auto p = makeDefaultParams();
    p.numVoices = 1;
    p.intervals[0] = 12;  // octave up
    p.voiceGain[0] = -3;
    p.voicePan[0] = 0.0f;
    return p;
}

static VCPluginDSP::Params makePresetAutoKey() {
    auto p = makeDefaultParams();
    p.numVoices = 2;
    p.intervals[0] = 4; p.intervals[1] = 7;
    p.voiceGain[0] = -3; p.voiceGain[1] = -6;
    p.autoKey = true;
    p.formantPreserve = 100.0f;
    return p;
}

static VCPluginDSP::Params makePresetUpOnly() {
    auto p = makeDefaultParams();
    p.numVoices = 3;
    p.intervals[0] = 3; p.intervals[1] = 7; p.intervals[2] = 12;
    p.direction = 1;  // up only
    p.voiceGain[0] = -3; p.voiceGain[1] = -6; p.voiceGain[2] = -9;
    p.voicePan[0] = -0.3f; p.voicePan[1] = 0.3f; p.voicePan[2] = 0.6f;
    return p;
}

static VCPluginDSP::Params makePresetSubHarmonic() {
    auto p = makeDefaultParams();
    p.numVoices = 2;
    p.intervals[0] = -5; p.intervals[1] = -7;
    p.direction = 2;  // down only
    p.voiceGain[0] = -3; p.voiceGain[1] = -6;
    p.voicePan[0] = -0.4f; p.voicePan[1] = 0.4f;
    p.formantPreserve = 80.0f;
    return p;
}

static const Preset presets[] = {
    {"3rd-5th",      "Classic 3rd + 5th harmony",        makePreset3rd5th()},
    {"choir",        "4-voice choir (3rd+5th+8va-5th)",  makePresetChoir()},
    {"octave",       "Octave doubler",                    makePresetOctave()},
    {"autokey",      "Auto key detect + 3rd+5th",        makePresetAutoKey()},
    {"up-only",      "Upward harmonies only",             makePresetUpOnly()},
    {"subharmonic",  "Sub-harmonic (5th+7th down)",       makePresetSubHarmonic()},
    {"bypass",       "No processing",                     makeDefaultParams()},
};

//==============================================================================
// Help text
//==============================================================================
void printHelp(const char* progName) {
    std::cout << "VC-Harmonizer - Intelligent Harmony Generator\n";
    std::cout << "  Part of VocalChain Series (VC-Tune / VC-PitchShift / VC-Harmonizer)\n\n";
    std::cout << "Usage: " << progName << " <input.wav> <output.wav> [options]\n\n";
    std::cout << "Harmony Voice Options:\n";
    std::cout << "  --voices <1-4>              Number of harmony voices [default: 2]\n";
    std::cout << "  --intervals <3,7,12>        Intervals in semitones, comma-separated [default: 3,7,12,-5]\n";
    std::cout << "  --voice-gain <dB,...>       Gain per voice in dB [default: 0,0,0,0]\n";
    std::cout << "  --voice-pan <-1~1,...>      Pan per voice (L=-1, R=+1) [default: -0.5,0.5,0.7,-0.7]\n";
    std::cout << "  --direction <up|down|both>  Harmony direction [default: both]\n\n";
    std::cout << "Formant & Scale:\n";
    std::cout << "  --formant-preserve <0-100>  Formant preservation amount [default: 100]\n";
    std::cout << "  --autokey <0|1>             Auto-detect musical key [default: 0]\n";
    std::cout << "  --scale <0-5>               Scale: 0=Chromatic, 1=Major, 2=Minor,\n";
    std::cout << "                              3=Pentatonic, 4=Blues, 5=Custom [default: 0]\n\n";
    std::cout << "MIDI Control (placeholder):\n";
    std::cout << "  --midi-track <num>          MIDI track for VST3 mode (-1=off) [default: -1]\n\n";
    std::cout << "Other:\n";
    std::cout << "  --bypass <0|1>              Bypass processing [default: 0]\n";
    std::cout << "  --preset <name>             Load a preset:\n";
    for (const auto& p : presets) {
        std::cout << "                              " << p.name << " - " << p.description << "\n";
    }
    std::cout << "\nExamples:\n";
    std::cout << "  " << progName << " vocal.wav harmony.wav --preset 3rd-5th\n";
    std::cout << "  " << progName << " vocal.wav harmony.wav --voices 3 --intervals 3,7,12\n";
    std::cout << "  " << progName << " vocal.wav harmony.wav --autokey 1 --intervals 4,7\n";
    std::cout << "  " << progName << " vocal.wav harmony.wav --direction up --voice-gain -3,-6,-9\n";
    std::cout << "  " << progName << " vocal.wav harmony.wav --voice-pan -0.5,0.5,0.7,-0.7 --formant-preserve 80\n";
}

//==============================================================================
// Parse comma-separated values
//==============================================================================
std::vector<float> parseCommaValues(const std::string& str) {
    std::vector<float> values;
    std::istringstream iss(str);
    std::string token;
    while (std::getline(iss, token, ',')) {
        try {
            values.push_back(std::stof(token));
        } catch (...) {
            std::cerr << "Warning: Cannot parse value '" << token << "'\n";
        }
    }
    return values;
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
// Load preset
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
// Main
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

    std::cout << "VC-Harmonizer - Intelligent Harmony Generator\n";
    std::cout << "Input:  " << inFile << "\n";
    std::cout << "Output: " << outFile << "\n";

    //==========================================================================
    // Read WAV
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
    std::cout << "Channels:    " << channels << "\n";
    std::cout << "Duration:    " << std::fixed << std::setprecision(2)
              << (double)totalFrames / sampleRate << " s\n";
    std::cout << "Frames:      " << totalFrames << "\n";

    // Convert interleaved to L/R
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

    VCPluginDSP::Params params = makeDefaultParams();

    // Load preset
    if (args.count("--preset")) {
        std::string presetName = args["--preset"];
        if (!loadPreset(presetName, params)) {
            std::cerr << "Error: Unknown preset '" << presetName << "'\n";
            return 1;
        }
        std::cout << "Preset: " << presetName << "\n";
    }

    // Override with CLI parameters
    if (args.count("--voices")) {
        params.numVoices = std::stoi(args["--voices"]);
        params.numVoices = std::clamp(params.numVoices, 1, VCPluginDSP::MAX_VOICES);
    }

    if (args.count("--intervals")) {
        auto values = parseCommaValues(args["--intervals"]);
        for (size_t i = 0; i < values.size() && i < VCPluginDSP::MAX_VOICES; i++) {
            params.intervals[i] = static_cast<int>(values[i]);
        }
    }

    if (args.count("--voice-gain")) {
        auto values = parseCommaValues(args["--voice-gain"]);
        for (size_t i = 0; i < values.size() && i < VCPluginDSP::MAX_VOICES; i++) {
            params.voiceGain[i] = values[i];
        }
    }

    if (args.count("--voice-pan")) {
        auto values = parseCommaValues(args["--voice-pan"]);
        for (size_t i = 0; i < values.size() && i < VCPluginDSP::MAX_VOICES; i++) {
            params.voicePan[i] = std::clamp(values[i], -1.0f, 1.0f);
        }
    }

    if (args.count("--formant-preserve")) {
        params.formantPreserve = std::stof(args["--formant-preserve"]);
        params.formantPreserve = std::clamp(params.formantPreserve, 0.0f, 100.0f);
    }

    if (args.count("--autokey")) {
        params.autoKey = (args["--autokey"] == "1");
    }

    if (args.count("--scale")) {
        params.scale = std::stoi(args["--scale"]);
        params.scale = std::clamp(params.scale, 0, 5);
    }

    if (args.count("--direction")) {
        std::string dir = args["--direction"];
        if (dir == "up") params.direction = 1;
        else if (dir == "down") params.direction = 2;
        else params.direction = 0;
    }

    if (args.count("--bypass")) {
        params.bypass = (args["--bypass"] == "1");
    }

    if (args.count("--midi-track")) {
        params.midiTrack = std::stoi(args["--midi-track"]);
    }

    // Print effective parameters
    std::cout << "\n--- Parameters ---\n";
    std::cout << "Voices:            " << params.numVoices << "\n";
    std::cout << "Intervals:         ";
    for (int v = 0; v < params.numVoices; v++) {
        std::cout << params.intervals[v];
        if (v < params.numVoices - 1) std::cout << ", ";
    }
    std::cout << " semitones\n";
    std::cout << "Voice Gain:        ";
    for (int v = 0; v < params.numVoices; v++) {
        std::cout << std::fixed << std::setprecision(1) << params.voiceGain[v] << " dB";
        if (v < params.numVoices - 1) std::cout << ", ";
    }
    std::cout << "\n";
    std::cout << "Voice Pan:         ";
    for (int v = 0; v < params.numVoices; v++) {
        std::cout << std::fixed << std::setprecision(2) << params.voicePan[v];
        if (v < params.numVoices - 1) std::cout << ", ";
    }
    std::cout << "\n";
    std::cout << "Direction:         " << (params.direction == 0 ? "both" : params.direction == 1 ? "up" : "down") << "\n";
    std::cout << "Formant Preserve:  " << std::fixed << std::setprecision(1) << params.formantPreserve << "%\n";
    std::cout << "Scale:             " << ScaleQuantizer::scaleName(static_cast<ScaleQuantizer::Scale>(params.scale)) << "\n";
    std::cout << "Auto Key:          " << (params.autoKey ? "ON" : "OFF") << "\n";
    std::cout << "MIDI Track:        " << (params.midiTrack >= 0 ? std::to_string(params.midiTrack) : "off") << "\n";
    std::cout << "Bypass:            " << (params.bypass ? "ON" : "OFF") << "\n";

    dsp.setParams(params);
    dsp.setEnabled(!params.bypass);

    //==========================================================================
    // Process
    //==========================================================================
    std::cout << "\nProcessing...\n";
    dsp.process(left.data(), right.data(), static_cast<int>(totalFrames));

    // Print key detection result if autoKey
    if (params.autoKey && dsp.hasKeyDetected()) {
        auto kr = dsp.getKeyResult();
        std::cout << "\n--- Key Detection Result ---\n";
        if (kr.detected) {
            std::cout << "Detected Key: " << KeyDetector::keyName(kr.key, kr.isMajor) << "\n";
            std::cout << "Confidence:   " << std::fixed << std::setprecision(3)
                      << kr.confidence << "\n";
            std::cout << "Auto Scale:   " << ScaleQuantizer::scaleName(
                            kr.isMajor ? ScaleQuantizer::Major : ScaleQuantizer::Minor) << "\n";
            std::cout << "Key Offset:   " << kr.key << " semitones from C\n";
        } else {
            std::cout << "Key detection failed (insufficient voiced signal)\n";
        }
    }

    //==========================================================================
    // Write output WAV
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

    std::cout << "\nDone! Output: " << outFile << "\n";
    return 0;
}
