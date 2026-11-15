#include "InstallerAssets.h"
#include <InstallerData.h>      // made from Installer/Assets by juce_add_binary_data

namespace dalune
{

Assets::Assets()
{
    portrait = juce::ImageFileFormat::loadFrom (InstallerData::portrait_jpg, (size_t) InstallerData::portrait_jpgSize);

    // Scaling a picture down to less than half its size in one go makes it gritty, so small
    // windows start from a copy that is already halved.
    if (portrait.isValid())
        portraitHalf = portrait.rescaled (portrait.getWidth() / 2, portrait.getHeight() / 2,
                                          juce::Graphics::highResamplingQuality);

    const auto load = [] (const char* data, int size)
    {
        return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
    };

    regular   = load (InstallerData::RedHatDisplayRegular_ttf, InstallerData::RedHatDisplayRegular_ttfSize);
    medium    = load (InstallerData::RedHatDisplayMedium_ttf,  InstallerData::RedHatDisplayMedium_ttfSize);
    bold      = load (InstallerData::RedHatDisplayBold_ttf,    InstallerData::RedHatDisplayBold_ttfSize);
    tech      = load (InstallerData::OrbitronSemiBold_ttf,     InstallerData::OrbitronSemiBold_ttfSize);
    techLight = load (InstallerData::OrbitronMedium_ttf,       InstallerData::OrbitronMedium_ttfSize);
}

const juce::Image& Assets::getPortrait (float pixelHeight) const
{
    return (portraitHalf.isValid() && pixelHeight <= 1.15f * (float) portraitHalf.getHeight()) ? portraitHalf
                                                                                               : portrait;
}

juce::StringArray Assets::getMissing() const
{
    juce::StringArray missing;

    if (! portrait.isValid())   missing.add ("the picture");
    if (regular == nullptr)     missing.add ("Red Hat Display Regular");
    if (medium == nullptr)      missing.add ("Red Hat Display Medium");
    if (bold == nullptr)        missing.add ("Red Hat Display Bold");
    if (tech == nullptr)        missing.add ("Orbitron SemiBold");
    if (techLight == nullptr)   missing.add ("Orbitron Medium");

    return missing;
}

juce::Font Assets::make (const juce::Typeface::Ptr& typeface, float height, float tracking)
{
    // (Should a typeface ever fail to load, the system's own sans serif stands in.)
    const auto options = typeface != nullptr ? juce::FontOptions (typeface) : juce::FontOptions();
    return juce::Font (options.withHeight (height).withKerningFactor (tracking));
}

} // namespace dalune
