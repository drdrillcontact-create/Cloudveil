#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include <array>
#include <cstdint>
#include <memory>

class CloudveilLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    CloudveilLookAndFeel() = default;

    void drawRotarySlider (juce::Graphics& g,
                           int x, int y, int width, int height,
                           float sliderPos,
                           float rotaryStartAngle,
                           float rotaryEndAngle,
                           juce::Slider& slider) override;

    void drawToggleButton (juce::Graphics& g,
                           juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;

    void drawComboBox (juce::Graphics& g,
                       int width, int height,
                       bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox& box) override;

    juce::Font getComboBoxFont (juce::ComboBox& box) override;
};

class CloudveilAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                            private juce::Timer
{
public:
    explicit CloudveilAudioProcessorEditor (CloudveilAudioProcessor&);
    ~CloudveilAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct KnobControl
    {
        juce::Slider slider;
        juce::Label  title;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    // Declared before child components so LookAndFeel outlives them safely
    CloudveilLookAndFeel lookAndFeel;
    CloudveilAudioProcessor& audioProcessor;

    juce::ComboBox     presetBox;
    juce::ToggleButton freezeButton { "FREEZE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttachment;

    std::array<KnobControl, 8> knobs;
    juce::Rectangle<int> visualizerBounds;

    float auroraPhase   = 0.0f;
    float visualWet     = 0.0f;
    float visualShimmer = 0.0f;

    // About 0.4 s at the 30 Hz timer: longer than the largest audio block a host will use
    static constexpr int maxIdleTicks = 12;
    std::uint32_t lastAudioCallbackCount = 0;
    int ticksSinceAudio = maxIdleTicks;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CloudveilAudioProcessorEditor)
};
