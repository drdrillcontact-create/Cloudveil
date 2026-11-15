#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "InstallerAssets.h"
#include "InstallerEngine.h"

namespace dalune
{

/** Everything the installer shows: the picture, the brand line, and the pages
    Welcome -> Install Location -> Installing -> Finished (or Failed).

    It is laid out once, for a size of designWidth x designHeight; the window scales it.
    Clicks that land on nothing fall through to the window, which is how the window is dragged.
*/
class InstallerView : public juce::Component,
                      private juce::Timer
{
public:
    enum class Page { welcome, location, installing, finished, failed };

    static constexpr int designWidth  = 980;
    static constexpr int designHeight = 386;

    /** `payloadError` is what went wrong if there is no payload. */
    InstallerView (const Assets&, std::unique_ptr<Payload>, std::vector<InstallTarget>,
                   const juce::String& payloadError);
    ~InstallerView() override;

    std::function<void()> onMinimise;           // the minimise button
    std::function<void()> onDone;               // Cancel, Finish, Close: the installer is to quit
    std::function<void (Page)> onPageChanged;

    Page getPage() const noexcept                               { return page; }
    const std::vector<InstallTarget>& getTargets() const        { return targets; }
    const InstallReport& getReport() const                      { return report; }
    double getShownProgress() const noexcept                    { return shownProgress; }

    /** What the main button does on the current page: Next, Next (install), Finish, Close. */
    void advance();

    /** Cancel, the close button, Escape. Does nothing while files are being copied. */
    void dismiss();

    void setTargetSelected (int index, bool shouldBeSelected);

    // Component IDs, for tests: "next", "cancel", "minimise", "close", "target-VST3", "target-AU"

    void paint (juce::Graphics&) override;

private:
    class PillButton;
    class ChromeButton;
    class TargetRow;
    class Job;

    void showPage (Page);
    void startInstalling();
    void timerCallback() override;

    void paintBackdrop (juce::Graphics&) const;
    void paintBrand (juce::Graphics&) const;
    void paintHeadline (juce::Graphics&, const juce::String& first,
                        const juce::String& white, const juce::String& red) const;
    void paintProgress (juce::Graphics&) const;

    const Assets& assets;
    std::unique_ptr<Payload> payload;
    std::vector<InstallTarget> targets;
    InstallReport report;
    Page page = Page::welcome;

    std::unique_ptr<PillButton> nextButton, cancelButton;
    std::unique_ptr<ChromeButton> minimiseButton, closeButton;
    juce::OwnedArray<TargetRow> rows;

    std::unique_ptr<Job> job;
    double shownProgress = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstallerView)
};

} // namespace dalune
