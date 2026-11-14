#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include <array>
#include <cstdint>
#include <functional>
#include <memory>

class CloudveilLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    CloudveilLookAndFeel();

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

    void drawCornerResizer (juce::Graphics& g, int width, int height,
                            bool isMouseOver, bool isMouseDragging) override;

    juce::Font getPopupMenuFont() override;
};

//==============================================================================
/** One of the two arrows next to the preset name. */
class PresetStepButton final : public juce::Button
{
public:
    PresetStepButton (const juce::String& name, bool pointsLeft);

private:
    void paintButton (juce::Graphics&, bool isHighlighted, bool isDown) override;

    const bool left;
};

/** Shows the name of the current preset; a click opens the list. */
class PresetNameButton final : public juce::Button
{
public:
    PresetNameButton();

    /** `edited` marks a preset whose controls have been moved since it was loaded. */
    void setPreset (const juce::String& presetName, bool edited);

    const juce::String& getPresetName() const noexcept { return shownName; }
    bool isMarkedAsEdited() const noexcept             { return shownEdited; }

private:
    void paintButton (juce::Graphics&, bool isHighlighted, bool isDown) override;

    juce::String shownName;
    bool shownEdited = false;
};

//==============================================================================
/** The aurora: three ribbons that follow what the reverb is doing.

    Left to right is low to high. Where the reverb has energy the ribbons swell, so their shape
    follows what is being played; how far they swell follows how loud the reverb is. The front
    ribbon reacts at once, the one behind it trails after, and the third one follows the octaves
    that the shimmer feeds back into the reverb. With nothing to show they settle into the calm
    lines of an idle plug-in. */
class AuroraVisualiser final : public juce::Component
{
public:
    AuroraVisualiser();

    /** Moves the picture on by one frame. Call it at a steady 30 Hz or so; it repaints itself. */
    void advance (const CloudveilAudioProcessor& processor, bool audioIsRunning, bool isFrozen);

    /** Draws what lies behind this component (in this component's coordinates). The aurora is
        redrawn 30 times a second, and painting the window's backdrop afresh each time costs
        several times what the ribbons cost. So the backdrop is drawn once, kept as an image,
        and only drawn again when the window is resized. */
    std::function<void (juce::Graphics&)> paintBehind;

    void paint (juce::Graphics&) override;

    /** How loud the picture takes the reverb to be at the moment, 0 .. 1. */
    float getActivity() const noexcept { return energy; }

    /** Where along the panel the front ribbon is swollen most: 0 = far left (bass), 1 = far
        right (treble). */
    float getCentreOfActivity() const noexcept;

private:
    static constexpr std::size_t fftSize   = 2048;
    static constexpr std::size_t numBands  = 48;
    static constexpr std::size_t numLayers = 3;
    static constexpr int spread = 6;            // how many bands either side a band's energy is spread to

    using Bands = std::array<float, numBands>;

    /** The level, in dB, of each band of the newest stretch of `scope`. */
    void measure (const ScopeBuffer& scope, double sampleRate, Bands& levelsDb);

    static float valueAt (const Bands& bands, float position) noexcept;

    cloudveil::FFT fft;
    std::array<float, fftSize> window {}, re {}, im {};
    std::array<float, 2 * spread + 1> spreadWeights {};
    Bands energyPerBand {};

    std::array<Bands, numLayers> shape {};      // 0 .. 1: how far each ribbon swells, per band
    std::array<float, numLayers> phase { 0.0f, 1.3f, 2.6f };
    float referenceDb = -70.0f;                  // follows the loudest band; the shape is relative to it
    float energy = 0.0f;                         // 0 .. 1: how loud the reverb is
    float sparkle = 0.0f;                        // 0 .. 1: how much shimmer is going round
    float twinkle = 0.0f;                        // runs on with time, for the glints
    bool  frozen = false;

    juce::Image backdrop;                        // the panel and what is behind it, at screen resolution
};

//==============================================================================
class CloudveilAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                            private juce::Timer
{
public:
    explicit CloudveilAudioProcessorEditor (CloudveilAudioProcessor&);
    ~CloudveilAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Loads a factory preset, exactly as the arrows and the preset list do. */
    void selectPreset (int index);

    /** Loads the preset `delta` places further on in the list, wrapping round at the ends. */
    void stepPreset (int delta);

    /** The preset list: one sub-menu per category. An item's ID is its preset's number plus one. */
    juce::PopupMenu createPresetMenu() const;

private:
    void timerCallback() override;
    void showPresetMenu();
    void refreshPresetName();

    /** Everything that is drawn, laid out for a window of 760 x 440. The editor scales it as a
        whole to the size the window actually has, so nothing in here needs to know about that. */
    struct Content final : public juce::Component
    {
        void paint (juce::Graphics&) override;
    };

    struct KnobControl
    {
        juce::Slider slider;
        juce::Label  title;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    // Declared before the components that use it, so that it outlives them
    CloudveilLookAndFeel lookAndFeel;
    CloudveilAudioProcessor& audioProcessor;

    Content content;
    AuroraVisualiser visualiser;

    PresetStepButton previousButton { "Previous preset", true };
    PresetStepButton nextButton     { "Next preset", false };
    PresetNameButton presetButton;

    juce::ToggleButton freezeButton { "FREEZE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttachment;

    std::array<KnobControl, 8> knobs;

    // About 0.4 s at the 30 Hz timer: longer than the largest audio block a host will use
    static constexpr int maxIdleTicks = 12;
    std::uint32_t lastAudioCallbackCount = 0;
    int ticksSinceAudio = maxIdleTicks;

    bool rememberSize = false;      // off while the constructor is still setting the window up

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CloudveilAudioProcessorEditor)
};
