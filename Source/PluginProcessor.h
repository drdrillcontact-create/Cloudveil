#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DreamyReverbDSP.h"
#include <array>
#include <atomic>
#include <cstdint>

struct CloudveilPreset
{
    const char* name;
    const char* category;   // presets of one category follow each other in the list
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

/** The most recent stretch of a signal: written by the audio thread, read by the editor for its
    display. Neither side ever waits for the other. A reader may catch a few samples that are
    being replaced at that moment, which a picture that is redrawn 30 times a second can afford. */
class ScopeBuffer
{
public:
    static constexpr std::size_t size = 4096;   // a power of two

    void push (float sample) noexcept
    {
        const auto index = writeIndex.load (std::memory_order_relaxed);
        samples[index & (size - 1)].store (sample, std::memory_order_relaxed);
        writeIndex.store (index + 1, std::memory_order_release);
    }

    /** Forgets everything. For the writing side. */
    void clear() noexcept
    {
        for (auto& sample : samples)
            sample.store (0.0f, std::memory_order_relaxed);
    }

    /** Copies the newest `count` samples (at most `size`), oldest first. */
    void readNewest (float* destination, std::size_t count) const noexcept
    {
        count = std::min (count, size);
        const auto end = writeIndex.load (std::memory_order_acquire);

        for (std::size_t i = 0; i < count; ++i)
            destination[i] = samples[(end - static_cast<std::uint32_t> (count - i)) & (size - 1)].load (std::memory_order_relaxed);
    }

private:
    std::array<std::atomic<float>, size> samples {};
    std::atomic<std::uint32_t> writeIndex { 0 };
};

class CloudveilAudioProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int numPresets = 50;

    /** The size the editor is designed for; it can be dragged to between these two widths and
        keeps its proportions. */
    static constexpr int editorBaseWidth  = 760;
    static constexpr int editorBaseHeight = 440;
    static constexpr int editorMinWidth   = 570;
    static constexpr int editorMaxWidth   = 2280;

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

    static const std::array<CloudveilPreset, numPresets>& getPresets() noexcept;

    /** For the editor's display: the reverb on its own (before it is mixed with the dry signal)
        and the octaves the shimmer is feeding back into it, both mono, at getScopeSampleRate(). */
    const ScopeBuffer& getWetScope() const noexcept      { return wetScope; }
    const ScopeBuffer& getShimmerScope() const noexcept  { return shimmerScope; }
    double getScopeSampleRate() const noexcept           { return scopeSampleRate.load (std::memory_order_relaxed); }

    /** The width the user last gave the editor window. It is saved with the project. */
    int  getEditorWidth() const noexcept { return editorWidth.load (std::memory_order_relaxed); }
    void setEditorWidth (int newWidth) noexcept;

private:
    static constexpr std::size_t numProgramParams = 9;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void clearTank() noexcept;
    void clearMeters() noexcept;
    void pushToScopes (float wet, float shimmer) noexcept;

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
    std::atomic<int>   editorWidth { editorBaseWidth };

    // The scopes run at 44.1 - 48 kHz whatever the host's rate is: at higher rates every
    // 2nd, 4th ... sample is kept (after averaging, which is filter enough for a picture)
    ScopeBuffer wetScope, shimmerScope;
    std::atomic<double> scopeSampleRate { 48000.0 };
    int   scopeStride = 1;        // audio thread only, as are the three below
    int   scopeCounter = 0;
    float scopeWetSum = 0.0f;
    float scopeShimmerSum = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CloudveilAudioProcessor)
};
