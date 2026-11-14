#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <memory>
#include <vector>

const std::array<CloudveilPreset, 6>& CloudveilAudioProcessor::getPresets() noexcept
{
    static const std::array<CloudveilPreset, 6> presets {{
        { "Velvet Cloud",       4.8f,  110.0f, 24.0f, 38.0f, 28.0f, 45.0f, 55.0f, 115.0f, false },
        { "Astral Shimmer",     9.5f,  140.0f, 40.0f, 52.0f, 75.0f, 60.0f, 38.0f, 130.0f, false },
        { "Midnight Cathedral", 12.0f, 170.0f, 55.0f, 48.0f, 15.0f, 40.0f, 65.0f, 125.0f, false },
        { "Tape Drift",         3.6f,   95.0f, 18.0f, 44.0f, 10.0f, 85.0f, 72.0f, 105.0f, false },
        { "Soft Halo",          1.8f,   75.0f, 12.0f, 30.0f, 22.0f, 35.0f, 48.0f, 100.0f, false },
        { "Infinite Horizon",   17.5f, 180.0f, 65.0f, 68.0f, 55.0f, 70.0f, 50.0f, 140.0f, false }
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
}

juce::AudioProcessorValueTreeState::ParameterLayout CloudveilAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "decay", 1 }, "Decay",
        juce::NormalisableRange<float> (0.4f, 18.0f, 0.01f, 0.45f), 4.8f,
        juce::AudioParameterFloatAttributes().withLabel ("s")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "size", 1 }, "Size",
        juce::NormalisableRange<float> (35.0f, 185.0f, 0.5f, 1.0f), 110.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "predelay", 1 }, "Pre-Delay",
        juce::NormalisableRange<float> (0.0f, 200.0f, 0.5f, 0.6f), 24.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "mix", 1 }, "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.5f, 1.0f), 38.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "shimmer", 1 }, "Shimmer",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.5f, 1.0f), 28.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "drift", 1 }, "Drift",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.5f, 1.0f), 45.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "warmth", 1 }, "Warmth",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.5f, 1.0f), 55.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "width", 1 }, "Width",
        juce::NormalisableRange<float> (0.0f, 150.0f, 0.5f, 1.0f), 115.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "freeze", 1 }, "Freeze", false));

    return { params.begin(), params.end() };
}

const juce::String CloudveilAudioProcessor::getName() const { return "Cloudveil"; }
bool CloudveilAudioProcessor::acceptsMidi() const           { return false; }
bool CloudveilAudioProcessor::producesMidi() const          { return false; }
bool CloudveilAudioProcessor::isMidiEffect() const          { return false; }

double CloudveilAudioProcessor::getTailLengthSeconds() const
{
    if (decayParam != nullptr)
        return static_cast<double> (decayParam->load (std::memory_order_relaxed));
    return 5.0;
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
    if (index < 0 || index >= static_cast<int> (presets.size()))
        return;

    currentProgramIdx.store (index, std::memory_order_relaxed);
    const auto& p = presets[static_cast<std::size_t> (index)];

    auto setParam = [this] (const char* id, float plainVal)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (plainVal));
    };

    setParam ("decay",    p.decay);
    setParam ("size",     p.size);
    setParam ("predelay", p.predelay);
    setParam ("mix",      p.mix);
    setParam ("shimmer",  p.shimmer);
    setParam ("drift",    p.drift);
    setParam ("warmth",   p.warmth);
    setParam ("width",    p.width);
    setParam ("freeze",   p.freeze ? 1.0f : 0.0f);
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
}

void CloudveilAudioProcessor::releaseResources()
{
    reverbEngine.clear();
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

    const int numSamples   = buffer.getNumSamples();
    const int numInputCh   = getTotalNumInputChannels();
    const int numOutputCh  = getTotalNumOutputChannels();

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

    const float* inL = buffer.getReadPointer (0);
    const float* inR = (numInputCh > 1) ? buffer.getReadPointer (1) : inL;

    float* outL = buffer.getWritePointer (0);
    float* outR = (numOutputCh > 1) ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        float procL = 0.0f;
        float procR = 0.0f;
        reverbEngine.processSample (inL[i], inR[i], procL, procR);

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

    wetMeterLevel.store (reverbEngine.getWetMeterLevel(), std::memory_order_relaxed);
    shimmerMeterLevel.store (reverbEngine.getShimmerMeterLevel(), std::memory_order_relaxed);
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
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
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
        apvts.replaceState (tree);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CloudveilAudioProcessor();
}