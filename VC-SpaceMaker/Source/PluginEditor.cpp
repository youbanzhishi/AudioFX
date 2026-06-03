// ============================================================================
// PluginEditor.cpp - VC-SpaceMaker JUCE Plugin Editor
// 实时频谱显示 + 控制面板
// ============================================================================

#include <juce_audio_utils/juce_audio_utils.h>

// Forward declaration
class VCSpaceMakerProcessor;

class VCSpaceMakerEditor : public juce::AudioProcessorEditor,
                           public juce::Timer
{
public:
    VCSpaceMakerEditor (VCSpaceMakerProcessor& p)
        : AudioProcessorEditor (&p), processor (p)
    {
        setSize (500, 400);

        // Amount 旋钮
        addAndMakeVisible (amountKnob);
        amountKnob.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        amountKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 20);
        amountAttachment.reset (new juce::AudioProcessorValueTreeState::SliderAttachment (
            createVTS(), "amount", amountKnob));

        // Attack 旋钮
        addAndMakeVisible (attackKnob);
        attackKnob.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        attackKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 20);

        // Release 旋钮
        addAndMakeVisible (releaseKnob);
        releaseKnob.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        releaseKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 20);

        // Labels
        amountLabel.setText ("Amount", juce::dontSendNotification);
        amountLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (amountLabel);

        attackLabel.setText ("Attack", juce::dontSendNotification);
        attackLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (attackLabel);

        releaseLabel.setText ("Release", juce::dontSendNotification);
        releaseLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (releaseLabel);

        startTimerHz (30);  // 30fps for spectrum display
    }

    ~VCSpaceMakerEditor() override = default;

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff1a1a2e));

        // 标题
        g.setColour (juce::Colour (0xffe94560));
        g.setFont (18.0f);
        g.drawText ("VC-SpaceMaker", getLocalBounds().removeFromTop (30),
                    juce::Justification::centred, true);

        // 频谱显示区域
        auto specArea = getLocalBounds().reduced (20, 50).removeFromTop (200);
        g.setColour (juce::Colour (0xff16213e));
        g.fillRect (specArea);
        g.setColour (juce::Colour (0xff0f3460));
        g.drawRect (specArea, 1);

        // 绘制侧链频谱（蓝色）
        auto& spectrumData = processor.getSpectrumData();
        auto* bandFreqs = processor.getBandFrequencies();
        float specWidth = specArea.getWidth() / 32.0f;

        g.setColour (juce::Colour (0x800000ff));
        for (int b = 0; b < 32; b++)
        {
            float level = juce::jmap (spectrumData.sidechainSpectrum[b], -80.0f, 0.0f, 0.0f, 1.0f);
            level = juce::jlimit (0.0f, 1.0f, level);
            float barHeight = level * specArea.getHeight();
            g.fillRect (specArea.getX() + b * specWidth,
                        specArea.getBottom() - barHeight,
                        specWidth - 1, barHeight);
        }

        // 绘制衰减曲线（白色）
        g.setColour (juce::Colour (0xc0ffffff));
        juce::Path reductionPath;
        for (int b = 0; b < 32; b++)
        {
            float reduction = spectrumData.reductionCurve[b];
            float y = juce::jmap (reduction, 0.0f, 18.0f, 1.0f, 0.0f);
            y = juce::jlimit (0.0f, 1.0f, y);
            float x = specArea.getX() + (b + 0.5f) * specWidth;
            float py = specArea.getY() + y * specArea.getHeight();
            if (b == 0)
                reductionPath.startNewSubPath (x, py);
            else
                reductionPath.lineTo (x, py);
        }
        g.strokePath (reductionPath, juce::PathStrokeType (2.0f));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (20);
        area.removeFromTop (260);  // 频谱区域

        auto knobArea = area.removeFromTop (100);
        auto knobWidth = 80;

        amountKnob.setBounds (knobArea.removeFromLeft (knobWidth));
        knobArea.removeFromLeft (20);
        attackKnob.setBounds (knobArea.removeFromLeft (knobWidth));
        knobArea.removeFromLeft (20);
        releaseKnob.setBounds (knobArea.removeFromLeft (knobWidth));

        // 标签
        auto labelArea = getLocalBounds().reduced (20);
        labelArea.removeFromTop (350);

        amountLabel.setBounds (amountKnob.getX(), amountKnob.getBottom() + 2, knobWidth, 20);
        attackLabel.setBounds (attackKnob.getX(), attackKnob.getBottom() + 2, knobWidth, 20);
        releaseLabel.setBounds (releaseKnob.getX(), releaseKnob.getBottom() + 2, knobWidth, 20);
    }

    void timerCallback() override
    {
        repaint();  // Refresh spectrum display
    }

private:
    // Helper: create a temporary VTS from processor parameters
    // (In production, the processor should own the VTS)
    juce::AudioProcessorValueTreeState createVTS()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        return juce::AudioProcessorValueTreeState (processor, nullptr, "PARAMS", std::move (layout));
    }

    VCSpaceMakerProcessor& processor;

    juce::Slider amountKnob, attackKnob, releaseKnob;
    juce::Label amountLabel, attackLabel, releaseLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VCSpaceMakerEditor)
};

// Forward declare the processor editor creation
juce::AudioProcessorEditor* VCSpaceMakerProcessor::createEditor()
{
    return new VCSpaceMakerEditor (*this);
}
