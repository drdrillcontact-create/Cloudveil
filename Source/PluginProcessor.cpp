#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <limits>
#include <memory>
#include <vector>

namespace
{
std::array<float, 9> presetValues (const CloudveilPreset& p) noexcept
{
    return { p.decay, p.size, p.predelay, p.mix, p.shimmer, p.drift, p.warmth, p.width, p.freeze ? 1.0f : 0.0f };
}

juce::String limitLength (const juce::String& text, int maximumLength)
{
    return maximumLength > 0 ? text.substring (0, maximumLength) : text;
}

// Value text shared by the host (automation lanes, generic editors) and the plug-in's own
// read-outs, so that both always show the same thing.
juce::String secondsText (float value, int maximumLength)
{
    return limitLength (juce::String (value, 2), maximumLength);
}

// For the controls that move in steps of 0.5: "38" rather than "38.0", but still "38.5"
juce::String halfStepText (float value, int maximumLength)
{
    const float snapped = std::round (value * 2.0f) * 0.5f;
    const float whole   = std::round (snapped);
    const bool  isWhole = std::abs (snapped - whole) < 0.25f;

    return limitLength (isWhole ? juce::String (juce::roundToInt (whole)) : juce::String (snapped, 1), maximumLength);
}

// juce::AudioParameterBool keeps the exact number it was last given. Handed 0.38 by the host it
// is "off", but it goes on reporting 0.38. If the host then recalls a state in which the switch
// is off as well, nothing changes as far as the plug-in is concerned, so the host is never told
// and keeps showing 0.38 for a switch that is off. This one reports what it does: 0 or 1.
class SwitchParameter final : public juce::AudioParameterBool
{
public:
    using juce::AudioParameterBool::AudioParameterBool;

private:
    float getValue() const override    { return get() ? 1.0f : 0.0f; }
};
}

const std::array<CloudveilPreset, CloudveilAudioProcessor::numPresets>& CloudveilAudioProcessor::getPresets() noexcept
{
    // The first six are the presets the plug-in started out with; they keep their numbers, so
    // that a project saved back then still opens with the same one.
    //
    // name, category, decay s, size %, pre-delay ms, mix %, shimmer %, drift %, warmth %, width %, freeze
    static const std::array<CloudveilPreset, numPresets> presets {{
        { "Velvet Cloud",       "Signature",        4.8f,  110.0f,  24.0f,  38.0f,  28.0f,  45.0f, 55.0f, 115.0f, false },
        { "Astral Shimmer",     "Signature",        9.5f,  140.0f,  40.0f,  52.0f,  75.0f,  60.0f, 38.0f, 130.0f, false },
        { "Midnight Cathedral", "Signature",       12.0f,  170.0f,  55.0f,  48.0f,  15.0f,  40.0f, 65.0f, 125.0f, false },
        { "Tape Drift",         "Signature",        3.6f,   95.0f,  18.0f,  44.0f,  10.0f,  85.0f, 72.0f, 105.0f, false },
        { "Soft Halo",          "Signature",        1.8f,   75.0f,  12.0f,  30.0f,  22.0f,  35.0f, 48.0f, 100.0f, false },
        { "Infinite Horizon",   "Signature",       17.5f,  180.0f,  65.0f,  68.0f,  55.0f,  70.0f, 50.0f, 140.0f, false },

        { "Drum Ambience",      "Rooms & Plates",   0.55f,  40.0f,   0.0f,  20.0f,   0.0f,   8.0f, 40.0f, 120.0f, false },
        { "Small Studio",       "Rooms & Plates",   0.7f,   45.0f,   4.0f,  22.0f,   0.0f,  12.0f, 50.0f,  90.0f, false },
        { "Glass Booth",        "Rooms & Plates",   0.9f,   50.0f,   6.0f,  24.0f,  12.0f,  18.0f, 12.0f,  95.0f, false },
        { "Wood Room",          "Rooms & Plates",   1.1f,   60.0f,   8.0f,  26.0f,   0.0f,  15.0f, 62.0f, 100.0f, false },
        { "Tiled Chamber",      "Rooms & Plates",   1.3f,   55.0f,  14.0f,  28.0f,   6.0f,  10.0f, 25.0f, 115.0f, false },
        { "Bright Plate",       "Rooms & Plates",   2.0f,   90.0f,   0.0f,  32.0f,   0.0f,  30.0f, 10.0f, 120.0f, false },
        { "Dark Plate",         "Rooms & Plates",   2.2f,   80.0f,  10.0f,  32.0f,   0.0f,  22.0f, 70.0f, 105.0f, false },

        { "Chamber Hall",       "Halls",            2.4f,  100.0f,  18.0f,  30.0f,   0.0f,  25.0f, 52.0f, 110.0f, false },
        { "Concert Hall",       "Halls",            3.2f,  125.0f,  28.0f,  34.0f,   4.0f,  30.0f, 58.0f, 120.0f, false },
        { "Opera House",        "Halls",            4.0f,  140.0f,  52.0f,  32.0f,   0.0f,  14.0f, 66.0f, 105.0f, false },
        { "Grand Hall",         "Halls",            4.6f,  150.0f,  36.0f,  36.0f,   8.0f,  32.0f, 60.0f, 125.0f, false },
        { "Marble Hall",        "Halls",            5.2f,  145.0f,  32.0f,  38.0f,  10.0f,  26.0f, 30.0f, 130.0f, false },
        { "Stone Church",       "Halls",            7.2f,  175.0f,  70.0f,  42.0f,   0.0f,  18.0f, 78.0f, 115.0f, false },
        { "Distant Choir",      "Halls",            9.0f,  185.0f, 140.0f,  50.0f,  30.0f,  30.0f, 70.0f, 150.0f, false },

        { "Silver Lining",      "Shimmer",          2.8f,   85.0f,  12.0f,  34.0f,  45.0f,  30.0f, 25.0f, 115.0f, false },
        { "Stardust",           "Shimmer",          3.5f,   90.0f,  15.0f,  38.0f,  60.0f,  55.0f, 15.0f, 135.0f, false },
        { "Octave Bloom",       "Shimmer",          5.0f,  110.0f,  20.0f,  42.0f,  50.0f,  35.0f, 40.0f, 120.0f, false },
        { "Crystal Shimmer",    "Shimmer",          6.0f,  120.0f,  30.0f,  45.0f,  65.0f,  40.0f, 20.0f, 130.0f, false },
        { "Angel Choir",        "Shimmer",          6.5f,  165.0f,  85.0f,  46.0f,  90.0f,  25.0f, 62.0f, 150.0f, false },
        { "Warm Aura",          "Shimmer",          7.0f,  130.0f,  35.0f,  46.0f,  55.0f,  45.0f, 70.0f, 125.0f, false },
        { "Glass Cathedral",    "Shimmer",         11.0f,  170.0f,  60.0f,  55.0f,  70.0f,  45.0f, 30.0f, 135.0f, false },
        { "Rising Light",       "Shimmer",         14.0f,  160.0f,  50.0f,  60.0f, 100.0f,  60.0f, 35.0f, 140.0f, false },

        { "Morning Mist",       "Ambient",          6.5f,  130.0f,  55.0f,  50.0f,  25.0f,  60.0f, 58.0f, 135.0f, false },
        { "Weightless",         "Ambient",          9.0f,  140.0f, 100.0f,  56.0f,  35.0f,  50.0f, 48.0f, 150.0f, false },
        { "Cloud Bed",          "Ambient",         10.0f,  155.0f,  50.0f,  62.0f,  20.0f,  55.0f, 68.0f, 140.0f, false },
        { "Northern Lights",    "Ambient",         12.5f,  165.0f,  60.0f,  60.0f,  60.0f,  75.0f, 35.0f, 150.0f, false },
        { "Frozen Lake",        "Ambient",         13.0f,  175.0f,  70.0f,  58.0f,  40.0f,  20.0f, 25.0f, 145.0f, false },
        { "Deep Space",         "Ambient",         16.0f,  185.0f,  90.0f,  65.0f,  30.0f,  65.0f, 60.0f, 150.0f, false },
        { "Slow Tide",          "Ambient",         18.0f,  185.0f, 120.0f,  70.0f,  15.0f,  90.0f, 72.0f, 145.0f, false },
        { "Event Horizon",      "Ambient",         18.0f,  185.0f,   0.0f,  80.0f,  85.0f,  80.0f, 40.0f, 150.0f, false },

        { "Old Radio",          "Lo-Fi & Tape",     0.9f,   40.0f,   0.0f,  30.0f,   0.0f,  45.0f, 92.0f,  40.0f, false },
        { "Dusty Spring",       "Lo-Fi & Tape",     1.4f,   50.0f,   0.0f,  34.0f,   0.0f,  60.0f, 66.0f,  70.0f, false },
        { "Mono Echo Room",     "Lo-Fi & Tape",     2.0f,   65.0f, 110.0f,  36.0f,   0.0f,  35.0f, 74.0f,   0.0f, false },
        { "Worn Cassette",      "Lo-Fi & Tape",     2.6f,   80.0f,  20.0f,  40.0f,   0.0f, 100.0f, 85.0f,  80.0f, false },
        { "Warped Vinyl",       "Lo-Fi & Tape",     3.0f,   90.0f,   8.0f,  44.0f,   0.0f, 100.0f, 60.0f, 120.0f, false },
        { "VHS Dream",          "Lo-Fi & Tape",     6.8f,  120.0f,  45.0f,  52.0f,  38.0f, 100.0f, 82.0f,  75.0f, false },

        { "Snare Plate",        "Instruments",      1.05f,  66.0f,  24.0f,  24.0f,   0.0f,   6.0f, 24.0f, 140.0f, false },
        { "Vocal Plate",        "Instruments",      1.9f,   78.0f,  48.0f,  24.0f,   0.0f,  18.0f, 42.0f, 105.0f, false },
        { "Vocal Halo",         "Instruments",      3.4f,  105.0f,  60.0f,  28.0f,  32.0f,  30.0f, 36.0f, 125.0f, false },
        { "Clean Guitar",       "Instruments",      2.3f,   85.0f,  26.0f,  28.0f,  10.0f,  40.0f, 46.0f, 120.0f, false },
        { "Guitar Swell",       "Instruments",     10.0f,  150.0f,   0.0f,  60.0f,  65.0f,  35.0f, 58.0f, 140.0f, false },
        { "Piano Hall",         "Instruments",      3.4f,  130.0f,  16.0f,  30.0f,   0.0f,   8.0f, 36.0f, 135.0f, false },
        { "Felt Piano",         "Instruments",      4.4f,  110.0f,  30.0f,  42.0f,  14.0f,  48.0f, 80.0f, 110.0f, false },
        { "Synth Pad Wash",     "Instruments",     11.5f,  160.0f,  40.0f,  58.0f,  45.0f,  70.0f, 52.0f, 145.0f, false }
    }};
    return presets;
}

CloudveilAudioProcessor::CloudveilAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "CloudveilParams", createParameterLayout())
{
    decayParam    = apvts.getRawParameterValue ("decay");
    sizeParam     = apvts.getRawParameterValue ("size");
    predelayParam = apvts.getRawParameterValue ("predelay");
    mixParam      = apvts.getRawParameterValue ("mix");
    shimmerParam  = apvts.getRawParameterValue ("shimmer");
    driftParam    = apvts.getRawParameterValue ("drift");
    warmthParam   = apvts.getRawParameterValue ("warmth");
    widthParam    = apvts.getRawParameterValue ("width");
    freezeParam   = apvts.getRawParameterValue ("freeze");

    constexpr std::array<const char*, numProgramParams> programParamIds {
        "decay", "size", "predelay", "mix", "shimmer", "drift", "warmth", "width", "freeze"
    };

    for (std::size_t i = 0; i < numProgramParams; ++i)
    {
        programParams[i] = apvts.getParameter (programParamIds[i]);
        jassert (programParams[i] != nullptr);
    }

    // Keeps the engine's buffers valid even if a host asks for audio before prepareToPlay()
    reverbEngine.prepare (44100.0);
}

juce::AudioProcessorValueTreeState::ParameterLayout CloudveilAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "decay", 1 }, "Decay",
        juce::NormalisableRange<float> (0.4f, 18.0f, 0.01f, 0.45f), 4.8f,
        juce::AudioParameterFloatAttributes().withLabel ("s").withStringFromValueFunction (secondsText)));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "size", 1 }, "Size",
        juce::NormalisableRange<float> (35.0f, 185.0f, 0.5f, 1.0f), 110.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (halfStepText)));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "predelay", 1 }, "Pre-Delay",
        juce::NormalisableRange<float> (0.0f, 200.0f, 0.5f, 0.6f), 24.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms").withStringFromValueFunction (halfStepText)));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "mix", 1 }, "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.5f, 1.0f), 38.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (halfStepText)));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "shimmer", 1 }, "Shimmer",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.5f, 1.0f), 28.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (halfStepText)));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "drift", 1 }, "Drift",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.5f, 1.0f), 45.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (halfStepText)));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "warmth", 1 }, "Warmth",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.5f, 1.0f), 55.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (halfStepText)));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "width", 1 }, "Width",
        juce::NormalisableRange<float> (0.0f, 150.0f, 0.5f, 1.0f), 115.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (halfStepText)));

    params.push_back (std::make_unique<SwitchParameter> (
        juce::ParameterID { "freeze", 1 }, "Freeze", false));

    return { params.begin(), params.end() };
}

const juce::String CloudveilAudioProcessor::getName() const { return "Cloudveil"; }
bool CloudveilAudioProcessor::acceptsMidi() const           { return false; }
bool CloudveilAudioProcessor::producesMidi() const          { return false; }
bool CloudveilAudioProcessor::isMidiEffect() const          { return false; }

double CloudveilAudioProcessor::getTailLengthSeconds() const
{
    if (decayParam == nullptr || predelayParam == nullptr || freezeParam == nullptr)
        return 5.0;

    // A frozen cloud never fades
    if (freezeParam->load (std::memory_order_relaxed) > 0.5f)
        return std::numeric_limits<double>::infinity();

    // Decay is an RT60, i.e. the time the tail needs to drop by 60 dB - at which point it is still
    // audible. Report long enough for it to fade right out (about -90 dB), plus the pre-delay that
    // sits in front of it, so that hosts do not cut the end of the tail off when they bounce.
    // The shimmer keeps feeding octaves into the tail and so lengthens it; the factors are the
    // smallest round ones that cover what the engine was measured to do at any setting.
    const auto decay    = static_cast<double> (decayParam->load (std::memory_order_relaxed));
    const auto preDelay = static_cast<double> (predelayParam->load (std::memory_order_relaxed)) * 0.001;
    const auto shimmer  = shimmerParam != nullptr ? static_cast<double> (shimmerParam->load (std::memory_order_relaxed)) * 0.01 : 0.0;

    return preDelay + decay * (1.5 + 0.6 * shimmer) + 0.75;
}

int CloudveilAudioProcessor::getNumPrograms()
{
    return static_cast<int> (getPresets().size());
}

int CloudveilAudioProcessor::getCurrentProgram()
{
    return currentProgramIdx.load (std::memory_order_relaxed);
}

void CloudveilAudioProcessor::setCurrentProgram (int index)
{
    const auto& presets = getPresets();
    if (! juce::isPositiveAndBelow (index, static_cast<int> (presets.size())))
        return;

    currentProgramIdx.store (index, std::memory_order_relaxed);
    const auto values = presetValues (presets[static_cast<std::size_t> (index)]);

    for (std::size_t i = 0; i < numProgramParams; ++i)
    {
        auto* param = programParams[i];
        if (param == nullptr)
            continue;

        const float normalised = param->convertTo0to1 (values[i]);
        if (juce::approximatelyEqual (param->getValue(), normalised))
            continue;

        // Hosts only take a parameter change that comes from the plug-in seriously (undo, automation,
        // their own parameter display) when it is wrapped in a gesture.
        param->beginChangeGesture();
        param->setValueNotifyingHost (normalised);
        param->endChangeGesture();
    }
}

void CloudveilAudioProcessor::selectProgramFromEditor (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms()))
        return;

    setCurrentProgram (index);

    // Unlike a program change that the host asked for, the host does not know about this one yet
    updateHostDisplay (ChangeDetails{}.withProgramChanged (true));
}

bool CloudveilAudioProcessor::isCurrentProgramModified() const
{
    const auto& presets = getPresets();
    const auto index = static_cast<std::size_t> (juce::jlimit (0, static_cast<int> (presets.size()) - 1,
                                                                currentProgramIdx.load (std::memory_order_relaxed)));
    const auto values = presetValues (presets[index]);

    for (std::size_t i = 0; i < numProgramParams; ++i)
        if (auto* param = programParams[i])
            if (std::abs (param->getValue() - param->convertTo0to1 (values[i])) > 1.0e-4f)
                return true;

    return false;
}

const juce::String CloudveilAudioProcessor::getProgramName (int index)
{
    const auto& presets = getPresets();
    if (index >= 0 && index < static_cast<int> (presets.size()))
        return presets[static_cast<std::size_t> (index)].name;
    return {};
}

void CloudveilAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

void CloudveilAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    reverbEngine.prepare (sampleRate);
    tankIsEmpty = true;

    const double usableRate = (sampleRate >= 1000.0 && sampleRate <= 1.0e6) ? sampleRate : 44100.0;
    scopeStride = juce::jmax (1, juce::roundToInt (usableRate / 48000.0));
    scopeSampleRate.store (usableRate / static_cast<double> (scopeStride), std::memory_order_relaxed);
    scopeCounter = 0;
    scopeWetSum = 0.0f;
    scopeShimmerSum = 0.0f;

    clearMeters();
}

void CloudveilAudioProcessor::releaseResources()
{
    clearTank();
}

void CloudveilAudioProcessor::reset()
{
    // The host is asking for all sound to stop (transport relocated, plug-in switched off, ...)
    clearTank();
}

void CloudveilAudioProcessor::clearTank() noexcept
{
    reverbEngine.clear();
    tankIsEmpty = true;
    clearMeters();

    // The display must not show the sound from before the reset when audio starts again
    wetScope.clear();
    shimmerScope.clear();
    scopeCounter = 0;
    scopeWetSum = 0.0f;
    scopeShimmerSum = 0.0f;
}

void CloudveilAudioProcessor::clearMeters() noexcept
{
    wetMeterLevel.store (0.0f, std::memory_order_relaxed);
    shimmerMeterLevel.store (0.0f, std::memory_order_relaxed);
}

void CloudveilAudioProcessor::pushToScopes (float wet, float shimmer) noexcept
{
    scopeWetSum += wet;
    scopeShimmerSum += shimmer;

    if (++scopeCounter >= scopeStride)
    {
        const float scale = 1.0f / static_cast<float> (scopeStride);
        wetScope.push (scopeWetSum * scale);
        shimmerScope.push (scopeShimmerSum * scale);
        scopeCounter = 0;
        scopeWetSum = 0.0f;
        scopeShimmerSum = 0.0f;
    }
}

void CloudveilAudioProcessor::setEditorWidth (int newWidth) noexcept
{
    editorWidth.store (juce::jlimit (editorMinWidth, editorMaxWidth, newWidth), std::memory_order_relaxed);
}

bool CloudveilAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& outSet = layouts.getMainOutputChannelSet();
    const auto& inSet  = layouts.getMainInputChannelSet();

    if (outSet != juce::AudioChannelSet::mono() && outSet != juce::AudioChannelSet::stereo())
        return false;

    if (inSet != juce::AudioChannelSet::mono() && inSet != juce::AudioChannelSet::stereo())
        return false;

    return inSet.size() <= outSet.size();
}

void CloudveilAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    audioCallbackCount.fetch_add (1, std::memory_order_relaxed);

    const int numSamples   = buffer.getNumSamples();
    const int numInputCh   = juce::jmin (getTotalNumInputChannels(),  buffer.getNumChannels());
    const int numOutputCh  = juce::jmin (getTotalNumOutputChannels(), buffer.getNumChannels());

    for (int ch = numInputCh; ch < numOutputCh; ++ch)
        buffer.clear (ch, 0, numSamples);

    if (numSamples == 0 || numOutputCh == 0)
        return;

    cloudveil::ReverbParams params;
    params.decaySeconds = decayParam->load (std::memory_order_relaxed);
    params.sizePercent  = sizeParam->load (std::memory_order_relaxed);
    params.preDelayMs   = predelayParam->load (std::memory_order_relaxed);
    params.mixPercent   = mixParam->load (std::memory_order_relaxed);
    params.shimmerPct   = shimmerParam->load (std::memory_order_relaxed);
    params.driftPct     = driftParam->load (std::memory_order_relaxed);
    params.warmthPct    = warmthParam->load (std::memory_order_relaxed);
    params.widthPercent = widthParam->load (std::memory_order_relaxed);
    params.freeze       = freezeParam->load (std::memory_order_relaxed) > 0.5f;

    reverbEngine.setParameters (params);
    tankIsEmpty = false;

    const float* inL = buffer.getReadPointer (0);
    const float* inR = (numInputCh > 1) ? buffer.getReadPointer (1) : inL;

    float* outL = buffer.getWritePointer (0);
    float* outR = (numOutputCh > 1) ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        float procL = 0.0f;
        float procR = 0.0f;
        reverbEngine.processSample (inL[i], inR[i], procL, procR);
        pushToScopes (reverbEngine.getLastWetSample(), reverbEngine.getLastShimmerSample());

        if (outR != nullptr)
        {
            outL[i] = procL;
            outR[i] = procR;
        }
        else
        {
            outL[i] = 0.5f * (procL + procR);
        }
    }

    if (! reverbEngine.isHealthy())
    {
        // Should be unreachable, because the engine never lets a non-finite sample in. But a reverb
        // that stays silent until the project is reloaded is bad enough to deserve a safety net.
        clearTank();

        for (int ch = 0; ch < numOutputCh; ++ch)
        {
            auto* samples = buffer.getWritePointer (ch);

            for (int i = 0; i < numSamples; ++i)
                if (! cloudveil::isFiniteSample (samples[i]))
                    samples[i] = 0.0f;
        }

        return;
    }

    wetMeterLevel.store (reverbEngine.getWetMeterLevel(), std::memory_order_relaxed);
    shimmerMeterLevel.store (reverbEngine.getShimmerMeterLevel(), std::memory_order_relaxed);
}

void CloudveilAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    audioCallbackCount.fetch_add (1, std::memory_order_relaxed);

    // Whatever was ringing when the plug-in was bypassed must not come back, out of context, when
    // it is switched in again. A frozen cloud is the exception: Freeze is still lit, so it stays.
    const bool frozen = freezeParam != nullptr && freezeParam->load (std::memory_order_relaxed) > 0.5f;

    if (! tankIsEmpty && ! frozen)
        clearTank();
    else
        clearMeters();

    // Nothing comes out of the reverb while it is bypassed, and the display should say so
    for (int i = buffer.getNumSamples(); --i >= 0;)
        pushToScopes (0.0f, 0.0f);

    juce::AudioProcessor::processBlockBypassed (buffer, midiMessages);
}

bool CloudveilAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* CloudveilAudioProcessor::createEditor()
{
    return new CloudveilAudioProcessorEditor (*this);
}

void CloudveilAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", getCurrentProgram(), nullptr);
    state.setProperty ("editorWidth", getEditorWidth(), nullptr);

    if (const auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void CloudveilAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
    {
        auto tree = juce::ValueTree::fromXml (*xmlState);
        if (tree.hasProperty ("program"))
        {
            const int restoredProgram = static_cast<int> (tree.getProperty ("program"));
            const int maxProgram = static_cast<int> (getPresets().size()) - 1;
            currentProgramIdx.store (juce::jlimit (0, maxProgram, restoredProgram),
                                     std::memory_order_relaxed);
        }

        if (tree.hasProperty ("editorWidth"))
            setEditorWidth (static_cast<int> (tree.getProperty ("editorWidth")));

        apvts.replaceState (tree);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CloudveilAudioProcessor();
}
