// VC-Drum Standalone CLI - Drum Synthesizer
// No JUCE dependency, uses dr_wav for WAV I/O
// Generates drum patterns and renders to WAV

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

// Include DSP header AFTER DR_WAV_IMPLEMENTATION and VC_STANDALONE
#include "../DSP/VCPluginDSP.h"

//==============================================================================
// Presets
//==============================================================================
struct Preset {
    const char* name;
    VCPluginDSP::Params params;
};

static VCPluginDSP::Params makeDefaultParams()
{
    VCPluginDSP::Params p;
    p.pattern    = 0;
    p.bpm        = 120.0f;
    p.swing      = 0.0f;
    p.humanize   = 0.0f;
    p.masterGain = 0.0f;
    p.enabled    = true;
    return p;
}

static const Preset presets[] = {
    {"bypass",       [](){ auto p = makeDefaultParams(); p.enabled = false; return p; }()},
    {"kick-only",    [](){ auto p = makeDefaultParams(); p.pattern = 1; return p; }()},
    {"snare-only",   [](){ auto p = makeDefaultParams(); p.pattern = 2; return p; }()},
    {"hihat-only",   [](){ auto p = makeDefaultParams(); p.pattern = 3; return p; }()},
    {"basic-beat",   [](){ auto p = makeDefaultParams(); p.pattern = 4; return p; }()},
    {"house",        [](){ auto p = makeDefaultParams(); p.pattern = 5; p.bpm = 126.0f; return p; }()},
    {"techno",       [](){ auto p = makeDefaultParams(); p.pattern = 6; p.bpm = 132.0f; return p; }()},
    {"hiphop",       [](){ auto p = makeDefaultParams(); p.pattern = 7; p.bpm = 90.0f; p.swing = 50.0f; return p; }()},
    {"trap",         [](){ auto p = makeDefaultParams(); p.pattern = 8; p.bpm = 140.0f; return p; }()},
    {"dnb",          [](){ auto p = makeDefaultParams(); p.pattern = 9; p.bpm = 174.0f; return p; }()},
};

//==============================================================================
// Help text
//==============================================================================
void printHelp(const char* progName) {
    std::cout << "VC-Drum: Drum Synthesizer CLI\n\n";
    std::cout << "Usage: " << progName << " <output.wav> [options]\n\n";
    std::cout << "Generates synthesized drum patterns to WAV file.\n\n";
    std::cout << "Options:\n";
    std::cout << "  --help, -h              Show this help\n";
    std::cout << "  --preset <name>         Preset: bypass, kick-only, snare-only, hihat-only,\n";
    std::cout << "                          basic-beat, house, techno, hiphop, trap, dnb\n";
    std::cout << "  --pattern <type>        Pattern: kick/snare/hihat/basic/full/house/techno/hiphop/trap/dnb\n";
    std::cout << "  --bpm <value>           Tempo in BPM (default: 120)\n";
    std::cout << "  --bars <N>              Number of bars to render (default: 4)\n";
    std::cout << "  --swing <0-100>         Swing percentage (default: 0)\n";
    std::cout << "  --humanize <0-100>      Humanize percentage (default: 0)\n";
    std::cout << "\n";
    std::cout << "  Kick parameters:\n";
    std::cout << "  --kick-freq-start <Hz>  Kick sweep start freq (default: 150)\n";
    std::cout << "  --kick-freq-end <Hz>    Kick sweep end freq (default: 50)\n";
    std::cout << "  --kick-decay <ms>       Kick decay time (default: 300)\n";
    std::cout << "\n";
    std::cout << "  Snare parameters:\n";
    std::cout << "  --snare-tone <0-1>      Snare body/noise mix (default: 0.5)\n";
    std::cout << "  --snare-decay <ms>      Snare decay time (default: 200)\n";
    std::cout << "\n";
    std::cout << "  Hi-hat parameters:\n";
    std::cout << "  --hihat-decay <ms>      Hi-hat decay time for closed (default: 50)\n";
    std::cout << "  --hihat-decay-open <ms> Hi-hat decay time for open (default: 300)\n";
    std::cout << "\n";
    std::cout << "  Clap parameters:\n";
    std::cout << "  --clap-count <3-8>      Number of clap bursts (default: 3)\n";
    std::cout << "  --clap-spread <ms>      Time between bursts (default: 15)\n";
    std::cout << "\n";
    std::cout << "  Bus compressor:\n";
    std::cout << "  --compressor <0|1>      Enable bus compressor (default: 1)\n";
    std::cout << "  --master-gain <dB>      Master gain (default: 0)\n";
    std::cout << "\n";
    std::cout << "Examples:\n";
    std::cout << "  " << progName << " drums.wav --preset house\n";
    std::cout << "  " << progName << " drums.wav --pattern basic --bpm 130 --bars 8\n";
    std::cout << "  " << progName << " drums.wav --pattern trap --bpm 140 --swing 30\n";
    std::cout << "  " << progName << " drums.wav --kick-freq-start 200 --kick-decay 500\n";
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
// Map pattern name to index
//==============================================================================
int patternNameToIndex(const std::string& name) {
    if (name == "kick")    return 1;
    if (name == "snare")   return 2;
    if (name == "hihat")   return 3;
    if (name == "basic")   return 4;
    if (name == "full")    return 0;
    if (name == "house")   return 5;
    if (name == "techno")  return 6;
    if (name == "hiphop")  return 7;
    if (name == "trap")    return 8;
    if (name == "dnb")     return 9;
    return -1;
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

    // Parse output file (first non-flag argument)
    std::string outFile;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] != '-') {
            outFile = argv[i];
            break;
        }
    }

    if (outFile.empty()) {
        std::cerr << "Error: Need output WAV file path\n\n";
        printHelp(argv[0]);
        return 1;
    }

    std::cout << "VC-Drum: Drum Synthesizer CLI\n";

    //==========================================================================
    // Initialize parameters
    //==========================================================================
    VCPluginDSP::Params params = makeDefaultParams();
    int bars = 4;
    unsigned int sampleRate = 44100;

    // Load preset first (sets defaults including BPM for style)
    if (args.count("--preset")) {
        std::string presetName = args["--preset"];
        if (!loadPreset(presetName, params)) {
            std::cerr << "Error: Unknown preset '" << presetName << "'\n";
            std::cerr << "Available: bypass, kick-only, snare-only, hihat-only, basic-beat,\n";
            std::cerr << "           house, techno, hiphop, trap, dnb\n";
            return 1;
        }
        std::cout << "Preset: " << presetName << "\n";
    }

    // Override pattern
    if (args.count("--pattern")) {
        int idx = patternNameToIndex(args["--pattern"]);
        if (idx < 0) {
            std::cerr << "Error: Unknown pattern '" << args["--pattern"] << "'\n";
            return 1;
        }
        params.pattern = idx;
    }

    // Override with CLI parameters
    if (args.count("--bpm"))        params.bpm = std::stof(args["--bpm"]);
    if (args.count("--bars"))       bars = std::stoi(args["--bars"]);
    if (args.count("--swing"))      params.swing = std::stof(args["--swing"]);
    if (args.count("--humanize"))   params.humanize = std::stof(args["--humanize"]);
    if (args.count("--master-gain")) params.masterGain = std::stof(args["--master-gain"]);

    // Kick params
    if (args.count("--kick-freq-start")) params.kick.freqStart = std::stof(args["--kick-freq-start"]);
    if (args.count("--kick-freq-end"))   params.kick.freqEnd = std::stof(args["--kick-freq-end"]);
    if (args.count("--kick-decay"))      params.kick.decay = std::stof(args["--kick-decay"]);

    // Snare params
    if (args.count("--snare-tone"))  params.snare.tone = std::stof(args["--snare-tone"]);
    if (args.count("--snare-decay")) params.snare.decay = std::stof(args["--snare-decay"]);

    // Hi-hat params
    if (args.count("--hihat-decay"))      params.hihat.decayClosed = std::stof(args["--hihat-decay"]);
    if (args.count("--hihat-decay-open")) params.hihat.decayOpen = std::stof(args["--hihat-decay-open"]);

    // Clap params
    if (args.count("--clap-count"))  params.clap.clapCount = std::stoi(args["--clap-count"]);
    if (args.count("--clap-spread")) params.clap.spread = std::stof(args["--clap-spread"]);

    // Compressor
    if (args.count("--compressor")) params.compressor.enabled = (args["--compressor"] == "1");

    //==========================================================================
    // Print settings
    //==========================================================================
    std::cout << "BPM: " << params.bpm << "\n";
    std::cout << "Bars: " << bars << "\n";
    std::cout << "Swing: " << params.swing << "%\n";
    std::cout << "Humanize: " << params.humanize << "%\n";
    std::cout << "Sample rate: " << sampleRate << " Hz\n";
    std::cout << "Kick: start=" << params.kick.freqStart << "Hz end=" << params.kick.freqEnd
              << "Hz decay=" << params.kick.decay << "ms\n";
    std::cout << "Snare: tone=" << params.snare.tone << " decay=" << params.snare.decay << "ms\n";
    std::cout << "Hi-hat: decay_closed=" << params.hihat.decayClosed
              << "ms decay_open=" << params.hihat.decayOpen << "ms\n";
    std::cout << "Clap: count=" << params.clap.clapCount << " spread=" << params.clap.spread << "ms\n";
    std::cout << "Compressor: " << (params.compressor.enabled ? "on" : "off") << "\n";
    std::cout << "Master gain: " << params.masterGain << " dB\n";

    //==========================================================================
    // Initialize DSP and render
    //==========================================================================
    VCPluginDSP dsp;
    dsp.prepare(sampleRate, 4096);
    dsp.setParams(params);

    std::cout << "Rendering " << bars << " bars...\n";

    std::vector<float> outLeft, outRight;
    dsp.renderBars(bars, outLeft, outRight);

    size_t totalFrames = outLeft.size();
    std::cout << "Total frames: " << totalFrames << "\n";
    std::cout << "Duration: " << (double)totalFrames / sampleRate << " seconds\n";

    // Calculate peak level
    float peakL = 0.0f, peakR = 0.0f;
    for (size_t i = 0; i < totalFrames; ++i) {
        peakL = std::max(peakL, std::abs(outLeft[i]));
        peakR = std::max(peakR, std::abs(outRight[i]));
    }
    float peakDbL = 20.0f * std::log10(std::max(peakL, 1e-10f));
    float peakDbR = 20.0f * std::log10(std::max(peakR, 1e-10f));
    std::cout << "Peak L: " << peakDbL << " dB, Peak R: " << peakDbR << " dB\n";

    //==========================================================================
    // Write output WAV file
    //==========================================================================
    drwav_data_format format;
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
    format.channels = 2;
    format.sampleRate = sampleRate;
    format.bitsPerSample = 32;

    drwav wav;
    if (!drwav_init_file_write(&wav, outFile.c_str(), &format, NULL)) {
        std::cerr << "Error: Cannot write file: " << outFile << "\n";
        return 1;
    }

    // Interleave
    std::vector<float> output(totalFrames * 2);
    for (size_t i = 0; i < totalFrames; ++i) {
        output[i * 2]     = outLeft[i];
        output[i * 2 + 1] = outRight[i];
    }

    drwav_write_pcm_frames(&wav, totalFrames, output.data());
    drwav_uninit(&wav);

    std::cout << "Done! Output: " << outFile << "\n";
    return 0;
}
