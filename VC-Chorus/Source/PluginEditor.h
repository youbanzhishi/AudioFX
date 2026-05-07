#pragma once

#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// Plugin Editor
//==============================================================================
class VC_ChorusEditor : public juce::AudioProcessorEditor
{
public:
    VC_ChorusEditor(VC_ChorusProcessor&);
    ~VC_ChorusEditor() override;

    //==========================================================================
    // Painting and Layout
    //==========================================================================
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    //==========================================================================
    // Processor reference
    //==========================================================================
    VC_ChorusProcessor& processor;

    //==========================================================================
    // Non-copyable
    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VC_ChorusEditor)
};
