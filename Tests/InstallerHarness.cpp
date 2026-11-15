// Runs the installer's own code without a display (Linux, local JUCE build): the routine that
// puts the plug-in in place, against temporary folders, and the pages, by clicking through them.
// It can also write a picture of every page, exactly as JUCE paints it.
//
//   Tests/run-installer-harness.sh [folder for the pictures]
//   Tests/run-installer-harness.sh --icon <file.png>              draws the installer's icon
//   Tests/run-installer-harness.sh --payload <zip> <folder>       installs a payload zip as the build
//                                                                 makes it into <folder>/<format>
#include "../Installer/Source/InstallerArt.h"
#include "../Installer/Source/InstallerConfig.h"
#include "../Installer/Source/InstallerWindow.h"
#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>

namespace
{
using namespace dalune;

int failures = 0;

void check (bool ok, const juce::String& what)
{
    std::printf ("  [%s] %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8());
    if (! ok) ++failures;
}

struct Entry
{
    juce::String path, content;
};

// A zip as the build makes it: one folder per format, the plug-in inside
juce::MemoryBlock makeZip (const std::vector<Entry>& entries)
{
    juce::ZipFile::Builder builder;

    for (const auto& e : entries)
        builder.addEntry (new juce::MemoryInputStream (e.content.toRawUTF8(), e.content.getNumBytesAsUTF8(), true),
                          6, e.path, juce::Time::getCurrentTime());

    juce::MemoryOutputStream out;
    builder.writeToStream (out, nullptr);
    return out.getMemoryBlock();
}

std::unique_ptr<Payload> openZip (const juce::MemoryBlock& zip, juce::String& error)
{
    return Payload::open (std::make_unique<juce::MemoryInputStream> (zip, true), error);
}

const juce::String binaryText = juce::String::repeatedString ("the plug-in's code, version two. ", 40000);     // 1.3 MB

std::vector<Entry> pluginEntries()
{
    return { { "VST3/D.A LUNE.vst3/Contents/x86_64-win/D.A LUNE.vst3",   binaryText },
             { "VST3/D.A LUNE.vst3/Contents/Resources/moduleinfo.json",  "{ \"Name\": \"D.A LUNE\" }" },
             { "AU/D.A LUNE.component/Contents/MacOS/D.A LUNE",          binaryText },
             { "AU/D.A LUNE.component/Contents/Info.plist",              "<plist/>" },
             { "AU/D.A LUNE.component/Contents/Resources/D.A LUNE.rsrc", "" } };
}

juce::StringArray listing (const juce::File& folder)
{
    juce::StringArray names;

    for (const auto& f : folder.findChildFiles (juce::File::findFilesAndDirectories, true))
        names.add (f.getRelativePathFrom (folder) + (f.isDirectory() ? "/" : ""));

    names.sort (false);
    return names;
}

void letRun (int milliseconds)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil (milliseconds);
}

bool writePng (const juce::Image& image, const juce::File& file)
{
    file.deleteFile();
    juce::FileOutputStream stream (file);
    return stream.openedOk() && juce::PNGImageFormat().writeImageToStream (image, stream);
}

// The installer's icon: the brand mark on a dark tile
juce::Image drawIcon (int size)
{
    juce::Image image (juce::Image::ARGB, size, size, true);
    juce::Graphics g (image);
    const float s = (float) size;
    const auto tile = juce::Rectangle<float> (s, s).reduced (0.085f * s);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1c1113), 0.5f * s, 0.2f * s,
                                             juce::Colour (0xff050404), 0.5f * s, 0.95f * s, false));
    g.fillRoundedRectangle (tile, 0.19f * s);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0x55ea0612), 0.5f * s, 0.58f * s,
                                             juce::Colour (0x00ea0612), 0.5f * s, 0.98f * s, true));
    g.fillRoundedRectangle (tile, 0.19f * s);

    g.setColour (juce::Colour (0xff3a2a2c));
    g.drawRoundedRectangle (tile.reduced (0.006f * s), 0.185f * s, 0.012f * s);

    const float markWidth = 0.56f * s;
    drawBrandMark (g, juce::Rectangle<float> (markWidth, markWidth * 43.7f / 56.5f).withCentre ({ 0.5f * s, 0.505f * s }),
                   juce::Colour (0xffea0612));
    return image;
}
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;

    if (argc > 2 && juce::String (argv[1]) == "--icon")
    {
        const auto file = juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]);
        const bool written = writePng (drawIcon (1024), file);
        std::printf ("%s %s\n", written ? "wrote" : "could not write", file.getFullPathName().toRawUTF8());
        return written ? 0 : 1;
    }

    if (argc > 3 && juce::String (argv[1]) == "--payload")
    {
        const auto zip = juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]);
        const auto folder = juce::File::getCurrentWorkingDirectory().getChildFile (argv[3]);

        juce::String error;
        auto payload = Payload::open (zip.createInputStream(), error);

        if (payload == nullptr)
        {
            std::printf ("refused: %s\n", error.replace ("\n", " ").toRawUTF8());
            return 1;
        }

        std::vector<InstallTarget> targets;

        for (const auto& format : payload->getFormats())
        {
            std::printf ("%s: %s, %lld bytes\n", format.toRawUTF8(), payload->getBundleName (format).toRawUTF8(), (long long) payload->getSize (format));
            targets.push_back ({ format, format, folder.getChildFile (format), {}, true });
        }

        const auto report = install (*payload, targets);

        for (const auto& path : report.installed)
            std::printf ("installed: %s\n", path.toRawUTF8());

        if (! report.succeeded)
            std::printf ("failed: %s\n", report.error.replace ("\n", " ").toRawUTF8());

        return report.succeeded ? 0 : 1;
    }

    const auto pictures = argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]) : juce::File();
    const auto scratch = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getChildFile ("dalune-installer-harness-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt()));
    scratch.createDirectory();

    const auto targetsIn = [] (const juce::File& root)
    {
        return std::vector<InstallTarget> { { "AU",   "Audio Unit", root.getChildFile ("Components"), "~/Library/Audio/Plug-Ins/Components", true },
                                            { "VST3", "VST3",       root.getChildFile ("VST3"),       "~/Library/Audio/Plug-Ins/VST3",       true } };
    };

    //==============================================================================
    std::printf ("what the installer carries\n");
    {
        juce::String error;
        auto payload = openZip (makeZip (pluginEntries()), error);
        check (payload != nullptr && error.isEmpty(), "a zip with one plug-in per format is accepted");

        if (payload == nullptr)
            return 1;

        check (payload->getFormats() == juce::StringArray { "VST3", "AU" }
                && payload->getBundleName ("VST3") == "D.A LUNE.vst3" && payload->getBundleName ("AU") == "D.A LUNE.component",
               "it lists the formats and what each plug-in is called");
        check (payload->getSize ("VST3") == (juce::int64) binaryText.length() + 22 && payload->getBundleName ("AAX").isEmpty(),
               "it knows how large a plug-in is, and that a format it does not carry is not there");

        juce::MemoryBlock garbage ("this is not a zip at all", 24);
        const bool refused = openZip (garbage, error) == nullptr && error.containsIgnoreCase ("damaged");
        check (refused, "something that is not a zip is refused  (\"" + error.replace ("\n", " ") + "\")");

        check (openZip (makeZip ({ { "VST3/../../outside.txt", "x" } }), error) == nullptr, "a zip with a path that leads out of the target folder is refused");
        check (openZip (makeZip ({ { "VST3/A.vst3/x", "x" }, { "VST3/B.vst3/x", "x" } }), error) == nullptr, "a zip with two plug-ins in one format is refused");
    }

    //==============================================================================
    std::printf ("installing\n");
    {
        juce::String error;
        auto payload = openZip (makeZip (pluginEntries()), error);
        const auto root = scratch.getChildFile ("first");
        auto targets = targetsIn (root);

        std::vector<double> progress;
        auto report = install (*payload, targets, [&] (double p) { progress.push_back (p); });

        const auto vst3 = root.getChildFile ("VST3/D.A LUNE.vst3");
        const auto au   = root.getChildFile ("Components/D.A LUNE.component");

        check (report.succeeded && report.error.isEmpty()
                && report.installed == juce::StringArray { au.getFullPathName(), vst3.getFullPathName() },
               "both plug-ins are reported as installed, with their full paths");
        check (vst3.getChildFile ("Contents/x86_64-win/D.A LUNE.vst3").loadFileAsString() == binaryText
                && vst3.getChildFile ("Contents/Resources/moduleinfo.json").loadFileAsString() == "{ \"Name\": \"D.A LUNE\" }"
                && au.getChildFile ("Contents/MacOS/D.A LUNE").loadFileAsString() == binaryText
                && au.getChildFile ("Contents/Resources/D.A LUNE.rsrc").existsAsFile(),
               "every file arrives, byte for byte (an empty one too)");
        check (listing (root) == juce::StringArray { "Components/", "Components/D.A LUNE.component/", "Components/D.A LUNE.component/Contents/",
                                                     "Components/D.A LUNE.component/Contents/Info.plist", "Components/D.A LUNE.component/Contents/MacOS/",
                                                     "Components/D.A LUNE.component/Contents/MacOS/D.A LUNE", "Components/D.A LUNE.component/Contents/Resources/",
                                                     "Components/D.A LUNE.component/Contents/Resources/D.A LUNE.rsrc",
                                                     "VST3/", "VST3/D.A LUNE.vst3/", "VST3/D.A LUNE.vst3/Contents/", "VST3/D.A LUNE.vst3/Contents/Resources/",
                                                     "VST3/D.A LUNE.vst3/Contents/Resources/moduleinfo.json", "VST3/D.A LUNE.vst3/Contents/x86_64-win/",
                                                     "VST3/D.A LUNE.vst3/Contents/x86_64-win/D.A LUNE.vst3" },
               "and nothing else is left in the folders");
        check (::access (au.getChildFile ("Contents/MacOS/D.A LUNE").getFullPathName().toRawUTF8(), X_OK) == 0,
               "the Audio Unit's binary is marked as a program");
        check (! progress.empty() && std::is_sorted (progress.begin(), progress.end()) && progress.front() >= 0.0 && progress.back() == 1.0,
               "progress only ever goes up, and ends at 100 %  (" + juce::String ((int) progress.size()) + " reports)");

        // an older version is already there
        vst3.getChildFile ("Contents/x86_64-win/D.A LUNE.vst3").replaceWithText ("the old code");
        vst3.getChildFile ("Contents/Resources/left-over-from-version-one.txt").replaceWithText ("x");
        root.getChildFile ("VST3/Some Other Plug-in.vst3/Contents").createDirectory();
        report = install (*payload, targets);
        check (report.succeeded && vst3.getChildFile ("Contents/x86_64-win/D.A LUNE.vst3").loadFileAsString() == binaryText
                && ! vst3.getChildFile ("Contents/Resources/left-over-from-version-one.txt").exists(),
               "an older copy is replaced as a whole: nothing of it stays behind");
        check (root.getChildFile ("VST3").getNumberOfChildFiles (juce::File::findFilesAndDirectories) == 2
                && root.getChildFile ("VST3/Some Other Plug-in.vst3/Contents").isDirectory(),
               "other plug-ins in the folder are not touched, and no working copies remain");

        // what an interrupted installation would leave behind
        root.getChildFile ("VST3/D.A LUNE.vst3.installing/Contents/half").create();
        root.getChildFile ("VST3/D.A LUNE.vst3.previous/Contents/old").create();
        report = install (*payload, targets);
        check (report.succeeded && root.getChildFile ("VST3").getNumberOfChildFiles (juce::File::findFilesAndDirectories) == 2,
               "the left-overs of an interrupted installation are cleared away");

        // only what is ticked
        const auto second = scratch.getChildFile ("second");
        auto onlyVst3 = targetsIn (second);
        onlyVst3[0].selected = false;
        report = install (*payload, onlyVst3);
        check (report.succeeded && report.installed.size() == 1 && second.getChildFile ("VST3/D.A LUNE.vst3").isDirectory()
                && ! second.getChildFile ("Components").exists(),
               "a format that is not ticked is not installed, and its folder is not even created");

        onlyVst3[1].selected = false;
        report = install (*payload, onlyVst3);
        check (! report.succeeded && report.error.isNotEmpty(), "with nothing ticked there is nothing to install  (\"" + report.error + "\")");
    }

    std::printf ("when it cannot be done\n");
    {
        juce::String error;
        auto payload = openZip (makeZip (pluginEntries()), error);

        // the target folder cannot be created: something else has its name
        const auto blocked = scratch.getChildFile ("blocked");
        blocked.createDirectory();
        blocked.getChildFile ("VST3").replaceWithText ("a file where the folder should be");
        auto targets = targetsIn (blocked);
        targets[0].selected = false;
        auto report = install (*payload, targets);
        check (! report.succeeded && report.error.contains (blocked.getChildFile ("VST3").getFullPathName()) && report.installed.isEmpty(),
               "a folder that cannot be created is named in the message  (\"" + report.error.replace ("\n", " ") + "\")");

        // the folder cannot be written to (as the VST3 folder on Windows without administrator rights)
        if (::geteuid() != 0)
        {
            const auto locked = scratch.getChildFile ("locked");
            const auto folder = locked.getChildFile ("VST3");
            folder.getChildFile ("D.A LUNE.vst3/Contents").createDirectory();
            folder.getChildFile ("D.A LUNE.vst3/Contents/version-one").replaceWithText ("still here");
            ::chmod (folder.getFullPathName().toRawUTF8(), 0555);

            targets = targetsIn (locked);
            targets[0].selected = false;
            report = install (*payload, targets);
            ::chmod (folder.getFullPathName().toRawUTF8(), 0755);

            check (! report.succeeded && report.error.isNotEmpty()
                    && folder.getChildFile ("D.A LUNE.vst3/Contents/version-one").loadFileAsString() == "still here"
                    && folder.getNumberOfChildFiles (juce::File::findFilesAndDirectories) == 1,
                   "without the right to write there it fails, and the copy that was there stays whole  (\"" + report.error.replace ("\n", " ") + "\")");
        }

        // a payload without the format that was asked for
        auto onlyAu = openZip (makeZip ({ { "AU/D.A LUNE.component/Contents/Info.plist", "<plist/>" } }), error);
        targets = targetsIn (scratch.getChildFile ("partial"));
        report = install (*onlyAu, targets);
        check (! report.succeeded && report.error.contains ("VST3") && report.installed.size() == 1,
               "a format the installer does not carry is reported, after what could be installed was");
    }

    //==============================================================================
    std::printf ("the pages\n");
    {
        const Assets assets;
        check (assets.getPortrait (1000.0f).isValid() && assets.getPortrait (1000.0f).getWidth() > assets.getPortrait (200.0f).getWidth()
                && assets.getPortrait (200.0f).isValid(),
               "the picture is built in, with a smaller copy for small windows");
        check (assets.text (20.0f).getTypefaceName() == "Red Hat Display" && assets.brand (20.0f, 0.0f).getTypefaceName() == "Orbitron",
               "so are the typefaces  (" + assets.text (20.0f).getTypefaceName() + ", " + assets.brand (20.0f, 0.0f).getTypefaceName() + ")");

        const auto snapshot = [&] (juce::Component& c, const juce::String& name, float scale = 2.0f)
        {
            if (pictures != juce::File())
            {
                pictures.createDirectory();
                writePng (c.createComponentSnapshot (c.getLocalBounds(), true, scale), pictures.getChildFile (name + ".png"));
            }
        };

        juce::String error;
        const auto root = scratch.getChildFile ("pages");
        auto targets = targetsIn (root);
        int doneCalls = 0;
        std::vector<InstallerView::Page> pagesSeen;

        InstallerShell shell (std::make_unique<InstallerView> (assets, openZip (makeZip (pluginEntries()), error), targets, juce::String()));
        auto& view = shell.getView();
        view.onDone = [&] { ++doneCalls; };
        view.onPageChanged = [&] (InstallerView::Page p) { pagesSeen.push_back (p); };

        auto* next     = dynamic_cast<juce::Button*> (view.findChildWithID ("next"));
        auto* cancel   = dynamic_cast<juce::Button*> (view.findChildWithID ("cancel"));
        auto* close    = dynamic_cast<juce::Button*> (view.findChildWithID ("close"));
        auto* minimise = dynamic_cast<juce::Button*> (view.findChildWithID ("minimise"));
        auto* tickAu   = dynamic_cast<juce::Button*> (view.findChildWithID ("target-AU"));
        auto* tickVst3 = dynamic_cast<juce::Button*> (view.findChildWithID ("target-VST3"));

        check (next != nullptr && cancel != nullptr && close != nullptr && minimise != nullptr && tickAu != nullptr && tickVst3 != nullptr,
               "there are Next and Cancel, minimise and close, and a tick box per format");

        if (next == nullptr || cancel == nullptr || close == nullptr || minimise == nullptr || tickAu == nullptr || tickVst3 == nullptr)
            return 1;

        const auto click = [] (juce::Button& b) { b.triggerClick(); letRun (20); };

        check (view.getPage() == InstallerView::Page::welcome && next->isVisible() && cancel->isVisible()
                && next->getButtonText() == "Next" && cancel->getButtonText() == "Cancel" && ! tickAu->isVisible(),
               "it opens on the welcome page, with Next and Cancel");
        snapshot (shell, "1-welcome");

        click (*next);
        check (view.getPage() == InstallerView::Page::location && tickAu->isVisible() && tickVst3->isVisible()
                && tickAu->getToggleState() && tickVst3->getToggleState() && next->isEnabled() && cancel->isVisible(),
               "Next leads to the install location, where every folder is ticked to begin with");
        snapshot (shell, "2-location");

        click (*tickAu);
        click (*tickVst3);
        check (! tickAu->getToggleState() && ! tickVst3->getToggleState() && ! next->isEnabled(), "with every tick removed, Next is switched off");
        snapshot (shell, "2b-location-nothing-ticked");
        click (*next);
        check (view.getPage() == InstallerView::Page::location && ! root.exists(), "and clicking it all the same does nothing");

        click (*tickVst3);
        check (tickVst3->getToggleState() && next->isEnabled() && view.getTargets()[1].selected && ! view.getTargets()[0].selected,
               "ticking one again switches Next back on");
        click (*tickAu);

        click (*next);
        check (view.getPage() == InstallerView::Page::installing && ! next->isVisible() && ! cancel->isVisible() && ! close->isEnabled(),
               "the second Next starts the installation; while it runs there is nothing to click");

        bool pictured = false;
        double shownBefore = -1.0;
        bool monotonic = true;
        const auto started = juce::Time::getMillisecondCounter();

        while (view.getPage() == InstallerView::Page::installing && juce::Time::getMillisecondCounter() - started < 20000)
        {
            view.dismiss();                 // what the close button and Escape do (somebody impatient)
            letRun (15);
            monotonic = monotonic && view.getShownProgress() >= shownBefore;
            shownBefore = view.getShownProgress();

            if (! pictured && view.getShownProgress() > 0.55)
            {
                snapshot (shell, "3-installing");
                pictured = true;
            }
        }

        const auto took = juce::Time::getMillisecondCounter() - started;
        check (view.getPage() == InstallerView::Page::finished && view.getReport().succeeded && doneCalls == 0,
               "it ends on the finished page; asking it to close meanwhile did not interrupt it");
        check (monotonic && took >= 700 && took < 5000, "the bar fills steadily and takes long enough to be seen  (" + juce::String ((int) took) + " ms)");
        check (root.getChildFile ("VST3/D.A LUNE.vst3/Contents/x86_64-win/D.A LUNE.vst3").loadFileAsString() == binaryText
                && root.getChildFile ("Components/D.A LUNE.component/Contents/MacOS/D.A LUNE").loadFileAsString() == binaryText,
               "and the plug-ins are where the page said they would go");
        check (next->isVisible() && next->getButtonText() == "Finish" && ! cancel->isVisible() && close->isEnabled(),
               "the finished page has one button: Finish");
        snapshot (shell, "4-finished");

        click (*next);
        check (doneCalls == 1, "Finish closes the installer");
        check (pagesSeen == std::vector<InstallerView::Page> { InstallerView::Page::location, InstallerView::Page::installing, InstallerView::Page::finished },
               "the pages came in order: location, installing, finished");

        // other sizes of the window
        shell.setSize (686, 270);
        snapshot (shell, "5-smallest", 1.0f);
        shell.setSize (1470, 579);
        snapshot (shell, "6-larger", 1.0f);
        shell.setSize (InstallerView::designWidth, InstallerView::designHeight);
        snapshot (shell, "7-at-100-percent", 1.0f);

        // Cancel and the close button
        {
            int done = 0;
            InstallerView other (assets, openZip (makeZip (pluginEntries()), error), targetsIn (scratch.getChildFile ("cancelled")), juce::String());
            other.onDone = [&] { ++done; };
            click (*dynamic_cast<juce::Button*> (other.findChildWithID ("cancel")));
            check (done == 1 && ! scratch.getChildFile ("cancelled").exists(), "Cancel on the welcome page closes the installer and installs nothing");
            click (*dynamic_cast<juce::Button*> (other.findChildWithID ("next")));
            click (*dynamic_cast<juce::Button*> (other.findChildWithID ("close")));
            check (done == 2 && ! scratch.getChildFile ("cancelled").exists(), "so does the close button on the location page");
        }

        // the page as it is on Windows: one folder
        {
            std::vector<InstallTarget> windows { { "VST3", "VST3", scratch.getChildFile ("windows/VST3"), "C:\\Program Files\\Common Files\\VST3", true } };
            InstallerShell windowsShell (std::make_unique<InstallerView> (assets, openZip (makeZip (pluginEntries()), error), windows, juce::String()));
            windowsShell.getView().advance();
            check (windowsShell.getView().findChildWithID ("target-VST3") != nullptr && windowsShell.getView().findChildWithID ("target-AU") == nullptr,
                   "with one target there is one tick box");
            snapshot (windowsShell, "2-location-windows");
        }

        // when the installation fails
        {
            const auto blocked = scratch.getChildFile ("page-blocked");
            blocked.createDirectory();
            blocked.getChildFile ("VST3").replaceWithText ("a file where the folder should be");
            std::vector<InstallTarget> one { { "VST3", "VST3", blocked.getChildFile ("VST3"), "C:\\Program Files\\Common Files\\VST3", true } };

            int done = 0;
            InstallerShell failing (std::make_unique<InstallerView> (assets, openZip (makeZip (pluginEntries()), error), one, juce::String()));
            failing.getView().onDone = [&] { ++done; };
            failing.getView().advance();
            failing.getView().advance();

            for (int i = 0; i < 200 && failing.getView().getPage() == InstallerView::Page::installing; ++i)
                letRun (15);

            auto* button = dynamic_cast<juce::Button*> (failing.getView().findChildWithID ("next"));
            check (failing.getView().getPage() == InstallerView::Page::failed && ! failing.getView().getReport().succeeded
                    && button->getButtonText() == "Close" && button->isVisible(),
                   "a failed installation ends on a page that says so, with a Close button");
            snapshot (failing, "4-failed");
            click (*button);
            check (done == 1, "which closes the installer");
        }

        // the window around it (not put on a screen here: there is none)
        {
            int done = 0;
            auto windowView = std::make_unique<InstallerView> (assets, openZip (makeZip (pluginEntries()), error),
                                                               targetsIn (scratch.getChildFile ("window")), juce::String());
            windowView->onDone = [&] { ++done; };
            InstallerWindow window ("D.A LUNE Installer", std::make_unique<InstallerShell> (std::move (windowView)));
            window.setVisible (true);

            const int w = window.getWidth(), h = window.getHeight();
            const auto at = [&] (int x, int y) { return window.getComponentAt (x, y); };
            const auto isEdge = [&] (int x, int y) { return dynamic_cast<juce::ResizableBorderComponent*> (at (x, y)) != nullptr; };
            const auto idAt = [&] (juce::Component* c) { return at (window.getLocalArea (c, c->getLocalBounds()).getCentreX(),
                                                                     window.getLocalArea (c, c->getLocalBounds()).getCentreY()); };

            auto* windowNext  = window.getView().findChildWithID ("next");
            auto* windowClose = window.getView().findChildWithID ("close");

            check (w == InstallerView::designWidth && h == InstallerView::designHeight && window.getShell().getBounds() == window.getLocalBounds(),
                   "the window is made for the view's size, and the view fills all of it");

            const int flags = window.getDesktopWindowStyleFlags();
            check ((flags & juce::ComponentPeer::windowHasTitleBar) == 0 && (flags & juce::ComponentPeer::windowIsResizable) != 0
                    && (flags & juce::ComponentPeer::windowHasMinimiseButton) != 0 && (flags & juce::ComponentPeer::windowHasCloseButton) != 0,
                   "it asks the system for no title bar, and to be resizable, minimisable and closable");

            check (isEdge (2, h / 2) && isEdge (w - 3, h / 2) && isEdge (w / 2, 2) && isEdge (w / 2, h - 3)
                    && isEdge (2, 2) && isEdge (w - 3, 2) && isEdge (2, h - 3) && isEdge (w - 3, h - 3),
                   "the mouse finds an edge to resize it by on all four sides and at all four corners");
            check (at (w / 4, h / 2) == &window && at (w / 2 + 40, 120) == &window && ! isEdge (InstallerWindow::edgeThickness + 2, h / 2),
                   "everywhere else that is not a button, the mouse takes hold of the window itself (to move it)");
            check (windowNext != nullptr && windowClose != nullptr && idAt (windowNext) == windowNext && idAt (windowClose) == windowClose,
                   "and on a button, the button");

            // other sizes: as the mouse would ask for them
            const auto proportional = [] (juce::Rectangle<int> b)
            {
                return std::abs ((double) b.getWidth() / (double) b.getHeight() - (double) InstallerView::designWidth / (double) InstallerView::designHeight) < 0.02;
            };

            auto* limits = window.getConstrainer();
            const auto asked = [&] (int width, int height)
            {
                juce::Rectangle<int> b (0, 0, width, height);
                limits->checkBounds (b, window.getBounds(), { 0, 0, 10000, 10000 }, false, false, true, true);
                return b;
            };

            const auto wide = asked (1400, 300), tiny = asked (100, 100), huge = asked (5000, 5000);
            check (limits != nullptr && proportional (wide) && proportional (tiny) && proportional (huge) && wide.getWidth() > w,
                   "resized, it keeps its proportions  (asked for 1400 x 300, it becomes " + juce::String (wide.getWidth()) + " x " + juce::String (wide.getHeight()) + ")");
            check (tiny.getWidth() == 686 && tiny.getHeight() == 270 && huge.getWidth() == 2450 && huge.getHeight() == 965,
                   "from 70 % to 250 % of its size  (" + juce::String (tiny.getWidth()) + " x " + juce::String (tiny.getHeight())
                       + " to " + juce::String (huge.getWidth()) + " x " + juce::String (huge.getHeight()) + ")");

            window.setSize (1470, 579);
            check (window.getShell().getBounds() == window.getLocalBounds() && isEdge (1468, 577) && idAt (windowNext) == windowNext,
                   "at another size the view still fills it, and edges and buttons are where they are drawn");
            snapshot (window, "8-window-at-150-percent", 1.0f);

            // the keyboard
            check (window.keyPressed (juce::KeyPress (juce::KeyPress::returnKey)) && window.getView().getPage() == InstallerView::Page::location,
                   "Return does what the main button does");
            check (window.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)) && done == 1 && ! scratch.getChildFile ("window").exists(),
                   "Escape closes the installer");
            window.userTriedToCloseWindow();
            check (done == 2, "and so does closing the window from outside (the taskbar, the Dock)");
        }

        // an installer without its payload
        {
            InstallerShell broken (std::make_unique<InstallerView> (assets, nullptr, targets,
                                                                     "This installer is incomplete: the plug-in files are missing from it.\nPlease download it again."));
            check (broken.getView().getPage() == InstallerView::Page::failed
                    && dynamic_cast<juce::Button*> (broken.getView().findChildWithID ("next"))->getButtonText() == "Close",
                   "an installer whose plug-in files are missing says so straight away");
            snapshot (broken, "0-no-payload");
        }
    }

    scratch.deleteRecursively();

    std::printf ("\n%s\n", failures == 0 ? "ALL PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
