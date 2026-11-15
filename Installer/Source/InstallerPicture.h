#pragma once

#include <juce_graphics/juce_graphics.h>

namespace dalune
{

/** A copy of a picture in another size.

    The copy is `width` x `height` pixels, and each of its pixels covers `sourcePixelsPerPixel`
    of the source's, starting at the top left corner. It is made with a Lanczos filter, which
    averages when the copy is smaller and keeps fine lines and small lettering clean either
    way. (The systems' own scaling differs from one to the next, and JUCE's is rough.)

    `source` has to be an RGB picture held in plain memory (juce::SoftwareImageType); the
    copy is one too. */
juce::Image resampled (const juce::Image& source, int width, int height, double sourcePixelsPerPixel);

} // namespace dalune
