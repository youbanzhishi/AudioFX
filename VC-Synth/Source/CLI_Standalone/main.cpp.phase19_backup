// VC-Synth Standalone CLI — Virtual Instrument (No JUCE Dependency)
// Uses dr_wav for WAV output
// Supports: --note, --midi, --scale modes for rendering

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
// Note name parsing helpers
//==============================================================================

// Parse note name like "C4", "A3", "F#5", "Bb2" to MIDI note number
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
    static const char* noteNamesFlat[] = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };

    int noteIdx = -1;
    int pos = 0;
    char letter = name[pos];
    pos++;

    // Find base note
    for (int i = 0; i < 12; ++i) {
        if (letter == noteNames[i][0]) {
            noteIdx = i;
            break;
        }
    }
    if (noteIdx < 0) return -1;

    // Check for sharp/flat
    if (pos < (int)name.size()) {
        if (name[pos] == '#') {
            noteIdx = (noteIdx + 1) % 12;
            pos++;
        } else if (name[pos] == 'b' && pos > 0 && letter != 'b') {
            noteIdx = (noteIdx + 11) % 12;
            pos++;
        }
    }

    // Parse octave
    if (pos >= (int)name.size()) return -1;
    int octave = std::stoi(name.substr(pos));

    // MIDI note = (octave + 1) * 12 + noteIdx
    return (octave + 1) * 12 + noteIdx;
}

// Parse comma-separated notes (e.g., "60,64,67" or "C4,E4,G4")
std::vector<int> parseNoteList(const std::string& str)
{
    std::vector<int> notes;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, ',')) {
        // Trim whitespace
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

// Parse scale range like "C4:C5"
bool parseScaleRange(const std::string& str, int& startNote, int& endNote)
{
    size_t colonPos = str.find(':');
    if (colonPos == std::string::npos) return false;

    startNote = parseNoteName(str.substr(0, colonPos));
    endNote = parseNoteName(str.substr(colonPos + 1));

    return (startNote >= 0 && endNote >= 0 && startNote <= 127 && endNote <= 127);
}

//==============================================================================
// Write stereo WAV file using dr_wav
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

    // Interleave
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
// Help text
//==============================================================================
void printHelp(const char* progName) {
    std::cout << "VC-Synth Standalone CLI — Subtractive Synthesizer\n\n";
    std::cout << "Usage: " << progName << "<output.wav> [options]\n\n";
    std::cout << "Note Input (one required):\n";
    std::cout << "  --note <MIDI>         Note number or name (e.g., 60 or C4), comma-separated for chords\n";
    std::cout << "  --velocity <0-127>    Note velocity (default: 100)\n";
    std::cout << "  --duration <sec>      Note duration in seconds (default: 2.0)\n";
    std::cout << "  --scale <C4:C5>       Scale range (render each note sequentially)\n";
    std::cout << "  --step <semitones>    Scale step size (default: 1)\n\n";
    std::cout << "Oscillator:\n";
    std::cout << "  --osc <type>          Oscillator: sine/saw/square/triangle/noise (default: saw)\n";
    std::cout << "  --unison <1-7>        Unison voices (default: 1)\n";
    std::cout << "  --detune <cents>      Detune spread in cents (default: 10)\n\n";
    std::cout << "Filter:\n";
    std::cout << "  --cutoff <Hz>         Filter cutoff 20-20000Hz (default: 8000)\n";
    std::cout << "  --resonance <0-1>     Filter resonance (default: 0.5)\n";
    std::cout << "  --filter-type <type>  Filter: lp/bp/hp (default: lp)\n\n";
    std::cout << "Envelope:\n";
    std::cout << "  --attack <ms>         Attack time (default: 10)\n";
    std::cout << "  --decay <ms>          Decay time (default: 100)\n";
    std::cout << "  --sustain <0-1>       Sustain level (default: 0.7)\n";
    std::cout << "  --release <ms>        Release time (default: 200)\n\n";
    std::cout << "Effects:\n";
    std::cout << "  --reverb-mix <0-1>    Reverb mix (default: 0)\n";
    std::cout << "  --delay-mix <0-1>     Delay mix (default: 0)\n";
    std::cout << "  --delay-time <ms>     Delay time (default: 375)\n\n";
    std::cout << "Output:\n";
    std::cout << "  --volume <dB>         Output volume in dB (default: 0)\n";
    std::cout << "  --preset <name>       Preset: bypass/init/pad/lead/bass/pluck/strings/organ/synth-brass/supersaw\n";
    std::cout << "  --sample-rate <Hz>    Sample rate (default: 44100)\n";
    std::cout << "  --help, -h            Show this help\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << progName << " output.wav --note 60 --duration 2.0 --osc sine\n";
    std::cout << "  " << progName << " lead.wav --preset lead --note 60 --duration 2.0\n";
    std::cout << "  " << progName << " chord.wav --preset supersaw --note 48,60,64 --duration 4.0\n";
    std::cout << "  " << progName << " scale.wav --scale C4:C5 --step 1 --preset pad --duration 0.5\n";
}

//==============================================================================
// Parse oscillator type from string
//==============================================================================
VCOscType parseOscType(const std::string& s) {
    if (s == "sine") return VC_OSC_SINE;
    if (s == "saw") return VC_OSC_SAW;
    if (s == "square") return VC_OSC_SQUARE;
    if (s == "triangle") return VC_OSC_TRIANGLE;
    if (s == "noise") return VC_OSC_NOISE;
    std::cerr << "Warning: Unknown osc type '" << s << "', using saw\n";
    return VC_OSC_SAW;
}

VCFilterType parseFilterType(const std::string& s) {
    if (s == "lp") return VC_FILTER_LP;
    if (s == "bp") return VC_FILTER_BP;
    if (s == "hp") return VC_FILTER_HP;
    std::cerr << "Warning: Unknown filter type '" << s << "', using lp\n";
    return VC_FILTER_LP;
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

    // Sample rate
    int sampleRate = 44100;
    if (args.count("--sample-rate")) {
        sampleRate = std::stoi(args["--sample-rate"]);
    }

    std::cout << "VC-Synth — Subtractive Synthesizer\n";
    std::cout << "Output: " << outFile << "\n";
    std::cout << "Sample rate: " << sampleRate << " Hz\n";

    //==========================================================================
    // Initialize synth
    //==========================================================================
    VCPluginDSP synth;
    synth.prepare(sampleRate, 4096);

    VCPluginDSP::Params params;

    // Load preset if specified
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

    // Override with command line parameters
    if (args.count("--osc")) params.oscType = parseOscType(args["--osc"]);
    if (args.count("--unison")) params.unison = std::clamp(std::stoi(args["--unison"]), 1, 7);
    if (args.count("--detune")) params.detune = std::stof(args["--detune"]);
    if (args.count("--cutoff")) params.cutoff = std::stof(args["--cutoff"]);
    if (args.count("--resonance")) params.resonance = std::clamp(std::stof(args["--resonance"]), 0.0f, 1.0f);
    if (args.count("--filter-type")) params.filterType = parseFilterType(args["--filter-type"]);
    if (args.count("--attack")) params.attack = std::stof(args["--attack"]);
    if (args.count("--decay")) params.decay = std::stof(args["--decay"]);
    if (args.count("--sustain")) params.sustain = std::clamp(std::stof(args["--sustain"]), 0.0f, 1.0f);
    if (args.count("--release")) params.release = std::stof(args["--release"]);
    if (args.count("--reverb-mix")) params.reverbMix = std::clamp(std::stof(args["--reverb-mix"]), 0.0f, 1.0f);
    if (args.count("--delay-mix")) params.delayMix = std::clamp(std::stof(args["--delay-mix"]), 0.0f, 1.0f);
    if (args.count("--delay-time")) params.delayTime = std::stof(args["--delay-time"]);
    if (args.count("--volume")) params.volumeDB = std::stof(args["--volume"]);

    synth.setParams(params);

    // Velocity
    float velocity = 100.0f / 127.0f;
    if (args.count("--velocity")) {
        velocity = std::clamp(std::stof(args["--velocity"]) / 127.0f, 0.0f, 1.0f);
    }

    // Duration
    float duration = 2.0f;
    if (args.count("--duration")) {
        duration = std::stof(args["--duration"]);
    }

    //==========================================================================
    // Render mode
    //==========================================================================
    std::vector<float> outL, outR;

    if (args.count("--scale")) {
        // Scale mode: render each note in a range sequentially
        int startNote, endNote;
        if (!parseScaleRange(args["--scale"], startNote, endNote)) {
            std::cerr << "Error: Invalid scale range. Use format: C4:C5\n";
            return 1;
        }

        int step = 1;
        if (args.count("--step")) step = std::stoi(args["--step"]);
        if (step == 0) step = 1;

        // Ensure start < end
        if (startNote > endNote) std::swap(startNote, endNote);

        std::cout << "Scale: " << startNote << " to " << endNote << " step " << step << "\n";

        float noteDuration = duration;

        for (int note = startNote; note <= endNote; note += step) {
            int noteStart = (int)outL.size();

            // Estimate max buffer size for this note (duration + release tail)
            int releaseSamples = (int)(params.release * sampleRate / 1000.0f * 1.5f);
            int maxSamples = (int)(noteDuration * sampleRate) + releaseSamples;

            // Extend output buffers with estimated max size
            outL.resize(noteStart + maxSamples, 0.0f);
            outR.resize(noteStart + maxSamples, 0.0f);

            // Render this note
            int rendered = 0;
            synth.renderNote(note, velocity, noteDuration,
                             outL.data() + noteStart, outR.data() + noteStart, rendered);

            // Resize to actual rendered amount
            outL.resize(noteStart + rendered);
            outR.resize(noteStart + rendered);

            std::cout << "  Note " << note << " (" << VCPluginDSP::midiNoteToFreq(note) << " Hz) - " << rendered << " samples\n";
        }

    } else if (args.count("--note")) {
        // Single note or chord mode
        std::vector<int> notes = parseNoteList(args["--note"]);
        if (notes.empty()) {
            std::cerr << "Error: No valid notes specified\n";
            return 1;
        }

        if (notes.size() == 1) {
            // Single note
            std::cout << "Note: " << notes[0] << " (" << VCPluginDSP::midiNoteToFreq(notes[0]) << " Hz)\n";
            std::cout << "Duration: " << duration << "s\n";

            // Calculate buffer size (duration + release tail)
            int releaseSamples = (int)(params.release * sampleRate / 1000.0f * 1.5f);
            int totalSamples = (int)(duration * sampleRate) + releaseSamples;

            outL.resize(totalSamples, 0.0f);
            outR.resize(totalSamples, 0.0f);

            int rendered = 0;
            synth.renderNote(notes[0], velocity, duration, outL.data(), outR.data(), rendered);
            outL.resize(rendered);
            outR.resize(rendered);
        } else {
            // Chord mode: trigger all notes simultaneously
            std::cout << "Chord: ";
            for (int n : notes) std::cout << n << " ";
            std::cout << "\n";

            int releaseSamples = (int)(params.release * sampleRate / 1000.0f * 1.5f);
            int totalSamples = (int)(duration * sampleRate) + releaseSamples;

            outL.resize(totalSamples, 0.0f);
            outR.resize(totalSamples, 0.0f);

            // Trigger all notes
            for (int n : notes) {
                synth.noteOn(n, velocity);
            }

            // Render main duration
            int mainSamples = (int)(duration * sampleRate);
            synth.render(outL.data(), outR.data(), mainSamples);

            // Release all notes
            for (int n : notes) {
                synth.noteOff(n);
            }

            // Render release tail
            synth.render(outL.data() + mainSamples, outR.data() + mainSamples, releaseSamples);
        }
    } else {
        std::cerr << "Error: No note input specified. Use --note or --scale\n\n";
        printHelp(argv[0]);
        return 1;
    }

    //==========================================================================
    // Write output file
    //==========================================================================
    if (outL.empty()) {
        std::cerr << "Error: No audio rendered\n";
        return 1;
    }

    std::cout << "Rendering " << outL.size() << " samples (" << (float)outL.size() / sampleRate << "s)\n";

    if (!writeWAV(outFile, outL, outR, sampleRate)) {
        return 1;
    }

    std::cout << "Done! Output: " << outFile << "\n";
    return 0;
}
