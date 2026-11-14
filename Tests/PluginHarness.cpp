// Runs the real processor and editor without a host or a display (Linux, local JUCE build).
// It checks the behaviour that hosts rely on, and writes a snapshot of the editor so that the
// GUI can be looked at exactly as JUCE paints it.
//
//   Tests/run-harness.sh [snapshot.png]
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <cstdio>

namespace
{
int failures = 0;

void check (bool ok, const juce::String& what)
{
    std::printf ("  [%s] %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8());
    if (! ok) ++failures;
}

float plain (CloudveilAudioProcessor& p, const char* id)
{
    return p.apvts.getRawParameterValue (id)->load();
}

void setPlain (CloudveilAudioProcessor& p, const char* id, float value)
{
    auto* param = p.apvts.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (value));
}

juce::String shownText (CloudveilAudioProcessor& p, const char* id)
{
    auto* param = p.apvts.getParameter (id);
    return param->getText (param->getValue(), 0);
}

double rms (const juce::AudioBuffer<float>& b)
{
    double sum = 0.0;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            sum += (double) b.getSample (ch, i) * b.getSample (ch, i);
    return std::sqrt (sum / juce::jmax (1, b.getNumChannels() * b.getNumSamples()));
}

bool allFinite (const juce::AudioBuffer<float>& b)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (b.getSample (ch, i)))
                return false;
    return true;
}

void fillNoise (juce::AudioBuffer<float>& b, juce::Random& r, float gain)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            b.setSample (ch, i, gain * (r.nextFloat() * 2.0f - 1.0f));
}

// Runs `seconds` of noise (or silence) through the plug-in and returns the level of the last block
double run (CloudveilAudioProcessor& p, double seconds, bool noise, juce::Random& r, bool bypassed = false, int channels = 2)
{
    juce::AudioBuffer<float> buffer (channels, 512);
    juce::MidiBuffer midi;
    double last = 0.0;
    bool finite = true;

    for (int n = (int) (seconds * p.getSampleRate() / 512.0); --n >= 0;)
    {
        if (noise) fillNoise (buffer, r, 0.4f); else buffer.clear();
        if (bypassed) p.processBlockBypassed (buffer, midi); else p.processBlock (buffer, midi);
        finite = finite && allFinite (buffer);
        last = rms (buffer);
    }

    if (! finite) { std::printf ("  [FAIL] non-finite output\n"); ++failures; }
    return last;
}
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::Random random (42);

    {
        CloudveilAudioProcessor p;
        p.setPlayConfigDetails (2, 2, 48000.0, 512);

        std::printf ("processing before prepareToPlay()\n");
        run (p, 0.1, true, random);
        check (true, "does not crash");

        p.prepareToPlay (48000.0, 512);

        std::printf ("parameter text (what hosts and the editor display)\n");
        check (shownText (p, "decay") == "4.80", "decay shows \"4.80\"  -> \"" + shownText (p, "decay") + "\"");
        check (shownText (p, "mix") == "38", "mix shows \"38\"  -> \"" + shownText (p, "mix") + "\"");
        setPlain (p, "mix", 38.5f);
        check (shownText (p, "mix") == "38.5", "mix 38.5 shows \"38.5\"  -> \"" + shownText (p, "mix") + "\"");
        check (std::abs (p.apvts.getParameter ("size")->getValueForText ("110 %") - p.apvts.getParameter ("size")->convertTo0to1 (110.0f)) < 1.0e-6f, "typing \"110 %\" is understood");

        std::printf ("programs\n");
        check (p.getNumPrograms() == 50, "fifty factory programs");
        {
            juce::StringArray names, categories;
            bool valuesInRange = true, categoriesTogether = true;

            for (const auto& preset : CloudveilAudioProcessor::getPresets())
            {
                names.addIfNotAlreadyThere (preset.name);

                if (categories.isEmpty() || categories[categories.size() - 1] != preset.category)
                {
                    categoriesTogether = categoriesTogether && ! categories.contains (preset.category);
                    categories.add (preset.category);
                }

                const float values[] { preset.decay, preset.size, preset.predelay, preset.mix, preset.shimmer, preset.drift, preset.warmth, preset.width };
                const char* ids[]    { "decay", "size", "predelay", "mix", "shimmer", "drift", "warmth", "width" };

                for (int i = 0; i < 8; ++i)
                {
                    // inside the control's range, and on one of its steps (so that the control shows the preset's number)
                    auto* param = p.apvts.getParameter (ids[i]);
                    const auto range = param->getNormalisableRange();
                    valuesInRange = valuesInRange && values[i] >= range.start && values[i] <= range.end
                                     && std::abs (param->convertFrom0to1 (param->convertTo0to1 (values[i])) - values[i]) < 1.0e-4f;
                }
            }

            check (names.size() == 50, "each with a name of its own");
            check (valuesInRange, "all their values are values the controls can show");
            check (categoriesTogether && categories.size() == 7, "sorted into " + juce::String (categories.size()) + " categories: " + categories.joinIntoString (", "));
            check (p.getProgramName (0) == "Velvet Cloud" && p.getProgramName (1) == "Astral Shimmer" && p.getProgramName (2) == "Midnight Cathedral"
                    && p.getProgramName (4) == "Soft Halo" && p.getProgramName (5) == "Infinite Horizon", "the original six are still the first six");
        }
        check (p.isCurrentProgramModified(), "a moved control marks the program as modified");
        p.setCurrentProgram (3);
        check (p.getCurrentProgram() == 3 && p.getProgramName (3) == "Tape Drift", "program 3 is Tape Drift");
        check (std::abs (plain (p, "decay") - 3.6f) < 1.0e-4f && std::abs (plain (p, "drift") - 85.0f) < 1.0e-4f && std::abs (plain (p, "width") - 105.0f) < 1.0e-4f, "its values are loaded");
        check (! p.isCurrentProgramModified(), "a freshly loaded program is not modified");
        p.setCurrentProgram (99);
        p.setCurrentProgram (-1);
        check (p.getCurrentProgram() == 3, "out-of-range program numbers are ignored");

        std::printf ("state\n");
        setPlain (p, "decay", 7.25f);
        setPlain (p, "freeze", 1.0f);
        juce::MemoryBlock saved;
        p.getStateInformation (saved);

        CloudveilAudioProcessor q;
        q.setStateInformation (saved.getData(), (int) saved.getSize());
        check (std::abs (plain (q, "decay") - 7.25f) < 1.0e-4f && plain (q, "freeze") > 0.5f && std::abs (plain (q, "drift") - 85.0f) < 1.0e-4f, "values survive save / load");
        check (q.getCurrentProgram() == 3 && q.isCurrentProgramModified(), "so does the program, and that it was edited");
        q.setStateInformation ("garbage", 7);
        q.setStateInformation (nullptr, 0);
        check (std::abs (plain (q, "decay") - 7.25f) < 1.0e-4f, "garbage state is ignored");

        std::printf ("what the host is told about the controls\n");
        {
            // A host may hand a control any value between 0 and 1, and it reads the value back
            // afterwards. Recalling a saved state has to bring every control back to exactly
            // what it reported when the state was saved, and saving again has to give the same
            // bytes. (pluginval tests this with one random value per control.)
            CloudveilAudioProcessor h;
            juce::MemoryBlock reference;
            h.getStateInformation (reference);

            juce::StringArray notRestored, stateChanged, notSnapped;

            for (auto* param : h.getParameters())
            {
                const float reported = param->getValue();

                for (int step = 0; step <= 200; ++step)
                {
                    const float given = step <= 100 ? (float) step / 100.0f : random.nextFloat();
                    param->setValueNotifyingHost (given);

                    if (param->isBoolean() && param->getValue() != 0.0f && param->getValue() != 1.0f)
                        notSnapped.addIfNotAlreadyThere (param->getName (64) + " given " + juce::String (given, 2) + " reports " + juce::String (param->getValue(), 2));

                    h.setStateInformation (reference.getData(), (int) reference.getSize());

                    if (std::abs (param->getValue() - reported) > 1.0e-6f)
                        notRestored.addIfNotAlreadyThere (param->getName (64) + " given " + juce::String (given, 2) + " comes back as " + juce::String (param->getValue(), 4) + " instead of " + juce::String (reported, 4));

                    juce::MemoryBlock again;
                    h.getStateInformation (again);

                    if (again != reference)
                        stateChanged.addIfNotAlreadyThere (param->getName (64) + " given " + juce::String (given, 2));
                }
            }

            // The same for a program change: VST3 hosts see the program as one more control
            const auto reportedValues = [&h]
            {
                juce::Array<float> values;
                for (auto* param : h.getParameters())
                    values.add (param->getValue());
                return values;
            };

            const auto reportedBefore = reportedValues();
            const int  programBefore  = h.getCurrentProgram();

            for (int program = 0; program < h.getNumPrograms(); ++program)
            {
                h.setCurrentProgram (program);
                h.setStateInformation (reference.getData(), (int) reference.getSize());

                if (h.getCurrentProgram() != programBefore || reportedValues() != reportedBefore)
                    notRestored.add ("after program " + juce::String (program));

                juce::MemoryBlock again;
                h.getStateInformation (again);

                if (again != reference)
                    stateChanged.add ("after program " + juce::String (program));
            }

            const auto list = [] (const juce::StringArray& items)
            {
                return items.isEmpty() ? juce::String()
                                       : "  -> " + juce::String (items.size()) + " cases, e.g. " + items[items.size() / 2];
            };

            check (notSnapped.isEmpty(), "a switch always reports 0 or 1" + list (notSnapped));
            check (notRestored.isEmpty(), "recalling a state brings every control back, whatever it was set to in between" + list (notRestored));
            check (stateChanged.isEmpty(), "saving again gives byte for byte the state that was loaded" + list (stateChanged));
        }

        std::printf ("tail length\n");
        check (std::isinf (p.getTailLengthSeconds()), "infinite while frozen");
        setPlain (p, "freeze", 0.0f);
        setPlain (p, "predelay", 200.0f);
        setPlain (p, "decay", 10.0f);
        const double plainTail = p.getTailLengthSeconds();
        setPlain (p, "shimmer", 100.0f);
        const double shimmerTail = p.getTailLengthSeconds();
        setPlain (p, "shimmer", 10.0f);
        check (plainTail > 16.0 && plainTail < 17.0 && shimmerTail > plainTail + 4.0,
               "otherwise pre-delay plus 1.5 to 2.1 times the decay, more with more shimmer  -> " + juce::String (plainTail, 2) + " s, " + juce::String (shimmerTail, 2) + " s with Shimmer at 100 %");

        std::printf ("bypass and reset\n");
        p.setCurrentProgram (5);       // Infinite Horizon: 17.5 s
        setPlain (p, "mix", 100.0f);
        const double ringing = run (p, 2.0, true, random);
        run (p, 0.05, false, random, true);
        const double afterBypass = run (p, 0.3, false, random);
        check (ringing > 0.01 && afterBypass < 1.0e-6, "the old tail does not come back after a bypass  (" + juce::String (ringing, 4) + " -> " + juce::String (afterBypass, 8) + ")");

        run (p, 2.0, true, random);
        p.reset();
        const double afterReset = run (p, 0.3, false, random);
        check (afterReset < 1.0e-6, "reset() stops the tail  (" + juce::String (afterReset, 8) + ")");

        run (p, 2.0, true, random);
        setPlain (p, "freeze", 1.0f);
        run (p, 1.0, false, random);
        const double held = run (p, 0.2, false, random);
        run (p, 0.2, false, random, true);
        const double heldAfterBypass = run (p, 0.2, false, random);
        check (held > 0.01 && std::abs (heldAfterBypass / held - 1.0) < 0.2, "a frozen cloud survives a bypass  (" + juce::String (held, 4) + " -> " + juce::String (heldAfterBypass, 4) + ")");
        setPlain (p, "freeze", 0.0f);

        std::printf ("channel layouts\n");
        for (auto [in, out] : { std::pair { 1, 1 }, std::pair { 1, 2 }, std::pair { 2, 2 } })
        {
            CloudveilAudioProcessor m;
            juce::AudioProcessor::BusesLayout layout;
            layout.inputBuses.add (in == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
            layout.outputBuses.add (out == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
            const bool supported = m.setBusesLayout (layout);
            m.setRateAndBufferSizeDetails (44100.0, 512);
            m.prepareToPlay (44100.0, 512);
            const double level = supported ? run (m, 1.0, true, random, false, out) : 0.0;
            check (supported && level > 0.01, juce::String (in) + " in / " + juce::String (out) + " out");
        }

        std::printf ("editor\n");
        p.setCurrentProgram (0);
        std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditorIfNeeded());
        check (editor != nullptr, "editor opens");

        if (editor != nullptr)
        {
            auto* content    = editor->findChildWithID ("content");
            auto* name       = content != nullptr ? dynamic_cast<PresetNameButton*> (content->findChildWithID ("presetName")) : nullptr;
            auto* next       = content != nullptr ? dynamic_cast<juce::Button*> (content->findChildWithID ("nextPreset")) : nullptr;
            auto* previous   = content != nullptr ? dynamic_cast<juce::Button*> (content->findChildWithID ("previousPreset")) : nullptr;
            auto* visualiser = content != nullptr ? dynamic_cast<AuroraVisualiser*> (content->findChildWithID ("visualiser")) : nullptr;
            auto* cloudveilEditor = dynamic_cast<CloudveilAudioProcessorEditor*> (editor.get());

            check (name != nullptr && next != nullptr && previous != nullptr && visualiser != nullptr && cloudveilEditor != nullptr,
                   "it has a preset name, an arrow either side of it, and the aurora");

            if (name == nullptr || next == nullptr || previous == nullptr || visualiser == nullptr || cloudveilEditor == nullptr)
                return 1;

            // Lets the message loop run for a moment, the editor's 30 Hz timer included
            const auto letRun = [] (int milliseconds)
            {
                juce::MessageManager::getInstance()->runDispatchLoopUntil (milliseconds);
            };

            std::printf ("presets in the editor\n");
            check (name->getPresetName() == "Velvet Cloud" && ! name->isMarkedAsEdited(), "the name of the current preset is shown");

            // The reported bug: every so often a preset that was picked did not load. The old
            // menu reported a pick a moment later, and if the editor's timer ran in between it
            // put the previous preset back. So: click, let the timer run first, then look.
            int lost = 0;
            for (int attempt = 0; attempt < 12; ++attempt)
            {
                const int wanted = (p.getCurrentProgram() + 1) % p.getNumPrograms();
                juce::Thread::sleep (40);                           // the editor's timer is due
                next->triggerClick();
                juce::Timer::callPendingTimersSynchronously();      // ...and gets in first
                letRun (30);
                if (p.getCurrentProgram() != wanted || name->getPresetName() != p.getProgramName (wanted))
                    ++lost;
            }
            check (lost == 0 && p.getCurrentProgram() == 12, "twelve clicks on the right arrow load the next twelve presets, none lost  (" + juce::String (lost) + " lost)");

            for (int i = 0; i < 13; ++i)
            {
                previous->triggerClick();
                letRun (5);
            }
            check (p.getCurrentProgram() == 49 && name->getPresetName() == p.getProgramName (49), "the left arrow goes back, and from the first preset round to the last");

            next->triggerClick();
            letRun (5);
            check (p.getCurrentProgram() == 0, "and the right arrow from the last one round to the first");

            lost = 0;
            for (int index : { 7, 33, 2, 49, 21, 0 })
            {
                juce::Thread::sleep (40);
                cloudveilEditor->selectPreset (index);              // what picking it in the list does
                juce::Timer::callPendingTimersSynchronously();
                letRun (30);
                if (p.getCurrentProgram() != index || name->getPresetName() != p.getProgramName (index))
                    ++lost;
            }
            check (lost == 0, "presets picked from the list load, whichever and in whatever order");

            {
                // The list itself: every preset once, under its category, the loaded one ticked
                p.setCurrentProgram (26);
                const auto menu = cloudveilEditor->createPresetMenu();
                int groups = 0, items = 0, ticked = 0, tickedGroups = 0;
                bool idsMatch = true;
                juce::String tickedName, tickedGroup;

                for (juce::PopupMenu::MenuItemIterator group (menu); group.next();)
                {
                    ++groups;
                    tickedGroups += group.getItem().isTicked ? 1 : 0;
                    if (group.getItem().isTicked) tickedGroup = group.getItem().text;

                    if (group.getItem().subMenu != nullptr)
                        for (juce::PopupMenu::MenuItemIterator item (*group.getItem().subMenu); item.next();)
                        {
                            ++items;
                            idsMatch = idsMatch && item.getItem().itemID == items && item.getItem().text == p.getProgramName (items - 1)
                                                && group.getItem().text == CloudveilAudioProcessor::getPresets()[(size_t) (items - 1)].category;
                            if (item.getItem().isTicked) { ++ticked; tickedName = item.getItem().text; }
                        }
                }

                check (groups == 7 && items == 50 && idsMatch, "the list has every preset once, under its category  (" + juce::String (groups) + " categories, " + juce::String (items) + " presets)");
                check (ticked == 1 && tickedGroups == 1 && tickedName == p.getProgramName (26), "with the loaded one ticked  (" + tickedGroup + " > " + tickedName + ")");
            }

            // Several clicks in a row, before any of them has been dealt with
            p.setCurrentProgram (10);
            for (int i = 0; i < 5; ++i) next->triggerClick();
            letRun (50);
            check (p.getCurrentProgram() == 15, "five quick clicks move on by five");

            p.setCurrentProgram (2);    // as if the host had done it
            letRun (100);
            check (name->getPresetName() == "Midnight Cathedral", "a program change made by the host shows up in the editor");

            setPlain (p, "decay", 3.0f);
            letRun (100);
            check (name->isMarkedAsEdited(), "moving a control marks the preset as edited");
            p.setCurrentProgram (2);
            letRun (100);
            check (! name->isMarkedAsEdited(), "loading it again clears the mark");

            std::printf ("window size\n");
            const double aspect = 760.0 / 440.0;
            check (editor->getWidth() == 760 && editor->getHeight() == 440, "opens at 760 x 440");

            editor->setSize (1140, 660);
            const auto scaled = content->getBoundsInParent();
            check (scaled.getWidth() == 1140 && scaled.getHeight() == 660 && content->getWidth() == 760,
                   "at 1140 x 660 everything is drawn one and a half times as large");

            editor->setBoundsConstrained ({ 0, 0, 1000, 900 });
            check (std::abs (editor->getWidth() / (double) editor->getHeight() - aspect) < 0.01, "whatever shape it is dragged to, it keeps its proportions  -> "
                    + juce::String (editor->getWidth()) + " x " + juce::String (editor->getHeight()));

            editor->setBoundsConstrained ({ 0, 0, 100, 100 });
            const int smallest = editor->getWidth();
            editor->setBoundsConstrained ({ 0, 0, 9000, 9000 });
            const int largest = editor->getWidth();
            check (smallest == 570 && largest == 2280, "between " + juce::String (smallest) + " and " + juce::String (largest) + " pixels wide");

            editor->setSize (950, 550);
            p.editorBeingDeleted (editor.get());
            editor.reset();
            editor.reset (p.createEditorIfNeeded());
            check (editor->getWidth() == 950 && editor->getHeight() == 550, "closed and opened again, it has the size it was left at");

            juce::MemoryBlock withSize;
            p.getStateInformation (withSize);
            {
                CloudveilAudioProcessor other;
                other.setStateInformation (withSize.getData(), (int) withSize.getSize());
                std::unique_ptr<juce::AudioProcessorEditor> otherEditor (other.createEditorIfNeeded());
                check (otherEditor->getWidth() == 950, "and the size is saved with the project");
                other.editorBeingDeleted (otherEditor.get());
            }

            editor->setSize (760, 440);
            content    = editor->findChildWithID ("content");
            visualiser = dynamic_cast<AuroraVisualiser*> (content->findChildWithID ("visualiser"));

            std::printf ("the aurora\n");
            const auto snapshot = [&] (const char* suffix)
            {
                if (argc <= 1)
                    return;

                const auto base = juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]);
                const auto file = base.getSiblingFile (base.getFileNameWithoutExtension() + suffix + ".png");
                const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
                file.deleteFile();
                juce::FileOutputStream stream (file);
                if (! (stream.openedOk() && juce::PNGImageFormat().writeImageToStream (image, stream)))
                    check (false, "snapshot written to " + file.getFullPathName());
            };

            // Plays `seconds` of sound through the plug-in while the editor keeps redrawing, as in a host
            juce::AudioBuffer<float> block (2, 1600);       // one 30th of a second
            juce::MidiBuffer midi;
            double tonePhase = 0.0;

            const auto play = [&] (double seconds, double toneHz, float level)
            {
                for (int n = juce::roundToInt (seconds * 30.0); --n >= 0;)
                {
                    for (int i = 0; i < block.getNumSamples(); ++i)
                    {
                        // a tone with a few harmonics, like a held note
                        float sample = 0.0f;
                        if (toneHz > 0.0)
                        {
                            sample = level * (float) (std::sin (tonePhase) + 0.5 * std::sin (2.0 * tonePhase) + 0.25 * std::sin (3.0 * tonePhase));
                            tonePhase += juce::MathConstants<double>::twoPi * toneHz / 48000.0;
                        }

                        block.setSample (0, i, sample);
                        block.setSample (1, i, sample);
                    }

                    p.processBlock (block, midi);
                    juce::Thread::sleep (34);
                    juce::Timer::callPendingTimersSynchronously();
                }
            };

            p.reset();                  // whatever the checks above left ringing
            p.setCurrentProgram (0);
            setPlain (p, "mix", 100.0f);
            play (0.6, 0.0, 0.0f);
            const float atRest = visualiser->getActivity();
            snapshot ("-1-at-rest");

            play (1.5, 110.0, 0.2f);
            const float lowNote = visualiser->getActivity();
            const float lowCentre = visualiser->getCentreOfActivity();
            snapshot ("-2-low-note");

            play (1.5, 1760.0, 0.2f);
            const float highCentre = visualiser->getCentreOfActivity();
            snapshot ("-3-high-note");

            check (atRest < 0.02f && lowNote > 0.4f, "it is calm in silence and swells when there is sound  (" + juce::String (atRest, 2) + " -> " + juce::String (lowNote, 2) + ")");
            check (highCentre > lowCentre + 0.2f, "a low note swells it on the left, a high note on the right  (centre at "
                    + juce::String (lowCentre, 2) + " and " + juce::String (highCentre, 2) + " of the width)");

            setPlain (p, "shimmer", 100.0f);
            play (2.5, 440.0, 0.2f);
            snapshot ("-4-shimmer");

            setPlain (p, "freeze", 1.0f);
            play (1.5, 0.0, 0.0f);
            const float frozenActivity = visualiser->getActivity();
            snapshot ("-5-frozen");
            check (frozenActivity > 0.3f, "a frozen cloud keeps it up without any input  (" + juce::String (frozenActivity, 2) + ")");

            setPlain (p, "freeze", 0.0f);
            setPlain (p, "shimmer", 28.0f);
            setPlain (p, "decay", 0.4f);
            play (2.5, 0.0, 0.0f);
            check (visualiser->getActivity() < 0.05f, "and it settles again once the tail has gone  (" + juce::String (visualiser->getActivity(), 2) + ")");

            juce::Thread::sleep (500);                      // the host stops calling the plug-in
            juce::Timer::callPendingTimersSynchronously();
            p.setCurrentProgram (0);

            letRun (100);
            editor->setSize (1140, 660);
            snapshot ("-6-larger");
            editor->setSize (1041, 603);                    // not a round multiple of the design size
            snapshot ("-7-odd-size");
            editor->setSize (760, 440);
            letRun (100);

            if (argc > 1)
            {
                const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
                const juce::File file (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
                file.deleteFile();
                juce::FileOutputStream stream (file);
                check (stream.openedOk() && juce::PNGImageFormat().writeImageToStream (image, stream), "snapshot written to " + file.getFullPathName());
            }

            p.editorBeingDeleted (editor.get());
            editor.reset();
        }
    }

    std::printf ("\n%s\n", failures == 0 ? "ALL PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
