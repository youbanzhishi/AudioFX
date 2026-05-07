#pragma once

#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// Minimal Plugin Editor for VC-DeEsser
//==============================================================================
class VCDeEsserEditor : public juce::AudioProcessorEditor
{
public:
    VCDeEsserEditor(VCDeEsserProcessor&);
    ~VCDeEsserEditor() override;

    //============================================================================
    // Painting and Layout
    //============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    //============================================================================
    // Processor reference
    //============================================================================
    VCDeEsserProcessor& processor;

    //============================================================================
    // Non-copyable
    //============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VCDeEsserEditor)
};
