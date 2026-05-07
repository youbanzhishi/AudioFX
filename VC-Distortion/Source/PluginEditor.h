#pragma once

#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// Minimal Plugin Editor
// TODO: Expand with your plugin's UI controls
//==============================================================================
class __PLUGIN_NAME__Editor : public juce::AudioProcessorEditor
{
public:
    __PLUGIN_NAME__Editor(__PLUGIN_NAME__Processor&);
    ~__PLUGIN_NAME__Editor() override;

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
    __PLUGIN_NAME__Processor& processor;

    //============================================================================
    // Non-copyable
    //============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(__PLUGIN_NAME__Editor)
};
