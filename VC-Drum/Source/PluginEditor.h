#pragma once

#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// VC-Drum Editor — Placeholder (GUI not yet implemented)
//==============================================================================
class VCDrumEditor : public juce::AudioProcessorEditor
{
public:
    VCDrumEditor(VCDrumProcessor&);
    ~VCDrumEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    VCDrumProcessor& mProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VCDrumEditor)
};
