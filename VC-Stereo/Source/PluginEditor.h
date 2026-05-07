#pragma once

#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// Plugin Editor
//==============================================================================
class VC_StereoEditor : public juce::AudioProcessorEditor
{
public:
    VC_StereoEditor(VC_StereoProcessor&);
    ~VC_StereoEditor() override;

    //==========================================================================
    // Painting and Layout
    //==========================================================================
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    //==========================================================================
    // Processor reference
    //==========================================================================
    VC_StereoProcessor& processor;

    //==========================================================================
    // Non-copyable
    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VC_StereoEditor)
};
