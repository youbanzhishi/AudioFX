#pragma once

#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// Plugin Editor
//==============================================================================
class VC_GateEditor : public juce::AudioProcessorEditor
{
public:
    VC_GateEditor(VC_GateProcessor&);
    ~VC_GateEditor() override;

    //==========================================================================
    // Painting and Layout
    //==========================================================================
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    //==========================================================================
    // Processor reference
    //==========================================================================
    VC_GateProcessor& processor;

    //==========================================================================
    // Non-copyable
    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VC_GateEditor)
};
