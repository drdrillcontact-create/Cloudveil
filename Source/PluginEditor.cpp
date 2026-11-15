#include "PluginEditor.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
constexpr float kPi    = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

constexpr int baseWidth  = CloudveilAudioProcessor::editorBaseWidth;
constexpr int baseHeight = CloudveilAudioProcessor::editorBaseHeight;

// The palette of the night-sky look
const juce::Colour skyTop      { 0xff070a16 };
const juce::Colour boxFill     { 0xff151a33 };
const juce::Colour boxFillOver { 0xff1d2344 };
const juce::Colour boxOutline  { 0x55818cf8 };
const juce::Colour softText    { 0xffe2e8f0 };
const juce::Colour accent      { 0xffa5b4fc };
const juce::Colour cyan        { 0xff67e8f9 };
const juce::Colour violet      { 0xffa78bfa };
const juce::Colour pink        { 0xfff472b6 };
const juce::Colour ice         { 0xffa5f3fc };

void drawBox (juce::Graphics& g, juce::Rectangle<float> bounds, bool highlighted, bool down)
{
    g.setColour (down ? boxFill.darker (0.2f) : (highlighted ? boxFillOver : boxFill));
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (highlighted ? boxOutline.withAlpha (0.65f) : boxOutline);
    g.drawRoundedRectangle (bounds, 8.0f, 1.0f);
}
}

CloudveilLookAndFeel::CloudveilLookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff11162c));
    setColour (juce::PopupMenu::textColourId, softText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff312e81));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::PopupMenu::headerTextColourId, accent);
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

void CloudveilLookAndFeel::drawCornerResizer (juce::Graphics& g, int width, int height,
                                              bool isMouseOver, bool isMouseDragging)
{
    // Three short diagonal strokes in the corner, in the colour of the outlines
    const float w = static_cast<float> (width);
    const float h = static_cast<float> (height);

    g.setColour (accent.withAlpha (isMouseOver || isMouseDragging ? 0.9f : 0.45f));

    for (float inset : { 0.30f, 0.55f, 0.80f })
        g.drawLine (w * inset, h - 2.0f, w - 2.0f, h * inset, 1.2f);
}

juce::Font CloudveilLookAndFeel::getPopupMenuFont()
{
    return juce::Font (juce::FontOptions { 13.0f, juce::Font::plain });
}

//==============================================================================

PresetStepButton::PresetStepButton (const juce::String& name, bool pointsLeft)
    : juce::Button (name), left (pointsLeft)
{
    setTooltip (name);
}

void PresetStepButton::paintButton (juce::Graphics& g, bool isHighlighted, bool isDown)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    drawBox (g, bounds, isHighlighted, isDown);

    const float cx = bounds.getCentreX() + (left ? -0.5f : 0.5f);
    const float cy = bounds.getCentreY();
    const float dx = left ? -3.0f : 3.0f;

    juce::Path chevron;
    chevron.startNewSubPath (cx - dx, cy - 5.0f);
    chevron.lineTo (cx + dx, cy);
    chevron.lineTo (cx - dx, cy + 5.0f);

    g.setColour (isHighlighted ? juce::Colours::white : accent);
    g.strokePath (chevron, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

PresetNameButton::PresetNameButton()
    : juce::Button ("Presets")
{
}

void PresetNameButton::setPreset (const juce::String& presetName, bool edited)
{
    if (presetName != shownName || edited != shownEdited)
    {
        shownName = presetName;
        shownEdited = edited;
        repaint();
    }
}

void PresetNameButton::paintButton (juce::Graphics& g, bool isHighlighted, bool isDown)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    drawBox (g, bounds, isHighlighted, isDown);

    // Dropdown arrow
    const float arrowX = bounds.getRight() - 15.0f;
    const float arrowY = bounds.getCentreY() - 2.0f;
    juce::Path arrow;
    arrow.addTriangle (arrowX - 4.0f, arrowY, arrowX + 4.0f, arrowY, arrowX, arrowY + 5.0f);
    g.setColour (accent);
    g.fillPath (arrow);

    // A dot for a preset that has been edited since it was loaded
    if (shownEdited)
    {
        g.setColour (cyan);
        g.fillEllipse (bounds.getX() + 9.0f, bounds.getCentreY() - 2.0f, 4.0f, 4.0f);
    }

    g.setColour (softText);
    g.setFont (juce::Font (juce::FontOptions { 12.5f, juce::Font::plain }));
    g.drawText (shownName, bounds.withTrimmedLeft (18.0f).withTrimmedRight (26.0f), juce::Justification::centred, true);
}

//==============================================================================

AuroraVisualiser::AuroraVisualiser()
{
    fft.prepare (fftSize);

    for (std::size_t i = 0; i < fftSize; ++i)
        window[i] = 0.5f - 0.5f * std::cos (kTwoPi * static_cast<float> (i) / static_cast<float> (fftSize));

    for (int offset = -spread; offset <= spread; ++offset)
        spreadWeights[static_cast<std::size_t> (offset + spread)] = std::exp (-0.5f * static_cast<float> (offset * offset) / (1.7f * 1.7f));

    setInterceptsMouseClicks (false, false);
    setOpaque (true);   // paint() covers every pixel itself (see paintBehind)
}

void AuroraVisualiser::measure (const ScopeBuffer& scope, double sampleRate, Bands& levelsDb)
{
    scope.readNewest (re.data(), fftSize);

    for (std::size_t i = 0; i < fftSize; ++i)
    {
        re[i] *= window[i];
        im[i] = 0.0f;
    }

    fft.forward (re.data(), im.data());

    // Bands of equal musical width from 50 Hz to 16 kHz. Each one sums the energy that falls
    // into it, so a reverb tail (which has about the same energy in every octave) comes out
    // level, and a single note stands out of it.
    constexpr float lowestHz = 50.0f, highestHz = 16000.0f;
    const float binHz  = static_cast<float> (sampleRate) / static_cast<float> (fftSize);
    const float ratio  = std::pow (highestHz / lowestHz, 1.0f / static_cast<float> (numBands));
    const float toFullScale = 16.0f / (1.5f * static_cast<float> (fftSize) * static_cast<float> (fftSize));

    float lowEdge = lowestHz / binHz;   // in bins

    for (std::size_t band = 0; band < numBands; ++band)
    {
        const float highEdge = lowEdge * ratio;
        float sum = 0.0f;

        // bin k covers k - 0.5 .. k + 0.5
        const auto firstBin = static_cast<std::size_t> (lowEdge + 0.5f);
        const auto lastBin  = std::min (fftSize / 2 - 1, static_cast<std::size_t> (highEdge + 0.5f));

        for (std::size_t k = firstBin; k <= lastBin; ++k)
        {
            const float from = std::max (lowEdge,  static_cast<float> (k) - 0.5f);
            const float to   = std::min (highEdge, static_cast<float> (k) + 0.5f);
            sum += std::max (0.0f, to - from) * (re[k] * re[k] + im[k] * im[k]);
        }

        energyPerBand[band] = sum * toFullScale;
        lowEdge = highEdge;
    }

    // A single note is one narrow spike in this. Spread every band's energy over its neighbours
    // (a bell about an octave wide), so that what the ribbons follow is the broad outline of
    // the sound - a rolling landscape rather than a row of needles.
    for (std::size_t band = 0; band < numBands; ++band)
    {
        float sum = 0.0f;

        for (int offset = -spread; offset <= spread; ++offset)
        {
            const auto neighbour = static_cast<int> (band) + offset;
            if (neighbour >= 0 && neighbour < static_cast<int> (numBands))
                sum += spreadWeights[static_cast<std::size_t> (offset + spread)] * energyPerBand[static_cast<std::size_t> (neighbour)];
        }

        levelsDb[band] = 10.0f * std::log10 (sum + 1.0e-12f);
    }
}

void AuroraVisualiser::advance (const CloudveilAudioProcessor& processor, bool audioIsRunning, bool isFrozen)
{
    frozen = isFrozen;

    constexpr float floorDb = -72.0f;       // below this the picture is at rest
    constexpr float fullDb  = -24.0f;       // from here on the ribbons are fully swollen
    constexpr float rangeDb = 34.0f;        // a band this far below the loudest one stays flat

    Bands wetDb, shimmerDb;
    wetDb.fill (floorDb - rangeDb);
    shimmerDb.fill (floorDb - rangeDb);

    if (audioIsRunning)
    {
        const double rate = processor.getScopeSampleRate();
        measure (processor.getWetScope(), rate, wetDb);
        measure (processor.getShimmerScope(), rate, shimmerDb);
    }

    // The shape is drawn relative to the loudest band, so a quiet tail still has a shape. The
    // reference follows a rising level at once and a falling one at 15 dB per second.
    const float loudest = *std::max_element (wetDb.begin(), wetDb.end());
    referenceDb = std::max ({ floorDb, loudest, referenceDb - 0.5f });

    const float targetEnergy = juce::jlimit (0.0f, 1.0f, (std::max (loudest, floorDb) - floorDb) / (fullDb - floorDb));
    energy += (targetEnergy > energy ? 0.5f : 0.12f) * (targetEnergy - energy);

    float shimmerSum = 0.0f;

    for (std::size_t band = 0; band < numBands; ++band)
    {
        const auto relative = [this, rangeDb] (float db)
        {
            const float x = juce::jlimit (0.0f, 1.0f, (db - (referenceDb - rangeDb)) / rangeDb);
            return x * x;       // squaring makes the peaks stand out
        };

        const float wet     = relative (wetDb[band]);
        const float shimmer = relative (shimmerDb[band] + 9.0f);    // the octaves are fed back quietly

        // front ribbon: quick; the one behind: slow, so it trails; third: the shimmer
        shape[0][band] += (wet > shape[0][band] ? 0.60f : 0.22f) * (wet - shape[0][band]);
        shape[1][band] += (wet > shape[1][band] ? 0.16f : 0.05f) * (wet - shape[1][band]);
        shape[2][band] += (shimmer > shape[2][band] ? 0.45f : 0.12f) * (shimmer - shape[2][band]);

        shimmerSum += shape[2][band];
    }

    sparkle += 0.2f * (juce::jlimit (0.0f, 1.0f, shimmerSum * 4.0f / static_cast<float> (numBands)) - sparkle);

    // The ribbons drift along slowly at rest, faster when there is sound, and nearly stop
    // when the reverb is frozen
    const float speed = (frozen ? 0.35f : 1.0f) * (0.045f + 0.06f * energy);

    for (std::size_t layer = 0; layer < numLayers; ++layer)
    {
        phase[layer] += speed * (1.0f + 0.35f * static_cast<float> (layer));
        if (phase[layer] >= kTwoPi * 64.0f)
            phase[layer] -= kTwoPi * 64.0f;
    }

    twinkle += 0.11f;
    if (twinkle >= kTwoPi * 64.0f)
        twinkle -= kTwoPi * 64.0f;

    repaint();
}

float AuroraVisualiser::getCentreOfActivity() const noexcept
{
    float weighted = 0.0f, total = 0.0f;

    for (std::size_t band = 0; band < numBands; ++band)
    {
        weighted += shape[0][band] * static_cast<float> (band);
        total += shape[0][band];
    }

    return total > 0.0f ? weighted / (total * static_cast<float> (numBands - 1)) : 0.5f;
}

float AuroraVisualiser::valueAt (const Bands& bands, float position) noexcept
{
    // position 0 .. 1 across the bands, smoothly interpolated
    const float x = juce::jlimit (0.0f, 1.0f, position) * static_cast<float> (numBands - 1);
    const auto i = std::min (numBands - 2, static_cast<std::size_t> (x));
    float t = x - static_cast<float> (i);
    t = t * t * (3.0f - 2.0f * t);
    return bands[i] + t * (bands[i + 1] - bands[i]);
}

void AuroraVisualiser::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat();

    // The panel and what is behind it: drawn once per window size, at the resolution of the screen
    const float pixelsPerUnit = g.getInternalContext().getPhysicalPixelScaleFactor();
    const int pixelWidth  = juce::jmax (1, juce::roundToInt (area.getWidth()  * pixelsPerUnit));
    const int pixelHeight = juce::jmax (1, juce::roundToInt (area.getHeight() * pixelsPerUnit));

    if (backdrop.getWidth() != pixelWidth || backdrop.getHeight() != pixelHeight)
    {
        backdrop = juce::Image (juce::Image::ARGB, pixelWidth, pixelHeight, true);
        juce::Graphics b (backdrop);
        b.addTransform (juce::AffineTransform::scale (static_cast<float> (pixelWidth) / area.getWidth(),
                                                      static_cast<float> (pixelHeight) / area.getHeight()));

        if (paintBehind != nullptr)
        {
            juce::Graphics::ScopedSaveState behind (b);
            paintBehind (b);
        }

        b.setColour (juce::Colour (0xaa0b0f21));
        b.fillRoundedRectangle (area, 12.0f);
        b.setColour (juce::Colour (0x44818cf8));
        b.drawRoundedRectangle (area.reduced (0.5f), 12.0f, 1.0f);
    }

    g.drawImage (backdrop, area);

    juce::Graphics::ScopedSaveState state (g);
    g.reduceClipRegion (getLocalBounds().reduced (2));

    const float midY   = area.getCentreY();
    const float width  = area.getWidth();
    const float reach  = area.getHeight() * 0.42f;      // how far a fully swollen ribbon goes
    constexpr float restAmplitude = 3.0f;               // the gentle wave of an idle plug-in
    constexpr int steps = 160;

    // Subtle horizon baseline
    g.setColour (juce::Colour (0x2294a3b8));
    g.drawHorizontalLine (static_cast<int> (midY), area.getX() + 8.0f, area.getRight() - 8.0f);

    const std::array<juce::Colour, numLayers> colours { frozen ? ice : cyan,
                                                        frozen ? cyan : violet,
                                                        frozen ? cyan.brighter (0.3f) : pink };

    // How far a ribbon can swell at each point: its rest wave, plus the sound's share. Part of
    // that share goes to the whole ribbon (so all of it comes alive with the sound), the larger
    // part goes where the sound's energy is. The ribbons are pinned at both ends of the panel.
    const auto swell = [&] (std::size_t layer, float position)
    {
        const float everywhere = layer == 2 ? 0.12f : 0.28f;
        const float sound = energy * reach * (everywhere + (1.0f - everywhere) * valueAt (shape[layer], position))
                                * (1.0f - 0.12f * static_cast<float> (layer));
        return (restAmplitude + sound) * std::sin (position * kPi);
    };

    // A soft glow where the front ribbon can reach: the outline of the sound itself
    if (energy > 0.01f)
    {
        juce::Path glow;

        for (int s = 0; s <= steps; ++s)
        {
            const float position = static_cast<float> (s) / static_cast<float> (steps);
            const float x = area.getX() + position * width;
            const float y = midY - swell (0, position);
            if (s == 0) glow.startNewSubPath (x, y); else glow.lineTo (x, y);
        }

        for (int s = steps; s >= 0; --s)
        {
            const float position = static_cast<float> (s) / static_cast<float> (steps);
            glow.lineTo (area.getX() + position * width, midY + swell (0, position));
        }

        glow.closeSubPath();

        juce::ColourGradient fill (colours[0].withAlpha (0.0f), 0.0f, midY - reach,
                                   colours[0].withAlpha (0.0f), 0.0f, midY + reach, false);
        fill.addColour (0.5, colours[1].withAlpha (0.20f * energy + 0.04f));
        g.setGradientFill (fill);
        g.fillPath (glow);
    }

    // The three ribbons, back to front
    for (std::size_t n = 0; n < numLayers; ++n)
    {
        const std::size_t layer = numLayers - 1 - n;
        const float waves = 1.6f + 0.7f * static_cast<float> (layer);
        juce::Path ribbon;

        for (int s = 0; s <= steps; ++s)
        {
            const float position = static_cast<float> (s) / static_cast<float> (steps);

            // a slow wave with a quicker ripple on it, so that a swollen stretch is never
            // caught exactly where the slow wave crosses the middle
            const float wave = 0.78f * std::sin (position * kTwoPi * waves + phase[layer])
                             + 0.22f * std::sin (position * kTwoPi * waves * 2.7f - 1.3f * phase[layer]);

            const float x = area.getX() + position * width;
            const float y = midY + wave * swell (layer, position);
            if (s == 0) ribbon.startNewSubPath (x, y); else ribbon.lineTo (x, y);
        }

        const auto colour = colours[layer];
        g.setColour (colour.withAlpha (0.22f));
        g.strokePath (ribbon, juce::PathStrokeType (5.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (colour.withAlpha (0.85f));
        g.strokePath (ribbon, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Glints where the shimmer is at work
    if (sparkle > 0.02f)
    {
        for (int i = 0; i < 26; ++i)
        {
            // fixed places, scattered by a simple hash; each one fades in and out at its own pace
            const auto hash = static_cast<std::uint32_t> (i) * 2654435761u;
            const float position = 0.06f + 0.88f * static_cast<float> ((hash >> 8) & 0xffffu) / 65535.0f;
            const float height   = static_cast<float> ((hash >> 20) & 0xffu) / 255.0f * 2.0f - 1.0f;
            const float pace     = 0.6f + static_cast<float> (hash & 0xffu) / 255.0f;

            const float local = valueAt (shape[2], position);
            const float glint = sparkle * local * (0.5f + 0.5f * std::sin (twinkle * pace + static_cast<float> (i)));
            if (glint < 0.03f)
                continue;

            const float x = area.getX() + position * width;
            const float y = midY + height * (4.0f + swell (2, position));
            const float r = 0.8f + 1.6f * glint;

            g.setColour (juce::Colours::white.withAlpha (juce::jmin (0.9f, 1.4f * glint)));
            g.fillEllipse (x - r, y - r, 2.0f * r, 2.0f * r);
            g.setColour (colours[2].withAlpha (juce::jmin (0.35f, 0.6f * glint)));
            g.fillEllipse (x - 2.6f * r, y - 2.6f * r, 5.2f * r, 5.2f * r);
        }
    }

    if (frozen)
    {
        g.setColour (cyan);
        g.setFont (juce::Font (juce::FontOptions { 10.5f, juce::Font::bold }));
        g.drawText ("FROZEN CLOUD", getLocalBounds().reduced (12, 6), juce::Justification::topRight);
    }
}

//==============================================================================

void CloudveilAudioProcessorEditor::Content::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    // Midnight-indigo gradient backdrop
    juce::ColourGradient bg (skyTop, 0.0f, 0.0f,
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

        // (not between the preset arrows and the name, where a star reads as a stray dot)
        if (! (sx > 370.0f && sy < 56.0f))
            g.fillEllipse (sx, sy, sz, sz);
    }

    // Header: the product's name, "D.A LUNE" - the maker's initials in the accent colour, the
    // name itself in white
    const juce::Font titleFont (juce::FontOptions { 22.0f, juce::Font::bold });
    const int initialsWidth = juce::roundToInt (juce::GlyphArrangement::getStringWidth (titleFont, "D.A |")
                                              - juce::GlyphArrangement::getStringWidth (titleFont, "|"));
    g.setFont (titleFont);
    g.setColour (accent);
    g.drawText ("D.A", 26, 14, initialsWidth, 26, juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xfff8fafc));
    g.drawText ("LUNE", 26 + initialsWidth, 14, 220 - initialsWidth, 26, juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xff94a3b8));
    g.setFont (juce::Font (juce::FontOptions { 10.5f, juce::Font::plain }));
    g.drawText ("DREAMY SHIMMER REVERB", 27, 40, 220, 14, juce::Justification::centredLeft);
}

//==============================================================================

CloudveilAudioProcessorEditor::CloudveilAudioProcessorEditor (CloudveilAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // (read before anything below gives the window a size, which resized() would remember)
    const int widthToOpenAt = audioProcessor.getEditorWidth();

    setLookAndFeel (&lookAndFeel);
    setOpaque (true);

    content.setComponentID ("content");
    addAndMakeVisible (content);

    visualiser.setComponentID ("visualiser");
    visualiser.paintBehind = [this] (juce::Graphics& g)
    {
        g.setOrigin (-visualiser.getX(), -visualiser.getY());
        content.paint (g);
    };
    content.addAndMakeVisible (visualiser);

    // Presets: the list, and an arrow either side of it to walk through them
    previousButton.setComponentID ("previousPreset");
    nextButton.setComponentID ("nextPreset");
    presetButton.setComponentID ("presetName");

    previousButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick     = [this] { stepPreset (1); };
    presetButton.onClick   = [this] { showPresetMenu(); };

    content.addAndMakeVisible (previousButton);
    content.addAndMakeVisible (presetButton);
    content.addAndMakeVisible (nextButton);
    refreshPresetName();

    // Freeze toggle
    freezeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.apvts, "freeze", freezeButton);
    content.addAndMakeVisible (freezeButton);

    // The number format of each read-out comes from its parameter (see createParameterLayout()),
    // so the plug-in and the host always show the same text.
    struct ParamMeta
    {
        const char* id;
        const char* label;
        const char* suffix;
    };

    constexpr std::array<ParamMeta, 8> meta {{
        { "decay",    "DECAY",     " s"  },
        { "size",     "SIZE",      " %"  },
        { "predelay", "PRE-DELAY", " ms" },
        { "mix",      "MIX",       " %"  },
        { "shimmer",  "SHIMMER",   " %"  },
        { "drift",    "DRIFT",     " %"  },
        { "warmth",   "WARMTH",    " %"  },
        { "width",    "WIDTH",     " %"  }
    }};

    for (std::size_t i = 0; i < 8; ++i)
    {
        auto& k = knobs[i];
        k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 18);
        k.slider.setTextValueSuffix (meta[i].suffix);
        k.slider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffcbd5e1));
        k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        k.slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0x280f172a));
        content.addAndMakeVisible (k.slider);

        k.title.setText (meta[i].label, juce::dontSendNotification);
        k.title.setJustificationType (juce::Justification::centred);
        k.title.setColour (juce::Label::textColourId, accent);
        k.title.setFont (juce::Font (juce::FontOptions { 11.5f, juce::Font::bold }));
        content.addAndMakeVisible (k.title);

        k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            audioProcessor.apvts, meta[i].id, k.slider);
    }

    // The layout inside `content` never changes; the window scales it (see resized())
    content.setSize (baseWidth, baseHeight);

    previousButton.setBounds (382, 18, 26, 28);
    presetButton.setBounds (412, 18, 190, 28);
    nextButton.setBounds (606, 18, 26, 28);
    freezeButton.setBounds (644, 18, 92, 28);
    visualiser.setBounds (24, 68, baseWidth - 48, 92);

    const int topY = 176;
    const int rowH = 124;
    const int colW = (baseWidth - 48) / 4;

    for (std::size_t i = 0; i < 8; ++i)
    {
        const int row = static_cast<int> (i / 4);
        const int col = static_cast<int> (i % 4);
        const auto cell = juce::Rectangle<int> (24 + col * colW, topY + row * rowH, colW, rowH);

        knobs[i].title.setBounds (cell.getX(), cell.getY(), cell.getWidth(), 18);
        knobs[i].slider.setBounds (cell.getX() + 14, cell.getY() + 18, cell.getWidth() - 28, 98);
    }

    // The window can be dragged larger or smaller by its corner (or by the host, where the host
    // offers that), keeps its proportions, and opens at the size it was last given.
    const double aspect = static_cast<double> (baseWidth) / static_cast<double> (baseHeight);
    const auto heightFor = [aspect] (int width) { return juce::roundToInt (static_cast<double> (width) / aspect); };

    setResizable (true, true);
    setResizeLimits (CloudveilAudioProcessor::editorMinWidth, heightFor (CloudveilAudioProcessor::editorMinWidth),
                     CloudveilAudioProcessor::editorMaxWidth, heightFor (CloudveilAudioProcessor::editorMaxWidth));

    if (auto* limits = getConstrainer())
        limits->setFixedAspectRatio (aspect);

    setSize (widthToOpenAt, heightFor (widthToOpenAt));
    audioProcessor.setEditorWidth (getWidth());
    rememberSize = true;

    startTimerHz (30);
}

CloudveilAudioProcessorEditor::~CloudveilAudioProcessorEditor()
{
    stopTimer();
    juce::PopupMenu::dismissAllActiveMenus();   // the preset list uses this editor's look and feel
    setLookAndFeel (nullptr);
}

void CloudveilAudioProcessorEditor::selectPreset (int index)
{
    audioProcessor.selectProgramFromEditor (index);
    refreshPresetName();
}

void CloudveilAudioProcessorEditor::stepPreset (int delta)
{
    const int count = audioProcessor.getNumPrograms();
    selectPreset (((audioProcessor.getCurrentProgram() + delta) % count + count) % count);
}

void CloudveilAudioProcessorEditor::refreshPresetName()
{
    // The processor is the only place that knows which preset is loaded: it may have been chosen
    // here, by the host's own preset list, by automation or by loading a project. The button
    // just shows what the processor says.
    presetButton.setPreset (audioProcessor.getProgramName (audioProcessor.getCurrentProgram()),
                            audioProcessor.isCurrentProgramModified());
}

juce::PopupMenu CloudveilAudioProcessorEditor::createPresetMenu() const
{
    const auto& presets = CloudveilAudioProcessor::getPresets();
    const int current = audioProcessor.getCurrentProgram();

    // The list is ordered so that the presets of a category follow each other
    juce::PopupMenu menu, group;
    const char* groupName = nullptr;
    bool groupHasCurrent = false;

    const auto finishGroup = [&]
    {
        if (groupName != nullptr)
            menu.addSubMenu (groupName, group, true, juce::Image(), groupHasCurrent);

        group = juce::PopupMenu();
        groupHasCurrent = false;
    };

    for (int i = 0; i < static_cast<int> (presets.size()); ++i)
    {
        const auto& preset = presets[static_cast<std::size_t> (i)];

        if (groupName == nullptr || std::strcmp (groupName, preset.category) != 0)
        {
            finishGroup();
            groupName = preset.category;
        }

        group.addItem (i + 1, preset.name, true, i == current);
        groupHasCurrent = groupHasCurrent || i == current;
    }

    finishGroup();
    return menu;
}

void CloudveilAudioProcessorEditor::showPresetMenu()
{
    auto menu = createPresetMenu();
    menu.setLookAndFeel (&lookAndFeel);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetButton)
                                                  .withMinimumWidth (presetButton.getWidth()),
                        [safeThis = juce::Component::SafePointer<CloudveilAudioProcessorEditor> (this)] (int chosen)
                        {
                            if (safeThis != nullptr && chosen > 0)
                                safeThis->selectPreset (chosen - 1);
                        });
}

void CloudveilAudioProcessorEditor::timerCallback()
{
    refreshPresetName();

    // The display is only fed while the host runs audio through the plug-in. Once it stops
    // doing that, the last thing it was fed would otherwise stay on screen for good.
    const auto callbackCount = audioProcessor.getAudioCallbackCount();
    if (callbackCount != lastAudioCallbackCount)
    {
        lastAudioCallbackCount = callbackCount;
        ticksSinceAudio = 0;
    }
    else if (ticksSinceAudio < maxIdleTicks)
    {
        ++ticksSinceAudio;
    }

    visualiser.advance (audioProcessor, ticksSinceAudio < maxIdleTicks, freezeButton.getToggleState());
}

void CloudveilAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Only seen if a host insists on a window of other proportions than the design's
    g.fillAll (skyTop);
}

void CloudveilAudioProcessorEditor::resized()
{
    // Scale the design to the window, and centre it if the proportions differ
    const float scale = juce::jmin (static_cast<float> (getWidth())  / static_cast<float> (baseWidth),
                                    static_cast<float> (getHeight()) / static_cast<float> (baseHeight));

    content.setTransform (juce::AffineTransform::scale (scale)
                              .translated (0.5f * (static_cast<float> (getWidth())  - scale * static_cast<float> (baseWidth)),
                                           0.5f * (static_cast<float> (getHeight()) - scale * static_cast<float> (baseHeight))));

    if (rememberSize)
        audioProcessor.setEditorWidth (getWidth());
}
