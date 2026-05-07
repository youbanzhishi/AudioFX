// VC-Noise Standalone CLI - Signal Generator
// Generates noise/signal without requiring input audio file
// Uses dr_wav for WAV I/O

#define DR_WAV_IMPLEMENTATION
#include <dr_wav.h>

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <cmath>

#include "../DSP/VCNoiseDSP.h"

//==============================================================================
// Presets
//==============================================================================
struct Preset {
    const char* name;
    VCPluginDSP::Params params;
};

static const Preset presets[] = {
    {"white-0db",    {0, 1000.0f, 20000.0f, 5.0f, true, 0.0f,  0, 0.0f, true}},
    {"pink-6db",     {1, 1000.0f, 20000.0f, 5.0f, true, -6.0f, 0, 0.0f, true}},
    {"brown-12db",   {2, 1000.0f, 20000.0f, 5.0f, true, -12.0f,0, 0.0f, true}},
    {"sine-1k",      {3, 1000.0f, 20000.0f, 5.0f, true, -6.0f, 0, 0.0f, true}},
    {"sine-440",     {3, 440.0f,  20000.0f, 5.0f, true, -6.0f, 0, 0.0f, true}},
    {"sweep-log",    {4, 20.0f,   20000.0f, 10.0f,true, -6.0f, 0, 0.0f, true}},
    {"sweep-linear", {4, 20.0f,   20000.0f, 10.0f,false,-6.0f, 0, 0.0f, true}},
    {"impulse",      {5, 1000.0f, 20000.0f, 5.0f, true, 0.0f,  0, 0.0f, true}},
    {"impulse-1s",   {5, 1000.0f, 20000.0f, 5.0f, true, 0.0f,  0, 1.0f, true}},
};

//==============================================================================
// Help text
//==============================================================================
void printHelp(const char* progName) {
    std::cout << "VC-Noise Standalone CLI - Signal Generator\n\n";
    std::cout << "Usage: " << progName << " <output.wav> [options]\n";
    std::cout << "  (No input file needed - generates signal from scratch)\n\n";
    std::cout << "Signal types:\n";
    std::cout << "  0=White  1=Pink  2=Brown  3=Sine  4=Sweep  5=Impulse\n\n";
    std::cout << "Options:\n";
    std::cout << "  --help              Show this help\n";
    std::cout << "  --preset <name>     Preset name\n";
    std::cout << "  --type <0-5>        Signal type (default: 0)\n";
    std::cout << "  --freq <Hz>         Frequency for sine/sweep start (default: 1000)\n";
    std::cout << "  --end-freq <Hz>     Sweep end frequency (default: 20000)\n";
    std::cout << "  --sweep-dur <sec>   Sweep duration 1-60s (default: 5)\n";
    std::cout << "  --sweep-log <0|1>   Log sweep=1, linear=0 (default: 1)\n";
    std::cout << "  --volume <dB>       Volume -60~0 dBFS (default: -6)\n";
    std::cout << "  --channel <0-3>     0=stereo 1=left 2=right 3=antiphase (default: 0)\n";
    std::cout << "  --pulse-period <s>  Impulse period 0-10s, 0=single (default: 0)\n";
    std::cout << "  --duration <sec>    Output duration in seconds (default: 10)\n";
    std::cout << "  --sample-rate <Hz>  Sample rate (default: 44100)\n\n";
    std::cout << "Presets:\n";
    for (const auto& p : presets) {
        std::cout << "  " << p.name << "\n";
    }
    std::cout << "\nExamples:\n";
    std::cout << "  " << progName << " out.wav --type 0 --duration 5 --volume -6\n";
    std::cout << "  " << progName << " out.wav --type 3 --freq 440 --duration 3\n";
    std::cout << "  " << progName << " out.wav --preset sweep-log --duration 10\n";
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
        std::cerr << "Error: Need output file\n\n";
        printHelp(argv[0]);
        return 1;
    }

    // Default parameters
    float durationSec = 10.0f;
    unsigned int sampleRate = 44100;

    if (args.count("--duration")) {
        durationSec = std::stof(args["--duration"]);
        durationSec = std::clamp(durationSec, 0.1f, 3600.0f);
    }
    if (args.count("--sample-rate")) {
        sampleRate = std::stoul(args["--sample-rate"]);
        if (sampleRate == 0) sampleRate = 44100;
    }

    std::cout << "VC-Noise Standalone CLI - Signal Generator\n";
    std::cout << "Output: " << outFile << "\n";

    //==========================================================================
    // Initialize DSP
    //==========================================================================
    VCPluginDSP dsp;
    dsp.prepare(sampleRate, 4096);

    VCPluginDSP::Params params;

    // Load preset if specified
    if (args.count("--preset")) {
        std::string presetName = args["--preset"];
        std::cout << "Preset: " << presetName << "\n";
        if (!loadPreset(presetName, params)) {
            std::cerr << "Error: Unknown preset\n";
            return 1;
        }
        dsp.setParams(params);
    }

    // Override with command line parameters
    if (args.count("--type")) {
        params.type = std::stoi(args["--type"]);
        params.type = std::clamp(params.type, 0, 5);
        dsp.setParams(params);
        const char* typeNames[] = {"White", "Pink", "Brown", "Sine", "Sweep", "Impulse"};
        std::cout << "Type: " << typeNames[params.type] << "\n";
    }

    if (args.count("--freq")) {
        params.frequency = std::stof(args["--freq"]);
        params.frequency = std::clamp(params.frequency, 20.0f, 20000.0f);
        dsp.setParams(params);
        std::cout << "Frequency: " << params.frequency << " Hz\n";
    }

    if (args.count("--end-freq")) {
        params.endFreq = std::stof(args["--end-freq"]);
        params.endFreq = std::clamp(params.endFreq, 20.0f, 20000.0f);
        dsp.setParams(params);
        std::cout << "End freq: " << params.endFreq << " Hz\n";
    }

    if (args.count("--sweep-dur")) {
        params.sweepDuration = std::stof(args["--sweep-dur"]);
        params.sweepDuration = std::clamp(params.sweepDuration, 1.0f, 60.0f);
        dsp.setParams(params);
        std::cout << "Sweep duration: " << params.sweepDuration << "s\n";
    }

    if (args.count("--sweep-log")) {
        params.sweepLog = (args["--sweep-log"] == "1");
        dsp.setParams(params);
        std::cout << "Sweep: " << (params.sweepLog ? "logarithmic" : "linear") << "\n";
    }

    if (args.count("--volume")) {
        params.volume = std::stof(args["--volume"]);
        params.volume = std::clamp(params.volume, -60.0f, 0.0f);
        dsp.setParams(params);
        std::cout << "Volume: " << params.volume << " dBFS\n";
    }

    if (args.count("--channel")) {
        params.channelMode = std::stoi(args["--channel"]);
        params.channelMode = std::clamp(params.channelMode, 0, 3);
        dsp.setParams(params);
        const char* chNames[] = {"Stereo", "Left only", "Right only", "Anti-phase"};
        std::cout << "Channel: " << chNames[params.channelMode] << "\n";
    }

    if (args.count("--pulse-period")) {
        params.pulsePeriod = std::stof(args["--pulse-period"]);
        params.pulsePeriod = std::clamp(params.pulsePeriod, 0.0f, 10.0f);
        dsp.setParams(params);
        std::cout << "Pulse period: " << params.pulsePeriod << "s\n";
    }

    //==========================================================================
    // Generate audio
    //==========================================================================
    drwav_uint64 totalFrames = (drwav_uint64)(durationSec * sampleRate);

    std::cout << "Duration: " << durationSec << "s (" << totalFrames << " frames)\n";
    std::cout << "Sample rate: " << sampleRate << " Hz\n";
    std::cout << "Generating...\n";

    std::vector<float> left(totalFrames);
    std::vector<float> right(totalFrames);

    dsp.generate(left.data(), right.data(), static_cast<int>(totalFrames));

    //==========================================================================
    // Write output file using dr_wav
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

    // Interleave output
    std::vector<float> output(totalFrames * 2);
    for (drwav_uint64 i = 0; i < totalFrames; ++i) {
        output[i * 2] = left[i];
        output[i * 2 + 1] = right[i];
    }

    drwav_write_pcm_frames(&wav, totalFrames, output.data());
    drwav_uninit(&wav);

    std::cout << "Done! Output: " << outFile << "\n";
    return 0;
}
