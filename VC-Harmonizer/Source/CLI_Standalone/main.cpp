// ============================================================================
// VC-Harmonizer Standalone CLI - Intelligent Harmony Generator
// Gen2: YIN Detect → Interval Shift → LPC Formant → Multi-voice Mix
// Uses dr_wav for WAV I/O
// ============================================================================

#define DR_WAV_IMPLEMENTATION
#include <dr_wav.h>

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <set>
#include <sstream>

// Include DSP header
#include "../DSP/VCPluginDSP.h"

void printHelp(const char* progName) {
    std::cout << "VC-Harmonizer Standalone CLI - Intelligent Harmony Generator\n";
    std::cout << "YIN Detect → Interval Shift → LPC Formant Preservation\n\n";
    std::cout << "Usage: " << progName << " <input.wav> <output.wav> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --help, -h              Show this help\n";
    std::cout << "  --voices 1-4            Number of harmony voices (default: 2)\n";
    std::cout << "  --intervals <n,...>     Intervals in semitones, comma-separated\n";
    std::cout << "                          default: 3,7 (3rd + 5th above)\n";
    std::cout << "  --voice-gain <dB,...>   Per-voice gain (default: -3,-3,-3,-3)\n";
    std::cout << "  --voice-pan <-1~1,...>  Per-voice pan (default: -0.5,0.5,-0.3,0.3)\n";
    std::cout << "  --formant-preserve 0-100 Formant preservation (default: 100)\n";
    std::cout << "  --autokey 0|1           Auto-detect musical key (default: 0)\n";
    std::cout << "  --scale 0-5             Scale: 0=Chromatic, 1=Major, 2=Minor,\n";
    std::cout << "                          3=Pentatonic, 4=Blues, 5=Custom (default: 0)\n";
    std::cout << "  --direction up|down|both Harmony direction (default: up)\n";
    std::cout << "  --mix 0-100             Dry/Wet mix (default: 50)\n";
    std::cout << "  --bypass 0|1            Bypass (default: 0)\n";
    std::cout << "\nPresets:\n";
    std::cout << "  --preset <name>\n";
    std::cout << "    third-above    Third above harmony (3 semitones)\n";
    std::cout << "    fifth-above    Fifth above harmony (7 semitones)\n";
    std::cout << "    choir          Full choir (3rd+5th+8ve above)\n";
    std::cout << "    low-harmony    Below harmony (-5 semitones)\n";
    std::cout << "    octaver        Octave above + below\n";
    std::cout << "\nExamples:\n";
    std::cout << "  " << progName << " vocal.wav harmony.wav --intervals 3,7 --formant-preserve 100\n";
    std::cout << "  " << progName << " vocal.wav harmony.wav --preset choir\n";
    std::cout << "  " << progName << " vocal.wav harmony.wav --voices 3 --intervals -5,3,7 --direction both\n";
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

std::vector<float> parseFloatList(const std::string& s) {
    std::vector<float> result;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
        result.push_back(std::stof(item));
    }
    return result;
}

int main(int argc, char** argv) {
    auto args = parseArgs(argc, argv);
    
    if (args.count("--help") || argc < 3) {
        printHelp(argv[0]);
        return 0;
    }
    
    // Parse input/output
    const char* inputPath = argv[1];
    const char* outputPath = argv[2];
    
    // Create DSP
    VCPluginDSP dsp;
    auto params = dsp.getParams();
    
    // Parse parameters
    if (args.count("--voices")) params.numVoices = std::clamp(std::stoi(args["--voices"]), 1, 4);
    if (args.count("--intervals")) {
        auto vals = parseFloatList(args["--intervals"]);
        for (size_t i = 0; i < vals.size() && i < 4; ++i)
            params.intervals[i] = vals[i];
    }
    if (args.count("--voice-gain")) {
        auto vals = parseFloatList(args["--voice-gain"]);
        for (size_t i = 0; i < vals.size() && i < 4; ++i)
            params.voiceGain[i] = vals[i];
    }
    if (args.count("--voice-pan")) {
        auto vals = parseFloatList(args["--voice-pan"]);
        for (size_t i = 0; i < vals.size() && i < 4; ++i)
            params.voicePan[i] = vals[i];
    }
    if (args.count("--formant-preserve")) params.formantPreserve = std::stof(args["--formant-preserve"]);
    if (args.count("--autokey")) params.autoKey = (args["--autokey"] == "1");
    if (args.count("--scale")) params.scale = std::stoi(args["--scale"]);
    if (args.count("--direction")) {
        std::string dir = args["--direction"];
        if (dir == "down") params.direction = 1;
        else if (dir == "both") params.direction = 2;
        else params.direction = 0;
    }
    if (args.count("--mix")) // no mix parameter in current Params = std::stof(args["--mix"]) / 100.0f;
    if (args.count("--bypass")) params.bypass = (args["--bypass"] == "1");
    
    // Presets
    if (args.count("--preset")) {
        std::string preset = args["--preset"];
        if (preset == "third-above") {
            params.numVoices = 1; params.intervals[0] = 3; params.formantPreserve = 100;
        } else if (preset == "fifth-above") {
            params.numVoices = 1; params.intervals[0] = 7; params.formantPreserve = 100;
        } else if (preset == "choir") {
            params.numVoices = 3; params.intervals[0] = 3; params.intervals[1] = 7; params.intervals[2] = 12;
            params.voiceGain[0] = -3; params.voiceGain[1] = -3; params.voiceGain[2] = -6;
        } else if (preset == "low-harmony") {
            params.numVoices = 1; params.intervals[0] = -5; params.formantPreserve = 100;
        } else if (preset == "octaver") {
            params.numVoices = 2; params.intervals[0] = 12; params.intervals[1] = -12;
            params.voiceGain[0] = -6; params.voiceGain[1] = -6;
        }
    }
    
    dsp.setParams(params);
    
    // Load WAV
    unsigned int channels, sampleRate;
    drwav_uint64 totalFrames;
    float* pSampleData = drwav_open_file_and_read_pcm_frames_f32(
        inputPath, &channels, &sampleRate, &totalFrames, nullptr);
    if (!pSampleData) {
        std::cerr << "Error: Cannot read " << inputPath << "\n";
        return 1;
    }
    
    std::cout << "VC-Harmonizer Standalone CLI\n";
    std::cout << "Input: " << inputPath << "\n";
    std::cout << "Output: " << outputPath << "\n";
    std::cout << "Sample rate: " << sampleRate << " Hz\n";
    std::cout << "Channels: " << channels << "\n";
    std::cout << "Total frames: " << totalFrames << "\n";
    std::cout << "Voices: " << (int)params.numVoices << "\n";
    
    // Process
    dsp.prepare(sampleRate, totalFrames);
    
    if (channels == 1) {
        dsp.process(pSampleData, nullptr, totalFrames);
    } else {
        // De-interleave
        std::vector<float> left(totalFrames), right(totalFrames);
        for (drwav_uint64 i = 0; i < totalFrames; ++i) {
            left[i] = pSampleData[i * channels];
            right[i] = pSampleData[i * channels + 1];
        }
        dsp.process(left.data(), right.data(), totalFrames);
        for (drwav_uint64 i = 0; i < totalFrames; ++i) {
            pSampleData[i * channels] = left[i];
            pSampleData[i * channels + 1] = right[i];
        }
    }
    
    // Write WAV
    drwav_data_format format;
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
    format.channels = channels;
    format.sampleRate = sampleRate;
    format.bitsPerSample = 32;
    
    drwav pWav;
    if (!drwav_init_file_write(&pWav, outputPath, &format, nullptr)) {
    if (false) {
        std::cerr << "Error: Cannot write " << outputPath << "\n";
        drwav_free(pSampleData, nullptr);
        return 1;
    }
    drwav_write_pcm_frames(&pWav, totalFrames, pSampleData);
    drwav_uninit(&pWav);
    drwav_free(pSampleData, nullptr);
    
    std::cout << "Done! Output: " << outputPath << "\n";
    return 0;
}
