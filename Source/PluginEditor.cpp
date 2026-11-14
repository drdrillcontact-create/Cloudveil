#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

namespace
{
constexpr float kTwoPi = 6.28318530717958647692f;
}

void CloudveilLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                             int x, int y, int width, int height,
                                             float sliderPos,
                                             float rotaryStartAngle,
                                             float rotaryEndAngle,
                                             juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> (static_cast<float> (x),
                                                static_cast<float> (y),
                                                static_cast<float> (width),
                                                static_cast<float> (height)).reduced (6.0f);

    const float radius  = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float centreX = bounds.getCentreX();
    const float centreY = bounds.getCentreY();
    const float arcR    = radius - 3.0f;
    const float angle   = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Background track arc
    juce::Path trackArc;
    trackArc.addCentredArc (centreX, centreY, arcR, arcR, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colour (0x3364748b));
    g.strokePath (trackArc, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Active glowing gradient arc
    if (sliderPos > 0.002f)
    {
        juce::Path activeArc;
        activeArc.addCentredArc (centreX, centreY, arcR, arcR, 0.0f, rotaryStartAngle, angle, true);

        juce::ColourGradient grad (juce::Colour (0xff67e8f9), centreX - arcR, centreY,
                                   juce::Colour (0xffc084fc), centreX + arcR, centreY, false);
        grad.addColour (0.55, juce::Colour (0xff818cf8));

        // Soft outer glow
        g.setColour (juce::Colour (0x2a67e8f9));
        g.strokePath (activeArc, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setGradientFill (grad);
        g.strokePath (activeArc, juce::PathStrokeType (3.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Inner glass cap
    const float capR = arcR - 6.0f;
    const auto capRect = juce::Rectangle<float> (centreX - capR, centreY - capR, capR * 2.0f, capR * 2.0f);

    juce::ColourGradient capGrad (juce::Colour (0xff1e2444), centreX, centreY - capR,
                                  juce::Colour (0xff0f1328), centreX, centreY + capR, false);
    g.setGradientFill (capGrad);
    g.fillEllipse (capRect);

    g.setColour (slider.isMouseOverOrDragging() ? juce::Colour (0x99a5b4fc) : juce::Colour (0x4494a3b8));
    g.drawEllipse (capRect, 1.2f);

    // Pointer indicator
    const float innerR = capR * 0.25f;
    const float outerR = capR * 0.84f;
    const float sinA   = std::sin (angle);
    const float cosA   = -std::cos (angle);

    g.setColour (juce::Colour (0xffe0f2fe));
    g.drawLine (centreX + sinA * innerR, centreY + cosA * innerR,
                centreX + sinA * outerR, centreY + cosA * outerR, 2.2f);

    g.setColour (juce::Colour (0xff67e8f9));
    g.fillEllipse (centreX + sinA * outerR - 2.2f, centreY + cosA * outerR - 2.2f, 4.4f, 4.4f);
}

void CloudveilLookAndFeel::drawToggleButton (juce::Graphics& g,
                                             juce::ToggleButton& button,
                                             bool shouldDrawButtonAsHighlighted,
                                             bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused (shouldDrawButtonAsDown);

    const auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const bool active = button.getToggleState();

    if (active)
    {
        g.setColour (juce::Colour (0x3367e8f9));
        g.fillRoundedRectangle (bounds.expanded (1.5f), 14.0f);

        juce::ColourGradient bg (juce::Colour (0xff0ea5e9), bounds.getX(), bounds.getY(),
                                 juce::Colour (0xff8b5cf6), bounds.getRight(), bounds.getBottom(), false);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (bounds, 13.0f);
    }
    else
    {
        g.setColour (shouldDrawButtonAsHighlighted ? juce::Colour (0xff23294d) : juce::Colour (0xff171c36));
        g.fillRoundedRectangle (bounds, 13.0f);
        g.setColour (juce::Colour (0x55818cf8));
        g.drawRoundedRectangle (bounds, 13.0f, 1.1f);
    }

    g.setColour (active ? juce::Colours::white : juce::Colour (0xffcbd5e1));
    g.setFont (juce::Font (juce::FontOptions { 12.0f, juce::Font::bold }));
    g.drawText (button.getButtonText(), bounds, juce::Justification::centred, false);
}

void CloudveilLookAndFeel::drawComboBox (juce::Graphics& g,
                                         int width, int height,
                                         bool isButtonDown,
                                         int buttonX, int buttonY, int buttonW, int buttonH,
                                         juce::ComboBox& box)
{
    juce::ignoreUnused (isButtonDown, buttonX, buttonY, buttonW, buttonH, box);

    const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (width), static_cast<float> (height)).reduced (1.0f);
    g.setColour (juce::Colour (0xff151a33));
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (juce::Colour (0x55818cf8));
    g.drawRoundedRectangle (bounds, 8.0f, 1.0f);

    // Subtle dropdown arrow
    const float arrowX = static_cast<float> (width) - 16.0f;
    const float arrowY = static_cast<float> (height) * 0.5f - 2.0f;
    juce::Path arrow;
    arrow.addTriangle (arrowX - 4.0f, arrowY, arrowX + 4.0f, arrowY, arrowX, arrowY + 5.0f);
    g.setColour (juce::Colour (0xffa5b4fc));
    g.fillPath (arrow);
}

juce::Font CloudveilLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    juce::ignoreUnused (box);
    return juce::Font (juce::FontOptions { 12.5f, juce::Font::plain });
}

//==============================================================================

CloudveilAudioProcessorEditor::CloudveilAudioProcessorEditor (CloudveilAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&lookAndFeel);
    lookAndFeel.setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff11162c));
    lookAndFeel.setColour (juce::PopupMenu::textColourId, juce::Colour (0xffe2e8f0));
    lookAndFeel.setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff312e81));

    // Presets
    const auto& presets = CloudveilAudioProcessor::getPresets();
    for (std::size_t i = 0; i < presets.size(); ++i)
        presetBox.addItem (presets[i].name, static_cast<int> (i) + 1);

    presetBox.setSelectedId (audioProcessor.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.setColour (juce::ComboBox::textColourId, juce::Colour (0xffe2e8f0));
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0)
            audioProcessor.setCurrentProgram (idx);
    };
    addAndMakeVisible (presetBox);

    // Freeze toggle
    freezeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.apvts, "freeze", freezeButton);
    addAndMakeVisible (freezeButton);

    struct ParamMeta
    {
        const char* id;
        const char* label;
        const char* suffix;
        int         decimals;
    };

    constexpr std::array<ParamMeta, 8> meta {{
        { "decay",    "DECAY",     " s",  2 },
        { "size",     "SIZE",      " %",  0 },
        { "predelay", "PRE-DELAY", " ms", 0 },
        { "mix",      "MIX",       " %",  0 },
        { "shimmer",  "SHIMMER",   " %",  0 },
        { "drift",    "DRIFT",     " %",  0 },
        { "warmth",   "WARMTH",    " %",  0 },
        { "width",    "WIDTH",     " %",  0 }
    }};

    for (std::size_t i = 0; i < 8; ++i)
    {
        auto& k = knobs[i];
        k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 18);
        k.slider.setTextValueSuffix (meta[i].suffix);
        k.slider.setNumDecimalPlacesToDisplay (meta[i].decimals);
        k.slider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffcbd5e1));
        k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        k.slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0x280f172a));
        addAndMakeVisible (k.slider);

        k.title.setText (meta[i].label, juce::dontSendNotification);
        k.title.setJustificationType (juce::Justification::centred);
        k.title.setColour (juce::Label::textColourId, juce::Colour (0xffa5b4fc));
        k.title.setFont (juce::Font (juce::FontOptions { 11.5f, juce::Font::bold }));
        addAndMakeVisible (k.title);

        k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            audioProcessor.apvts, meta[i].id, k.slider);
    }

    setSize (760, 440);
    startTimerHz (30);
}

CloudveilAudioProcessorEditor::~CloudveilAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void CloudveilAudioProcessorEditor::timerCallback()
{
    auroraPhase += 0.045f;
    if (auroraPhase >= kTwoPi)
        auroraPhase -= kTwoPi;

    const float targetWet  = std::clamp (audioProcessor.getWetMeterLevel() * 2.5f, 0.0f, 1.0f);
    const float targetShim = std::clamp (audioProcessor.getShimmerMeterLevel() * 4.0f, 0.0f, 1.0f);

    visualWet     += 0.25f * (targetWet - visualWet);
    visualShimmer += 0.25f * (targetShim - visualShimmer);

    repaint (visualizerBounds);
}

void CloudveilAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    // Midnight-indigo gradient backdrop
    juce::ColourGradient bg (juce::Colour (0xff070a16), 0.0f, 0.0f,
                             juce::Colour (0xff161230), bounds.getWidth(), bounds.getHeight(), false);
    bg.addColour (0.5, juce::Colour (0xff0d1226));
    g.setGradientFill (bg);
    g.fillRect (bounds);

    // Soft atmospheric nebula bloom
    juce::ColourGradient nebula (juce::Colour (0x287c3aed), bounds.getCentreX(), 110.0f,
                                 juce::Colours::transparentBlack, bounds.getCentreX(), 340.0f, true);
    g.setGradientFill (nebula);
    g.fillRect (bounds);

    // Deterministic procedural starlight specks
    g.setColour (juce::Colour (0x55e2e8f0));
    for (int i = 0; i < 38; ++i)
    {
        const float sx = static_cast<float> ((i * 197 + 43) % 748 + 6);
        const float sy = static_cast<float> ((i * 131 + 19) % 428 + 6);
        const float sz = (i % 4 == 0) ? 1.8f : 1.1f;
        g.fillEllipse (sx, sy, sz, sz);
    }

    // Header brand title
    g.setColour (juce::Colour (0xfff8fafc));
    g.setFont (juce::Font (juce::FontOptions { 22.0f, juce::Font::bold }));
    g.drawText ("CLOUDVEIL", 26, 14, 220, 26, juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xff94a3b8));
    g.setFont (juce::Font (juce::FontOptions { 10.5f, juce::Font::plain }));
    g.drawText ("DREAMY SHIMMER REVERB", 27, 40, 220, 14, juce::Justification::centredLeft);

    // Visualizer panel
    const auto visF = visualizerBounds.toFloat();
    g.setColour (juce::Colour (0xaa0b0f21));
    g.fillRoundedRectangle (visF, 12.0f);
    g.setColour (juce::Colour (0x44818cf8));
    g.drawRoundedRectangle (visF, 12.0f, 1.0f);

    g.saveState();
    g.reduceClipRegion (visualizerBounds.reduced (2));

    const bool frozen = freezeButton.getToggleState();
    const float midY = visF.getCentreY();
    const float ampBase = 6.0f + visualWet * (visF.getHeight() * 0.34f);

    // Subtle horizon baseline
    g.setColour (juce::Colour (0x2294a3b8));
    g.drawHorizontalLine (static_cast<int> (midY), visF.getX() + 8.0f, visF.getRight() - 8.0f);

    // Draw 3 layered aurora ribbons
    for (int layer = 0; layer < 3; ++layer)
    {
        juce::Path ribbon;
        const float freq = 1.6f + static_cast<float> (layer) * 0.7f;
        const float phaseShift = auroraPhase * (1.0f + 0.35f * static_cast<float> (layer)) + static_cast<float> (layer) * 1.3f;
        const float layerAmp = ampBase * (1.0f - 0.18f * static_cast<float> (layer))
                             + (layer == 2 ? visualShimmer * 14.0f : 0.0f);

        constexpr int steps = 72;
        for (int s = 0; s <= steps; ++s)
        {
            const float norm = static_cast<float> (s) / static_cast<float> (steps);
            const float px   = visF.getX() + norm * visF.getWidth();
            const float env  = std::sin (norm * 3.14159265f);
            const float py   = midY + std::sin (norm * kTwoPi * freq + phaseShift) * layerAmp * env;

            if (s == 0)
                ribbon.startNewSubPath (px, py);
            else
                ribbon.lineTo (px, py);
        }

        const juce::Colour c = frozen
            ? (layer == 0 ? juce::Colour (0xffa5f3fc) : juce::Colour (0xff67e8f9))
            : (layer == 0 ? juce::Colour (0xff67e8f9)
                          : (layer == 1 ? juce::Colour (0xffa78bfa) : juce::Colour (0xfff472b6)));

        g.setColour (c.withAlpha (0.22f));
        g.strokePath (ribbon, juce::PathStrokeType (5.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (c.withAlpha (0.85f));
        g.strokePath (ribbon, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    if (frozen)
    {
        g.setColour (juce::Colour (0xff67e8f9));
        g.setFont (juce::Font (juce::FontOptions { 10.5f, juce::Font::bold }));
        g.drawText ("FROZEN CLOUD", visualizerBounds.reduced (12, 6), juce::Justification::topRight);
    }

    g.restoreState();
}

void CloudveilAudioProcessorEditor::resized()
{
    presetBox.setBounds (getWidth() - 318, 18, 190, 28);
    freezeButton.setBounds (getWidth() - 116, 18, 92, 28);

    visualizerBounds = juce::Rectangle<int> (24, 68, getWidth() - 48, 92);

    const int topY = 176;
    const int rowH = 124;
    const int colW = (getWidth() - 48) / 4;

    for (std::size_t i = 0; i < 8; ++i)
    {
        const int row = static_cast<int> (i / 4);
        const int col = static_cast<int> (i % 4);
        const auto cell = juce::Rectangle<int> (24 + col * colW, topY + row * rowH, colW, rowH);

        knobs[i].title.setBounds (cell.getX(), cell.getY(), cell.getWidth(), 18);
        knobs[i].slider.setBounds (cell.getX() + 14, cell.getY() + 18, cell.getWidth() - 28, 98);
    }
}