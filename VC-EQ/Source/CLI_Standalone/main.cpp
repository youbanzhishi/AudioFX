// VC-EQ Standalone CLI - No JUCE dependency
// Uses dr_wav for WAV I/O

#define DR_WAV_IMPLEMENTATION
#include <dr_wav.h>

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cmath>

// Include DSP header (must come before JUCE headers in normal mode)
#include "../DSP/VCEQDSP.h"

struct Preset { const char* name; VCEQDSP::BandParams bands[VCEQDSP::kNumBands]; };

static const Preset presets[] = {
    {"vocal-boost", {{ true, VCEQDSP::FilterType::LowShelf, 80.0f, 0.707f, 1.0f }, { true, VCEQDSP::FilterType::Parametric, 300.0f, 1.0f, 0.5f }, { true, VCEQDSP::FilterType::Parametric, 1000.0f, 1.0f, -1.0f }, { true, VCEQDSP::FilterType::Parametric, 3000.0f, 1.2f, 2.0f }, { true, VCEQDSP::FilterType::HighShelf, 8000.0f, 0.707f, 2.5f }}},
    {"vocal-cut", {{ true, VCEQDSP::FilterType::LowShelf, 80.0f, 0.707f, 0.5f }, { true, VCEQDSP::FilterType::Parametric, 300.0f, 1.0f, 0.5f }, { true, VCEQDSP::FilterType::Parametric, 1000.0f, 1.0f, 0.0f }, { true, VCEQDSP::FilterType::Parametric, 3000.0f, 1.5f, -2.0f }, { true, VCEQDSP::FilterType::HighShelf, 8000.0f, 0.707f, -3.0f }}},
    {"warmth", {{ true, VCEQDSP::FilterType::LowShelf, 80.0f, 0.707f, 3.0f }, { true, VCEQDSP::FilterType::Parametric, 300.0f, 1.0f, 1.5f }, { true, VCEQDSP::FilterType::Parametric, 1000.0f, 1.0f, 0.0f }, { true, VCEQDSP::FilterType::Parametric, 3000.0f, 1.0f, -1.0f }, { true, VCEQDSP::FilterType::HighShelf, 8000.0f, 0.707f, -1.5f }}},
    {"brightness", {{ true, VCEQDSP::FilterType::LowShelf, 80.0f, 0.707f, -1.0f }, { true, VCEQDSP::FilterType::Parametric, 300.0f, 1.0f, -0.5f }, { true, VCEQDSP::FilterType::Parametric, 1000.0f, 1.0f, 0.0f }, { true, VCEQDSP::FilterType::Parametric, 3000.0f, 1.2f, 1.5f }, { true, VCEQDSP::FilterType::HighShelf, 8000.0f, 0.707f, 3.0f }}},
    {"flat", {{ true, VCEQDSP::FilterType::LowShelf, 80.0f, 0.707f, 0.0f }, { true, VCEQDSP::FilterType::Parametric, 300.0f, 1.0f, 0.0f }, { true, VCEQDSP::FilterType::Parametric, 1000.0f, 1.0f, 0.0f }, { true, VCEQDSP::FilterType::Parametric, 3000.0f, 1.0f, 0.0f }, { true, VCEQDSP::FilterType::HighShelf, 8000.0f, 0.707f, 0.0f }}}
};

void printHelp(const char* progName) {
    std::cout << "VC-EQ Standalone CLI - 5-band Parametric EQ (No JUCE)\n\n";
    std::cout << "Usage: " << progName << " <input.wav> <output.wav> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --help, -h              Show this help\n";
    std::cout << "  --preset <name>         Preset (vocal-boost, vocal-cut, warmth, brightness, flat)\n\n";
    std::cout << "Band parameters (band0-band4):\n";
    std::cout << "  --band<N>-freq <Hz>     Frequency\n";
    std::cout << "  --band<N>-gain <dB>     Gain\n";
    std::cout << "  --band<N>-q <value>     Q factor\n";
    std::cout << "  --band<N>-type <0-4>    Type: 0=LowShelf, 1=HighShelf, 2=Parametric, 3=LowPass, 4=HighPass\n";
    std::cout << "  --band<N>-on <0|1>      Enable/disable\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << progName << " in.wav out.wav --preset vocal-boost\n";
    std::cout << "  " << progName << " in.wav out.wav --band3-freq 3000 --band3-gain 2.5\n";
}

std::map<std::string, std::string> parseArgs(int argc, char** argv) {
    std::map<std::string, std::string> args;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            args["--help"] = "";
        } else if (arg.substr(0, 2) == "--") {
            std::string key = arg;
            std::string value;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                value = argv[++i];
            }
            args[key] = value;
        }
    }
    return args;
}

bool buildBandParams(const std::map<std::string, std::string>& args, int idx, VCEQDSP::BandParams& p) {
    std::string prefix = "--band" + std::to_string(idx);
    bool has = false;
    for (const auto& x : args) if (x.first.find(prefix) == 0) { has = true; break; }
    if (!has) return false;
    p = VCEQDSP::BandParams();
    p.frequency = VCEQDSP::kDefaultFrequencies[idx];
    p.q = VCEQDSP::kDefaultQ[idx];
    p.gainDB = VCEQDSP::kDefaultGains[idx];
    p.enabled = true;
    if (args.count(prefix + "-freq")) p.frequency = std::stof(args.at(prefix + "-freq"));
    if (args.count(prefix + "-gain")) p.gainDB = std::stof(args.at(prefix + "-gain"));
    if (args.count(prefix + "-q")) p.q = std::stof(args.at(prefix + "-q"));
    if (args.count(prefix + "-type")) p.type = static_cast<VCEQDSP::FilterType>(std::stoi(args.at(prefix + "-type")));
    if (args.count(prefix + "-on")) p.enabled = (args.at(prefix + "-on") == "1");
    return true;
}

bool loadPreset(const std::string& name, VCEQDSP::BandParams b[VCEQDSP::kNumBands]) {
    for (const auto& p : presets) if (name == p.name) { for (int i=0;i<VCEQDSP::kNumBands;++i) b[i]=p.bands[i]; return true; }
    return false;
}

int main(int argc, char** argv) {
    if (argc < 2) { printHelp(argv[0]); return 1; }
    
    auto args = parseArgs(argc, argv);
    if (args.count("--help")) { printHelp(argv[0]); return 0; }
    
    std::vector<std::string> files;
    for (int i = 1; i < argc; ++i) if (argv[i][0] != '-') files.push_back(argv[i]);
    if (files.size() < 2) {
        std::cerr << "Error: Need input and output files\n\n";
        printHelp(argv[0]);
        return 1;
    }
    
    std::string inFile = files[0];
    std::string outFile = files[1];
    
    std::cout << "VC-EQ Standalone CLI (No JUCE)\n";
    std::cout << "Input: " << inFile << "\n";
    std::cout << "Output: " << outFile << "\n";
    
    // Read WAV using dr_wav
    unsigned int channels = 0;
    unsigned int sampleRate = 0;
    drwav_uint64 totalFrames = 0;
    
    float* pSampleData = drwav_open_file_and_read_pcm_frames_f32(inFile.c_str(), &channels, &sampleRate, &totalFrames, NULL);
    if (pSampleData == NULL) {
        std::cerr << "Error: Cannot read file: " << inFile << "\n";
        return 1;
    }
    
    std::cout << "Sample rate: " << sampleRate << " Hz\n";
    std::cout << "Channels: " << channels << "\n";
    std::cout << "Total frames: " << totalFrames << "\n";
    
    // Convert to L/R arrays
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
    
    // Initialize DSP
    VCEQDSP dsp;
    dsp.prepare(sampleRate, 4096);
    VCEQDSP::BandParams bands[VCEQDSP::kNumBands];
    
    if (args.count("--preset")) {
        std::string pname = args["--preset"];
        std::cout << "Preset: " << pname << "\n";
        if (!loadPreset(pname, bands)) {
            std::cerr << "Error: Unknown preset\n";
            return 1;
        }
        dsp.setAllBands(bands);
    }
    
    for (int i = 0; i < VCEQDSP::kNumBands; ++i) {
        VCEQDSP::BandParams bp;
        if (buildBandParams(args, i, bp)) {
            dsp.setBand(i, bp);
            std::cout << "Band " << i << ": freq=" << bp.frequency << "Hz, gain=" << bp.gainDB << "dB, Q=" << bp.q << "\n";
        }
    }
    
    // Process
    std::cout << "Processing...\n";
    dsp.process(left.data(), right.data(), static_cast<int>(totalFrames));
    
    // Write WAV using dr_wav
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
