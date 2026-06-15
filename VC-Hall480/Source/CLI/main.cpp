// VC-Hall480 CLI — Lexicon 480L-Class Reverb Command Line Tool
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include "../DSP/VCPluginDSP.h"

void printHelp(const char* p) {
    std::cout << "VC-Hall480 CLI — Lexicon 480L-Class Algorithmic Reverb\n\n";
    std::cout << "Usage: " << p << " <input.wav> <output.wav> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --help, -h              Show help\n";
    std::cout << "  --algorithm <0|1|2>     0=Hall, 1=Random Hall, 2=Plate (default: 0)\n";
    std::cout << "  --room <0-100>          Room size (default: 50)\n";
    std::cout << "  --decay <0.3-20>        Decay time T60 in seconds (default: 2.0)\n";
    std::cout << "  --predelay <0-200>      Pre-delay in ms (default: 20)\n";
    std::cout << "  --diffusion <0-100>     Early reflection diffusion (default: 70)\n";
    std::cout << "  --shape <0-100>         Envelope shape (default: 50)\n";
    std::cout << "  --spread <0-100>        Stereo width (default: 80)\n";
    std::cout << "  --hidecay <0.1-2.0>     HF decay multiplier (default: 0.5)\n";
    std::cout << "  --lodecay <0.1-2.0>     LF decay multiplier (default: 1.0)\n";
    std::cout << "  --chorusrate <0-5>      Chorus rate in Hz (default: 1.0)\n";
    std::cout << "  --chorusdepth <0-100>   Chorus depth (default: 30)\n";
    std::cout << "  --mix <0-100>           Dry/Wet mix (default: 30)\n";
    std::cout << "\nExample:\n";
    std::cout << "  " << p << " in.wav out.wav --algorithm 1 --room 80 --decay 4.0 --mix 40\n";
}

std::map<std::string, std::string> parseArgs(int c, char** v) {
    std::map<std::string, std::string> a;
    for (int i = 1; i < c; ++i) {
        std::string arg = v[i];
        if (arg == "--help" || arg == "-h") a["--help"] = "";
        else if (arg.substr(0, 2) == "--") {
            std::string k = arg, val;
            if (i + 1 < c && v[i + 1][0] != '-') val = v[++i];
            a[k] = val;
        }
    }
    return a;
}

int main(int c, char** v) {
    if (c < 2) { printHelp(v[0]); return 1; }
    auto a = parseArgs(c, v);
    if (a.count("--help")) { printHelp(v[0]); return 0; }

    std::vector<std::string> files;
    for (int i = 1; i < c; ++i) if (v[i][0] != '-') files.push_back(v[i]);
    if (files.size() < 2) { std::cerr << "Need input and output files\n"; return 1; }
    std::string in = files[0], out = files[1];
    std::cout << "VC-Hall480 CLI\nInput: " << in << "\nOutput: " << out << "\n";

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r(fm.createReaderFor(juce::File(in)));
    if (!r) { std::cerr << "Cannot read file: " << in << "\n"; return 1; }
    std::cout << "Sample rate: " << r->sampleRate << "Hz, Channels: " << r->numChannels << "\n";

    juce::AudioBuffer<float> buf(static_cast<int>(r->numChannels), static_cast<int>(r->lengthInSamples));
    r->read(buf.getArrayOfWritePointers(), buf.getNumChannels(), 0, buf.getNumSamples());

    VCPluginDSP dsp;
    dsp.prepare(r->sampleRate, 4096);

    VCPluginDSP::Params p = dsp.getParams();

    // Apply CLI parameter overrides
    if (a.count("--algorithm"))  p.algorithm = std::stoi(a["--algorithm"]);
    if (a.count("--room"))      p.roomSize = std::stof(a["--room"]);
    if (a.count("--decay"))     p.decayTime = std::stof(a["--decay"]);
    if (a.count("--predelay"))  p.preDelay = std::stof(a["--predelay"]);
    if (a.count("--diffusion")) p.diffusion = std::stof(a["--diffusion"]);
    if (a.count("--shape"))     p.shape = std::stof(a["--shape"]);
    if (a.count("--spread"))    p.spread = std::stof(a["--spread"]);
    if (a.count("--hidecay"))   p.hiDecay = std::stof(a["--hidecay"]);
    if (a.count("--lodecay"))   p.loDecay = std::stof(a["--lodecay"]);
    if (a.count("--chorusrate"))    p.chorusRate = std::stof(a["--chorusrate"]);
    if (a.count("--chorusdepth"))   p.chorusDepth = std::stof(a["--chorusdepth"]);
    if (a.count("--mix"))       p.mix = std::stof(a["--mix"]);

    dsp.setParams(p);

    std::cout << "Processing with: algorithm=" << p.algorithm
              << " room=" << p.roomSize
              << " decay=" << p.decayTime << "s"
              << " mix=" << p.mix << "%\n";

    if (buf.getNumChannels() >= 2) {
        dsp.process(buf.getWritePointer(0), buf.getWritePointer(1), buf.getNumSamples());
    } else if (buf.getNumChannels() == 1) {
        auto* l = buf.getWritePointer(0);
        dsp.process(l, l, buf.getNumSamples());
    }

    juce::WavAudioFormat wavFmt;
    std::unique_ptr<juce::AudioFormatWriter> writer(wavFmt.createWriterFor(
        new juce::FileOutputStream(juce::File(out)), r->sampleRate,
        static_cast<unsigned int>(buf.getNumChannels()), 32, {}, 0));
    if (writer) writer->writeFromAudioSampleBuffer(buf, 0, buf.getNumSamples());
    else { std::cerr << "Cannot write file: " << out << "\n"; return 1; }

    std::cout << "Done!\n";
    return 0;
}
