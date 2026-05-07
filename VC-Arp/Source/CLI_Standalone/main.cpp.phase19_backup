// VC-Arp Standalone CLI — Arpeggiator (No JUCE Dependency)
// Uses dr_wav for WAV output
// Generates arpeggio patterns from MIDI notes and renders to WAV

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
#include <set>
#include <cmath>
#include <sstream>
#include <algorithm>

// Include DSP header AFTER DR_WAV_IMPLEMENTATION and VC_STANDALONE
#include "../DSP/VCPluginDSP.h"

//==============================================================================
// Note name parsing (shared with VC-Synth)
//==============================================================================

int parseNoteName(const std::string& name)
{
    if (name.empty()) return -1;

    // Check if it's a pure number (MIDI note number)
    bool isNumber = true;
    for (char c : name) {
        if (!std::isdigit(c) && c != '-') { isNumber = false; break; }
    }
    if (isNumber) return std::stoi(name);

    // Parse note name format: [A-G][#|b][0-9]
    static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    int noteIdx = -1;
    int pos = 0;
    char letter = name[pos];
    pos++;

    for (int i = 0; i < 12; ++i) {
        if (letter == noteNames[i][0]) {
            noteIdx = i;
            break;
        }
    }
    if (noteIdx < 0) return -1;

    if (pos < (int)name.size()) {
        if (name[pos] == '#') {
            noteIdx = (noteIdx + 1) % 12;
            pos++;
        } else if (name[pos] == 'b' && pos > 0 && letter != 'b') {
            noteIdx = (noteIdx + 11) % 12;
            pos++;
        }
    }

    if (pos >= (int)name.size()) return -1;
    int octave = std::stoi(name.substr(pos));
    return (octave + 1) * 12 + noteIdx;
}

std::vector<int> parseNoteList(const std::string& str)
{
    std::vector<int> notes;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, ',')) {
        size_t start = token.find_first_not_of(" \t");
        size_t end = token.find_last_not_of(" \t");
        if (start != std::string::npos) {
            token = token.substr(start, end - start + 1);
        }
        int note = parseNoteName(token);
        if (note >= 0 && note <= 127) {
            notes.push_back(note);
        } else {
            std::cerr << "Warning: Invalid note '" << token << "', skipping\n";
        }
    }
    return notes;
}

//==============================================================================
// Write stereo WAV file
//==============================================================================
bool writeWAV(const std::string& filename, const std::vector<float>& left,
              const std::vector<float>& right, int sampleRate)
{
    int numFrames = (int)left.size();
    if (numFrames == 0) return false;

    drwav_data_format format;
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
    format.channels = 2;
    format.sampleRate = sampleRate;
    format.bitsPerSample = 32;

    drwav wav;
    if (!drwav_init_file_write(&wav, filename.c_str(), &format, NULL)) {
        std::cerr << "Error: Cannot write file: " << filename << "\n";
        return false;
    }

    std::vector<float> interleaved(numFrames * 2);
    for (int i = 0; i < numFrames; ++i) {
        interleaved[i * 2] = left[i];
        interleaved[i * 2 + 1] = right[i];
    }

    drwav_write_pcm_frames(&wav, (drwav_uint64)numFrames, interleaved.data());
    drwav_uninit(&wav);
    return true;
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
// Parse helpers
//==============================================================================
VCArpMode parseArpMode(const std::string& s) {
    if (s == "up")         return VC_ARP_UP;
    if (s == "down")       return VC_ARP_DOWN;
    if (s == "up-down")    return VC_ARP_UP_DOWN;
    if (s == "down-up")    return VC_ARP_DOWN_UP;
    if (s == "random")     return VC_ARP_RANDOM;
    if (s == "as-played")  return VC_ARP_AS_PLAYED;
    if (s == "chord")      return VC_ARP_CHORD;
    std::cerr << "Warning: Unknown arp mode '" << s << "', using up\n";
    return VC_ARP_UP;
}

VCArpRate parseRate(const std::string& s) {
    if (s == "1/1")  return VC_RATE_1_1;
    if (s == "1/2")  return VC_RATE_1_2;
    if (s == "1/4")  return VC_RATE_1_4;
    if (s == "1/8")  return VC_RATE_1_8;
    if (s == "1/16") return VC_RATE_1_16;
    if (s == "1/32") return VC_RATE_1_32;
    std::cerr << "Warning: Unknown rate '" << s << "', using 1/8\n";
    return VC_RATE_1_8;
}

VCVelocityMode parseVelocityMode(const std::string& s) {
    if (s == "original")  return VC_VEL_ORIGINAL;
    if (s == "ascending") return VC_VEL_ASCENDING;
    if (s == "descending") return VC_VEL_DESCENDING;
    if (s == "random")    return VC_VEL_RANDOM;
    std::cerr << "Warning: Unknown velocity mode '" << s << "', using original\n";
    return VC_VEL_ORIGINAL;
}

VCArpSynth::Waveform parseWaveform(const std::string& s) {
    if (s == "sine")   return VCArpSynth::SINE;
    if (s == "saw")    return VCArpSynth::SAW;
    if (s == "square") return VCArpSynth::SQUARE;
    std::cerr << "Warning: Unknown waveform '" << s << "', using sine\n";
    return VCArpSynth::SINE;
}

const char* arpModeToString(VCArpMode m) {
    switch (m) {
    case VC_ARP_UP:        return "up";
    case VC_ARP_DOWN:      return "down";
    case VC_ARP_UP_DOWN:   return "up-down";
    case VC_ARP_DOWN_UP:   return "down-up";
    case VC_ARP_RANDOM:    return "random";
    case VC_ARP_AS_PLAYED: return "as-played";
    case VC_ARP_CHORD:     return "chord";
    default:               return "unknown";
    }
}

const char* rateToString(VCArpRate r) {
    switch (r) {
    case VC_RATE_1_1:  return "1/1";
    case VC_RATE_1_2:  return "1/2";
    case VC_RATE_1_4:  return "1/4";
    case VC_RATE_1_8:  return "1/8";
    case VC_RATE_1_16: return "1/16";
    case VC_RATE_1_32: return "1/32";
    default:           return "1/8";
    }
}

//==============================================================================
// Help text
//==============================================================================
void printHelp(const char* progName) {
    std::cout << "VC-Arp: Arpeggiator CLI\n\n";
    std::cout << "Usage: " << progName << " <output.wav> --notes <notes> [options]\n\n";
    std::cout << "Generates arpeggio patterns from MIDI notes and renders to WAV.\n\n";
    std::cout << "Required:\n";
    std::cout << "  --notes <list>         MIDI notes (e.g., 60,64,67 or C4,E4,G4)\n\n";
    std::cout << "Arpeggio:\n";
    std::cout << "  --mode <type>          up/down/up-down/down-up/random/as-played/chord (default: up)\n";
    std::cout << "  --rate <value>         1/1,1/2,1/4,1/8,1/16,1/32 (default: 1/8)\n";
    std::cout << "  --octave-range <1-4>   Octave expansion range (default: 1)\n";
    std::cout << "  --gate <1-200%>        Note length as % of step (default: 100)\n";
    std::cout << "  --swing <0-100%>       Swing amount (default: 0)\n";
    std::cout << "  --velocity-mode <type> original/ascending/descending/random (default: original)\n";
    std::cout << "  --transpose <semi>     Transpose in semitones (default: 0)\n";
    std::cout << "  --humanize <0-100%>    Timing/velocity randomization (default: 0)\n\n";
    std::cout << "Timing:\n";
    std::cout << "  --bpm <value>          Tempo (default: 120)\n";
    std::cout << "  --bars <N>             Number of bars (default: 4)\n\n";
    std::cout << "Synth (CLI only):\n";
    std::cout << "  --waveform <type>      sine/saw/square (default: sine)\n";
    std::cout << "  --volume <dB>          Output volume (default: -6)\n\n";
    std::cout << "Presets:\n";
    std::cout << "  --preset <name>        bypass, up-8th, down-8th, up-down-16th,\n";
    std::cout << "                         trance-gate, random-bells, chord-pad,\n";
    std::cout << "                         octave-run, ping-pong\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << progName << " out.wav --notes 60,64,67 --mode up --rate 1/8 --bpm 120 --bars 4\n";
    std::cout << "  " << progName << " out.wav --preset trance-gate --notes 60,64,67,72 --bpm 128 --bars 8\n";
    std::cout << "  " << progName << " out.wav --notes C4,E4,G4 --mode up-down --rate 1/16 --waveform saw\n";
    std::cout << "  " << progName << " out.wav --preset octave-run --notes 48,60,72 --bpm 140 --bars 4\n";
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

    // Find output file (first non-flag argument)
    std::string outFile;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] != '-') {
            outFile = argv[i];
            break;
        }
    }

    if (outFile.empty()) {
        std::cerr << "Error: No output file specified\n\n";
        printHelp(argv[0]);
        return 1;
    }

    //==========================================================================
    // Parse notes (required)
    //==========================================================================
    std::vector<int> notes;
    if (args.count("--notes")) {
        notes = parseNoteList(args["--notes"]);
    }

    if (notes.empty()) {
        std::cerr << "Error: No notes specified. Use --notes 60,64,67 or --notes C4,E4,G4\n\n";
        printHelp(argv[0]);
        return 1;
    }

    //==========================================================================
    // Initialize parameters
    //==========================================================================
    int sampleRate = 44100;
    if (args.count("--sample-rate")) {
        sampleRate = std::stoi(args["--sample-rate"]);
    }

    VCPluginDSP::Params params;

    // Load preset first
    if (args.count("--preset")) {
        std::string presetName = args["--preset"];
        if (!VCPluginDSP::getPresetByName(presetName.c_str(), params)) {
            std::cerr << "Error: Unknown preset '" << presetName << "'\n";
            std::cerr << "Available presets: ";
            for (int i = 0; i < VCPluginDSP::getNumPresets(); ++i) {
                if (i > 0) std::cerr << ", ";
                std::cerr << VCPluginDSP::getPresetName(i);
            }
            std::cerr << "\n";
            return 1;
        }
        std::cout << "Preset: " << presetName << "\n";
    }

    // Override with CLI parameters
    if (args.count("--mode"))          params.mode = parseArpMode(args["--mode"]);
    if (args.count("--rate"))          params.rate = parseRate(args["--rate"]);
    if (args.count("--octave-range"))  params.octaveRange = std::clamp(std::stoi(args["--octave-range"]), 1, 4);
    if (args.count("--gate"))          params.gate = std::clamp(std::stof(args["--gate"]), 1.0f, 200.0f);
    if (args.count("--swing"))         params.swing = std::clamp(std::stof(args["--swing"]), 0.0f, 100.0f);
    if (args.count("--velocity-mode")) params.velocityMode = parseVelocityMode(args["--velocity-mode"]);
    if (args.count("--bpm"))           params.bpm = std::clamp(std::stof(args["--bpm"]), 20.0f, 300.0f);
    if (args.count("--transpose"))     params.transpose = std::stoi(args["--transpose"]);
    if (args.count("--humanize"))      params.humanize = std::clamp(std::stof(args["--humanize"]), 0.0f, 100.0f);
    if (args.count("--waveform"))      params.waveform = parseWaveform(args["--waveform"]);
    if (args.count("--volume"))        params.volumeDB = std::stof(args["--volume"]);

    int bars = 4;
    if (args.count("--bars")) bars = std::max(1, std::stoi(args["--bars"]));

    //==========================================================================
    // Print settings
    //==========================================================================
    std::cout << "VC-Arp: Arpeggiator CLI\n";
    std::cout << "Output: " << outFile << "\n";
    std::cout << "Notes: ";
    for (size_t i = 0; i < notes.size(); ++i) {
        if (i > 0) std::cout << ",";
        std::cout << notes[i];
    }
    std::cout << "\n";
    std::cout << "Mode: " << arpModeToString(params.mode) << "\n";
    std::cout << "Rate: " << rateToString(params.rate) << "\n";
    std::cout << "BPM: " << params.bpm << "\n";
    std::cout << "Bars: " << bars << "\n";
    std::cout << "Octave range: " << params.octaveRange << "\n";
    std::cout << "Gate: " << params.gate << "%\n";
    std::cout << "Swing: " << params.swing << "%\n";
    std::cout << "Humanize: " << params.humanize << "%\n";
    std::cout << "Transpose: " << params.transpose << " semitones\n";
    std::cout << "Sample rate: " << sampleRate << " Hz\n";

    //==========================================================================
    // Initialize DSP and render
    //==========================================================================
    VCPluginDSP dsp;
    dsp.prepare(sampleRate, 4096);
    dsp.setParams(params);
    dsp.setChordNotes(notes);

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
    if (!writeWAV(outFile, outLeft, outRight, sampleRate)) {
        return 1;
    }

    std::cout << "Done! Output: " << outFile << "\n";
    return 0;
}
