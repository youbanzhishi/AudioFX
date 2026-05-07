// VC-Comp Standalone CLI - No JUCE dependency
// Uses dr_wav for WAV I/O

#define DR_WAV_IMPLEMENTATION
#include <dr_wav.h>

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <set>

// Include DSP header
#include "../DSP/VCCompDSP.h"

void printHelp(const char* progName) {
    std::cout << "VC-Comp Standalone CLI - Sidechain Compressor (No JUCE)\n\n";
    std::cout << "Usage: " << progName << " <input.wav> <output.wav> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --help, -h           Show this help\n";
    std::cout << "  --threshold <dB>    Threshold (-60 ~ 0), default: 0\n";
    std::cout << "  --ratio <ratio>      Compression ratio (0.5 ~ 50), default: 1.0\n";
    std::cout << "  --attack <ms>        Attack time (0.5 ~ 500), default: 16\n";
    std::cout << "  --release <ms>      Release time (5 ~ 5000), default: 160\n";
    std::cout << "  --gain <dB>          Makeup gain (-30 ~ 30), default: 0\n";
    std::cout << "  --knee <mode>        Knee mode: soft, hard, default: soft\n";
    std::cout << "  --character <mode>  Character: clean, warm, default: warm\n";
    std::cout << "  --mix <%>            Dry/wet mix (0 ~ 100), default: 100\n";
    std::cout << "  --trim <dB>          Trim output (-18 ~ 18), default: 0\n";
    std::cout << "\nPresets:\n";
    std::cout << "  --preset <name>     Preset name\n";
    std::cout << "    vocal-1db    Light compression (threshold -20dB, ratio 2:1)\n";
    std::cout << "    vocal-3db    Medium compression (threshold -10dB, ratio 4:1)\n";
    std::cout << "    vocal-6db    Heavy compression (threshold -5dB, ratio 6:1)\n";
    std::cout << "    drums        Drum compression (threshold -15dB, ratio 3:1)\n";
    std::cout << "    bass         Bass compression (threshold -12dB, ratio 4:1)\n";
    std::cout << "    limiter      Limiter mode (threshold -1dB, ratio 20:1)\n";
    std::cout << "\nExamples:\n";
    std::cout << "  " << progName << " in.wav out.wav --threshold -20 --ratio 3 --attack 10\n";
    std::cout << "  " << progName << " in.wav out.wav --preset vocal-3db\n";
}

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

bool loadPreset(const std::string& name, VCCompDSP::Params& p) {
    if (name == "vocal-1db") {
        p.threshold = -20.0f; p.ratio = 2.0f; p.attack = 15.0f; p.release = 200.0f;
        p.gain = 0.0f; p.mix = 100.0f;
        return true;
    } else if (name == "vocal-3db") {
        p.threshold = -10.0f; p.ratio = 4.0f; p.attack = 10.0f; p.release = 150.0f;
        p.gain = 2.0f; p.mix = 100.0f;
        return true;
    } else if (name == "vocal-6db") {
        p.threshold = -5.0f; p.ratio = 6.0f; p.attack = 5.0f; p.release = 100.0f;
        p.gain = 4.0f; p.mix = 100.0f;
        return true;
    } else if (name == "drums") {
        p.threshold = -15.0f; p.ratio = 3.0f; p.attack = 1.0f; p.release = 50.0f;
        p.gain = 2.0f; p.mix = 100.0f;
        return true;
    } else if (name == "bass") {
        p.threshold = -12.0f; p.ratio = 4.0f; p.attack = 20.0f; p.release = 300.0f;
        p.gain = 3.0f; p.mix = 100.0f;
        return true;
    } else if (name == "limiter") {
        p.threshold = -1.0f; p.ratio = 20.0f; p.attack = 0.5f; p.release = 50.0f;
        p.gain = 0.0f; p.mix = 100.0f; p.trim = -0.3f;
        return true;
    }
    return false;
}

float getFloatArg(const std::map<std::string, std::string>& args, const std::string& key, float defaultVal) {
    auto it = args.find(key);
    if (it != args.end() && !it->second.empty()) {
        try {
            return std::stof(it->second);
        } catch (...) {
            return defaultVal;
        }
    }
    return defaultVal;
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
    
    std::cout << "VC-Comp Standalone CLI (No JUCE)\n";
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
    VCCompDSP dsp;
    dsp.prepare(sampleRate, 4096);
    
    VCCompDSP::Params params;
    
    // Load preset if specified
    if (args.count("--preset")) {
        std::string presetName = args["--preset"];
        std::cout << "Preset: " << presetName << "\n";
        if (!loadPreset(presetName, params)) {
            std::cerr << "Error: Unknown preset\n";
            return 1;
        }
    }
    
    // Override with command line arguments
    params.threshold = getFloatArg(args, "--threshold", params.threshold);
    params.ratio = getFloatArg(args, "--ratio", params.ratio);
    params.attack = getFloatArg(args, "--attack", params.attack);
    params.release = getFloatArg(args, "--release", params.release);
    params.gain = getFloatArg(args, "--gain", params.gain);
    params.mix = getFloatArg(args, "--mix", params.mix);
    params.trim = getFloatArg(args, "--trim", params.trim);
    
    if (args.count("--knee")) {
        std::string knee = args["--knee"];
        if (knee == "hard") params.kneeMode = 0;
        else if (knee == "soft") params.kneeMode = 1;
        else if (knee == "auto") params.kneeMode = 2;
    }
    
    if (args.count("--character")) {
        std::string chr = args["--character"];
        if (chr == "clean") params.character = 0;
        else if (chr == "warm") params.character = 1;
    }
    
    // Print settings
    std::cout << "\nParameters:\n";
    std::cout << "  Threshold: " << params.threshold << " dB\n";
    std::cout << "  Ratio: " << params.ratio << ":1\n";
    std::cout << "  Attack: " << params.attack << " ms\n";
    std::cout << "  Release: " << params.release << " ms\n";
    std::cout << "  Makeup: " << params.gain << " dB\n";
    std::cout << "  Dry/Wet: " << params.mix << "%\n";
    std::cout << "  Trim: " << params.trim << " dB\n";
    std::cout << "  Knee: " << (params.kneeMode == 0 ? "Hard" : params.kneeMode == 1 ? "Soft" : "Auto") << "\n";
    std::cout << "  Character: " << (params.character == 0 ? "Clean" : "Warm") << "\n";
    
    dsp.setParams(params);
    
    // Process
    std::cout << "\nProcessing...\n";
    dsp.process(left.data(), right.data(), static_cast<int>(totalFrames));
    
    // Print GR stats
    float gr = dsp.getGainReduction();
    std::cout << "Gain reduction: " << gr << " dB\n";
    
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
