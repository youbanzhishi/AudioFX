#pragma once

#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// Minimal Plugin Editor
// TODO: Expand with your plugin's UI controls
//==============================================================================
class VC-DrumEditor : public juce::AudioProcessorEditor
{
public:
    VC-DrumEditor(VC-DrumProcessor&);
    ~VC-DrumEditor() override;

    //============================================================================
    // Painting and Layout
    //============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    //============================================================================
    // TODO: Define control members
    // Example:
    // juce::Label gainLabel;
    // juce::Slider gainSlider;
    // std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;
    //============================================================================

    //============================================================================
    // Processor reference
    //============================================================================
    VC-DrumProcessor& processor;

    //============================================================================
    // Non-copyable
    //============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VC-DrumEditor)
};
