#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// VC-Harmonizer Plugin Editor
//==============================================================================
class VCHarmonizerEditor : public juce::AudioProcessorEditor
{
public:
    VCHarmonizerEditor(VCHarmonizerProcessor&);
    ~VCHarmonizerEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    VCHarmonizerProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VCHarmonizerEditor)
};
