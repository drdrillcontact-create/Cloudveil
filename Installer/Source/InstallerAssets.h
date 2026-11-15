#pragma once

#include <juce_graphics/juce_graphics.h>

namespace dalune
{

/** The picture and the typefaces that are built into the installer (Installer/Assets). */
class Assets
{
public:
    Assets();

    /** The picture on the left, in the size that suits `pixelHeight` physical pixels. */
    const juce::Image& getPortrait (float pixelHeight) const;

    /** What could not be loaded: empty when everything is there. */
    juce::StringArray getMissing() const;

    // Red Hat Display, for everything that is read
    juce::Font text       (float height) const      { return make (regular, height, 0.0f); }
    juce::Font textMedium (float height) const      { return make (medium,  height, 0.0f); }
    juce::Font textBold   (float height) const      { return make (bold,    height, 0.0f); }

    // Orbitron, for the brand line. Tracking is extra space between letters, in font heights.
    juce::Font brand      (float height, float tracking) const   { return make (tech,      height, tracking); }
    juce::Font brandLight (float height, float tracking) const   { return make (techLight, height, tracking); }

private:
    static juce::Font make (const juce::Typeface::Ptr&, float height, float tracking);

    juce::Image portrait, portraitHalf;
    juce::Typeface::Ptr regular, medium, bold, tech, techLight;
};

} // namespace dalune
