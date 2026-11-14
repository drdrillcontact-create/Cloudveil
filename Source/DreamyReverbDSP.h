#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace cloudveil
{

constexpr float kPi    = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

/** True unless x is NaN or +/-Inf. Tests the raw bit pattern, so it keeps working even if the
    compiler is allowed to assume finite maths. */
inline bool isFiniteSample (float x) noexcept
{
    std::uint32_t bits = 0;
    std::memcpy (&bits, &x, sizeof (bits));
    return (bits & 0x7f800000u) != 0x7f800000u;
}

/** Exact comparison that does not trip -Wfloat-equal (and treats NaN as "equal", i.e. settled). */
inline bool exactlyEqual (float a, float b) noexcept
{
    return ! (a < b) && ! (a > b);
}

/** Clamps x to [lo, hi]; NaN (which std::clamp would pass straight through) becomes fallback. */
inline float clampOr (float x, float lo, float hi, float fallback) noexcept
{
    if (x >= lo && x <= hi) return x;
    if (x < lo)             return lo;
    if (x > hi)             return hi;
    return fallback;
}

/** One step of a one-pole glide from current towards target.

    A plain one-pole stalls a few ULPs short of its target (the increment rounds to nothing), so
    "100 %" would never quite be 100 % and Freeze would never reach an exactly lossless loop.
    The move is therefore finished explicitly once it is inaudibly close or has stalled. */
inline float stepTowards (float current, float target, float coeff) noexcept
{
    const float delta = target - current;
    const float next  = current + coeff * delta;

    if (std::abs (delta) < 1.0e-6f || exactlyEqual (next, current))
        return target;

    return next;
}

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

    /** Jumps to the target without gliding (used for the first block after a reset). */
    void snapToTarget() noexcept
    {
        current = target;
    }

    float getNext() noexcept
    {
        current = stepTowards (current, target, coeff);
        return current;
    }

    float getCurrent() const noexcept { return current; }
    float getTarget() const noexcept  { return target; }

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

/** Radix-2 FFT on separate real and imaginary arrays.

    Small on purpose: it serves the shimmer's octave shifter and the editor's spectrum display,
    so the plug-in needs no FFT library. Neither direction scales its result.

    A transform can be done in one go, or in portions (reorderPart() over all indices, then
    stagePart() over all butterflies of stage 0, 1, ... in turn), so that the audio thread can
    spread the work over many samples instead of doing it all in the middle of one buffer. */
class FFT
{
public:
    void prepare (std::size_t sizePowerOfTwo)
    {
        size = std::max<std::size_t> (4, sizePowerOfTwo);

        numStages = 0;
        while ((std::size_t (1) << numStages) < size)
            ++numStages;

        reversed.resize (size);
        for (std::size_t i = 0; i < size; ++i)
        {
            std::size_t r = 0;
            for (std::size_t b = 0; b < numStages; ++b)
                if ((i >> b) & 1u)
                    r |= std::size_t (1) << (numStages - 1 - b);

            reversed[i] = static_cast<std::uint32_t> (r);
        }

        // The twiddle factors of every stage, one stage after the other, so that the inner loop
        // of stage() walks through plain consecutive memory (which compilers can vectorise).
        cosTable.resize (size);
        sinTable.resize (size);

        for (std::size_t half = 1; half < size; half <<= 1)
            for (std::size_t k = 0; k < half; ++k)
            {
                const double angle = -3.14159265358979323846 * static_cast<double> (k) / static_cast<double> (half);
                cosTable[half - 1 + k] = static_cast<float> (std::cos (angle));
                sinTable[half - 1 + k] = static_cast<float> (std::sin (angle));
            }
    }

    std::size_t getSize() const noexcept      { return size; }
    std::size_t getNumStages() const noexcept { return numStages; }

    void forward (float* re, float* im) const noexcept
    {
        reorderPart (re, im, 0, size);
        for (std::size_t i = 0; i < numStages; ++i)
            stagePart (re, im, i, 0, size / 2);
    }

    /** The inverse transform is the forward one with real and imaginary parts swapped. */
    void inverse (float* re, float* im) const noexcept { forward (im, re); }

    /** Puts the elements first .. first + count - 1 into bit-reversed order. */
    void reorderPart (float* re, float* im, std::size_t first, std::size_t count) const noexcept
    {
        const std::size_t end = std::min (size, first + count);

        for (std::size_t i = first; i < end; ++i)
        {
            const std::size_t j = reversed[i];
            if (j > i)
            {
                std::swap (re[i], re[j]);
                std::swap (im[i], im[j]);
            }
        }
    }

    /** Does `count` of the size / 2 butterflies of one stage, starting with number `first`. */
    void stagePart (float* re, float* im, std::size_t stageIndex, std::size_t first, std::size_t count) const noexcept
    {
        const std::size_t half = std::size_t (1) << stageIndex;
        const float* c = cosTable.data() + (half - 1);
        const float* s = sinTable.data() + (half - 1);

        std::size_t remaining = std::min (count, size / 2 - std::min (first, size / 2));
        std::size_t group = first / half;
        std::size_t k = first % half;

        while (remaining > 0)
        {
            const std::size_t n = std::min (half - k, remaining);

            float* aRe = re + group * 2 * half + k;
            float* aIm = im + group * 2 * half + k;
            float* bRe = aRe + half;
            float* bIm = aIm + half;
            const float* cc = c + k;
            const float* ss = s + k;

            for (std::size_t i = 0; i < n; ++i)
            {
                const float tRe = bRe[i] * cc[i] - bIm[i] * ss[i];
                const float tIm = bRe[i] * ss[i] + bIm[i] * cc[i];
                bRe[i] = aRe[i] - tRe;
                bIm[i] = aIm[i] - tIm;
                aRe[i] += tRe;
                aIm[i] += tIm;
            }

            remaining -= n;
            k = 0;
            ++group;
        }
    }

private:
    std::size_t size = 0, numStages = 0;
    std::vector<std::uint32_t> reversed;
    std::vector<float> cosTable, sinTable;
};

/** Plays its input one octave higher.

    The shimmer lives or dies by this stage. A delay-line pitch shifter (read a delay line at
    double speed, cross-fade to a fresh read position every few milliseconds) is cheap, but each
    cross-fade joins two unrelated pieces of the wave: a held note comes out beating, and tuned to
    a grid set by the cross-fade rate rather than to its octave - up to a third of a semitone off.

    This one works on the spectrum instead. A steady partial of the sound shows up as a peak
    with a few bands either side that belong to it. Every band keeps its level and moves to twice
    its frequency, and each partial is turned by the phase of its peak - which doubles its phase
    and keeps its bands in step with each other. Doubling the phase is what makes consecutive
    frames line up, so a steady note comes out as a steady note, exactly an octave up. As a side
    effect nothing can alias: bands that would land above the top of the spectrum are left out.

    The frame is long (about 170 ms) so that the notes of a chord fall into separate bands. That
    also delays and blurs the octave by about as much, which is what one wants in a shimmer:
    it blooms out of the reverb instead of doubling the attack.

    Two things keep it cheap. Only the part of the input that can be shifted at all (up to
    7.5 kHz) is analysed, from every second, fourth or eighth sample depending on the sample
    rate. And the work for a frame is not done on the sample where the frame is complete, but in
    small portions over the following few milliseconds, so that no audio buffer is left holding
    the whole bill. */
class OctaveUpShifter
{
public:
    /** hopPhase (0..1) staggers the frames of several shifters. */
    void prepare (double sampleRate, float hopPhase)
    {
        // The analysis runs at 22 - 36 kHz whatever the host's rate is
        decimation = 1;
        while (sampleRate / static_cast<double> (decimation) > 36000.0 && decimation < 8)
            decimation *= 2;

        const double innerRate = sampleRate / static_cast<double> (decimation);

        frameSize = 2048;
        while (static_cast<double> (frameSize) < 0.16 * innerRate && frameSize < 16384)
            frameSize <<= 1;

        hopSize   = frameSize / 4;
        grainSize = frameSize * decimation / 2;     // a frame at double speed, in output samples

        fftIn.prepare (frameSize);
        fftOut.prepare (grainSize);

        const std::size_t workSize = std::max (frameSize, grainSize);
        re.assign (workSize, 0.0f);
        im.assign (workSize, 0.0f);
        outRe.assign (grainSize, 0.0f);
        outIm.assign (grainSize, 0.0f);
        turnRe.assign (frameSize / 2 + 1, 0.0f);
        turnIm.assign (frameSize / 2 + 1, 0.0f);
        power.assign (frameSize / 2 + 1, 0.0f);
        input.assign (frameSize, 0.0f);

        // The work for one frame, cut into portions of about the same size. One portion is
        // done every sliceInterval samples, so that the frame is finished shortly before its
        // first sample is due at the output, workDelay samples after the frame was complete.
        plan.clear();
        const auto addPortions = [this] (Portion::Kind kind, std::size_t stage, std::size_t total)
        {
            for (std::size_t first = 0; first < total; first += kPortionSize)
                plan.push_back ({ kind, stage, first, std::min (kPortionSize, total - first) });
        };

        addPortions (Portion::reorderIn, 0, frameSize);
        for (std::size_t i = 0; i < fftIn.getNumStages(); ++i)
            addPortions (Portion::stageIn, i, frameSize / 2);

        plan.push_back ({ Portion::findPartials, 0, 0, frameSize / 2 });
        addPortions (Portion::clearOut, 0, grainSize);
        addPortions (Portion::shift, 0, std::min (frameSize / 2, grainSize / 2));
        addPortions (Portion::reorderOut, 0, grainSize);
        for (std::size_t i = 0; i < fftOut.getNumStages(); ++i)
            addPortions (Portion::stageOut, i, grainSize / 2);

        addPortions (Portion::addToOutput, 0, grainSize);

        // (At ordinary sample rates the first limit applies. The second keeps a frame from
        // still being in the works when the next one is due, at rates far below any real one.)
        workDelay     = std::min (768 * decimation, (hopSize - 64) * decimation);
        sliceInterval = std::max<std::size_t> (1, (workDelay - 64 * decimation) / plan.size());

        std::size_t outSize = 16;
        while (outSize < grainSize + workDelay + 16)
            outSize <<= 1;

        output.assign (outSize, 0.0f);
        outMask = outSize - 1;

        // Hann window. At double speed a frame is half as long, so with a quarter of a frame
        // between frames the shortened frames overlap by half - where Hann windows add up to
        // exactly one, and a steady tone leaves at exactly the level it came in with.
        window.resize (frameSize);
        for (std::size_t i = 0; i < frameSize; ++i)
            window[i] = 0.5f - 0.5f * std::cos (kTwoPi * static_cast<float> (i) / static_cast<float> (frameSize));

        // What is let through, per input band. Bass is kept out (octaves of bass turn to mud).
        // Towards the top the result is turned down a little more with every octave, so that
        // the sound gets softer as it climbs, and it ends before the top of the hearing range.
        const float binHz   = static_cast<float> (innerRate) / static_cast<float> (frameSize);
        const float topFade = std::min (9000.0f, 0.30f * static_cast<float> (sampleRate));
        const float topEnd  = std::min (15000.0f, 0.46f * static_cast<float> (sampleRate));

        bandGain.assign (frameSize / 2, 0.0f);
        for (std::size_t k = 1; k < frameSize / 2; ++k)
        {
            const float inHz  = static_cast<float> (k) * binHz;
            const float outHz = 2.0f * inHz;
            const float tilt  = std::pow (std::max (1.0f, outHz / kTiltStartHz), kTiltPerOctave);

            bandGain[k] = smoothStep ((inHz - 90.0f) / (230.0f - 90.0f))
                        * (1.0f - smoothStep ((outHz - topFade) / (topEnd - topFade)))
                        * tilt
                        / static_cast<float> (frameSize);   // the scale of the two transforms
        }

        // Low-pass in front of the down-sampling: flat to 7.5 kHz, gone where it would fold back
        firCoeffs.clear();
        firHistory.assign (1, 0.0f);
        firMask = 0;

        if (decimation > 1)
        {
            const double cutoff = 0.5 * innerRate / sampleRate;                  // cycles per sample
            std::size_t taps = static_cast<std::size_t> (4.2 * sampleRate / 9000.0) | 1u;
            firCoeffs.resize (taps);

            double sum = 0.0;
            for (std::size_t i = 0; i < taps; ++i)
            {
                const double x = static_cast<double> (i) - 0.5 * static_cast<double> (taps - 1);
                const double sinc = std::abs (x) < 1.0e-9 ? 2.0 * cutoff
                                                          : std::sin (2.0 * 3.14159265358979323846 * cutoff * x) / (3.14159265358979323846 * x);
                const double w = 2.0 * 3.14159265358979323846 * static_cast<double> (i) / static_cast<double> (taps - 1);
                const double blackman = 0.42 - 0.5 * std::cos (w) + 0.08 * std::cos (2.0 * w);
                firCoeffs[i] = static_cast<float> (sinc * blackman);
                sum += sinc * blackman;
            }

            for (auto& c : firCoeffs)
                c = static_cast<float> (static_cast<double> (c) / sum);

            std::size_t historySize = 16;
            while (historySize < taps)
                historySize <<= 1;

            firHistory.assign (historySize, 0.0f);
            firMask = historySize - 1;
        }

        initialHop = std::min (hopSize - 1, static_cast<std::size_t> (std::clamp (hopPhase, 0.0f, 1.0f) * static_cast<float> (hopSize)));
        clear();
    }

    void clear() noexcept
    {
        std::fill (input.begin(), input.end(), 0.0f);
        std::fill (output.begin(), output.end(), 0.0f);
        std::fill (firHistory.begin(), firHistory.end(), 0.0f);
        inputPos = 0;
        outputPos = 0;
        firPos = 0;
        decimationCounter = 0;
        hopCounter = initialHop;
        workStep = plan.size();
        sliceCounter = 0;
    }

    /** The delay between a sound going in and its octave coming out. */
    std::size_t getLatencySamples() const noexcept
    {
        return firCoeffs.size() / 2 + frameSize * decimation / 2 + workDelay + grainSize / 2;
    }

    /** While `active` is false no new frames are started (and no work is done), but the input
        keeps being recorded, so the octave is back the moment it is asked for again. */
    float process (float in, bool active) noexcept
    {
        if (decimation == 1)
        {
            pushInner (in, active);
        }
        else
        {
            firHistory[firPos] = in;
            firPos = (firPos + 1) & firMask;

            if (++decimationCounter >= decimation)
            {
                decimationCounter = 0;

                float sum = 0.0f;
                std::size_t index = firPos;
                for (const float c : firCoeffs)
                {
                    index = (index - 1) & firMask;
                    sum += c * firHistory[index];
                }

                pushInner (sum, active);
            }
        }

        if (workStep < plan.size() && ++sliceCounter >= sliceInterval)
        {
            sliceCounter = 0;
            doPortion (plan[workStep++]);
        }

        const float out = output[outputPos];
        output[outputPos] = 0.0f;
        outputPos = (outputPos + 1) & outMask;
        return out;
    }

private:
    static constexpr float kTiltStartHz   = 1500.0f;
    static constexpr float kTiltPerOctave = -0.25f;     // as an exponent: about -1.5 dB per octave
    static constexpr std::size_t kPortionSize = 2048;

    struct Portion
    {
        enum Kind { reorderIn, stageIn, findPartials, clearOut, shift, reorderOut, stageOut, addToOutput };

        Kind kind;
        std::size_t stage, first, count;
    };

    static float smoothStep (float x) noexcept
    {
        x = std::clamp (x, 0.0f, 1.0f);
        return x * x * (3.0f - 2.0f * x);
    }

    void pushInner (float sample, bool active) noexcept
    {
        input[inputPos] = sample;
        inputPos = (inputPos + 1) & (frameSize - 1);

        if (++hopCounter >= hopSize)
        {
            hopCounter = 0;

            // (The previous frame can only still be in the works here if the numbers in
            // prepare() were changed carelessly; this frame is then skipped rather than started
            // on top of it.)
            if (active && workStep >= plan.size())
                startFrame();
        }
    }

    void startFrame() noexcept
    {
        // The last frameSize samples, oldest first
        for (std::size_t i = 0; i < frameSize; ++i)
        {
            re[i] = input[(inputPos + i) & (frameSize - 1)] * window[i];
            im[i] = 0.0f;
        }

        grainStart = (outputPos + workDelay) & outMask;
        workStep = 0;
        sliceCounter = 0;
    }

    void doPortion (const Portion& p) noexcept
    {
        switch (p.kind)
        {
            case Portion::reorderIn:
                fftIn.reorderPart (re.data(), im.data(), p.first, p.count);
                break;

            case Portion::stageIn:
                fftIn.stagePart (re.data(), im.data(), p.stage, p.first, p.count);
                break;

            case Portion::clearOut:
                std::fill_n (outRe.begin() + static_cast<std::ptrdiff_t> (p.first), p.count, 0.0f);
                std::fill_n (outIm.begin() + static_cast<std::ptrdiff_t> (p.first), p.count, 0.0f);
                break;

            case Portion::findPartials:
                findPartials (p.count);
                break;

            case Portion::shift:
                // Each band, turned by the phase of its partial. The output transform is as long
                // as the frame is at double speed, which puts band k at twice its frequency
                // without moving it.
                for (std::size_t k = std::max<std::size_t> (1, p.first); k < p.first + p.count; ++k)
                {
                    const float yRe = (re[k] * turnRe[k] - im[k] * turnIm[k]) * bandGain[k];
                    const float yIm = (re[k] * turnIm[k] + im[k] * turnRe[k]) * bandGain[k];

                    outRe[k] = yRe;
                    outIm[k] = yIm;
                    outRe[grainSize - k] = yRe;     // mirror image, so that the wave comes out real
                    outIm[grainSize - k] = -yIm;
                }
                break;

            // The inverse transform is the forward one with real and imaginary parts swapped
            case Portion::reorderOut:
                fftOut.reorderPart (outIm.data(), outRe.data(), p.first, p.count);
                break;

            case Portion::stageOut:
                fftOut.stagePart (outIm.data(), outRe.data(), p.stage, p.first, p.count);
                break;

            case Portion::addToOutput:
            {
                // What came back starts in the middle of the frame; turn it round and add it in
                const std::size_t half = grainSize / 2;

                for (std::size_t i = p.first; i < p.first + p.count; ++i)
                    output[(grainStart + i) & outMask] += outRe[(i + half) & (grainSize - 1)];

                break;
            }
        }
    }

    /** Works out, for every band, which partial it belongs to and by what it has to be turned.

        A partial is a local peak of the spectrum; its bands reach to the quietest band between
        it and the next peak. All of them are turned by the phase the peak has in the middle of
        the frame (the transform measures phase at the start of the frame, half a frame earlier,
        which flips the sign of every other band - hence the sign below). */
    void findPartials (std::size_t numBands) noexcept
    {
        for (std::size_t k = 0; k <= numBands; ++k)
            power[k] = re[k] * re[k] + im[k] * im[k];

        const auto setTurn = [this] (std::size_t firstBand, std::size_t lastBand, std::size_t peak)
        {
            const float norm = 1.0f / (std::sqrt (power[peak]) + 1.0e-30f);
            const float peakRe = re[peak] * norm;
            const float peakIm = im[peak] * norm;

            for (std::size_t k = firstBand; k <= lastBand; ++k)
            {
                const float sign = (((k + peak) & 1u) != 0) ? -1.0f : 1.0f;
                turnRe[k] = sign * peakRe;
                turnIm[k] = sign * peakIm;
            }
        };

        std::size_t regionStart = 1;        // first band that has not been given to a partial yet
        std::size_t peak = 0;               // the last peak found (0 = none so far)
        std::size_t quietest = 1;           // quietest band since that peak

        for (std::size_t k = 1; k < numBands; ++k)
        {
            if (peak != 0 && power[k] < power[quietest])
                quietest = k;

            if (power[k] > power[k - 1] && power[k] >= power[k + 1])
            {
                if (peak != 0)
                {
                    setTurn (regionStart, quietest, peak);
                    regionStart = quietest + 1;
                }

                peak = k;
                quietest = k;
            }
        }

        if (peak != 0)
        {
            setTurn (regionStart, numBands - 1, peak);
        }
        else
        {
            // no peak anywhere: silence (or a spectrum without a single bump), nothing to shift
            std::fill (turnRe.begin(), turnRe.end(), 0.0f);
            std::fill (turnIm.begin(), turnIm.end(), 0.0f);
        }
    }

    FFT fftIn, fftOut;
    std::vector<float> input, output, window, re, im, outRe, outIm, turnRe, turnIm, power, bandGain, firCoeffs, firHistory;

    std::size_t decimation = 1, frameSize = 0, hopSize = 0, grainSize = 0;
    std::size_t sliceInterval = 16, workDelay = 512, outMask = 0, firMask = 0;

    std::size_t inputPos = 0, outputPos = 0, firPos = 0, grainStart = 0;
    std::size_t decimationCounter = 0, hopCounter = 0, initialHop = 0, sliceCounter = 0;

    std::vector<Portion> plan;
    std::size_t workStep = 0;       // index of the next portion; plan.size() when idle
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
        // Some hosts probe a plug-in with a zero or garbage sample rate before the real one arrives
        if (! (newSampleRate >= 1000.0 && newSampleRate <= 1.0e6))
            newSampleRate = 44100.0;

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

        // The two shifters take turns, half a hop apart, so their frames never fall on the same sample
        shimmerL.prepare (newSampleRate, 0.0f);
        shimmerR.prepare (newSampleRate, 0.5f);

        // Smoothers. The start values are only placeholders: the first setParameters() call after
        // prepare() / clear() jumps straight to the live parameter values (see snapPending).
        smDecay.prepare (newSampleRate, 35.0f, 4.8f);
        smSize.prepare (newSampleRate, 80.0f, 1.10f);
        smPreDelay.prepare (newSampleRate, 35.0f, 0.024f * sampleRate);
        smMix.prepare (newSampleRate, 20.0f, 0.38f);
        smShimmer.prepare (newSampleRate, 30.0f, 0.28f);
        smDrift.prepare (newSampleRate, 30.0f, 0.45f);
        smWarmth.prepare (newSampleRate, 30.0f, 0.55f);
        smWidth.prepare (newSampleRate, 20.0f, 1.15f);
        smFreeze.prepare (newSampleRate, 25.0f, 0.0f);
        smFreezeHold.prepare (newSampleRate, 300.0f, 0.0f);

        // Detuned slow LFO rates in Hz
        constexpr std::array<float, kNumChannels> lfoHz { 0.19f, 0.23f, 0.29f, 0.34f, 0.41f, 0.46f, 0.52f, 0.59f };
        for (std::size_t i = 0; i < kNumChannels; ++i)
            lfoInc[i] = kTwoPi * lfoHz[i] / sampleRate;

        // Time constants that used to be hard-coded per-sample factors, so they no longer change
        // with the sample rate
        meterCoeff      = 1.0f - std::exp (-1.0f / (0.010f * sampleRate));
        holdOffsetCoeff = 1.0f - std::exp (-static_cast<float> (kControlInterval) / (0.100f * sampleRate));

        clear();
    }

    /** Empties every delay line and filter. The next setParameters() call then jumps straight to
        the live parameter values instead of gliding there from stale ones. */
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

            // The Drift movement starts from the same place every time, so that rendering the
            // same passage twice gives the same result twice.
            lfoPhase[i] = static_cast<float> (i) * (kTwoPi / static_cast<float> (kNumChannels));
        }

        shimmerL.clear();
        shimmerR.clear();
        wetEnv = 0.0f;
        shimEnv = 0.0f;
        lastWet = 0.0f;
        lastShimmer = 0.0f;
        controlCounter = 0;
        snapPending = true;
    }

    void setParameters (const ReverbParams& p) noexcept
    {
        smDecay.setTarget (clampOr (p.decaySeconds, 0.4f, 18.0f, 4.8f));
        smSize.setTarget (clampOr (p.sizePercent * 0.01f, 0.35f, 1.85f, 1.10f));
        smPreDelay.setTarget (clampOr (p.preDelayMs * 0.001f, 0.0f, 0.22f, 0.024f) * sampleRate);
        smMix.setTarget (clampOr (p.mixPercent * 0.01f, 0.0f, 1.0f, 0.38f));
        smShimmer.setTarget (clampOr (p.shimmerPct * 0.01f, 0.0f, 1.0f, 0.28f));
        smDrift.setTarget (clampOr (p.driftPct * 0.01f, 0.0f, 1.0f, 0.45f));
        smWarmth.setTarget (clampOr (p.warmthPct * 0.01f, 0.0f, 1.0f, 0.55f));
        smWidth.setTarget (clampOr (p.widthPercent * 0.01f, 0.0f, 1.5f, 1.15f));
        smFreeze.setTarget (p.freeze ? 1.0f : 0.0f);
        smFreezeHold.setTarget (p.freeze ? 1.0f : 0.0f);

        if (snapPending)
        {
            // First block after prepare() / clear(): the tank is empty, so there is nothing to
            // glide from. Starting at the live values avoids a burst of dry signal at 100 % wet,
            // a faded-in first transient, a pitch glide and input leaking into a frozen tank.
            snapPending = false;

            for (auto* sm : { &smDecay, &smSize, &smPreDelay, &smMix, &smShimmer,
                              &smDrift, &smWarmth, &smWidth, &smFreeze, &smFreezeHold })
                sm->snapToTarget();

            for (std::size_t i = 0; i < kNumChannels; ++i)
                holdOffset[i] = wholeSampleOffset (i, smSize.getTarget());

            controlCounter = 0; // refresh the cached coefficients on the very next sample
        }
    }

    void processSample (float inL, float inR, float& outL, float& outR) noexcept
    {
        // A single NaN / Inf sample (e.g. from a misbehaving plug-in upstream) would otherwise
        // circulate in the feedback network forever and silence the plug-in until it is reloaded.
        if (! isFiniteSample (inL)) inL = 0.0f;
        if (! isFiniteSample (inR)) inR = 0.0f;

        const float decaySec    = smDecay.getNext();
        const float sizeScale   = smSize.getNext();
        const float preDelaySmp = smPreDelay.getNext();
        const float mix         = smMix.getNext();
        const float shimmerAmt  = smShimmer.getNext();
        const float driftAmt    = smDrift.getNext();
        const float warmthNorm  = smWarmth.getNext();
        const float width       = smWidth.getNext();
        const float freezeBlend = smFreeze.getNext();
        const float freezeHold  = smFreezeHold.getNext();

        // Update expensive filter / decay coefficients every 16 samples
        if (controlCounter == 0)
            updateCoefficients (decaySec, sizeScale, warmthNorm, freezeBlend);
        controlCounter = (controlCounter + 1) & (kControlInterval - 1);

        // 1. Pre-delay + gentle input stereo cross-blend + mud HPF
        preDelayL.push (std::clamp (inL, -kMaxTankInput, kMaxTankInput));
        preDelayR.push (std::clamp (inR, -kMaxTankInput, kMaxTankInput));
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
        //    d[]   is what recirculates through the feedback matrix,
        //    tap[] is what is sent to the wet output.
        //    They are the same read unless Freeze is (partly) engaged: a frozen loop reads at a
        //    whole number of samples, because the fractional interpolation needed for the Drift
        //    modulation is slightly lossy at high frequencies and would dull the held cloud over
        //    time. The Drift movement is then applied to the output taps only.
        const float modDepthSamples = driftAmt * 0.0022f * sampleRate; // up to +/- 2.2 ms
        const bool  holdingLoop     = freezeHold > 0.0f;
        std::array<float, kNumChannels> d {};
        std::array<float, kNumChannels> tap {};

        for (std::size_t i = 0; i < kNumChannels; ++i)
        {
            const float lfo = std::sin (lfoPhase[i]);
            lfoPhase[i] += lfoInc[i];
            if (lfoPhase[i] >= kTwoPi)
                lfoPhase[i] -= kTwoPi;

            const float baseSamples = baseDelaySamples (i, sizeScale);
            const float modulation  = lfo * modDepthSamples;

            tap[i] = fdnDelays[i].readCubicBeforePush (std::max (8.0f, baseSamples + modulation));

            if (holdingLoop)
            {
                const float loopDelay = baseSamples + freezeHold * holdOffset[i] + (1.0f - freezeHold) * modulation;
                d[i] = fdnDelays[i].readCubicBeforePush (std::max (8.0f, loopDelay));
            }
            else
            {
                d[i] = tap[i];
            }
        }

        // 4. Filter inside the tank. Freeze fades the DC blocker and the damping out together so
        //    that the frozen loop is exactly lossless (the DC blocker alone has a pass-band gain
        //    slightly below 1 and used to drain a "frozen" tail by roughly 0.1 dB per second).
        const float tankLoss = 1.0f - freezeBlend;
        std::array<float, kNumChannels> filtered {};
        for (std::size_t i = 0; i < kNumChannels; ++i)
        {
            const float dcClean = dcBlockers[i].process (d[i]);
            const float damped  = dampingLP[i].process (dcClean);
            filtered[i] = d[i] - tankLoss * (d[i] - damped);
        }

        // 5. Fast 8-point normalized Walsh-Hadamard mixing matrix (strictly unitary)
        std::array<float, kNumChannels> mixed {};
        applyHadamard8 (filtered, mixed);

        // 6. Apply RT60 decay gains
        std::array<float, kNumChannels> decayed {};
        for (std::size_t i = 0; i < kNumChannels; ++i)
            decayed[i] = cachedLoopGain[i] * mixed[i];

        // 7. Shimmer: what the tank is sending to the output, one octave up, goes back into
        //    the tank. Nothing is taken away from the reverb itself to make room for it, so
        //    turning Shimmer up adds octaves on top of the tail instead of cutting the tail short.
        //    Each trip round the tank raises the sound by another octave; it cannot run away,
        //    because nothing ever comes back at the pitch it went in with, and the top is damped.
        const float tapL = 0.5f * (tap[0] - tap[2] + tap[4] - tap[6]);
        const float tapR = 0.5f * (tap[1] + tap[3] - tap[5] - tap[7]);

        const float shimmerGain = std::min (kMaxShimmerGain, shimmerAmt * cachedShimmerScale) * (1.0f - freezeBlend);
        const bool  shimmerOn   = shimmerGain > 0.0f;
        const float shimL = shimmerGain * shimmerL.process (tapL, shimmerOn);
        const float shimR = shimmerGain * shimmerR.process (tapR, shimmerOn);

        // 8. Inject the diffused input and the shimmer into the FDN, on top of the feedback
        constexpr std::array<float, kNumChannels> polarity { 1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f, 1.0f };
        const float inputDrive = 0.5f * cachedInputScale * (1.0f - freezeBlend);

        for (std::size_t i = 0; i < kNumChannels; ++i)
        {
            const float injected = (i % 2 == 0) ? inputDrive * diffL + shimL
                                                : inputDrive * diffR + shimR;
            fdnDelays[i].push (softLimitSafe (polarity[i] * injected + decayed[i]));
        }

        // 9. Stereo wet output + Mid/Side width
        const float mid  = 0.5f * (tapL + tapR);
        const float side = 0.5f * (tapL - tapR) * width;
        const float wetL = (mid + side) * 0.65f;
        const float wetR = (mid - side) * 0.65f;

        // 10. Equal-power Dry/Wet blend
        const float angle   = mix * (0.5f * kPi);
        const float dryGain = std::cos (angle);
        const float wetGain = std::sin (angle);

        outL = dryGain * inL + wetGain * wetL;
        outR = dryGain * inR + wetGain * wetR;

        // For the display: the reverb on its own, and the octaves being fed back into it
        lastWet     = 0.5f * (wetL + wetR);
        lastShimmer = 0.5f * (shimL + shimR);

        const float wetEnergy = 0.5f * (std::abs (wetL) + std::abs (wetR));
        const float shimEnergy = 0.5f * (std::abs (shimL) + std::abs (shimR));
        wetEnv  += meterCoeff * (wetEnergy - wetEnv);
        shimEnv += meterCoeff * (shimEnergy - shimEnv);
    }

    /** The reverb signal of the sample just processed, before it is mixed with the dry signal. */
    float getLastWetSample() const noexcept     { return lastWet; }

    /** The octave-up signal that went back into the tank with the sample just processed. */
    float getLastShimmerSample() const noexcept { return lastShimmer; }

    /** How long the shimmer takes to answer: a sound's octave appears this much later. */
    float getShimmerDelaySeconds() const noexcept
    {
        return static_cast<float> (shimmerL.getLatencySamples()) / sampleRate;
    }

    float getWetMeterLevel() const noexcept { return wetEnv; }
    float getShimmerMeterLevel() const noexcept { return shimEnv; }

    /** False if a non-finite value has made it into the tank after all. The envelope followers
        integrate everything that leaves the tank, so they are a cheap once-per-block probe. */
    bool isHealthy() const noexcept { return isFiniteSample (wetEnv) && isFiniteSample (shimEnv); }

private:
    static constexpr std::size_t kNumChannels = 8;
    static constexpr int kControlInterval = 16; // must be a power of two
    static constexpr float kMaxTankInput = 1.0e4f; // +80 dBFS: only ever touches garbage input

    // Shimmer at 100 %: every octave carries about this many times the energy of the one below
    // it, before the damping has its say. Above 1 the sound keeps climbing while it rings.
    static constexpr float kShimmerEnergyRatio = 1.2f;
    static constexpr float kMaxShimmerGain = 0.9f;
    static constexpr std::array<float, kNumChannels> kBaseDelaysMs {
        31.7f, 37.3f, 43.9f, 52.1f, 61.7f, 73.3f, 87.7f, 103.3f
    };

    float baseDelaySamples (std::size_t channel, float sizeScale) const noexcept
    {
        return kBaseDelaysMs[channel] * 0.001f * sampleRate * sizeScale;
    }

    /** Distance from a channel's (generally fractional) delay to the nearest whole sample. */
    float wholeSampleOffset (std::size_t channel, float sizeScale) const noexcept
    {
        const float base = baseDelaySamples (channel, sizeScale);
        return std::round (base) - base;
    }

    void updateCoefficients (float decaySec, float sizeScale, float warmthNorm, float freezeBlend) noexcept
    {
        const float dampFreq = 16000.0f * std::pow (1200.0f / 16000.0f, warmthNorm);
        const float tankLoss = 1.0f - freezeBlend;
        float gSum = 0.0f;

        for (std::size_t i = 0; i < kNumChannels; ++i)
        {
            dampingLP[i].setCutoff (dampFreq, sampleRate);
            const float delaySec = kBaseDelaysMs[i] * 0.001f * sizeScale;
            const float normalGain = std::pow (10.0f, (-3.0f * delaySec) / decaySec);
            gSum += normalGain;
            cachedLoopGain[i] = 1.0f - tankLoss * (1.0f - normalGain); // exactly 1 when frozen

            // Where the frozen loop has to read to land on a whole sample. It follows the Size
            // *target*, so the loop becomes exactly lossless as soon as Size has stopped moving.
            holdOffset[i] = stepTowards (holdOffset[i], wholeSampleOffset (i, smSize.getTarget()), holdOffsetCoeff);
        }

        const float gAvg = gSum / static_cast<float> (kNumChannels);
        const float oneMinusG2 = std::max (0.01f, 1.0f - gAvg * gAvg);
        cachedInputScale = std::clamp (std::pow (oneMinusG2, 0.35f), 0.22f, 0.85f);

        // How hard the octave is fed back for a given Shimmer setting. A long tail collects what
        // is fed into it for longer, so it needs less per trip to reach the same balance between
        // the reverb and its octaves; a larger tank holds more and needs more. In numbers: the
        // octave ends up with (gain^2 * decay / size) times the energy of what it is made from.
        //
        // An octave takes a moment to arrive (the shifter's delay plus a trip through the tank).
        // With a short decay that moment is long compared with the tail, and octave after octave
        // would trail on well after the reverb itself has gone. The last factor turns the
        // shimmer down as the decay gets too short for it, so that the octaves fade at least as
        // quickly as a reverb of twice the decay time would.
        const float octaveDelaySec = getShimmerDelaySeconds() + 0.06f * sizeScale;
        cachedShimmerScale = std::sqrt (kShimmerEnergyRatio * (sizeScale / decaySec)
                                          * std::pow (10.0f, -3.0f * octaveDelaySec / decaySec));
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
    std::array<float, kNumChannels> holdOffset {};

    OctaveUpShifter shimmerL, shimmerR;

    SmoothedFloat smDecay, smSize, smPreDelay, smMix, smShimmer, smDrift, smWarmth, smWidth, smFreeze;
    SmoothedFloat smFreezeHold; // slow twin of smFreeze: glides the loop taps onto whole samples

    float sampleRate = 48000.0f;
    float cachedInputScale = 0.5f;
    float cachedShimmerScale = 0.5f;
    float lastWet = 0.0f;
    float lastShimmer = 0.0f;
    float meterCoeff = 0.002f;
    float holdOffsetCoeff = 0.003f;
    float wetEnv = 0.0f;
    float shimEnv = 0.0f;
    int   controlCounter = 0;
    bool  snapPending = true;
};

} // namespace cloudveil
