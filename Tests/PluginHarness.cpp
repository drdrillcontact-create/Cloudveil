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
            juce::ComboBox* box = nullptr;

            for (auto* child : editor->getChildren())
                if (box == nullptr)
                    box = dynamic_cast<juce::ComboBox*> (child);

            check (box != nullptr && box->getText() == "Velvet Cloud", "preset menu shows the current program");

            run (p, 0.5, true, random);
            p.setCurrentProgram (2);    // as if the host had done it

            // let the editor's timer run for a moment
            juce::Timer::callAfterDelay (300, [] { juce::MessageManager::getInstance()->stopDispatchLoop(); });
            juce::MessageManager::getInstance()->runDispatchLoop();

            check (box != nullptr && box->getText() == "Midnight Cathedral", "preset menu follows a program change made by the host");

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
