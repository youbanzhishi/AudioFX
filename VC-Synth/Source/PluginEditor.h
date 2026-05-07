#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

//==============================================================================
// VC-Synth Editor — Placeholder (GUI not yet implemented)
//==============================================================================
class VCSynthEditor : public juce::AudioProcessorEditor
{
public:
    VCSynthEditor(VCSynthProcessor&);
    ~VCSynthEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    VCSynthProcessor& mProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VCSynthEditor)
};
