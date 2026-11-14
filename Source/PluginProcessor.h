#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DreamyReverbDSP.h"
#include <array>
#include <atomic>
#include <cstdint>

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
    void reset() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    using juce::AudioProcessor::processBlock;
    using juce::AudioProcessor::processBlockBypassed;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

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

    /** Goes up by one with every audio callback, so the editor can tell when the host has stopped
        calling the plug-in (and stop showing the last meter reading as if it were still live). */
    std::uint32_t getAudioCallbackCount() const noexcept { return audioCallbackCount.load (std::memory_order_relaxed); }

    /** Loads a factory preset on behalf of the editor and tells the host that the program changed.
        (setCurrentProgram() is what the host itself calls, and must not echo back to it.) */
    void selectProgramFromEditor (int index);

    /** True once any control has been moved away from the values of the current factory preset. */
    bool isCurrentProgramModified() const;

    static const std::array<CloudveilPreset, 6>& getPresets() noexcept;

private:
    static constexpr std::size_t numProgramParams = 9;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void clearTank() noexcept;
    void clearMeters() noexcept;

    cloudveil::DreamyReverbEngine reverbEngine;
    bool tankIsEmpty = true; // only touched while the host is (or has just stopped) processing

    std::atomic<float>* decayParam    = nullptr;
    std::atomic<float>* sizeParam     = nullptr;
    std::atomic<float>* predelayParam = nullptr;
    std::atomic<float>* mixParam      = nullptr;
    std::atomic<float>* shimmerParam  = nullptr;
    std::atomic<float>* driftParam    = nullptr;
    std::atomic<float>* warmthParam   = nullptr;
    std::atomic<float>* widthParam    = nullptr;
    std::atomic<float>* freezeParam   = nullptr;

    // The parameters a factory preset sets, in the order of the fields in CloudveilPreset
    std::array<juce::RangedAudioParameter*, numProgramParams> programParams {};

    std::atomic<float> wetMeterLevel     { 0.0f };
    std::atomic<float> shimmerMeterLevel { 0.0f };
    std::atomic<int>   currentProgramIdx { 0 };
    std::atomic<std::uint32_t> audioCallbackCount { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CloudveilAudioProcessor)
};
