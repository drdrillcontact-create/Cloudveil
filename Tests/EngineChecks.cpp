// Checks on the reverb engine by itself (no JUCE, no host): what the shimmer does to a note, that
// the tail behaves, and that the engine survives whatever is thrown at it.
//
//   Tests/run-engine-checks.sh
#include "../Source/DreamyReverbDSP.h"
#include <complex>
#include <cstdio>
#include <limits>
#include <random>
#include <string>

using namespace cloudveil;

namespace
{
int failures = 0;

void check (bool ok, const std::string& what)
{
    std::printf ("  [%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (! ok) ++failures;
}

std::string num (double value, int decimals = 2)
{
    char text[64];
    std::snprintf (text, sizeof (text), "%.*f", decimals, value);
    return text;
}

constexpr double pi = 3.14159265358979323846;

double dB (double x) { return 20.0 * std::log10 (std::max (x, 1.0e-12)); }

struct Stereo { std::vector<float> l, r; };

double rms (const std::vector<float>& v, std::size_t a, std::size_t b)
{
    b = std::min (b, v.size());
    if (b <= a) return 0.0;
    double sum = 0.0;
    for (std::size_t i = a; i < b; ++i) sum += double (v[i]) * v[i];
    return std::sqrt (sum / double (b - a));
}

double peak (const Stereo& s)
{
    double p = 0.0;
    for (float x : s.l) p = std::max (p, (double) std::abs (x));
    for (float x : s.r) p = std::max (p, (double) std::abs (x));
    return p;
}

bool allFinite (const Stereo& s)
{
    for (float x : s.l) if (! std::isfinite (x)) return false;
    for (float x : s.r) if (! std::isfinite (x)) return false;
    return true;
}

// White noise for `on` seconds, then silence up to `total` seconds
Stereo noise (double sr, double on, double total, float gain, unsigned seed)
{
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> u (-1.0f, 1.0f);
    Stereo s;
    s.l.assign ((std::size_t) (sr * total), 0.0f);
    s.r = s.l;
    for (std::size_t i = 0; i < (std::size_t) (sr * on); ++i) { s.l[i] = gain * u (rng); s.r[i] = gain * u (rng); }
    return s;
}

Stereo run (DreamyReverbEngine& e, const ReverbParams& p, const Stereo& in, std::size_t block = 256)
{
    Stereo out;
    out.l.resize (in.l.size());
    out.r.resize (in.l.size());
    for (std::size_t i = 0; i < in.l.size(); ++i)
    {
        if (i % block == 0) e.setParameters (p);
        e.processSample (in.l[i], in.r[i], out.l[i], out.r[i]);
    }
    return out;
}

// Amplitude spectrum (Blackman-Harris window, side lobes below -92 dB); a full-scale sine reads 1
std::vector<double> spectrum (const std::vector<float>& v, std::size_t start, std::size_t n)
{
    FFT fft;
    fft.prepare (n);
    std::vector<float> re (n), im (n, 0.0f);
    for (std::size_t i = 0; i < n; ++i)
    {
        const double x = 2.0 * pi * double (i) / double (n);
        re[i] = (float) (double (v[start + i]) * (0.35875 - 0.48829 * std::cos (x) + 0.14128 * std::cos (2 * x) - 0.01168 * std::cos (3 * x)));
    }
    fft.forward (re.data(), im.data());
    std::vector<double> mag (n / 2);
    for (std::size_t k = 0; k < n / 2; ++k) mag[k] = std::sqrt (double (re[k]) * re[k] + double (im[k]) * im[k]) * 2.0 / (0.35875 * double (n));
    return mag;
}

// Schroeder decay time: -5 to -35 dB, doubled
double decayTime (const std::vector<float>& v, std::size_t start, double sr)
{
    std::vector<double> remaining (v.size() - start);
    double sum = 0.0;
    for (std::size_t i = v.size(); i-- > start;) { sum += double (v[i]) * v[i]; remaining[i - start] = sum; }
    if (sum <= 0.0) return 0.0;
    double t5 = -1.0, t35 = -1.0;
    for (std::size_t i = 0; i < remaining.size(); ++i)
    {
        const double level = 10.0 * std::log10 (remaining[i] / sum + 1.0e-30);
        if (t5 < 0.0 && level <= -5.0) t5 = double (i) / sr;
        if (level <= -35.0) { t35 = double (i) / sr; break; }
    }
    return (t5 >= 0.0 && t35 >= 0.0) ? 2.0 * (t35 - t5) : -1.0;
}

//==============================================================================
void shifterTurnsNotesIntoTheirOctave()
{
    std::printf ("the shimmer's octave shifter, fed a steady note\n");

    double worstLevelError = 0.0, worstRest = -200.0, worstWobble = 0.0;
    std::string worstCase;

    // (11.025 kHz is lower than any host goes; it is there because the shifter's timing is
    // tightest at low rates.)
    for (double sr : { 11025.0, 44100.0, 48000.0, 96000.0 })
        for (double f : { 261.63, 329.63, 392.0, 440.0, 455.0, 587.33 })      // octaves below 1.5 kHz: no tilt yet
        {
            OctaveUpShifter shifter;
            shifter.prepare (sr, 0.0f);

            const std::size_t n = (std::size_t) (4.0 * sr);
            std::vector<float> out (n);
            for (std::size_t i = 0; i < n; ++i)
                out[i] = shifter.process ((float) std::sin (2.0 * pi * f * double (i) / sr), true);

            const std::size_t fftSize = sr > 50000.0 ? 131072 : (sr > 20000.0 ? 65536 : 16384);
            const auto spec = spectrum (out, n - fftSize - 100, fftSize);
            const std::size_t bin = (std::size_t) std::llround (2.0 * f * double (fftSize) / sr);

            double atOctave = 0.0, rest = 0.0;
            for (std::size_t k = 1; k < spec.size(); ++k)
                ((k + 5 >= bin && k <= bin + 5) ? atOctave : rest) += spec[k] * spec[k];

            // level of the octave over the last second: demodulate at 2f, 5 ms windows
            double lowest = 1.0e9, highest = 0.0;
            const std::size_t slice = (std::size_t) (0.005 * sr);
            for (std::size_t a = n - (std::size_t) sr; a + slice < n; a += slice / 2)
            {
                double c = 0.0, s = 0.0;
                for (std::size_t i = a; i < a + slice; ++i)
                {
                    const double w = 0.5 - 0.5 * std::cos (2.0 * pi * double (i - a) / double (slice));
                    const double phase = 2.0 * pi * 2.0 * f * double (i) / sr;
                    c += w * out[i] * std::cos (phase);
                    s += w * out[i] * std::sin (phase);
                }
                const double level = std::sqrt (c * c + s * s);
                lowest = std::min (lowest, level);
                highest = std::max (highest, level);
            }

            const double levelError = std::abs (10.0 * std::log10 (atOctave / 2.0044));
            const double restLevel  = 10.0 * std::log10 (rest / 2.0044 + 1.0e-30);
            const double wobble     = dB (highest / lowest);

            if (levelError > worstLevelError || restLevel > worstRest || wobble > worstWobble)
                worstCase = num (f) + " Hz at " + num (sr, 0) + " Hz";

            worstLevelError = std::max (worstLevelError, levelError);
            worstRest = std::max (worstRest, restLevel);
            worstWobble = std::max (worstWobble, wobble);
        }

    check (worstLevelError < 0.05, "the octave comes out at the level the note went in  (largest error " + num (worstLevelError, 3) + " dB)");
    check (worstRest < -55.0, "and nothing else to speak of  (everything that is not the octave: " + num (worstRest, 1) + " dB at worst)");
    check (worstWobble < 0.1, "at a steady level  (largest wobble " + num (worstWobble, 3) + " dB)");
}

void shimmerDoesNotShortenTheTail()
{
    std::printf ("shimmer and the length of the tail (Decay 10 s)\n");
    const double sr = 48000.0;
    double times[3] {};
    int index = 0;

    for (float shimmer : { 0.0f, 50.0f, 100.0f })
    {
        DreamyReverbEngine e;
        e.prepare (sr);
        ReverbParams p;
        p.decaySeconds = 10.0f; p.sizePercent = 110.0f; p.preDelayMs = 0.0f; p.mixPercent = 100.0f;
        p.shimmerPct = shimmer; p.driftPct = 45.0f; p.warmthPct = 55.0f; p.widthPercent = 100.0f;
        const auto out = run (e, p, noise (sr, 0.5, 40.0, 0.5f, 5));
        times[index++] = decayTime (out.l, (std::size_t) (0.5 * sr), sr);
    }

    check (times[1] >= times[0] && times[2] >= times[1],
           "more shimmer never makes the tail shorter  (" + num (times[0]) + " s, " + num (times[1]) + " s, " + num (times[2]) + " s at 0 / 50 / 100 %)");
    check (times[2] < 2.0 * times[0], "nor endless  (at most twice as long)");
}

void shimmerAddsOctavesOfAChord()
{
    std::printf ("a C major chord held through the reverb, Shimmer 60 %%\n");
    const double sr = 48000.0;
    DreamyReverbEngine e;
    e.prepare (sr);
    ReverbParams p;
    // (A short decay and a long note, so that what is measured is the chord and its octaves and
    // not the tank still ringing from the moment the chord began.)
    p.decaySeconds = 1.5f; p.sizePercent = 110.0f; p.preDelayMs = 0.0f; p.mixPercent = 100.0f;
    p.shimmerPct = 60.0f; p.driftPct = 0.0f; p.warmthPct = 30.0f; p.widthPercent = 100.0f;

    const double notes[] = { 261.63, 329.63, 392.0 };
    Stereo in;
    in.l.assign ((std::size_t) (8.0 * sr), 0.0f);
    for (std::size_t i = 0; i < in.l.size(); ++i)
    {
        const double fadeIn = std::min (1.0, double (i) / (0.2 * sr));
        for (double f : notes)
            in.l[i] += (float) (0.15 * fadeIn * std::sin (2.0 * pi * f * double (i) / sr));
    }
    in.r = in.l;

    const auto out = run (e, p, in);
    const std::size_t fftSize = 65536;
    const auto spec = spectrum (out.l, in.l.size() - fftSize - 100, fftSize);
    const double binHz = sr / double (fftSize);

    // Around each octave (and the octave above it): the strongest thing within a semitone has to
    // be the octave itself, in tune, and whatever else is in there has to be far below it.
    // (A delay-line shifter puts a pair of beating tones there instead, up to 15 Hz off.)
    double worstTuning = 0.0, worstMargin = 200.0;

    for (double f : notes)
        for (double target : { 2.0 * f, 4.0 * f })
        {
            const auto first = (std::size_t) (target / 1.0595 / binHz), last = (std::size_t) (target * 1.0595 / binHz);
            std::size_t strongest = first;
            for (std::size_t k = first; k <= last; ++k)
                if (spec[k] > spec[strongest]) strongest = k;

            double rest = 0.0;
            for (std::size_t k = first; k <= last; ++k)
                if (k + 5 < strongest || k > strongest + 5) rest = std::max (rest, spec[k]);

            worstTuning = std::max (worstTuning, std::abs (double (strongest) * binHz - target));
            worstMargin = std::min (worstMargin, dB (spec[strongest] / rest));
        }

    check (worstTuning <= binHz, "every note has its octave and the octave above that, in tune  (largest offset " + num (worstTuning) + " Hz)");
    check (worstMargin > 20.0, "with nothing else near them  (the next thing within a semitone is at least " + num (worstMargin, 0) + " dB down)");
}

void stressAtTheExtremes()
{
    std::printf ("everything turned up: Decay 18 s, Size 185 %%, Shimmer 100 %%, no damping, full-scale noise\n");

    for (double sr : { 44100.0, 96000.0 })
    {
        DreamyReverbEngine e;
        e.prepare (sr);
        ReverbParams p;
        p.decaySeconds = 18.0f; p.sizePercent = 185.0f; p.preDelayMs = 0.0f; p.mixPercent = 100.0f;
        p.shimmerPct = 100.0f; p.driftPct = 100.0f; p.warmthPct = 0.0f; p.widthPercent = 150.0f;
        const auto out = run (e, p, noise (sr, 10.0, 70.0, 1.0f, 7));
        const double end = dB (rms (out.l, (std::size_t) (69.0 * sr), (std::size_t) (70.0 * sr)));
        check (allFinite (out) && peak (out) < 4.0 && end < -100.0,
               num (sr, 0) + " Hz: stays bounded and dies away  (peak " + num (peak (out)) + ", " + num (end, 0) + " dB one minute after the input stops)");
    }
}

void freezeHoldsItsLevel()
{
    std::printf ("Freeze\n");
    double worst = 0.0;

    for (double sr : { 44100.0, 48000.0 })
        for (float size : { 77.5f, 185.0f })
        {
            DreamyReverbEngine e;
            e.prepare (sr);
            ReverbParams p;
            p.decaySeconds = 3.0f; p.sizePercent = size; p.preDelayMs = 0.0f; p.mixPercent = 100.0f;
            p.shimmerPct = 60.0f; p.driftPct = 0.0f; p.warmthPct = 20.0f; p.widthPercent = 100.0f;
            run (e, p, noise (sr, 1.5, 1.5, 0.5f, 3));
            p.freeze = true;

            Stereo quiet;
            quiet.l.assign ((std::size_t) (4.0 * sr), 0.0f);
            quiet.r = quiet.l;

            run (e, p, quiet);                                      // let the transition finish
            const auto first = run (e, p, quiet);
            for (int i = 0; i < 28; ++i) run (e, p, quiet);         // about two minutes
            const auto last = run (e, p, quiet);
            worst = std::max (worst, std::abs (dB (rms (last.l, 0, last.l.size())) - dB (rms (first.l, 0, first.l.size()))));
        }

    check (worst < 0.05, "a frozen cloud keeps its level for two minutes  (largest change " + num (worst, 3) + " dB)");
}

void sameResultHoweverItIsCalled()
{
    std::printf ("repeatability\n");
    const double sr = 48000.0;
    ReverbParams p;
    p.shimmerPct = 70.0f;
    const auto in = noise (sr, 1.0, 3.0, 0.4f, 11);

    DreamyReverbEngine a, b, c;
    a.prepare (sr); b.prepare (sr); c.prepare (sr);
    const auto outA = run (a, p, in, 1);
    const auto outB = run (b, p, in, 4096);
    check (outA.l == outB.l && outA.r == outB.r, "the output does not depend on the host's buffer size");

    run (c, p, noise (sr, 2.0, 2.5, 0.8f, 99));
    c.clear();
    const auto outC = run (c, p, in, 4096);
    check (outC.l == outB.l && outC.r == outB.r, "after clear() the engine is as good as new");
}

void survivesGarbage()
{
    std::printf ("garbage in\n");
    const double sr = 48000.0;
    DreamyReverbEngine e;
    e.prepare (sr);
    ReverbParams p;
    p.shimmerPct = 80.0f; p.mixPercent = 100.0f;

    auto in = noise (sr, 2.0, 4.0, 0.4f, 21);
    in.l[1000] = std::numeric_limits<float>::quiet_NaN();
    in.r[2000] = std::numeric_limits<float>::infinity();
    in.l[3000] = -std::numeric_limits<float>::infinity();
    const auto out = run (e, p, in);
    check (allFinite (out) && e.isHealthy() && rms (out.l, (std::size_t) sr, (std::size_t) (2.0 * sr)) > 0.01,
           "NaN and infinity in the input neither get out nor silence the reverb");

    bool ok = true;
    for (double rate : { 0.0, -1.0, 8000.0, 22050.0, 32000.0, 176400.0, 192000.0, 384000.0, std::numeric_limits<double>::quiet_NaN() })
    {
        DreamyReverbEngine odd;
        odd.prepare (rate);
        ReverbParams q;
        q.shimmerPct = 100.0f; q.mixPercent = 100.0f;
        q.decaySeconds = std::numeric_limits<float>::quiet_NaN();
        q.sizePercent = 1.0e9f;
        const double used = (rate >= 1000.0 && rate <= 1.0e6) ? rate : 44100.0;
        const auto result = run (odd, q, noise (used, 0.25, 0.75, 0.5f, 5));
        ok = ok && allFinite (result) && peak (result) < 4.0;
    }
    check (ok, "odd sample rates (0, 8 kHz ... 384 kHz, NaN) and nonsense parameter values are survived");
}

void dryAndWetAreWhatTheySay()
{
    std::printf ("Mix\n");
    const double sr = 48000.0;
    const auto in = noise (sr, 0.5, 0.5, 0.5f, 31);

    DreamyReverbEngine dry;
    dry.prepare (sr);
    ReverbParams p;
    p.mixPercent = 0.0f;
    const auto outDry = run (dry, p, in);
    check (outDry.l == in.l && outDry.r == in.r, "0 % is the untouched input, from the first sample on");

    DreamyReverbEngine wet;
    wet.prepare (sr);
    p.mixPercent = 100.0f; p.preDelayMs = 20.0f;
    const auto outWet = run (wet, p, in);
    check (rms (outWet.l, 0, (std::size_t) (0.015 * sr)) < 1.0e-6, "100 % lets no dry signal through, from the first sample on");
}
}

int main()
{
    shifterTurnsNotesIntoTheirOctave();
    shimmerAddsOctavesOfAChord();
    shimmerDoesNotShortenTheTail();
    stressAtTheExtremes();
    freezeHoldsItsLevel();
    sameResultHoweverItIsCalled();
    survivesGarbage();
    dryAndWetAreWhatTheySay();

    std::printf ("\n%s\n", failures == 0 ? "ALL PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
