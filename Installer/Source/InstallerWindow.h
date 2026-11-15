#pragma once

#include "InstallerShell.h"

namespace dalune
{

/** The installer's window. It has no title bar of the system's: the view draws its own frame,
    with a minimise and a close button, and the window is moved by dragging it anywhere that is
    not a button. It is resized by dragging any of its edges or corners, and keeps its
    proportions while that is done. */
class InstallerWindow final : public juce::ResizableWindow
{
public:
    /** How far in from an edge the window can be taken hold of to resize it. */
    static constexpr int edgeThickness = 6;

    // From 70 % to 250 % of the size the view was drawn for
    static constexpr double smallestScale = 0.7, largestScale = 2.5;

    InstallerWindow (const juce::String& title, std::unique_ptr<InstallerShell> shellToShow)
        : juce::ResizableWindow (title, juce::Colour (0xff040303), false),
          shell (*shellToShow)
    {
        setUsingNativeTitleBar (false);
        setOpaque (true);
        setContentOwned (shellToShow.release(), false);
        setResizable (true, false);

        const int w = InstallerView::designWidth, h = InstallerView::designHeight;

        setResizeLimits (juce::roundToInt (smallestScale * w), juce::roundToInt (smallestScale * h),
                         juce::roundToInt (largestScale * w),  juce::roundToInt (largestScale * h));

        if (auto* limits = getConstrainer())
            limits->setFixedAspectRatio ((double) w / (double) h);

        setSize (w, h);
    }

    /** Puts the window in the middle of the main screen and shows it: at 100 %, or smaller on
        a screen that could not hold that. */
    void showOnScreen()
    {
        const int w = InstallerView::designWidth, h = InstallerView::designHeight;
        juce::Rectangle<int> screen (0, 0, w, h);
        double scale = 1.0;

        if (const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        {
            screen = display->userBounds.toNearestInt();
            scale = juce::jlimit (smallestScale, 1.0, juce::jmin (0.92 * (double) screen.getWidth()  / (double) w,
                                                                  0.92 * (double) screen.getHeight() / (double) h));
        }

        setBounds (screen.withSizeKeepingCentre (juce::roundToInt (scale * w), juce::roundToInt (scale * h)));
        addToDesktop();
        setVisible (true);
        toFront (true);
    }

    InstallerView& getView() noexcept       { return shell.getView(); }
    InstallerShell& getShell() noexcept     { return shell; }

    int getDesktopWindowStyleFlags() const override
    {
        // (So that the system can still minimise and close it: the taskbar, Alt+F4, the Dock.)
        return juce::ResizableWindow::getDesktopWindowStyleFlags()
             | juce::ComponentPeer::windowHasMinimiseButton
             | juce::ComponentPeer::windowHasCloseButton;
    }

    // The edges that resize the window lie underneath the view, which fills the whole window
    // and lets the mouse through wherever it has no button.
    juce::BorderSize<int> getBorderThickness() const override           { return juce::BorderSize<int> (edgeThickness); }
    juce::BorderSize<int> getContentComponentBorder() const override    { return {}; }

    void userTriedToCloseWindow() override
    {
        getView().dismiss();
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::returnKey)   { getView().advance(); return true; }
        if (key == juce::KeyPress::escapeKey)   { getView().dismiss(); return true; }

        return false;
    }

private:
    InstallerShell& shell;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstallerWindow)
};

} // namespace dalune
