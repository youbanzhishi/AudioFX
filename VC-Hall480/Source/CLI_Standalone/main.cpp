// VC-Hall480 CLI Standalone — Zero-dependency command-line reverb tool
// Uses dr_wav for WAV I/O, no JUCE dependency
#define VC_STANDALONE
#include "../DSP/VCPluginDSP.h"

#define DR_WAV_IMPLEMENTATION
#include <dr_wav.h>

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <cstring>

void printHelp(const char* p) {
    std::cout << "VC-Hall480 CLI Standalone — Lexicon 480L-Class Reverb\n\n";
    std::cout << "Usage: " << p << " <input.wav> <output.wav> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --help, -h              Show help\n";
    std::cout << "  --algorithm <0|1|2>     0=Hall, 1=Random Hall, 2=Plate\n";
    std::cout << "  --room <0-100>          Room size\n";
    std::cout << "  --decay <0.3-20>        Decay time T60 (seconds)\n";
    std::cout << "  --predelay <0-200>      Pre-delay (ms)\n";
    std::cout << "  --diffusion <0-100>     Diffusion\n";
    std::cout << "  --shape <0-100>         Shape\n";
    std::cout << "  --spread <0-100>        Stereo spread\n";
    std::cout << "  --hidecay <0.1-2.0>     HF decay multiplier\n";
    std::cout << "  --lodecay <0.1-2.0>     LF decay multiplier\n";
    std::cout << "  --chorusrate <0-5>      Chorus rate (Hz)\n";
    std::cout << "  --chorusdepth <0-100>   Chorus depth\n";
    std::cout << "  --mix <0-100>           Dry/Wet mix\n";
}

std::map<std::string, std::string> parseArgs(int c, char** v) {
    std::map<std::string, std::string> a;
    for (int i = 1; i < c; ++i) {
        std::string arg = v[i];
        if (arg == "--help" || arg == "-h") a["--help"] = "";
        else if (arg.substr(0, 2) == "--") {
            std::string k = arg;
            if (i + 1 < c && v[i + 1][0] != '-') { a[k] = v[++i]; }
            else { a[k] = ""; }
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
    const char* inPath = files[0].c_str();
    const char* outPath = files[1].c_str();

    // Read input WAV
    unsigned int channels = 0, sampleRate = 0;
    drwav_uint64 totalFrames = 0;
    float* pPCM = drwav_open_file_and_read_pcm_frames_f32(inPath, &channels, &sampleRate, &totalFrames, nullptr);
    if (!pPCM) { std::cerr << "Cannot read: " << inPath << "\n"; return 1; }

    std::cout << "Input: " << inPath << " " << sampleRate << "Hz " << channels << "ch " << totalFrames << " frames\n";

    // De-interleave to stereo buffers
    int numFrames = static_cast<int>(totalFrames);
    int numCh = static_cast<int>(channels);
    std::vector<float> left(numFrames, 0.0f);
    std::vector<float> right(numFrames, 0.0f);

    for (int i = 0; i < numFrames; ++i) {
        left[i] = pPCM[i * numCh];
        right[i] = (numCh > 1) ? pPCM[i * numCh + 1] : pPCM[i * numCh];
    }
    drwav_free(pPCM, nullptr);

    // Initialize DSP
    VCPluginDSP dsp;
    dsp.prepare(static_cast<double>(sampleRate), 4096);

    VCPluginDSP::Params p = dsp.getParams();
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

    // Process
    dsp.process(left.data(), right.data(), numFrames);

    // Interleave back
    std::vector<float> outPCM(numFrames * 2);
    for (int i = 0; i < numFrames; ++i) {
        outPCM[i * 2] = left[i];
        outPCM[i * 2 + 1] = right[i];
    }

    // Write output WAV
    drwav_data_format format;
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
    format.channels = 2;
    format.sampleRate = sampleRate;
    format.bitsPerSample = 32;

    drwav* pWav = drwav_open_file_write(outPath, &format);
    if (!pWav) { std::cerr << "Cannot write: " << outPath << "\n"; return 1; }
    drwav_write_pcm_frames(pWav, totalFrames, outPCM.data());
    drwav_close(pWav);

    std::cout << "Output: " << outPath << " Done!\n";
    return 0;
}
