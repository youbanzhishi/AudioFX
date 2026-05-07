#pragma once

#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// VC-NoiseProfile Plugin Editor
//==============================================================================
class VCNoiseProfileEditor : public juce::AudioProcessorEditor
{
public:
    VCNoiseProfileEditor(VCNoiseProfileProcessor&);
    ~VCNoiseProfileEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    VCNoiseProfileProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VCNoiseProfileEditor)
};
