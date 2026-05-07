// VC-Tune Standalone CLI - Pitch Correction / Auto-Tune - Gen2
// Full PSOLA + LPC Formant Preservation
// No JUCE dependency. Uses dr_wav for WAV I/O.

#define DR_WAV_IMPLEMENTATION
#include <dr_wav.h>

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <sstream>

#include <fstream>
#include "../DSP/VCPluginDSP.h"

//==============================================================================
// Presets (Gen2 updated)
//==============================================================================
struct Preset {
    const char* name;
    const char* description;
    VCTuneDSP::Params params;
};

static const Preset presets[] = {
    {"bypass",     "No processing",                              {0.0f,  0, 0.0f, 0.0f, 100.0f, 0.0f, 0.0f, 50.0f, true,  false}},
    {"tpain",      "Hard correction (T-Pain)",                   {100.0f,0, 0.0f, 0.0f, 100.0f, 0.0f, 0.0f, 50.0f, false, false}},
    {"natural",    "Natural correction (Major)",                 {30.0f, 1, 0.0f, 0.0f, 100.0f, 0.0f, 0.0f, 50.0f, false, false}},
    {"subtle",     "Subtle pitch fix (Chromatic)",               {10.0f, 0, 0.0f, 0.0f, 100.0f, 0.0f, 0.0f, 50.0f, false, false}},
    {"blues",      "Blues scale correction",                     {50.0f, 4, 0.0f, 0.0f, 100.0f, 0.0f, 0.0f, 50.0f, false, false}},
    {"pentatonic", "Pentatonic correction",                      {50.0f, 3, 0.0f, 0.0f, 100.0f, 0.0f, 0.0f, 50.0f, false, false}},
    {"autokey",    "Auto key detection + correct",               {50.0f, 0, 0.0f, 0.0f, 100.0f, 0.0f, 0.0f, 50.0f, false, true}},
    {"shift-up",   "Shift up 2 semitones",                      {100.0f,0, 2.0f, 0.0f, 100.0f, 0.0f, 0.0f, 50.0f, false, false}},
    {"shift-down", "Shift down 3 semitones",                    {100.0f,0,-3.0f, 0.0f, 100.0f, 0.0f, 0.0f, 50.0f, false, false}},
    {"chipmunk",   "Chipmunk (formant shift up)",               {100.0f,0, 0.0f, 0.0f, 0.0f,  5.0f, 0.0f, 50.0f, false, false}},
    {"deep",       "Deep voice (formant shift down)",           {100.0f,0, 0.0f, 0.0f, 0.0f, -5.0f, 0.0f, 50.0f, false, false}},
    {"vibrato",    "Preserve vibrato + natural correction",     {40.0f,  1, 0.0f, 0.0f, 100.0f, 0.0f, 80.0f, 70.0f, false, false}},
};

//==============================================================================
// Help text (Gen2)
//==============================================================================
void printHelp(const char* progName) {
    std::cout << "VC-Tune Gen2 - Pitch Correction / Auto-Tune Plugin\n";
    std::cout << "  Full PSOLA + LPC Formant Preservation\n\n";
    std::cout << "Usage: " << progName << " <input.wav> <output.wav> [options]\n\n";
    std::cout << "Pitch Correction Options:\n";
    std::cout << "  --speed <0-100>            Correction speed (0=none, 100=T-Pain) [default: 50]\n";
    std::cout << "  --scale <0-5>              Scale: 0=Chromatic, 1=Major, 2=Minor,\n";
    std::cout << "                             3=Pentatonic, 4=Blues, 5=Custom [default: 0]\n";
    std::cout << "  --transpose <-12-12>       Global transpose in semitones [default: 0]\n";
    std::cout << "  --cents <-100-100>         Fine-tuning in cents [default: 0]\n";
    std::cout << "  --bypass <0|1>             Bypass processing [default: 0]\n\n";
    std::cout << "Gen2 - Formant & Voice Character:\n";
    std::cout << "  --formant-preserve <0-100> Formant preservation (0=off, 100=full) [default: 100]\n";
    std::cout << "  --formant-shift <-12-12>   Formant shift in semitones [default: 0]\n";
    std::cout << "  --vibrato-preserve <0-100> Vibrato preservation (0=remove, 100=keep) [default: 0]\n";
    std::cout << "  --transition-smooth <0-100> Pitch transition smoothing [default: 50]\n\n";
    std::cout << "Key Detection:\n";
    std::cout << "  --autokey <0|1>            Auto-detect key and set scale [default: 0]\n\n";
    std::cout << "Analysis:\n";
    std::cout << "  --report                   Output pitch detection report to stdout\n";
    std::cout << "  --report-file <path>       Save report to file\n\n";
    std::cout << "Presets:\n";
    std::cout << "  --preset <name>            Load a preset:\n";
    for (const auto& p : presets) {
        std::cout << "                             " << p.name << " - " << p.description << "\n";
    }
    std::cout << "\nExamples:\n";
    std::cout << "  " << progName << " vocal.wav corrected.wav --preset tpain\n";
    std::cout << "  " << progName << " vocal.wav corrected.wav --speed 80 --scale 1\n";
    std::cout << "  " << progName << " vocal.wav corrected.wav --formant-preserve 100 --vibrato-preserve 50\n";
    std::cout << "  " << progName << " vocal.wav corrected.wav --formant-shift -5 --speed 100\n";
    std::cout << "  " << progName << " vocal.wav corrected.wav --autokey 1 --speed 40 --transition-smooth 80\n";
    std::cout << "  " << progName << " vocal.wav /dev/null --report --autokey 1\n";
}

//==============================================================================
// Parse command line
//==============================================================================
std::map<std::string, std::string> parseArgs(int argc, char** argv) {
    std::map<std::string, std::string> args;
    std::set<std::string> noValueFlags = {"--help", "-h", "--report"};
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            args["--help"] = "";
        } else if (arg == "--report") {
            args["--report"] = "1";
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
// Load preset
//==============================================================================
bool loadPreset(const std::string& name, VCTuneDSP::Params& p) {
    for (const auto& preset : presets) {
        if (name == preset.name) {
            p = preset.params;
            return true;
        }
    }
    return false;
}

//==============================================================================
// Format frequency nicely
//==============================================================================
std::string formatFreq(float freq) {
    if (freq <= 0.0f) return "  ---  ";
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << std::setw(7) << freq << " Hz";
    return oss.str();
}

//==============================================================================
// Write report to stream
//==============================================================================
void writeReport(std::ostream& out, const std::vector<VCTuneDSP::PitchReport>& report,
                 const KeyDetector::Result& keyResult, bool autoKey)
{
    out << "================================================================\n";
    out << "VC-Tune Gen2 Pitch Detection Report\n";
    out << "================================================================\n\n";

    if (autoKey && keyResult.detected) {
        out << "Auto Key Detection:\n";
        out << "  Key:         " << KeyDetector::keyName(keyResult.key, keyResult.isMajor) << "\n";
        out << "  Confidence:  " << std::fixed << std::setprecision(3) << keyResult.confidence << "\n";
        out << "  Scale:       " << ScaleQuantizer::scaleName(
                keyResult.isMajor ? ScaleQuantizer::Major : ScaleQuantizer::Minor) << "\n";
        out << "  Key Offset:  " << keyResult.key << " semitones from C\n\n";
    } else if (autoKey) {
        out << "Auto Key Detection: FAILED (insufficient voiced signal)\n\n";
    }

    out << "Frame-by-Frame Analysis:\n";
    out << "----------------------------------------------------------------\n";
    out << std::left
        << std::setw(10) << "Time(ms)"
        << std::setw(10) << "F0(Hz)"
        << std::setw(12) << "Target(Hz)"
        << std::setw(6) << "Note"
        << std::setw(6) << "Tgt"
        << std::setw(8) << "Dev(c)"
        << std::setw(8) << "Conf"
        << "Status\n";
    out << "----------------------------------------------------------------\n";

    for (const auto& r : report) {
        out << std::fixed << std::setprecision(1)
            << std::setw(10) << r.timeMs;

        if (r.voiced) {
            out << std::setw(10) << r.detectedF0
                << std::setw(12) << r.targetF0
                << std::setw(6) << ScaleQuantizer::noteName(r.detectedNote)
                << std::setw(6) << ScaleQuantizer::noteName(r.targetNote)
                << std::setw(8) << std::setprecision(1) << r.deviationCents
                << std::setw(8) << std::setprecision(2) << r.confidence
                << "VOICED";
        } else {
            out << std::setw(10) << "---"
                << std::setw(12) << "---"
                << std::setw(6) << "-"
                << std::setw(6) << "-"
                << std::setw(8) << "-"
                << std::setw(8) << "-"
                << "unvoiced";
        }
        out << "\n";
    }

    out << "----------------------------------------------------------------\n";
    out << "Total frames: " << report.size() << "\n";

    int voiced = 0;
    float avgDev = 0.0f;
    for (const auto& r : report) {
        if (r.voiced) {
            voiced++;
            avgDev += std::fabs(r.deviationCents);
        }
    }
    if (voiced > 0) {
        avgDev /= voiced;
        out << "Voiced frames: " << voiced << " ("
            << std::fixed << std::setprecision(1)
            << 100.0 * voiced / report.size() << "%)\n";
        out << "Average deviation: " << std::fixed << std::setprecision(1)
            << avgDev << " cents\n";
    }
}

//==============================================================================
// Main
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

    // Parse input/output files
    std::vector<std::string> files;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] != '-') {
            files.push_back(argv[i]);
        }
    }

    if (files.size() < 2) {
        std::cerr << "Error: Need input and output files\n\n";
        printHelp(argv[0]);
        return 1;
    }

    std::string inFile = files[0];
    std::string outFile = files[1];

    std::cout << "VC-Tune Gen2 - Pitch Correction / Auto-Tune\n";
    std::cout << "  Full PSOLA + LPC Formant Preservation\n";
    std::cout << "Input:  " << inFile << "\n";
    std::cout << "Output: " << outFile << "\n";

    //==========================================================================
    // Read WAV
    //==========================================================================
    unsigned int channels = 0;
    unsigned int sampleRate = 0;
    drwav_uint64 totalFrames = 0;

    float* pSampleData = drwav_open_file_and_read_pcm_frames_f32(
        inFile.c_str(), &channels, &sampleRate, &totalFrames, NULL);

    if (pSampleData == NULL) {
        std::cerr << "Error: Cannot read file: " << inFile << "\n";
        return 1;
    }

    std::cout << "Sample rate: " << sampleRate << " Hz\n";
    std::cout << "Channels:    " << channels << "\n";
    std::cout << "Duration:    " << std::fixed << std::setprecision(2)
              << (double)totalFrames / sampleRate << " s\n";
    std::cout << "Frames:      " << totalFrames << "\n";

    // Convert interleaved to L/R
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

    //==========================================================================
    // Initialize DSP
    //==========================================================================
    VCTuneDSP dsp;
    dsp.prepare(sampleRate, 4096);

    VCTuneDSP::Params params;

    // Load preset
    if (args.count("--preset")) {
        std::string presetName = args["--preset"];
        if (!loadPreset(presetName, params)) {
            std::cerr << "Error: Unknown preset '" << presetName << "'\n";
            return 1;
        }
        std::cout << "Preset: " << presetName << "\n";
    }

    // Override with CLI parameters (Gen1 compat + Gen2 new params)
    if (args.count("--speed")) {
        params.speed = std::stof(args["--speed"]);
        params.speed = std::clamp(params.speed, 0.0f, 100.0f);
    }
    if (args.count("--scale")) {
        params.scale = std::stoi(args["--scale"]);
        params.scale = std::clamp(params.scale, 0, 5);
    }
    if (args.count("--transpose")) {
        params.transpose = std::stof(args["--transpose"]);
        params.transpose = std::clamp(params.transpose, -12.0f, 12.0f);
    }
    if (args.count("--cents")) {
        params.cents = std::stof(args["--cents"]);
        params.cents = std::clamp(params.cents, -100.0f, 100.0f);
    }
    // Gen1 compat: --formant maps to formantPreserve (0-1 scale -> 0-100)
    if (args.count("--formant")) {
        float oldScale = std::stof(args["--formant"]);
        oldScale = std::clamp(oldScale, 0.0f, 1.0f);
        params.formantPreserve = oldScale * 100.0f;
    }
    // Gen2: --formant-preserve (0-100)
    if (args.count("--formant-preserve")) {
        params.formantPreserve = std::stof(args["--formant-preserve"]);
        params.formantPreserve = std::clamp(params.formantPreserve, 0.0f, 100.0f);
    }
    // Gen2: --formant-shift (-12 to +12)
    if (args.count("--formant-shift")) {
        params.formantShift = std::stof(args["--formant-shift"]);
        params.formantShift = std::clamp(params.formantShift, -12.0f, 12.0f);
    }
    // Gen2: --vibrato-preserve (0-100)
    if (args.count("--vibrato-preserve")) {
        params.vibratoPreserve = std::stof(args["--vibrato-preserve"]);
        params.vibratoPreserve = std::clamp(params.vibratoPreserve, 0.0f, 100.0f);
    }
    // Gen2: --transition-smooth (0-100)
    if (args.count("--transition-smooth")) {
        params.transitionSmooth = std::stof(args["--transition-smooth"]);
        params.transitionSmooth = std::clamp(params.transitionSmooth, 0.0f, 100.0f);
    }
    if (args.count("--bypass")) {
        params.bypass = (args["--bypass"] == "1");
    }
    if (args.count("--autokey")) {
        params.autoKey = (args["--autokey"] == "1");
    }

    // Print effective parameters
    std::cout << "\n--- Parameters ---\n";
    std::cout << "Speed:             " << params.speed << "%\n";
    std::cout << "Scale:             " << ScaleQuantizer::scaleName(
                    static_cast<ScaleQuantizer::Scale>(params.scale)) << "\n";
    std::cout << "Transpose:         " << params.transpose << " semitones\n";
    std::cout << "Cents:             " << params.cents << "\n";
    std::cout << "Formant Preserve:  " << params.formantPreserve << "%\n";
    std::cout << "Formant Shift:     " << params.formantShift << " semitones\n";
    std::cout << "Vibrato Preserve:  " << params.vibratoPreserve << "%\n";
    std::cout << "Transition Smooth: " << params.transitionSmooth << "%\n";
    std::cout << "Bypass:            " << (params.bypass ? "ON" : "OFF") << "\n";
    std::cout << "Auto Key:          " << (params.autoKey ? "ON" : "OFF") << "\n";

    // Enable report mode if requested
    bool reportMode = args.count("--report") > 0;
    dsp.setReportMode(reportMode);

    dsp.setParams(params);
    dsp.setEnabled(!params.bypass);

    //==========================================================================
    // Process
    //==========================================================================
    std::cout << "\nProcessing (Gen2: PSOLA + LPC)...\n";
    dsp.process(left.data(), right.data(), static_cast<int>(totalFrames));

    // Print key detection result if autoKey
    if (params.autoKey && dsp.hasKeyDetected()) {
        auto kr = dsp.getKeyResult();
        std::cout << "\n--- Key Detection Result ---\n";
        if (kr.detected) {
            std::cout << "Detected Key: " << KeyDetector::keyName(kr.key, kr.isMajor) << "\n";
            std::cout << "Confidence:   " << std::fixed << std::setprecision(3)
                      << kr.confidence << "\n";
            std::cout << "Auto Scale:   " << ScaleQuantizer::scaleName(
                            kr.isMajor ? ScaleQuantizer::Major : ScaleQuantizer::Minor) << "\n";
            std::cout << "Key Offset:   " << kr.key << " semitones from C\n";
        } else {
            std::cout << "Key detection failed (insufficient voiced signal)\n";
        }
    }

    // Output report if requested
    if (reportMode) {
        const auto& report = dsp.getReport();

        if (args.count("--report-file")) {
            std::string reportPath = args["--report-file"];
            std::ofstream reportOut(reportPath);
            if (reportOut.is_open()) {
                writeReport(reportOut, report, dsp.getKeyResult(), params.autoKey);
                std::cout << "Report saved to: " << reportPath << "\n";
            } else {
                std::cerr << "Error: Cannot write report file: " << reportPath << "\n";
            }
        } else {
            writeReport(std::cout, report, dsp.getKeyResult(), params.autoKey);
        }
    }

    //==========================================================================
    // Write output WAV
    //==========================================================================
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

    std::cout << "\nDone! Output: " << outFile << "\n";
    return 0;
}
