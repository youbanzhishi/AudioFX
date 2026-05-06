// VC-Smooth Standalone CLI - No JUCE dependency
// Uses dr_wav for WAV I/O

#define DR_WAV_IMPLEMENTATION
#include <dr_wav.h>

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cmath>

// Include DSP header
#include "../DSP/VCSmoothDSP.h"

void printHelp(const char* progName) {
    std::cout << "VC-Smooth Standalone CLI - Spectral Resonance Smoother (No JUCE)\n\n";
    std::cout << "Usage: " << progName << " <input.wav> <output.wav> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --help, -h              Show this help\n";
    std::cout << "  --depth <0-1>           Smoothing depth (default: 0.5)\n";
    std::cout << "  --speed <0.1-10>        Envelope tracking speed (default: 2.0)\n";
    std::cout << "  --freq-low <Hz>         Low frequency boundary (default: 200)\n";
    std::cout << "  --freq-high <Hz>        High frequency boundary (default: 16000)\n";
    std::cout << "  --sharpness <0.1-5>     Sharpness/threshold factor (default: 1.5)\n";
    std::cout << "  --mix <0-1>             Dry/wet mix (default: 1.0)\n";
    std::cout << "  --input-gain <dB>       Input gain (default: 0)\n";
    std::cout << "  --output-gain <dB>      Output gain (default: 0)\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << progName << " in.wav out.wav\n";
    std::cout << "  " << progName << " in.wav out.wav --depth 0.7 --speed 3.0\n";
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
    
    std::cout << "VC-Smooth Standalone CLI (No JUCE)\n";
    std::cout << "Input: " << inFile << "\n";
    std::cout << "Output: " << outFile << "\n";
    
    // Parse parameters
    VCSmoothDSP::Params params;
    
    if (args.count("--depth")) {
        params.depth = std::stof(args["--depth"]);
        std::cout << "Depth: " << params.depth << "\n";
    }
    if (args.count("--speed")) {
        params.speed = std::stof(args["--speed"]);
        std::cout << "Speed: " << params.speed << "\n";
    }
    if (args.count("--freq-low")) {
        params.freqLow = std::stof(args["--freq-low"]);
        std::cout << "Freq Low: " << params.freqLow << " Hz\n";
    }
    if (args.count("--freq-high")) {
        params.freqHigh = std::stof(args["--freq-high"]);
        std::cout << "Freq High: " << params.freqHigh << " Hz\n";
    }
    if (args.count("--sharpness")) {
        params.sharpness = std::stof(args["--sharpness"]);
        std::cout << "Sharpness: " << params.sharpness << "\n";
    }
    if (args.count("--mix")) {
        params.mix = std::stof(args["--mix"]);
        std::cout << "Mix: " << params.mix << "\n";
    }
    if (args.count("--input-gain")) {
        params.inputGain = std::stof(args["--input-gain"]);
        std::cout << "Input Gain: " << params.inputGain << " dB\n";
    }
    if (args.count("--output-gain")) {
        params.outputGain = std::stof(args["--output-gain"]);
        std::cout << "Output Gain: " << params.outputGain << " dB\n";
    }
    
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
    VCSmoothDSP dsp;
    dsp.prepare(sampleRate, 4096);
    dsp.setParams(params);
    
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
