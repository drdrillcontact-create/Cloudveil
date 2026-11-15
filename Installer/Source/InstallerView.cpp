#include "InstallerView.h"
#include "InstallerArt.h"
#include "InstallerConfig.h"

namespace dalune
{

namespace
{
// The colours of the design
const juce::Colour backdropColour { 0xff040303 };
const juce::Colour panelColour    { 0xff020202 };
const juce::Colour red            { 0xffea030f };      // the Next button, the tick, the light lines
const juce::Colour textRed        { 0xffdd0510 };      // the red word of a headline
const juce::Colour markRed        { 0xffe4010c };
const juce::Colour white          { 0xffdedede };      // headlines
const juce::Colour brightWhite    { 0xffeeeeee };      // button text and the window controls
const juce::Colour grey           { 0xffaaa9a9 };      // running text
const juce::Colour dimGrey        { 0xff757575 };
const juce::Colour outlineGrey    { 0xff4a4a4a };

// Where things are, in the view's own units (designWidth x designHeight)
const juce::Rectangle<float> frame     { 10.0f, 10.0f, 960.0f, 366.0f };
const juce::Rectangle<float> photoArea { 11.0f, 10.8f, 463.2f, 364.6f };
constexpr float chamfer    = 9.4f;      // how much the frame's corners are cut
constexpr float textLeft   = 479.5f;
constexpr float textRight  = 919.3f;    // the right edge of the Cancel button

const juce::Rectangle<int> nextBounds     { 479, 307, 218, 39 };
const juce::Rectangle<int> cancelBounds   { 719, 307, 200, 39 };
constexpr int glowMargin = 14;          // room around a button for the light it gives off
const juce::Rectangle<int> minimiseBounds { 893, 19, 25, 23 };
const juce::Rectangle<int> closeBounds    { 928, 19, 25, 23 };

const juce::Rectangle<float> progressBar  { textLeft + 1.0f, 226.0f, textRight - textLeft - 1.0f, 8.0f };

juce::Path makeFramePath()
{
    const auto r = frame;
    juce::Path p;
    p.startNewSubPath (r.getX() + chamfer, r.getY());
    p.lineTo (r.getRight() - chamfer, r.getY());
    p.lineTo (r.getRight(), r.getY() + chamfer);
    p.lineTo (r.getRight(), r.getBottom() - chamfer);
    p.lineTo (r.getRight() - chamfer, r.getBottom());
    p.lineTo (r.getX() + chamfer, r.getBottom());
    p.lineTo (r.getX(), r.getBottom() - chamfer);
    p.lineTo (r.getX(), r.getY() + chamfer);
    p.closeSubPath();
    return p;
}

// The width a piece of text takes up in a line. (Measured between two markers, so that
// spaces at its ends count.)
float widthOf (const juce::Font& font, const juce::String& text)
{
    return juce::GlyphArrangement::getStringWidth (font, "|" + text + "|")
         - juce::GlyphArrangement::getStringWidth (font, "||");
}

struct Run
{
    juce::Font font;
    juce::Colour colour;
    juce::String text;
};

// Draws pieces of text one after the other on a baseline; returns where the line ends
float drawRuns (juce::Graphics& g, float x, float baseline, const std::vector<Run>& runs)
{
    for (const auto& run : runs)
    {
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText (run.font, run.text, x, baseline);
        g.setColour (run.colour);
        glyphs.draw (g);
        x += widthOf (run.font, run.text);
    }

    return x;
}

// Text with its letters spread out so that it is exactly `width` wide
void drawSpreadText (juce::Graphics& g, const juce::Font& font, const juce::String& text,
                     float x, float baseline, float width)
{
    const auto natural = juce::GlyphArrangement::getStringWidth (font, text);
    const auto gaps = (float) juce::jmax (1, text.length() - 1);
    const auto spread = font.withExtraKerningFactor (juce::jmax (0.0f, (width - natural) / gaps / font.getHeight()));

    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText (spread, text, x, baseline);
    glyphs.draw (g);
}

// A line of light: the line itself over three wider, fainter ones
void strokeGlowing (juce::Graphics& g, const juce::Path& path, const juce::ColourGradient& light, float thickness)
{
    const std::pair<float, float> halo[] { { 7.0f, 0.045f }, { 4.0f, 0.09f }, { 1.8f, 0.2f } };

    for (const auto& [extra, alpha] : halo)
    {
        auto faint = light;
        faint.multiplyOpacity (alpha);
        g.setGradientFill (faint);
        g.strokePath (path, juce::PathStrokeType (thickness + extra, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
    }

    g.setGradientFill (light);
    g.strokePath (path, juce::PathStrokeType (thickness, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
}

// Red that is full at `from` (and before it) and has faded away at `to`
juce::ColourGradient fadingRed (float alpha, juce::Point<float> from, juce::Point<float> to)
{
    return juce::ColourGradient (red.withAlpha (alpha), from.x, from.y, red.withAlpha (0.0f), to.x, to.y, false);
}

juce::String productName()      { return dalune::config::productName; }
}

//==============================================================================
/** Next / Finish / Close (red) and Cancel (outlined). */
class InstallerView::PillButton : public juce::Button
{
public:
    PillButton (const Assets& a, bool isMainButton)
        : juce::Button ({}), assets (a), main (isMainButton)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setWantsKeyboardFocus (false);
    }

    void setLabel (const juce::String& label, bool withArrow)
    {
        setButtonText (label);
        arrow = withArrow;
        repaint();
    }

    /** The button itself, without the room for its glow. */
    void setPillBounds (juce::Rectangle<int> pill)      { setBounds (pill.expanded (glowMargin)); }

    bool hitTest (int x, int y) override
    {
        return getLocalBounds().reduced (glowMargin).contains (x, y);
    }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        const auto area = getLocalBounds().reduced (glowMargin).toFloat().reduced (0.25f);
        const auto font = assets.textBold (22.2f);
        const bool enabled = isEnabled();

        if (main)
        {
            auto fill = enabled ? red : juce::Colour (0xff4a1216);
            if (enabled && down)       fill = fill.darker (0.18f);
            else if (enabled && over)  fill = fill.brighter (0.12f);

            if (enabled)
            {
                // The red glow around the button: rings laid over each other, so that it is
                // strongest at the button's edge and gone about twelve units out.
                g.setColour (red.withAlpha (over ? 0.026f : 0.021f));

                for (int ring = 8; ring >= 1; --ring)
                    g.fillRoundedRectangle (area.expanded (1.5f * (float) ring), 7.0f + 1.5f * (float) ring);
            }

            g.setColour (fill);
            g.fillRoundedRectangle (area.reduced (down ? 0.5f : 0.0f), 7.0f);

            // a faint light along the top edge
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (enabled ? 0.16f : 0.05f), 0.0f, area.getY(),
                                                     juce::Colours::transparentWhite, 0.0f, area.getY() + 9.0f, false));
            g.fillRoundedRectangle (area, 7.0f);
        }
        else
        {
            g.setColour (juce::Colour (down ? 0xff141414 : (over ? 0xff0e0e0e : 0xff050505)));
            g.fillRoundedRectangle (area, 11.5f);
            g.setColour (over ? juce::Colour (0xff9a9a9a) : outlineGrey);
            g.drawRoundedRectangle (area, 11.5f, 1.2f);
        }

        const auto textColour = main ? juce::Colours::black.withAlpha (enabled ? 1.0f : 0.55f)
                                     : brightWhite.withAlpha (enabled ? 1.0f : 0.5f);

        const float textWidth  = widthOf (font, getButtonText());
        const float arrowGap   = 9.0f, arrowWidth = 15.0f;
        const float total      = textWidth + (arrow ? arrowGap + arrowWidth : 0.0f);
        const float x          = area.getCentreX() - 0.5f * total;
        const float baseline   = area.getCentreY() + 5.9f;        // (the capitals are 11.7 high)

        drawRuns (g, x, baseline, { { font, textColour, getButtonText() } });

        if (arrow)
        {
            const float ax = x + textWidth + arrowGap;
            const float ay = area.getCentreY() + 0.4f;

            juce::Path shaft;
            shaft.startNewSubPath (ax, ay);
            shaft.lineTo (ax + arrowWidth - 0.6f, ay);

            juce::Path head;
            head.startNewSubPath (ax + arrowWidth - 5.6f, ay - 5.0f);
            head.lineTo (ax + arrowWidth, ay);
            head.lineTo (ax + arrowWidth - 5.6f, ay + 5.0f);

            g.setColour (textColour);
            g.strokePath (shaft, juce::PathStrokeType (1.9f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
            g.strokePath (head,  juce::PathStrokeType (1.9f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
        }
    }

private:
    const Assets& assets;
    const bool main;
    bool arrow = false;
};

//==============================================================================
/** The two small controls in the top right corner: minimise and close. */
class InstallerView::ChromeButton : public juce::Button
{
public:
    explicit ChromeButton (bool isCloseButton)
        : juce::Button (isCloseButton ? "Close" : "Minimise"), closes (isCloseButton)
    {
        setWantsKeyboardFocus (false);
    }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        const auto centre = getLocalBounds().toFloat().getCentre().translated (0.0f, 0.4f);
        const float half = 6.4f;

        g.setColour (! isEnabled() ? brightWhite.withAlpha (0.25f)
                                   : (over || down) ? (closes ? red.brighter (0.25f) : juce::Colours::white)
                                                    : brightWhite);

        juce::Path glyph;

        if (closes)
        {
            glyph.startNewSubPath (centre.x - half, centre.y - half);
            glyph.lineTo (centre.x + half, centre.y + half);
            glyph.startNewSubPath (centre.x + half, centre.y - half);
            glyph.lineTo (centre.x - half, centre.y + half);
        }
        else
        {
            glyph.startNewSubPath (centre.x - half, centre.y);
            glyph.lineTo (centre.x + half, centre.y);
        }

        g.strokePath (glyph, juce::PathStrokeType (1.5f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
    }

private:
    const bool closes;
};

//==============================================================================
/** One line of the Install Location page: a tick box, what is installed, and where to. */
class InstallerView::TargetRow : public juce::Button
{
public:
    TargetRow (const Assets& a, const InstallTarget& target)
        : juce::Button (target.title), assets (a), title (target.title), path (target.shownPath)
    {
        setClickingTogglesState (true);
        setToggleState (target.selected, juce::dontSendNotification);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setWantsKeyboardFocus (false);
    }

    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        const auto box = juce::Rectangle<float> (3.0f, 3.0f, 17.0f, 17.0f);
        const bool ticked = getToggleState();

        if (ticked)
        {
            g.setColour (red.withAlpha (0.16f));
            g.fillRoundedRectangle (box.expanded (2.5f), 6.0f);
            g.setColour (over ? red.brighter (0.12f) : red);
            g.fillRoundedRectangle (box, 4.0f);

            juce::Path tick;
            tick.startNewSubPath (box.getX() + 4.2f, box.getCentreY() + 0.3f);
            tick.lineTo (box.getX() + 7.4f, box.getBottom() - 4.6f);
            tick.lineTo (box.getRight() - 3.8f, box.getY() + 4.8f);
            g.setColour (juce::Colours::black);
            g.strokePath (tick, juce::PathStrokeType (2.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
        }
        else
        {
            g.setColour (juce::Colour (0xff070707));
            g.fillRoundedRectangle (box, 4.0f);
            g.setColour (over ? juce::Colour (0xff9a9a9a) : outlineGrey);
            g.drawRoundedRectangle (box, 4.0f, 1.2f);
        }

        const float left = box.getRight() + 10.0f;
        drawRuns (g, left, 16.4f, { { assets.textBold (19.5f), ticked ? white : grey, title } });
        drawRuns (g, left, 32.2f, { { assets.text (16.0f), ticked ? grey : dimGrey, path } });
    }

private:
    const Assets& assets;
    const juce::String title, path;
};

//==============================================================================
/** Copies the files on a thread of its own, so that the window stays alive meanwhile. */
class InstallerView::Job : public juce::Thread
{
public:
    Job (Payload& p, std::vector<InstallTarget> t)
        : juce::Thread ("D.A LUNE installation"), payload (p), targets (std::move (t))
    {
    }

    ~Job() override
    {
        stopThread (60000);
    }

    void run() override
    {
        report = install (payload, targets, [this] (double fraction) { progress.store (fraction); });
        done.store (true);
    }

    std::atomic<double> progress { 0.0 };
    std::atomic<bool> done { false };
    InstallReport report;                   // to be read once `done` is set

private:
    Payload& payload;
    const std::vector<InstallTarget> targets;
};

//==============================================================================
InstallerView::InstallerView (const Assets& a, std::unique_ptr<Payload> p, std::vector<InstallTarget> t,
                              const juce::String& payloadError)
    : assets (a), payload (std::move (p)), targets (std::move (t))
{
    setOpaque (true);
    setInterceptsMouseClicks (false, true);     // the empty parts belong to the window (dragging)
    setSize (designWidth, designHeight);

    // Only what this installer actually carries is offered
    if (payload != nullptr)
        targets.erase (std::remove_if (targets.begin(), targets.end(),
                                       [this] (const InstallTarget& target) { return payload->getBundleName (target.format).isEmpty(); }),
                       targets.end());

    nextButton     = std::make_unique<PillButton> (assets, true);
    cancelButton   = std::make_unique<PillButton> (assets, false);
    minimiseButton = std::make_unique<ChromeButton> (false);
    closeButton    = std::make_unique<ChromeButton> (true);

    nextButton->setComponentID ("next");
    cancelButton->setComponentID ("cancel");
    minimiseButton->setComponentID ("minimise");
    closeButton->setComponentID ("close");

    nextButton->setPillBounds (nextBounds);
    cancelButton->setPillBounds (cancelBounds);
    minimiseButton->setBounds (minimiseBounds);
    closeButton->setBounds (closeBounds);

    cancelButton->setLabel ("Cancel", false);

    nextButton->onClick     = [this] { advance(); };
    cancelButton->onClick   = [this] { dismiss(); };
    closeButton->onClick    = [this] { dismiss(); };
    minimiseButton->onClick = [this] { if (onMinimise) onMinimise(); };

    addAndMakeVisible (*nextButton);
    addAndMakeVisible (*cancelButton);
    addAndMakeVisible (*minimiseButton);
    addAndMakeVisible (*closeButton);

    // One row per plug-in format, each ticked to begin with
    const int rowHeight = 36, rowGap = 4;
    const int firstRow = targets.size() > 1 ? 219 : 230;

    for (int i = 0; i < (int) targets.size(); ++i)
    {
        auto* row = rows.add (new TargetRow (assets, targets[(size_t) i]));
        row->setComponentID ("target-" + targets[(size_t) i].format);
        row->setBounds ((int) textLeft - 2, firstRow + i * (rowHeight + rowGap), (int) (textRight - textLeft) + 2, rowHeight);
        row->onClick = [this, i, row] { setTargetSelected (i, row->getToggleState()); };
        addChildComponent (row);
    }

    if (payload == nullptr)
    {
        report.error = payloadError;
        showPage (Page::failed);
    }
    else if (targets.empty())
    {
        report.error = "This installer carries no plug-in for this system.";
        showPage (Page::failed);
    }
    else
    {
        showPage (Page::welcome);
    }
}

InstallerView::~InstallerView()
{
    stopTimer();
    job.reset();        // (waits for the copying to end)
}

//==============================================================================
void InstallerView::setTargetSelected (int index, bool shouldBeSelected)
{
    if (! juce::isPositiveAndBelow (index, (int) targets.size()))
        return;

    targets[(size_t) index].selected = shouldBeSelected;
    rows[index]->setToggleState (shouldBeSelected, juce::dontSendNotification);

    // Next is only offered while something is ticked
    const bool any = std::any_of (targets.begin(), targets.end(), [] (const InstallTarget& t) { return t.selected; });
    nextButton->setEnabled (page != Page::location || any);
}

void InstallerView::showPage (Page newPage)
{
    page = newPage;

    for (auto* row : rows)
        row->setVisible (page == Page::location);

    const bool asking = page == Page::welcome || page == Page::location;

    nextButton->setVisible (page != Page::installing);
    cancelButton->setVisible (asking);
    nextButton->setLabel (asking ? "Next" : (page == Page::finished ? "Finish" : "Close"), asking);

    const bool any = std::any_of (targets.begin(), targets.end(), [] (const InstallTarget& t) { return t.selected; });
    nextButton->setEnabled (page != Page::location || any);
    closeButton->setEnabled (page != Page::installing);

    repaint();

    if (onPageChanged)
        onPageChanged (page);
}

void InstallerView::advance()
{
    switch (page)
    {
        case Page::welcome:     showPage (Page::location); break;
        case Page::location:    startInstalling(); break;
        case Page::installing:  break;

        case Page::finished:
        case Page::failed:
            if (onDone)
                onDone();
            break;
    }
}

void InstallerView::dismiss()
{
    // (Not while files are being copied: that takes a moment and is then over.)
    if (page != Page::installing && onDone)
        onDone();
}

void InstallerView::startInstalling()
{
    if (payload == nullptr || job != nullptr
        || std::none_of (targets.begin(), targets.end(), [] (const InstallTarget& t) { return t.selected; }))
        return;

    shownProgress = 0.0;
    job = std::make_unique<Job> (*payload, targets);
    showPage (Page::installing);
    job->startThread();
    startTimerHz (60);
}

void InstallerView::timerCallback()
{
    if (job == nullptr)
    {
        stopTimer();
        return;
    }

    // The files are copied in a blink. The bar takes its time (about a second for the full
    // length), so that the page is seen rather than flashing by.
    const double step = (double) getTimerInterval() / 900.0;
    shownProgress = juce::jmin (job->progress.load(), shownProgress + step);
    repaint (progressBar.expanded (8.0f, 30.0f).getSmallestIntegerContainer());

    if (job->done.load() && (! job->report.succeeded || shownProgress >= 1.0))
    {
        stopTimer();
        job->stopThread (60000);
        report = job->report;
        job.reset();
        showPage (report.succeeded ? Page::finished : Page::failed);
    }
}

//==============================================================================
void InstallerView::paintBackdrop (juce::Graphics& g) const
{
    // Around the frame: near black, with a little red light from the lower right
    g.fillAll (backdropColour);
    g.setGradientFill (juce::ColourGradient (red.withAlpha (0.10f), 930.0f, 400.0f,
                                             red.withAlpha (0.0f), 640.0f, 250.0f, true));
    g.fillRect (getLocalBounds());

    const auto framePath = makeFramePath();

    {
        const juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (framePath);

        g.setColour (panelColour);
        g.fillRect (frame);

        // The woven texture of the right-hand side: fine diagonal stripes, barely there, a
        // little clearer towards the top right
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.034f), frame.getRight(), frame.getY(),
                                                 juce::Colours::white.withAlpha (0.012f), photoArea.getRight(), frame.getBottom(), false));

        for (float x = photoArea.getRight() - frame.getHeight(); x < frame.getRight(); x += 6.2f)
            g.drawLine (x, frame.getBottom(), x + frame.getHeight(), frame.getY(), 2.3f);

        // The picture ends in black on its right, and the texture comes up out of that black
        g.setGradientFill (juce::ColourGradient (panelColour, photoArea.getRight() - 6.0f, 0.0f,
                                                 panelColour.withAlpha (0.0f), photoArea.getRight() + 80.0f, 0.0f, false));
        g.fillRect (juce::Rectangle<float> (frame.getX(), frame.getY(), photoArea.getRight() + 80.0f - frame.getX(), frame.getHeight()));

        if (const auto& picture = assets.getPortrait (photoArea.getHeight() * g.getInternalContext().getPhysicalPixelScaleFactor());
            picture.isValid())
        {
            g.setOpacity (1.0f);        // (pictures are drawn with the opacity of the last colour)
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            g.drawImage (picture, photoArea, juce::RectanglePlacement::stretchToFit);
        }

        // Red light lines down the right edge and across the lower right corner; each fades
        // out where it runs towards the middle of the window
        const float edge = frame.getRight() + 2.0f;

        juce::Path side;
        side.startNewSubPath (edge, 85.5f);
        side.lineTo (960.3f, 100.0f);
        side.lineTo (960.3f, 233.4f);
        side.lineTo (903.3f, 292.6f);
        strokeGlowing (g, side, fadingRed (1.0f, { 960.3f, 233.4f }, { 906.0f, 290.0f }), 1.3f);

        juce::Path band;
        band.startNewSubPath (edge, 253.4f);
        band.lineTo (910.5f, 307.0f);
        g.setGradientFill (fadingRed (0.9f, { 967.0f, 256.0f }, { 913.0f, 305.0f }));
        g.strokePath (band, juce::PathStrokeType (2.6f));

        juce::Path corner;
        corner.startNewSubPath (edge, 339.8f);
        corner.lineTo (922.0f, 377.0f);
        g.setGradientFill (fadingRed (0.55f, { 968.0f, 341.0f }, { 926.0f, 376.0f }));
        g.strokePath (corner, juce::PathStrokeType (6.0f));

        juce::Path lower;
        lower.startNewSubPath (edge, 324.3f);
        lower.lineTo (919.9f, 366.3f);
        lower.lineTo (871.7f, 366.3f);
        lower.lineTo (859.0f, 377.0f);
        strokeGlowing (g, lower, juce::ColourGradient::vertical (red, 0.0f, red, 1.0f), 1.3f);
    }

    // The frame's own line: lighter at the top, darker below, and red down the right side
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff7c7775), 0.0f, frame.getY(),
                                             juce::Colour (0xff312f2f), 0.0f, frame.getBottom(), false));
    g.strokePath (framePath, juce::PathStrokeType (1.5f));

    juce::Path rightEdge;
    rightEdge.startNewSubPath (frame.getRight(), 150.0f);
    rightEdge.lineTo (frame.getRight(), frame.getBottom() - chamfer);
    g.setGradientFill (juce::ColourGradient (red.withAlpha (0.0f), 0.0f, 150.0f,
                                             red.withAlpha (0.5f), 0.0f, 300.0f, false));
    g.strokePath (rightEdge, juce::PathStrokeType (1.5f));
}

void InstallerView::paintBrand (juce::Graphics& g) const
{
    drawBrandMark (g, { 478.1f, 36.9f, 56.5f, 43.7f }, markRed);

    g.setColour (juce::Colour (0xffe8e8e8));
    drawSpreadText (g, assets.brand (18.8f, 0.0f), juce::String (config::companyName).toUpperCase(),
                    558.4f, 60.4f, 264.1f);

    g.setColour (juce::Colour (0xffb5b5b5));
    drawSpreadText (g, assets.brandLight (10.1f, 0.0f), "PROFESSIONAL AUDIO PLUGINS", 558.4f, 75.3f, 238.0f);
}

void InstallerView::paintHeadline (juce::Graphics& g, const juce::String& first,
                                   const juce::String& whitePart, const juce::String& redPart) const
{
    drawRuns (g, textLeft, 155.9f, { { assets.text (46.6f), white.brighter (0.03f), first } });

    const auto bold = assets.textBold (53.0f);
    drawRuns (g, textLeft, 198.0f, { { bold, white, whitePart }, { bold, textRed, redPart } });
}

void InstallerView::paintProgress (juce::Graphics& g) const
{
    const auto bar = progressBar;
    const float radius = 0.5f * bar.getHeight();

    g.setColour (juce::Colour (0xff111111));
    g.fillRoundedRectangle (bar, radius);
    g.setColour (outlineGrey.withAlpha (0.8f));
    g.drawRoundedRectangle (bar, radius, 1.0f);

    const float filled = (float) juce::jlimit (0.0, 1.0, shownProgress) * bar.getWidth();

    if (filled > 1.0f)
    {
        const auto fill = bar.withWidth (juce::jmax (bar.getHeight(), filled));

        g.setColour (red.withAlpha (0.14f));
        g.fillRoundedRectangle (fill.expanded (3.0f), radius + 3.0f);
        g.setColour (red);
        g.fillRoundedRectangle (fill, radius);
    }

    const auto small = assets.text (19.0f);
    const auto percent = juce::String (juce::roundToInt (100.0 * juce::jlimit (0.0, 1.0, shownProgress))) + " %";

    drawRuns (g, textLeft, 258.0f, { { small, grey, "Copying the plug-in files" + juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xa6")) } });
    drawRuns (g, textRight - widthOf (small, percent), 258.0f, { { small, white, percent } });
}

void InstallerView::paint (juce::Graphics& g)
{
    paintBackdrop (g);
    paintBrand (g);

    const auto body   = assets.text (21.1f);
    const auto strong = assets.textMedium (21.1f);
    const auto name   = productName();
    const float line1 = 236.6f, line2 = 257.1f;

    switch (page)
    {
        case Page::welcome:
            paintHeadline (g, "Welcome to the", name + " ", "Installer");
            drawRuns (g, textLeft, line1, { { body, grey, "This will install " }, { strong, white, name }, { body, grey, " on your computer." } });
            drawRuns (g, textLeft, line2, { { body, grey, "Click Next to continue, or Cancel to exit." } });
            break;

        case Page::location:
            paintHeadline (g, "Select the", "Install ", "Location");
            break;

        case Page::installing:
            paintHeadline (g, "Please wait", "Installing ", name);
            paintProgress (g);
            break;

        case Page::finished:
            paintHeadline (g, "Installation complete", name + " is ", "Ready");
            drawRuns (g, textLeft, line1, { { strong, white, name }, { body, grey, " has been installed on your computer." } });
            drawRuns (g, textLeft, line2, { { body, grey, "Click Finish to close the installer." } });
            drawRuns (g, textLeft, 281.0f, { { assets.text (16.5f), dimGrey, "If your DAW does not list it yet, let it rescan its plug-ins." } });
            break;

        case Page::failed:
        {
            paintHeadline (g, "Installation failed", name + " ", "Not Installed");

            juce::AttributedString message;
            message.setJustification (juce::Justification::topLeft);
            message.setWordWrap (juce::AttributedString::byWord);
            message.append (report.error, assets.text (18.5f), grey);

            juce::TextLayout layout;
            layout.createLayout (message, textRight - textLeft + 20.0f);
            layout.draw (g, { textLeft, 217.0f, textRight - textLeft + 20.0f, 84.0f });
            break;
        }
    }
}

} // namespace dalune
