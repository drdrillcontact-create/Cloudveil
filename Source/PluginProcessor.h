#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DreamyReverbDSP.h"
#include <array>
#include <atomic>

struct CloudveilPreset
{
    const char* name;
    float decay;
    float size;
    float predelay;
    float mix;
    float shimmer;
    float drift;
    float warmth;
    float width;
    bool  freeze;
};

class CloudveilAudioProcessor final : public juce::AudioProcessor
{
public:
    CloudveilAudioProcessor();
    ~CloudveilAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    using juce::AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    float getWetMeterLevel() const noexcept     { return wetMeterLevel.load (std::memory_order_relaxed); }
    float getShimmerMeterLevel() const noexcept { return shimmerMeterLevel.load (std::memory_order_relaxed); }

    static const std::array<CloudveilPreset, 6>& getPresets() noexcept;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    cloudveil::DreamyReverbEngine reverbEngine;

    std::atomic<float>* decayParam    = nullptr;
    std::atomic<float>* sizeParam     = nullptr;
    std::atomic<float>* predelayParam = nullptr;
    std::atomic<float>* mixParam      = nullptr;
    std::atomic<float>* shimmerParam  = nullptr;
    std::atomic<float>* driftParam    = nullptr;
    std::atomic<float>* warmthParam   = nullptr;
    std::atomic<float>* widthParam    = nullptr;
    std::atomic<float>* freezeParam   = nullptr;

    std::atomic<float> wetMeterLevel     { 0.0f };
    std::atomic<float> shimmerMeterLevel { 0.0f };
    std::atomic<int>   currentProgramIdx { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CloudveilAudioProcessor)
};