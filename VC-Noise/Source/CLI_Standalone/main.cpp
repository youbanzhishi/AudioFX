// VC-NoiseProfile Standalone CLI (Gen2)
// Noise Profile Analysis + Adaptive Spectral Subtraction + Noise Gate
// Gen1 signal generator mode preserved
// Uses dr_wav for WAV I/O

#define DR_WAV_IMPLEMENTATION
#include <dr_wav.h>

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <cmath>
#include <iomanip>

#include "../DSP/VCNoiseDSP.h"

//==============================================================================
// Presets (Gen1 preserved + Gen2 added)
//==============================================================================
struct Preset {
    const char* name;
    VCPluginDSP::Params params;
};

static const Preset presets[] = {
    // Gen1 signal generator presets
    {"white-0db",    {0, 1000.0f, 20000.0f, 5.0f, true, 0.0f,  0, 0.0f, true, 500.0f, 10.0f, 5.0f, -40.0f, 5.0f, 50.0f, 2}},
    {"pink-6db",     {1, 1000.0f, 20000.0f, 5.0f, true, -6.0f, 0, 0.0f, true, 500.0f, 10.0f, 5.0f, -40.0f, 5.0f, 50.0f, 2}},
    {"brown-12db",   {2, 1000.0f, 20000.0f, 5.0f, true, -12.0f,0, 0.0f, true, 500.0f, 10.0f, 5.0f, -40.0f, 5.0f, 50.0f, 2}},
    {"sine-1k",      {3, 1000.0f, 20000.0f, 5.0f, true, -6.0f, 0, 0.0f, true, 500.0f, 10.0f, 5.0f, -40.0f, 5.0f, 50.0f, 2}},
    {"sine-440",     {3, 440.0f,  20000.0f, 5.0f, true, -6.0f, 0, 0.0f, true, 500.0f, 10.0f, 5.0f, -40.0f, 5.0f, 50.0f, 2}},
    {"sweep-log",    {4, 20.0f,   20000.0f, 10.0f,true, -6.0f, 0, 0.0f, true, 500.0f, 10.0f, 5.0f, -40.0f, 5.0f, 50.0f, 2}},
    {"sweep-linear", {4, 20.0f,   20000.0f, 10.0f,false,-6.0f, 0, 0.0f, true, 500.0f, 10.0f, 5.0f, -40.0f, 5.0f, 50.0f, 2}},
    {"impulse",      {5, 1000.0f, 20000.0f, 5.0f, true, 0.0f,  0, 0.0f, true, 500.0f, 10.0f, 5.0f, -40.0f, 5.0f, 50.0f, 2}},
    {"impulse-1s",   {5, 1000.0f, 20000.0f, 5.0f, true, 0.0f,  0, 1.0f, true, 500.0f, 10.0f, 5.0f, -40.0f, 5.0f, 50.0f, 2}},
    // Gen2 noise profile presets
    {"denoise-mild",     {0, 1000.0f, 20000.0f, 5.0f, true, 0.0f, 0, 0.0f, true, 500.0f,  6.0f,  5.0f, -40.0f, 5.0f, 50.0f, 0}},
    {"denoise-moderate", {0, 1000.0f, 20000.0f, 5.0f, true, 0.0f, 0, 0.0f, true, 500.0f, 12.0f,  5.0f, -40.0f, 5.0f, 50.0f, 0}},
    {"denoise-aggressive",{0,1000.0f, 20000.0f, 5.0f, true, 0.0f, 0, 0.0f, true, 500.0f, 20.0f,  3.0f, -40.0f, 5.0f, 50.0f, 0}},
    {"gate-only",        {0, 1000.0f, 20000.0f, 5.0f, true, 0.0f, 0, 0.0f, true, 500.0f, 10.0f,  5.0f, -30.0f, 5.0f, 50.0f, 1}},
    {"denoise-gate",     {0, 1000.0f, 20000.0f, 5.0f, true, 0.0f, 0, 0.0f, true, 500.0f, 12.0f,  5.0f, -35.0f, 5.0f, 80.0f, 2}},
    {"analyze-only",     {0, 1000.0f, 20000.0f, 5.0f, true, 0.0f, 0, 0.0f, true, 500.0f, 10.0f,  5.0f, -40.0f, 5.0f, 50.0f, 3}},
};

//==============================================================================
// Help text
//==============================================================================
void printHelp(const char* progName) {
    std::cout << "VC-NoiseProfile Standalone CLI (Gen2)\n\n";
    std::cout << "Usage:\n";
    std::cout << "  Generate mode:  " << progName << " <output.wav> [gen options]\n";
    std::cout << "  Process mode:   " << progName << " <input.wav> <output.wav> --process [options]\n\n";
    std::cout << "Signal types (generate mode):\n";
    std::cout << "  0=White  1=Pink  2=Brown  3=Sine  4=Sweep  5=Impulse\n\n";
    std::cout << "Generate options:\n";
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
    std::cout << "Process options (noise profile / denoise):\n";
    std::cout << "  --process           Enable process mode (input→output)\n";
    std::cout << "  --learn-ms <ms>     Learn noise from first N ms (default: 500, range: 100-5000)\n";
    std::cout << "  --reduction <dB>    Spectral subtraction amount 0-30 dB (default: 10)\n";
    std::cout << "  --floor <1-20>      Spectral floor ratio (default: 5, prevents musical noise)\n";
    std::cout << "  --threshold <dB>    Noise gate threshold -80~0 dB (default: -40)\n";
    std::cout << "  --attack <ms>       Gate attack 0.1-100 ms (default: 5)\n";
    std::cout << "  --release <ms>      Gate release 1-1000 ms (default: 50)\n";
    std::cout << "  --mode <0-3>        0=denoise 1=gate 2=both 3=analyze (default: 2)\n\n";
    std::cout << "Presets:\n";
    for (const auto& p : presets) {
        std::cout << "  " << p.name << "\n";
    }
    std::cout << "\nExamples:\n";
    std::cout << "  # Generate white noise:\n";
    std::cout << "  " << progName << " out.wav --type 0 --duration 5 --volume -6\n";
    std::cout << "  # Denoise audio:\n";
    std::cout << "  " << progName << " noisy.wav clean.wav --process --learn-ms 500 --reduction 12\n";
    std::cout << "  # Analyze noise profile:\n";
    std::cout << "  " << progName << " noisy.wav analysis.wav --process --mode 3 --learn-ms 1000\n";
    std::cout << "  # Noise gate only:\n";
    std::cout << "  " << progName << " input.wav output.wav --process --mode 1 --threshold -30\n";
}

//==============================================================================
// Parse command line arguments
//==============================================================================
std::map<std::string, std::string> parseArgs(int argc, char** argv) {
    std::map<std::string, std::string> args;
    std::set<std::string> noValueFlags = {"--help", "-h", "--process"};
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            args["--help"] = "";
        } else if (arg == "--process") {
            args["--process"] = "";
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
// Print noise profile (64-band energy)
//==============================================================================
void printNoiseProfile(const VCNoiseProfile& profile, float sampleRate) {
    const float* energies = profile.getBandEnergies();
    int numBands = profile.getNumBands();

    std::cout << "\n=== Noise Profile (64-band energy) ===\n";
    std::cout << "Sample rate: " << sampleRate << " Hz\n";
    std::cout << "Frames analyzed: " << profile.getFrameCount() << "\n\n";

    // Compute mel band edges for display
    float nyquist = sampleRate / 2.0f;
    float melMax = 2595.0f * std::log10(1.0f + nyquist / 700.0f);
    float melStep = melMax / (float)numBands;

    std::cout << "Band  Freq(Hz)     Energy(dB)\n";
    std::cout << "----  ----------   ----------\n";

    for (int i = 0; i < numBands; ++i) {
        float melLow = melStep * i;
        float melHigh = melStep * (i + 1);
        float hzLow = 700.0f * (std::pow(10.0f, melLow / 2595.0f) - 1.0f);
        float hzHigh = 700.0f * (std::pow(10.0f, melHigh / 2595.0f) - 1.0f);
        float hzCenter = (hzLow + hzHigh) / 2.0f;
        float energyDb = (energies[i] > 1e-10f) ? 10.0f * std::log10(energies[i]) : -100.0f;

        // Print every 4th band for readability, plus first and last
        if (i < 4 || i >= numBands - 2 || i % 4 == 0) {
            std::cout << std::setw(4) << i << "  "
                      << std::fixed << std::setprecision(1) << std::setw(8) << hzCenter
                      << " Hz  " << std::setprecision(2) << std::setw(8) << energyDb << " dB\n";
        }
    }

    // ASCII bar graph
    std::cout << "\n  Noise Spectrum Visualization:\n  ";
    for (int i = 0; i < numBands; ++i) {
        float energyDb = (energies[i] > 1e-10f) ? 10.0f * std::log10(energies[i]) : -100.0f;
        // Map -80..0 dB to 0..40 chars
        int barLen = (int)((energyDb + 80.0f) * 0.5f);
        if (barLen < 0) barLen = 0;
        if (barLen > 40) barLen = 40;
        for (int b = 0; b < barLen; ++b) std::cout << '#';
        std::cout << '\n' << "  ";
    }
    std::cout << std::endl;
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

    //==========================================================================
    // Determine mode: Generate or Process
    //==========================================================================
    bool processMode = args.count("--process") > 0;

    if (!processMode) {
        //======================================================================
        // GENERATE MODE (Gen1 preserved)
        //======================================================================
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

        std::cout << "VC-NoiseProfile CLI - Signal Generator (Gen2)\n";
        std::cout << "Output: " << outFile << "\n";

        VCPluginDSP dsp;
        dsp.prepare(sampleRate, 4096);
        VCPluginDSP::Params params;

        if (args.count("--preset")) {
            std::string presetName = args["--preset"];
            std::cout << "Preset: " << presetName << "\n";
            if (!loadPreset(presetName, params)) {
                std::cerr << "Error: Unknown preset\n";
                return 1;
            }
            dsp.setParams(params);
        }

        if (args.count("--type")) {
            params.type = std::stoi(args["--type"]);
            params.type = std::clamp(params.type, 0, 5);
            const char* typeNames[] = {"White", "Pink", "Brown", "Sine", "Sweep", "Impulse"};
            std::cout << "Type: " << typeNames[params.type] << "\n";
            dsp.setParams(params);
        }
        if (args.count("--freq")) { params.frequency = std::clamp(std::stof(args["--freq"]), 20.0f, 20000.0f); dsp.setParams(params); }
        if (args.count("--end-freq")) { params.endFreq = std::clamp(std::stof(args["--end-freq"]), 20.0f, 20000.0f); dsp.setParams(params); }
        if (args.count("--sweep-dur")) { params.sweepDuration = std::clamp(std::stof(args["--sweep-dur"]), 1.0f, 60.0f); dsp.setParams(params); }
        if (args.count("--sweep-log")) { params.sweepLog = (args["--sweep-log"] == "1"); dsp.setParams(params); }
        if (args.count("--volume")) { params.volume = std::clamp(std::stof(args["--volume"]), -60.0f, 0.0f); dsp.setParams(params); }
        if (args.count("--channel")) { params.channelMode = std::clamp(std::stoi(args["--channel"]), 0, 3); dsp.setParams(params); }
        if (args.count("--pulse-period")) { params.pulsePeriod = std::clamp(std::stof(args["--pulse-period"]), 0.0f, 10.0f); dsp.setParams(params); }

        drwav_uint64 totalFrames = (drwav_uint64)(durationSec * sampleRate);
        std::cout << "Duration: " << durationSec << "s (" << totalFrames << " frames)\n";
        std::cout << "Generating...\n";

        std::vector<float> left(totalFrames), right(totalFrames);
        dsp.generate(left.data(), right.data(), static_cast<int>(totalFrames));

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
        std::vector<float> output(totalFrames * 2);
        for (drwav_uint64 i = 0; i < totalFrames; ++i) {
            output[i * 2] = left[i];
            output[i * 2 + 1] = right[i];
        }
        drwav_write_pcm_frames(&wav, totalFrames, output.data());
        drwav_uninit(&wav);

        std::cout << "Done! Output: " << outFile << "\n";

    } else {
        //======================================================================
        // PROCESS MODE (Gen2: Noise Profile + Denoise/Gate)
        //======================================================================
        // Parse input and output files
        std::string inFile, outFile;
        int fileCount = 0;
        for (int i = 1; i < argc; ++i) {
            if (argv[i][0] != '-') {
                if (fileCount == 0) { inFile = argv[i]; fileCount++; }
                else if (fileCount == 1) { outFile = argv[i]; fileCount++; }
            }
        }

        if (inFile.empty() || outFile.empty()) {
            std::cerr << "Error: Process mode requires <input.wav> <output.wav>\n\n";
            printHelp(argv[0]);
            return 1;
        }

        std::cout << "VC-NoiseProfile CLI - Noise Analysis & Reduction (Gen2)\n";
        std::cout << "Input:  " << inFile << "\n";
        std::cout << "Output: " << outFile << "\n";

        // Read input WAV
        drwav wavIn;
        if (!drwav_init_file(&wavIn, inFile.c_str(), NULL)) {
            std::cerr << "Error: Cannot read input file: " << inFile << "\n";
            return 1;
        }

        unsigned int sampleRate = wavIn.sampleRate;
        unsigned int channels = wavIn.channels;
        drwav_uint64 totalFrames = wavIn.totalPCMFrameCount;

        if (channels < 1 || channels > 2) {
            std::cerr << "Error: Only mono/stereo supported (got " << channels << " channels)\n";
            drwav_uninit(&wavIn);
            return 1;
        }

        std::cout << "Sample rate: " << sampleRate << " Hz\n";
        std::cout << "Channels: " << channels << "\n";
        std::cout << "Duration: " << std::fixed << std::setprecision(2) << (double)totalFrames / sampleRate << "s\n";
        std::cout << "Frames: " << totalFrames << "\n";

        // Read all samples as float
        std::vector<float> rawSamples(totalFrames * channels);
        drwav_read_pcm_frames_f32(&wavIn, totalFrames, rawSamples.data());
        drwav_uninit(&wavIn);

        // De-interleave to L/R
        std::vector<float> left(totalFrames), right(totalFrames);
        if (channels == 1) {
            std::memcpy(left.data(), rawSamples.data(), totalFrames * sizeof(float));
            std::memcpy(right.data(), rawSamples.data(), totalFrames * sizeof(float));
        } else {
            for (drwav_uint64 i = 0; i < totalFrames; ++i) {
                left[i] = rawSamples[i * 2];
                right[i] = rawSamples[i * 2 + 1];
            }
        }

        // Initialize DSP
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

        // Parse Gen2 parameters
        if (args.count("--learn-ms")) {
            params.learnMs = std::clamp(std::stof(args["--learn-ms"]), 100.0f, 5000.0f);
            dsp.setParams(params);
        }
        if (args.count("--reduction")) {
            params.reduction = std::clamp(std::stof(args["--reduction"]), 0.0f, 30.0f);
            dsp.setParams(params);
        }
        if (args.count("--floor")) {
            params.floor = std::clamp(std::stof(args["--floor"]), 1.0f, 20.0f);
            dsp.setParams(params);
        }
        if (args.count("--threshold")) {
            params.threshold = std::clamp(std::stof(args["--threshold"]), -80.0f, 0.0f);
            dsp.setParams(params);
        }
        if (args.count("--attack")) {
            params.attack = std::clamp(std::stof(args["--attack"]), 0.1f, 100.0f);
            dsp.setParams(params);
        }
        if (args.count("--release")) {
            params.release = std::clamp(std::stof(args["--release"]), 1.0f, 1000.0f);
            dsp.setParams(params);
        }
        if (args.count("--mode")) {
            params.mode = std::clamp(std::stoi(args["--mode"]), 0, 3);
            dsp.setParams(params);
        }

        const char* modeNames[] = {"Denoise", "Gate", "Both", "Analyze"};
        std::cout << "\nParameters:\n";
        std::cout << "  Mode: " << modeNames[params.mode] << "\n";
        std::cout << "  Learn time: " << params.learnMs << " ms\n";
        std::cout << "  Reduction: " << params.reduction << " dB\n";
        std::cout << "  Floor: " << params.floor << "\n";
        std::cout << "  Threshold: " << params.threshold << " dB\n";
        std::cout << "  Attack: " << params.attack << " ms\n";
        std::cout << "  Release: " << params.release << " ms\n";

        //======================================================================
        // Step 1: Learn noise profile from first N ms
        //======================================================================
        int learnSamples = (int)(params.learnMs * 0.001f * sampleRate);
        learnSamples = std::min(learnSamples, (int)totalFrames);
        std::cout << "\nLearning noise profile from first " << params.learnMs
                  << " ms (" << learnSamples << " samples)...\n";

        // Use mono mix for profile learning
        std::vector<float> monoMix(learnSamples);
        for (int i = 0; i < learnSamples; ++i) {
            monoMix[i] = 0.5f * (left[i] + right[i]);
        }
        dsp.learnNoiseProfile(monoMix.data(), learnSamples, 1);

        if (!dsp.hasNoiseProfile()) {
            std::cerr << "Error: Failed to learn noise profile (insufficient samples)\n";
            return 1;
        }
        std::cout << "Noise profile learned successfully.\n";

        // Print noise profile
        printNoiseProfile(dsp.getNoiseProfile(), (float)sampleRate);

        //======================================================================
        // Step 2: Process audio with learned profile
        //======================================================================
        ProcessMode mode = static_cast<ProcessMode>(params.mode);

        if (mode == ProcessMode::Analyze) {
            // Analyze mode: output original file + profile info (already printed)
            std::cout << "\nAnalyze mode: output is original audio (profile printed above).\n";
        } else {
            std::cout << "\nProcessing audio (" << modeNames[params.mode] << " mode)...\n";

            // Process in chunks for large files
            const int CHUNK_SIZE = 8192;
            std::vector<float> chunkL(CHUNK_SIZE), chunkR(CHUNK_SIZE);
            std::vector<float> outL(totalFrames), outR(totalFrames);

            // Copy original for overlap-add (processWithProfile modifies in-place)
            std::vector<float> workL(left.begin(), left.end());
            std::vector<float> workR(right.begin(), right.end());

            // Zero output buffer
            std::fill(outL.begin(), outL.end(), 0.0f);
            std::fill(outR.begin(), outR.end(), 0.0f);

            // Process entire file as one chunk for proper overlap-add
            // For very large files, we'd need streaming STFT, but for CLI batch this is fine
            dsp.processWithProfile(workL.data(), workR.data(), (int)totalFrames);

            // For non-denoise modes, we need to copy original first
            if (mode == ProcessMode::Gate) {
                std::memcpy(workL.data(), left.data(), totalFrames * sizeof(float));
                std::memcpy(workR.data(), right.data(), totalFrames * sizeof(float));
                // Apply gate only (re-process with gate)
                VCNoiseGate gate;
                gate.prepare(sampleRate);
                gate.setThreshold(params.threshold);
                gate.setAttack(params.attack);
                gate.setRelease(params.release);
                gate.process(workL.data(), workR.data(), (int)totalFrames);
            }

            std::cout << "Processing complete.\n";

            // Copy processed output
            std::memcpy(left.data(), workL.data(), totalFrames * sizeof(float));
            std::memcpy(right.data(), workR.data(), totalFrames * sizeof(float));
        }

        //======================================================================
        // Step 3: Write output WAV
        //======================================================================
        drwav_data_format format;
        format.container = drwav_container_riff;
        format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
        format.channels = 2;
        format.sampleRate = sampleRate;
        format.bitsPerSample = 32;

        drwav wavOut;
        if (!drwav_init_file_write(&wavOut, outFile.c_str(), &format, NULL)) {
            std::cerr << "Error: Cannot write output file: " << outFile << "\n";
            return 1;
        }

        std::vector<float> output(totalFrames * 2);
        for (drwav_uint64 i = 0; i < totalFrames; ++i) {
            output[i * 2] = left[i];
            output[i * 2 + 1] = right[i];
        }
        drwav_write_pcm_frames(&wavOut, totalFrames, output.data());
        drwav_uninit(&wavOut);

        std::cout << "\nDone! Output: " << outFile << "\n";
    }

    return 0;
}
