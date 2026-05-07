#pragma once

#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// Plugin Editor
//==============================================================================
class VC_DelayEditor : public juce::AudioProcessorEditor
{
public:
    VC_DelayEditor(VC_DelayProcessor&);
    ~VC_DelayEditor() override;

    //============================================================================
    // Painting and Layout
    //============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    //============================================================================
    // Processor reference
    //============================================================================
    VC_DelayProcessor& processor;

    //============================================================================
    // Non-copyable
    //============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VC_DelayEditor)
};
