#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace cloudveil
{

constexpr float kPi    = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

inline float softLimitSafe (float x) noexcept
{
    constexpr float threshold = 1.5f;
    const float absX = std::abs (x);
    if (absX <= threshold)
        return x;

    const float compressed = threshold + std::tanh (absX - threshold);
    return x >= 0.0f ? compressed : -compressed;
}

class SmoothedFloat
{
public:
    void prepare (double sampleRate, float smoothingMs, float initialValue) noexcept
    {
        const float timeSec = std::max (0.0005f, smoothingMs * 0.001f);
        coeff = 1.0f - std::exp (-1.0f / (timeSec * static_cast<float> (sampleRate)));
        current = initialValue;
        target  = initialValue;
    }

    void reset (float value) noexcept
    {
        current = value;
        target  = value;
    }

    void setTarget (float newTarget) noexcept
    {
        target = newTarget;
    }

    float getNext() noexcept
    {
        current += coeff * (target - current);
        return current;
    }

    float getCurrent() const noexcept { return current; }

private:
    float current = 0.0f;
    float target  = 0.0f;
    float coeff   = 0.01f;
};

class OnePoleLP
{
public:
    void reset() noexcept { z = 0.0f; }

    void setCutoff (float freqHz, float sampleRate) noexcept
    {
        const float clamped = std::clamp (freqHz, 5.0f, 0.45f * sampleRate);
        const float x = std::exp (-kTwoPi * clamped / sampleRate);
        a = 1.0f - x;
        b = x;
    }

    float process (float in) noexcept
    {
        z = a * in + b * z;
        return z;
    }

private:
    float z = 0.0f;
    float a = 1.0f;
    float b = 0.0f;
};

class OnePoleHP
{
public:
    void reset() noexcept { lp.reset(); }

    void setCutoff (float freqHz, float sampleRate) noexcept
    {
        lp.setCutoff (freqHz, sampleRate);
    }

    float process (float in) noexcept
    {
        return in - lp.process (in);
    }

private:
    OnePoleLP lp;
};

class DelayLine
{
public:
    void prepare (std::size_t minCapacity)
    {
        std::size_t cap = 16;
        while (cap < minCapacity + 8)
            cap <<= 1;

        buffer.assign (cap, 0.0f);
        mask = cap - 1;
        writeIndex = 0;
    }

    void clear() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writeIndex = 0;
    }

    void push (float sample) noexcept
    {
        buffer[writeIndex] = sample;
        writeIndex = (writeIndex + 1) & mask;
    }

    float readNearestBeforePush (std::size_t delaySamples) const noexcept
    {
        const std::size_t d = std::clamp (delaySamples, std::size_t (1), mask - 1);
        return buffer[(writeIndex - d) & mask];
    }

    float readLinearAfterPush (float delaySamples) const noexcept
    {
        const float maxD = static_cast<float> (mask - 2);
        const float d = std::clamp (delaySamples, 0.0f, maxD);
        const std::size_t i0 = static_cast<std::size_t> (d);
        const float frac = d - static_cast<float> (i0);

        const std::size_t idx0 = (writeIndex - 1 - i0) & mask;
        const std::size_t idx1 = (writeIndex - 2 - i0) & mask;
        return buffer[idx0] + frac * (buffer[idx1] - buffer[idx0]);
    }

    float readCubicBeforePush (float delaySamples) const noexcept
    {
        const float maxD = static_cast<float> (mask - 4);
        const float d = std::clamp (delaySamples, 2.0f, maxD);
        const std::size_t i1 = static_cast<std::size_t> (d);
        const float frac = d - static_cast<float> (i1);

        const float y0 = buffer[(writeIndex - (i1 - 1)) & mask];
        const float y1 = buffer[(writeIndex - i1) & mask];
        const float y2 = buffer[(writeIndex - (i1 + 1)) & mask];
        const float y3 = buffer[(writeIndex - (i1 + 2)) & mask];

        const float c0 = y1;
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

private:
    std::vector<float> buffer = std::vector<float> (16, 0.0f);
    std::size_t mask = 15;
    std::size_t writeIndex = 0;
};

class AllpassDiffuser
{
public:
    void prepare (double sampleRate, float delayMs, float feedbackGain)
    {
        delaySamples = std::max<std::size_t> (
            2,
            static_cast<std::size_t> (std::llround (static_cast<double> (delayMs) * 0.001 * sampleRate)));
        gain = feedbackGain;
        delay.prepare (delaySamples + 4);
    }

    void clear() noexcept { delay.clear(); }

    float process (float x) noexcept
    {
        const float delayed = delay.readNearestBeforePush (delaySamples);
        const float w = x + gain * delayed;
        delay.push (w);
        return delayed - gain * w;
    }

private:
    DelayLine delay;
    std::size_t delaySamples = 64;
    float gain = 0.6f;
};

class ShimmerPitchShifter
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = static_cast<float> (newSampleRate);
        windowSamples = std::max (512.0f, 0.064f * sampleRate);
        phaseInc = 1.0f / windowSamples;
        phase = 0.0f;

        const std::size_t capacity = static_cast<std::size_t> (0.16 * newSampleRate) + 64;
        bufL.prepare (capacity);
        bufR.prepare (capacity);

        hpfL.setCutoff (220.0f, sampleRate);
        hpfR.setCutoff (220.0f, sampleRate);
        lpfL.setCutoff (6200.0f, sampleRate);
        lpfR.setCutoff (6200.0f, sampleRate);
        clear();
    }

    void clear() noexcept
    {
        bufL.clear();
        bufR.clear();
        hpfL.reset();
        hpfR.reset();
        lpfL.reset();
        lpfR.reset();
        phase = 0.0f;
    }

    void process (float inL, float inR, float& outL, float& outR) noexcept
    {
        const float phaseA = phase;
        float phaseB = phase + 0.5f;
        if (phaseB >= 1.0f)
            phaseB -= 1.0f;

        const float delayA = (1.0f - phaseA) * windowSamples + 4.0f;
        const float delayB = (1.0f - phaseB) * windowSamples + 4.0f;

        const float winA = std::sin (kPi * phaseA);
        const float winB = std::sin (kPi * phaseB);

        const float rawL = winA * bufL.readCubicBeforePush (delayA)
                         + winB * bufL.readCubicBeforePush (delayB);
        const float rawR = winA * bufR.readCubicBeforePush (delayA)
                         + winB * bufR.readCubicBeforePush (delayB);

        bufL.push (inL);
        bufR.push (inR);

        phase += phaseInc;
        if (phase >= 1.0f)
            phase -= 1.0f;

        outL = lpfL.process (hpfL.process (rawL));
        outR = lpfR.process (hpfR.process (rawR));
    }

private:
    DelayLine bufL;
    DelayLine bufR;
    OnePoleHP hpfL, hpfR;
    OnePoleLP lpfL, lpfR;

    float sampleRate = 48000.0f;
    float windowSamples = 3072.0f;
    float phase = 0.0f;
    float phaseInc = 1.0f / 3072.0f;
};

struct ReverbParams
{
    float decaySeconds = 4.8f;
    float sizePercent  = 110.0f;
    float preDelayMs   = 24.0f;
    float mixPercent   = 38.0f;
    float shimmerPct   = 28.0f;
    float driftPct     = 45.0f;
    float warmthPct    = 55.0f;
    float widthPercent = 115.0f;
    bool  freeze       = false;
};

class DreamyReverbEngine
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = static_cast<float> (newSampleRate);

        // Pre-delay up to 250 ms
        const std::size_t preDelayCap = static_cast<std::size_t> (0.28 * newSampleRate) + 32;
        preDelayL.prepare (preDelayCap);
        preDelayR.prepare (preDelayCap);

        // Input conditioning HPF (clean out sub-bass mud)
        inputHpL.setCutoff (90.0f, sampleRate);
        inputHpR.setCutoff (90.0f, sampleRate);

        // 4-stage stereo all-pass diffusers
        constexpr std::array<float, 4> diffTimesL { 4.3f, 6.7f, 11.1f, 16.9f };
        constexpr std::array<float, 4> diffTimesR { 4.7f, 7.3f, 11.9f, 17.7f };
        constexpr std::array<float, 4> diffGains  { 0.62f, 0.60f, 0.56f, 0.52f };

        for (std::size_t i = 0; i < 4; ++i)
        {
            diffusersL[i].prepare (newSampleRate, diffTimesL[i], diffGains[i]);
            diffusersR[i].prepare (newSampleRate, diffTimesR[i], diffGains[i]);
        }

        // 8-channel FDN delay lines (with headroom for 2.0x size + LFO excursion)
        constexpr float maxSizeScale = 2.0f;
        for (std::size_t i = 0; i < kNumChannels; ++i)
        {
            const double maxDelaySec = static_cast<double> (kBaseDelaysMs[i]) * 0.001 * maxSizeScale + 0.015;
            fdnDelays[i].prepare (static_cast<std::size_t> (maxDelaySec * newSampleRate) + 64);
            dcBlockers[i].setCutoff (14.0f, sampleRate);
            dampingLP[i].setCutoff (4000.0f, sampleRate);
        }

        shimmerShifter.prepare (newSampleRate);

        // Smoothers
        smDecay.prepare (newSampleRate, 35.0f, 4.8f);
        smSize.prepare (newSampleRate, 80.0f, 1.10f);
        smPreDelay.prepare (newSampleRate, 35.0f, 0.024f * sampleRate);
        smMix.prepare (newSampleRate, 20.0f, 0.38f);
        smShimmer.prepare (newSampleRate, 30.0f, 0.28f);
        smDrift.prepare (newSampleRate, 30.0f, 0.45f);
        smWarmth.prepare (newSampleRate, 30.0f, 0.55f);
        smWidth.prepare (newSampleRate, 20.0f, 1.15f);
        smFreeze.prepare (newSampleRate, 25.0f, 0.0f);

        // Detuned slow LFO rates in Hz
        constexpr std::array<float, kNumChannels> lfoHz { 0.19f, 0.23f, 0.29f, 0.34f, 0.41f, 0.46f, 0.52f, 0.59f };
        for (std::size_t i = 0; i < kNumChannels; ++i)
        {
            lfoPhase[i] = static_cast<float> (i) * (kTwoPi / static_cast<float> (kNumChannels));
            lfoInc[i]   = kTwoPi * lfoHz[i] / sampleRate;
        }

        controlCounter = 0;
        clear();
    }

    void clear() noexcept
    {
        preDelayL.clear();
        preDelayR.clear();
        inputHpL.reset();
        inputHpR.reset();

        for (auto& d : diffusersL) d.clear();
        for (auto& d : diffusersR) d.clear();

        for (std::size_t i = 0; i < kNumChannels; ++i)
        {
            fdnDelays[i].clear();
            dampingLP[i].reset();
            dcBlockers[i].reset();
        }

        shimmerShifter.clear();
        wetEnv = 0.0f;
        shimEnv = 0.0f;
    }

    void setParameters (const ReverbParams& p) noexcept
    {
        smDecay.setTarget (std::clamp (p.decaySeconds, 0.4f, 18.0f));
        smSize.setTarget (std::clamp (p.sizePercent * 0.01f, 0.35f, 1.85f));
        smPreDelay.setTarget (std::clamp (p.preDelayMs * 0.001f, 0.0f, 0.22f) * sampleRate);
        smMix.setTarget (std::clamp (p.mixPercent * 0.01f, 0.0f, 1.0f));
        smShimmer.setTarget (std::clamp (p.shimmerPct * 0.01f, 0.0f, 1.0f));
        smDrift.setTarget (std::clamp (p.driftPct * 0.01f, 0.0f, 1.0f));
        smWarmth.setTarget (std::clamp (p.warmthPct * 0.01f, 0.0f, 1.0f));
        smWidth.setTarget (std::clamp (p.widthPercent * 0.01f, 0.0f, 1.5f));
        smFreeze.setTarget (p.freeze ? 1.0f : 0.0f);
    }

    void processSample (float inL, float inR, float& outL, float& outR) noexcept
    {
        const float decaySec    = smDecay.getNext();
        const float sizeScale   = smSize.getNext();
        const float preDelaySmp = smPreDelay.getNext();
        const float mix         = smMix.getNext();
        const float shimmerAmt  = smShimmer.getNext();
        const float driftAmt    = smDrift.getNext();
        const float warmthNorm  = smWarmth.getNext();
        const float width       = smWidth.getNext();
        const float freezeBlend = smFreeze.getNext();

        // Update expensive filter / decay coefficients every 16 samples
        if ((controlCounter & 15) == 0)
            updateCoefficients (decaySec, sizeScale, warmthNorm, freezeBlend);
        ++controlCounter;

        // 1. Pre-delay + gentle input stereo cross-blend + mud HPF
        preDelayL.push (inL);
        preDelayR.push (inR);
        const float pdL = preDelayL.readLinearAfterPush (preDelaySmp);
        const float pdR = preDelayR.readLinearAfterPush (preDelaySmp);

        float diffL = inputHpL.process (0.85f * pdL + 0.15f * pdR);
        float diffR = inputHpR.process (0.85f * pdR + 0.15f * pdL);

        // 2. 4-stage stereo all-pass input diffusion
        for (std::size_t i = 0; i < 4; ++i)
        {
            diffL = diffusersL[i].process (diffL);
            diffR = diffusersR[i].process (diffR);
        }

        // 3. Read modulated 8-channel FDN delay lines
        const float modDepthSamples = driftAmt * 0.0022f * sampleRate; // up to +/- 2.2 ms
        std::array<float, kNumChannels> d {};

        for (std::size_t i = 0; i < kNumChannels; ++i)
        {
            const float lfo = std::sin (lfoPhase[i]);
            lfoPhase[i] += lfoInc[i];
            if (lfoPhase[i] >= kTwoPi)
                lfoPhase[i] -= kTwoPi;

            const float baseSamples = kBaseDelaysMs[i] * 0.001f * sampleRate * sizeScale;
            const float tapDelay = std::max (8.0f, baseSamples + lfo * modDepthSamples);
            d[i] = fdnDelays[i].readCubicBeforePush (tapDelay);
        }

        // 4. Filter inside the tank (damping opens smoothly during Freeze)
        std::array<float, kNumChannels> filtered {};
        for (std::size_t i = 0; i < kNumChannels; ++i)
        {
            const float dcClean = dcBlockers[i].process (d[i]);
            const float damped  = dampingLP[i].process (dcClean);
            filtered[i] = damped + freezeBlend * (dcClean - damped);
        }

        // 5. Fast 8-point normalized Walsh-Hadamard mixing matrix (strictly unitary)
        std::array<float, kNumChannels> mixed {};
        applyHadamard8 (filtered, mixed);

        // 6. Apply RT60 decay gains
        std::array<float, kNumChannels> decayed {};
        for (std::size_t i = 0; i < kNumChannels; ++i)
            decayed[i] = cachedLoopGain[i] * mixed[i];

        // 7. Energy-conserving octave-up shimmer pitch shifter
        const float shimInL = 0.5f * (decayed[0] + decayed[2] + decayed[4] + decayed[6]);
        const float shimInR = 0.5f * (decayed[1] + decayed[3] + decayed[5] + decayed[7]);
        float shimOutL = 0.0f;
        float shimOutR = 0.0f;
        shimmerShifter.process (shimInL, shimInR, shimOutL, shimOutR);

        const float theta = shimmerAmt * 0.52f * (1.0f - freezeBlend);
        const float cosTheta = std::cos (theta);
        const float sinTheta = std::sin (theta);

        // 8. Inject diffused stereo signal + mixed feedback back into FDN
        constexpr std::array<float, kNumChannels> polarity { 1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f, 1.0f };
        const float inputDrive = cachedInputScale * (1.0f - freezeBlend);

        for (std::size_t i = 0; i < kNumChannels; ++i)
        {
            const float srcDiff = (i % 2 == 0) ? diffL : diffR;
            const float srcShim = (i % 2 == 0) ? shimOutL : shimOutR;

            const float injected = inputDrive * 0.5f * polarity[i] * srcDiff;
            const float feedback = cosTheta * decayed[i] + sinTheta * srcShim;
            fdnDelays[i].push (softLimitSafe (injected + feedback));
        }

        // 9. Stereo wet output tap + gentle shimmer halo + Mid/Side width
        float wetL = 0.5f * (d[0] - d[2] + d[4] - d[6]) + shimmerAmt * 0.20f * shimOutL;
        float wetR = 0.5f * (d[1] + d[3] - d[5] - d[7]) + shimmerAmt * 0.20f * shimOutR;

        const float mid  = 0.5f * (wetL + wetR);
        const float side = 0.5f * (wetL - wetR) * width;
        wetL = (mid + side) * 0.65f;
        wetR = (mid - side) * 0.65f;

        // 10. Equal-power Dry/Wet blend
        const float angle   = mix * (0.5f * kPi);
        const float dryGain = std::cos (angle);
        const float wetGain = std::sin (angle);

        outL = dryGain * inL + wetGain * wetL;
        outR = dryGain * inR + wetGain * wetR;

        // Envelope followers for UI visualization
        const float wetEnergy = 0.5f * (std::abs (wetL) + std::abs (wetR));
        const float shimEnergy = 0.5f * (std::abs (shimOutL) + std::abs (shimOutR)) * shimmerAmt;
        wetEnv  += 0.002f * (wetEnergy - wetEnv);
        shimEnv += 0.002f * (shimEnergy - shimEnv);
    }

    float getWetMeterLevel() const noexcept { return wetEnv; }
    float getShimmerMeterLevel() const noexcept { return shimEnv; }

private:
    static constexpr std::size_t kNumChannels = 8;
    static constexpr std::array<float, kNumChannels> kBaseDelaysMs {
        31.7f, 37.3f, 43.9f, 52.1f, 61.7f, 73.3f, 87.7f, 103.3f
    };

    void updateCoefficients (float decaySec, float sizeScale, float warmthNorm, float freezeBlend) noexcept
    {
        const float dampFreq = 16000.0f * std::pow (1200.0f / 16000.0f, warmthNorm);
        float gSum = 0.0f;

        for (std::size_t i = 0; i < kNumChannels; ++i)
        {
            dampingLP[i].setCutoff (dampFreq, sampleRate);
            const float delaySec = kBaseDelaysMs[i] * 0.001f * sizeScale;
            const float normalGain = std::pow (10.0f, (-3.0f * delaySec) / decaySec);
            gSum += normalGain;
            cachedLoopGain[i] = normalGain + freezeBlend * (1.0f - normalGain);
        }

        const float gAvg = gSum / static_cast<float> (kNumChannels);
        const float oneMinusG2 = std::max (0.01f, 1.0f - gAvg * gAvg);
        cachedInputScale = std::clamp (std::pow (oneMinusG2, 0.35f), 0.22f, 0.85f);
    }

    static void applyHadamard8 (const std::array<float, 8>& in, std::array<float, 8>& out) noexcept
    {
        constexpr float kInvSqrt8 = 0.35355339059327376f;

        const float a0 = in[0] + in[1], a1 = in[0] - in[1];
        const float a2 = in[2] + in[3], a3 = in[2] - in[3];
        const float a4 = in[4] + in[5], a5 = in[4] - in[5];
        const float a6 = in[6] + in[7], a7 = in[6] - in[7];

        const float b0 = a0 + a2, b2 = a0 - a2;
        const float b1 = a1 + a3, b3 = a1 - a3;
        const float b4 = a4 + a6, b6 = a4 - a6;
        const float b5 = a5 + a7, b7 = a5 - a7;

        out[0] = (b0 + b4) * kInvSqrt8;
        out[4] = (b0 - b4) * kInvSqrt8;
        out[1] = (b1 + b5) * kInvSqrt8;
        out[5] = (b1 - b5) * kInvSqrt8;
        out[2] = (b2 + b6) * kInvSqrt8;
        out[6] = (b2 - b6) * kInvSqrt8;
        out[3] = (b3 + b7) * kInvSqrt8;
        out[7] = (b3 - b7) * kInvSqrt8;
    }

    DelayLine preDelayL;
    DelayLine preDelayR;
    OnePoleHP inputHpL;
    OnePoleHP inputHpR;

    std::array<AllpassDiffuser, 4> diffusersL;
    std::array<AllpassDiffuser, 4> diffusersR;

    std::array<DelayLine, kNumChannels> fdnDelays;
    std::array<OnePoleLP, kNumChannels> dampingLP;
    std::array<OnePoleHP, kNumChannels> dcBlockers;
    std::array<float, kNumChannels> lfoPhase {};
    std::array<float, kNumChannels> lfoInc {};
    std::array<float, kNumChannels> cachedLoopGain { 0.9f, 0.9f, 0.9f, 0.9f, 0.9f, 0.9f, 0.9f, 0.9f };

    ShimmerPitchShifter shimmerShifter;

    SmoothedFloat smDecay, smSize, smPreDelay, smMix, smShimmer, smDrift, smWarmth, smWidth, smFreeze;

    float sampleRate = 48000.0f;
    float cachedInputScale = 0.5f;
    float wetEnv = 0.0f;
    float shimEnv = 0.0f;
    int   controlCounter = 0;
};

} // namespace cloudveil