#pragma once

#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// Minimal Plugin Editor for VC-Saturator
//==============================================================================
class VCSaturatorEditor : public juce::AudioProcessorEditor
{
public:
    VCSaturatorEditor(VCSaturatorProcessor&);
    ~VCSaturatorEditor() override;

    //============================================================================
    // Painting and Layout
    //============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    //============================================================================
    // Processor reference
    //============================================================================
    VCSaturatorProcessor& processor;

    //============================================================================
    // Non-copyable
    //============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VCSaturatorEditor)
};
