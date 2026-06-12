// VC-BreathControl JUCE CLI
// Command-line tool using JUCE for WAV I/O

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include <iostream>
#include <string>

#include "../DSP/VCPluginDSP.h"

int main(int argc, char* argv[])
{
    if (argc < 3) {
        std::cout << "VC-BreathControl JUCE CLI\n";
        std::cout << "Usage: " << argv[0] << " <input.wav> <output.wav>\n";
        return 1;
    }

    std::string inFile = argv[1];
    std::string outFile = argv[2];

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(juce::File(inFile)));
    if (!reader) {
        std::cerr << "Error: Cannot read file: " << inFile << "\n";
        return 1;
    }

    int numSamples = static_cast<int>(reader->lengthInSamples);
    int channels = static_cast<int>(reader->numChannels);
    double sampleRate = reader->sampleRate;

    std::cout << "Sample rate: " << sampleRate << " Hz\n";
    std::cout << "Channels: " << channels << "\n";
    std::cout << "Total frames: " << numSamples << "\n";

    juce::AudioBuffer<float> buffer(channels, numSamples);
    reader->read(&buffer, 0, numSamples, 0, true, true);

    float* left = buffer.getWritePointer(0);
    float* right = channels > 1 ? buffer.getWritePointer(1) : left;

    VCPluginDSP dsp;
    dsp.prepare(sampleRate, 4096);
    dsp.process(left, right, numSamples);

    std::unique_ptr<juce::AudioFormatWriter> writer(
        formatManager.findFormatForFileExtension("wav")->createWriterFor(
            new juce::FileOutputStream(juce::File(outFile)),
            sampleRate, static_cast<unsigned int>(channels), 16, {}, 0));

    if (!writer) {
        std::cerr << "Error: Cannot write file: " << outFile << "\n";
        return 1;
    }

    writer->writeFromAudioSampleBuffer(buffer, 0, numSamples);
    std::cout << "Done! Output: " << outFile << "\n";
    return 0;
}
