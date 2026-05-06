#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// Custom Components
//==============================================================================

// Vertical VU Meter with GR
class VUMeter : public juce::Component
{
public:
    VUMeter() = default;
    
    void setLevel(float newLevel)
    {
        level = juce::jlimit(0.0f, 1.0f, newLevel);
        repaint();
    }
    
    void setGR(float newGR)
    {
        gr = juce::jlimit(-24.0f, 0.0f, newGR);
        repaint();
    }
    
    void setColors(juce::Colour inputCol, juce::Colour outputCol)
    {
        inputColour = inputCol;
        outputColour = outputCol;
    }
    
private:
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        
        // Background
        g.setColour(juce::Colours::black.withAlpha(0.8f));
        g.fillRoundedRectangle(bounds, 4.0f);
        
        // Border
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
        
        // Meter bar (gradient from green to yellow to red)
        float meterHeight = bounds.getHeight() * 0.85f;
        float levelHeight = meterHeight * level;
        float bottom = bounds.getBottom() - 2.0f;
        
        juce::ColourGradient grad(
            juce::Colours::greenyellow, 0.0f, bottom,
            juce::Colours::red, 0.0f, bounds.getY(), false
        );
        
        g.setGradientFill(grad);
        g.fillRect(bounds.getX() + 2, bottom - levelHeight,
                   bounds.getWidth() - 4, levelHeight);
        
        // GR indicator line
        if (gr < 0.0f)
        {
            float grY = bottom - (meterHeight * (1.0f + gr / 24.0f));
            g.setColour(juce::Colours::yellow);
            g.drawHorizontalLine((int)grY, bounds.getX() + 2, bounds.getRight() - 2);
        }
    }
    
    float level = 0.0f;
    float gr = 0.0f;
    juce::Colour inputColour = juce::Colours::greenyellow;
    juce::Colour outputColour = juce::Colours::cyan;
};

// Rotary Knob
class RotaryKnob : public juce::Component
{
public:
    RotaryKnob()
    {
        setSize(60, 70);
    }
    
    void setValue(float* val, float min, float max, const juce::String& label, const juce::String& unit = "")
    {
        value = val;
        minVal = min;
        maxVal = max;
        labelText = label;
        unitText = unit;
        repaint();
    }
    
    void setValue(float* val)
    {
        value = val;
        repaint();
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        
        // Label
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.setFont(10.0f);
        g.drawText(labelText, bounds, juce::Justification::centredTop);
        
        // Knob background
        float knobSize = juce::jmin(bounds.getWidth(), bounds.getHeight() - 20.0f) - 10.0f;
        auto knobBounds = juce::Rectangle<float>(knobSize, knobSize)
            .withCentre(bounds.getCentre().translated(0, 5));
        
        g.setColour(juce::Colours::darkgrey);
        g.fillEllipse(knobBounds);
        
        // Knob indicator
        if (value != nullptr)
        {
            float normalized = (*value - minVal) / (maxVal - minVal);
            float angle = juce::MathConstants<float>::pi * 0.75f + 
                         normalized * juce::MathConstants<float>::pi * 1.5f;
            
            float cx = knobBounds.getCentreX();
            float cy = knobBounds.getCentreY();
            float r = knobSize * 0.35f;
            
            float px = cx + std::cos(angle) * r;
            float py = cy + std::sin(angle) * r;
            
            g.setColour(juce::Colours::white);
            g.drawLine(cx, cy, px, py, 3.0f);
            
            g.fillEllipse(juce::Rectangle<float>(6, 6).withCentre({px, py}));
        }
        
        // Value text
        if (value != nullptr)
        {
            g.setColour(juce::Colours::white);
            g.setFont(11.0f);
            juce::String txt = juce::String(*value, 1) + unitText;
            g.drawText(txt, bounds.removeFromBottom(16), juce::Justification::centredBottom);
        }
    }
    
    bool hitTest(int x, int y) override
    {
        auto bounds = getLocalBounds().toFloat();
        float knobSize = juce::jmin(bounds.getWidth(), bounds.getHeight() - 20.0f) - 10.0f;
        auto knobBounds = juce::Rectangle<float>(knobSize, knobSize)
            .withCentre(bounds.getCentre().translated(0, 5));
        return knobBounds.contains((float)x, (float)y);
    }
    
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (value == nullptr) return;
        
        float delta = -e.getDistanceFromDragStartY() * 0.005f * (maxVal - minVal);
        *value = juce::jlimit(minVal, maxVal, *value + delta);
        repaint();
    }
    
private:
    float* value = nullptr;
    float minVal = 0.0f, maxVal = 1.0f;
    juce::String labelText, unitText;
};

// Toggle Button
class ToggleButton : public juce::Component
{
public:
    ToggleButton() { setSize(70, 24); }
    
    void setToggle(float* val, const juce::StringArray& choices)
    {
        value = val;
        optionChoices = choices;
        repaint();
    }
    
    void setToggle(int* val, const juce::StringArray& choices)
    {
        intValue = val;
        optionChoices = choices;
        repaint();
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        
        g.setColour(juce::Colours::darkgrey.withAlpha(0.8f));
        g.fillRoundedRectangle(bounds, 4.0f);
        
        int idx = 0;
        if (value != nullptr)
            idx = (int)*value;
        else if (intValue != nullptr)
            idx = *intValue;
        
        idx = juce::jlimit(0, optionChoices.size() - 1, idx);
        
        float btnWidth = bounds.getWidth() / optionChoices.size();
        auto activeBounds = bounds.removeFromLeft(btnWidth).reduced(2, 2);
        
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.fillRoundedRectangle(activeBounds, 3.0f);
        
        g.setColour(juce::Colours::white);
        g.setFont(10.0f);
        g.drawText(optionChoices[idx], getLocalBounds(), juce::Justification::centred);
    }
    
    void mouseDown(const juce::MouseEvent& e) override
    {
        int idx = (int)(e.position.x / (getWidth() / (float)optionChoices.size()));
        
        if (value != nullptr)
            *value = (float)idx;
        else if (intValue != nullptr)
            *intValue = idx;
        
        repaint();
    }
    
private:
    float* value = nullptr;
    int* intValue = nullptr;
    juce::StringArray optionChoices;
};

// Limiter LED Indicator
class LimiterLED : public juce::Component
{
public:
    LimiterLED() { setSize(16, 16); }
    
    void setActive(bool yellow, bool red)
    {
        isYellow = yellow;
        isRed = red;
        repaint();
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        
        if (isRed)
        {
            g.setColour(juce::Colours::red);
        }
        else if (isYellow)
        {
            g.setColour(juce::Colours::yellow);
        }
        else
        {
            g.setColour(juce::Colours::darkred.withAlpha(0.5f));
        }
        
        g.fillEllipse(bounds);
        
        if (isYellow || isRed)
        {
            g.setColour(juce::Colours::white.withAlpha(0.5f));
            g.fillEllipse(bounds.reduced(3, 3).withPosition(bounds.getX() + 1, bounds.getY() + 1));
        }
    }
    
private:
    bool isYellow = false, isRed = false;
};

// SC Listen Toggle Button
class SCListenButton : public juce::ToggleButton
{
public:
    SCListenButton(juce::AudioProcessorValueTreeState& state, const juce::String& paramID)
        : attachment(state, paramID, *this)
    {}
    
    juce::AudioProcessorValueTreeState::ButtonAttachment attachment;
};

//==============================================================================
// Main Editor - inherits from Timer
//==============================================================================
class VCCompAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   private juce::Timer
{
public:
    explicit VCCompAudioProcessorEditor(VCCompAudioProcessor&);
    ~VCCompAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    VCCompAudioProcessor& processor;
    
    // Attachments
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> scListenAttachment;
    
    // UI Components
    VUMeter inputMeter, grMeter, outputMeter;
    juce::Slider thresholdSlider, ratioSlider, attackSlider, releaseSlider;
    juce::Slider gainSlider, mixSlider, trimSlider;
    juce::ComboBox releaseModeBox, compBehaviorBox, kneeModeBox, characterBox;
    juce::ComboBox scSourceBox, scHPFBox;
    juce::ToggleButton bypassButton;
    juce::ToggleButton scListenButton;
    LimiterLED limiterYellow, limiterRed;
    
    // Labels
    juce::Label titleLabel;
    juce::ComboBox paramSetBox;
    
    // Timer for meter updates
    static constexpr int updateTimerHz = 30;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VCCompAudioProcessorEditor)
};
