#pragma once

#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// VC-Arp Editor — Placeholder (GUI not yet implemented)
//==============================================================================
class VCArpEditor : public juce::AudioProcessorEditor
{
public:
    VCArpEditor(VCArpProcessor&);
    ~VCArpEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    VCArpProcessor& mProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VCArpEditor)
};
