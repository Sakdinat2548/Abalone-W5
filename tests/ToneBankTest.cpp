#include "../src/dsp/ToneBank.h"
#include "../analysis/tone_targets.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace
{

constexpr double kPi = 3.14159265358979323846;

// NOTE: MSVC Release builds define NDEBUG, which neuters assert(). These
// tests must verify in Release (ctest -C Release), so REQUIRE stays active.
int gFailures = 0;

#define REQUIRE(cond)                                                                                                  \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(cond))                                                                                                   \
        {                                                                                                              \
            std::printf ("REQUIRE failed %s:%d: %s\n", __FILE__, __LINE__, #cond);                                     \
            ++gFailures;                                                                                               \
        }                                                                                                              \
    } while (0)

#if !defined(TONE_CSV_PATH)
#define TONE_CSV_PATH "../analysis/u5_tone_curves_digitized.csv"
#endif

struct DenseCurve
{
    std::vector<float> freqHz;
    std::vector<float> db[7]; // index 1..6
};

bool loadDenseCsv (const char* path, DenseCurve& out)
{
    std::FILE* f = std::fopen (path, "r");
    if (f == nullptr)
        return false;

    char line[1024];
    if (std::fgets (line, sizeof (line), f) == nullptr)
    {
        std::fclose (f);
        return false;
    }

    while (std::fgets (line, sizeof (line), f) != nullptr)
    {
        double freq = 0.0;
        double v[6] = {0};
        if (std::sscanf (line, "%lf,%lf,%lf,%lf,%lf,%lf,%lf", &freq, &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 7)
            continue;
        out.freqHz.push_back (static_cast<float> (freq));
        for (int t = 1; t <= 6; ++t)
            out.db[t].push_back (static_cast<float> (v[t - 1]));
    }
    std::fclose (f);
    return out.freqHz.size() > 100;
}

// Log-frequency linear interpolation of the dense digitized curve.
float denseAt (const DenseCurve& csv, int tone, float freqHz)
{
    const std::vector<float>& f = csv.freqHz;
    const std::vector<float>& d = csv.db[tone];
    const auto n = f.size();
    REQUIRE (n > 0);
    if (freqHz <= f.front())
        return d.front();
    if (freqHz >= f.back())
        return d.back();

    const double lf = std::log (static_cast<double> (freqHz));
    for (size_t i = 1; i < n; ++i)
    {
        if (f[i] >= freqHz)
        {
            const double l0 = std::log (static_cast<double> (f[i - 1]));
            const double l1 = std::log (static_cast<double> (f[i]));
            const double t = (lf - l0) / (l1 - l0);
            return static_cast<float> ((1.0 - t) * d[i - 1] + t * d[i]);
        }
    }
    return d.back();
}

float sineRmsDb (ToneBank& bank, double sampleRate, double freqHz)
{
    const int settle = static_cast<int> (sampleRate * 1.0);
    const int measure = static_cast<int> (sampleRate * 0.5);
    double sumIn = 0.0;
    double sumOut = 0.0;
    for (int i = 0; i < settle + measure; ++i)
    {
        const float x = 0.5f * static_cast<float> (std::sin (2.0 * kPi * freqHz * i / sampleRate));
        const float y = bank.processSample (x);
        REQUIRE (std::isfinite (y));
        if (i >= settle)
        {
            sumIn += static_cast<double> (x) * x;
            sumOut += static_cast<double> (y) * y;
        }
    }
    return 20.0f * static_cast<float> (std::log10 (std::sqrt (sumOut / sumIn)));
}

// Measured IR shapes (Task 12 leg, ADVISORY since Task 15): Welch
// cross-spectral relative shapes of AVALON_TONEn.wav vs AVALON_TONE0.wav
// (highcut-off captures only, per user Ruling A), 1 kHz-normalized, in dB.
// Recomputed fresh from the WAVs 2026-10-02 (coherence >= 0.9997 in-band); do
// not hand-edit — see analysis/IR_VALIDATION.md section 5 table (b). The
// Task-15 blend trusts these captures only lightly: deltas are REPORTED
// here, not gated; the manual chart is binding again.
struct IrSpot
{
    int tone;
    float freqHz;
    float db;
};

constexpr IrSpot kIrSpots[] = {
    {1, 40.0f, 6.60f},     {1, 63.0f, 6.37f},     {1, 100.0f, 5.38f},    {1, 150.0f, 3.99f},    {1, 250.0f, 2.05f},
    {1, 400.0f, 0.71f},    {1, 630.0f, 0.05f},    {1, 1000.0f, 0.00f},   {1, 1600.0f, 0.59f},   {1, 2500.0f, 1.83f},
    {1, 4000.0f, 3.76f},   {1, 6300.0f, 5.77f},   {1, 10000.0f, 7.39f},  {1, 15000.0f, 8.29f},  {2, 40.0f, 12.75f},
    {2, 63.0f, 12.69f},    {2, 100.0f, 11.83f},   {2, 150.0f, 10.19f},   {2, 250.0f, 6.62f},    {2, 400.0f, 1.15f},
    {2, 630.0f, -6.66f},   {2, 1000.0f, -0.00f},  {2, 1600.0f, 6.28f},   {2, 2500.0f, 10.24f},  {2, 4000.0f, 13.07f},
    {2, 6300.0f, 14.72f},  {2, 10000.0f, 15.59f}, {2, 15000.0f, 15.95f}, {3, 40.0f, 3.69f},     {3, 63.0f, 3.44f},
    {3, 100.0f, 2.43f},    {3, 150.0f, 1.45f},    {3, 250.0f, 0.60f},    {3, 400.0f, 0.20f},    {3, 630.0f, 0.04f},
    {3, 1000.0f, -0.00f},  {3, 1600.0f, 0.05f},   {3, 2500.0f, 0.23f},   {3, 4000.0f, 0.62f},   {3, 6300.0f, 1.28f},
    {3, 10000.0f, 2.11f},  {3, 15000.0f, 2.74f},  {4, 40.0f, -0.58f},    {4, 63.0f, -0.12f},    {4, 100.0f, 0.09f},
    {4, 150.0f, 0.16f},    {4, 250.0f, 0.19f},    {4, 400.0f, 0.18f},    {4, 630.0f, 0.13f},    {4, 1000.0f, -0.00f},
    {4, 1600.0f, -0.33f},  {4, 2500.0f, -1.06f},  {4, 4000.0f, -2.56f},  {4, 6300.0f, -4.35f},  {4, 10000.0f, -3.91f},
    {4, 15000.0f, -1.93f}, {5, 40.0f, -13.41f},   {5, 63.0f, -9.74f},    {5, 100.0f, -6.36f},   {5, 150.0f, -3.90f},
    {5, 250.0f, -1.77f},   {5, 400.0f, -0.70f},   {5, 630.0f, -0.22f},   {5, 1000.0f, -0.00f},  {5, 1600.0f, 0.09f},
    {5, 2500.0f, 0.13f},   {5, 4000.0f, 0.14f},   {5, 6300.0f, 0.15f},   {5, 10000.0f, 0.15f},  {5, 15000.0f, 0.15f},
    {6, 40.0f, -12.20f},   {6, 63.0f, -8.62f},    {6, 100.0f, -5.38f},   {6, 150.0f, -3.17f},   {6, 250.0f, -1.34f},
    {6, 400.0f, -0.49f},   {6, 630.0f, -0.13f},   {6, 1000.0f, 0.00f},   {6, 1600.0f, -0.02f},  {6, 2500.0f, -0.20f},
    {6, 4000.0f, -0.69f},  {6, 6300.0f, -1.69f},  {6, 10000.0f, -3.51f}, {6, 15000.0f, -5.85f},
};

void checkIRShapes ()
{
    // REPORT-ONLY since Task 15: the blend trusts the T3K captures only
    // lightly (user ruling), so IR deltas are recorded here, not gated.
    // Inverted hierarchy vs Task 12 (which gated IR and reported chart).
    for (int tone = 1; tone <= 6; ++tone)
    {
        ToneBank bank;
        bank.setSampleRate (48000.0);
        bank.setTone (tone);
        const float norm = bank.magnitudeAt (1000.0f);
        float worst = 0.0f;
        float worstFreq = 0.0f;
        int count = 0;
        for (const IrSpot& s : kIrSpots)
        {
            if (s.tone != tone)
                continue;
            ++count;
            const float d = std::fabs ((bank.magnitudeAt (s.freqHz) - norm) - s.db);
            if (d > worst)
            {
                worst = d;
                worstFreq = s.freqHz;
            }
        }
        REQUIRE (count == 14);
        std::printf ("tone %d IR-shape worst %+.3fdB at %.0fHz (%d pts, report only)\n", tone, worst, worstFreq, count);
    }
}

// Oracle-consistency table (measured 2026-10-02): the Task 2 eye-read header
// and the machine-digitized CSV disagree by >1dB at 14 points, e.g. T2@400Hz
// (header -8.5dB vs CSV -12.0dB) and T5@400Hz (-0.5dB vs +1.4dB). The
// independently-fitted JSON reference SOS cascades (max err 0.08-0.44dB vs
// the CSV) reproduce the CSV values at those points, so the eye-read is the
// outlier. No physical filter can sit within +/-1dB of both oracles where
// they differ by up to 3.5dB, so each header point passes when it lands
// within +/-1dB of EITHER oracle. Task 8 IR comparison adjudicates.
// Binding gate (Task 15, numbers refit by Task 17): the manual chart is
// binding again — every tone within +/-1dB of EITHER chart oracle (eye-read
// header or digitized CSV) at every 40Hz-15kHz header point, Task-4 style.
// Either-oracle worsts (Task-17 numbers): T1 0.32 / T2 0.78 / T3 0.30 /
// T4 0.67 / T5 0.17 / T6 0.37 dB.
void checkHeaderOracle (const DenseCurve& csv)
{
    // GATED since Task 15 (was report-only under Task 12): the fits track
    // the chart again, so either-oracle agreement is asserted. Kept printed
    // because analysis/ir_check.py --verify-port compares its Python port
    // against these C++ either-oracle values.
    for (int tone = 1; tone <= 6; ++tone)
    {
        ToneBank bank;
        bank.setSampleRate (48000.0);
        bank.setTone (tone);
        float worstEither = 0.0f;
        for (const ToneTarget& t : getToneTargets())
        {
            if (t.tone != tone)
                continue;
            const float m = bank.magnitudeAt (t.freqHz);
            const float dHeader = std::fabs (m - t.db);
            const float dCsv = std::fabs (m - denseAt (csv, tone, t.freqHz));
            const float dEither = dHeader < dCsv ? dHeader : dCsv;
            if (dEither > worstEither)
                worstEither = dEither;
            REQUIRE (dEither <= 1.0f);
        }
        std::printf ("tone %d header-oracle worst either-oracle delta %+.3fdB\n", tone, worstEither);
    }
}

void checkDenseCsv (const DenseCurve& csv)
{
    // GATED since Task 15 (was report-only under Task 12): the blend sits
    // within +/-1dB of the digitized chart on the full dense grid too
    // (Task-17 worsts T1-T6: 0.57/0.97/0.33/0.71/0.18/0.56 dB — T2's 0.97 dB
    // at ~956 Hz is the thinnest margin, recorded honestly; it improves on
    // the Task-15 0.98 dB worst).
    for (int tone = 1; tone <= 6; ++tone)
    {
        ToneBank bank;
        bank.setSampleRate (48000.0);
        bank.setTone (tone);
        float worst = 0.0f;
        float worstFreq = 0.0f;
        int count = 0;
        for (size_t i = 0; i < csv.freqHz.size(); ++i)
        {
            const float f = csv.freqHz[i];
            if (f < 40.0f || f > 15000.0f)
                continue;
            ++count;
            const float d = std::fabs (bank.magnitudeAt (f) - csv.db[tone][i]);
            if (d > worst)
            {
                worst = d;
                worstFreq = f;
            }
            REQUIRE (d <= 1.0f);
        }
        REQUIRE (count > 50);
        std::printf ("tone %d dense-CSV worst %+.3fdB at %.1fHz (%d pts)\n", tone, worst, worstFreq, count);
    }
}

void checkLowEndCsv (const DenseCurve& csv)
{
    // GATED since Task 17 (Rulings B+C): the low end tracks the DIGITIZED
    // CSV 100% over [40,200] Hz — every tone within +/-0.3dB at every CSV
    // point in-band, except T2 at +/-0.5dB (crossover-boundary + notch-skirt
    // tension at ~196Hz, accepted as physics). The 10-40Hz CSV band is
    // EXCLUDED: T1/T3/T4 read identical within <=0.11dB there despite
    // different low-end circuits (digitization floor), and the 5Hz
    // DC-blocker owns sub-40 behavior by design. T3/T4/T6 hold their
    // Task-15 numbers (already passing); T1/T2/T5 were refit numbers-only.
    for (int tone = 1; tone <= 6; ++tone)
    {
        ToneBank bank;
        bank.setSampleRate (48000.0);
        bank.setTone (tone);
        const float gate = (tone == 2) ? 0.5f : 0.3f;
        float worst = 0.0f;
        float worstFreq = 0.0f;
        int count = 0;
        for (size_t i = 0; i < csv.freqHz.size(); ++i)
        {
            const float f = csv.freqHz[i];
            if (f < 40.0f || f > 200.0f)
                continue;
            ++count;
            const float d = std::fabs (bank.magnitudeAt (f) - csv.db[tone][i]);
            if (d > worst)
            {
                worst = d;
                worstFreq = f;
            }
            REQUIRE (d <= gate);
        }
        REQUIRE (count > 10);
        std::printf ("tone %d low-end-CSV worst %+.3fdB at %.1fHz (%d pts, gate %.1f)\n", tone, worst, worstFreq, count,
                     gate);
    }
}

void checkBypassFlat ()
{
    // Flat bypass: magnitudeAt returns 0dB from 5Hz to 0.95*Nyquist
    // (100kHz exceeds Nyquist at 44.1k/48k, hence the cap), and
    // processSample is bit-transparent.
    const double rates[2] = {44100.0, 48000.0};
    for (int r = 0; r < 2; ++r)
    {
        ToneBank bank;
        bank.setSampleRate (rates[r]);
        bank.setTone (0);
        const double nyquist = rates[r] * 0.5;
        for (int i = 0; i <= 200; ++i)
        {
            const double f = 5.0 * std::pow (nyquist * 0.95 / 5.0, i / 200.0);
            REQUIRE (std::fabs (bank.magnitudeAt (static_cast<float> (f))) <= 0.5f);
        }
        const float probes[5] = {-1.0f, -0.25f, 0.0f, 0.25f, 1.0f};
        for (float x : probes)
            REQUIRE (bank.processSample (x) == x);
    }
    std::puts ("bypass flat + bit-transparent ok");
}

void checkTone2NotchDepth ()
{
    ToneBank bank;
    bank.setSampleRate (48000.0);
    bank.setTone (2);
    float deepest = 0.0f;
    float deepFreq = 0.0f;
    for (int i = 0; i <= 200; ++i)
    {
        const float f = 500.0f * static_cast<float> (std::pow (2.0, i / 200.0)); // 500..1000Hz
        const float m = bank.magnitudeAt (f);
        if (m < deepest)
        {
            deepest = m;
            deepFreq = f;
        }
    }
    // Measured IR notch tip (1kHz-normalized): -6.84dB @ 661Hz; the absolute
    // tip sits on the cascade's 1kHz level, so the gate stays well below it.
    std::printf ("tone 2 notch tip %+.2fdB at %.1fHz\n", deepest, deepFreq);
    REQUIRE (deepest < -12.0f);
}

void checkTone4DipPresent ()
{
    // Measured IR dip (1kHz-normalized): -4.59dB @ ~7.6kHz; assert the fitted
    // shape carries a dip in 4-8kHz without pinning its exact tip.
    ToneBank bank;
    bank.setSampleRate (48000.0);
    bank.setTone (4);
    float deepest = 0.0f;
    for (int i = 0; i <= 200; ++i)
    {
        const float f = 4000.0f * static_cast<float> (std::pow (2.0, i / 200.0)); // 4..8kHz
        const float m = bank.magnitudeAt (f);
        if (m < deepest)
            deepest = m;
    }
    std::printf ("tone 4 dip region deepest %+.2fdB\n", deepest);
    REQUIRE (deepest < -2.0f);
}

void checkProcessSampleAgreement ()
{
    // Tone 3: steady-state sine RMS must agree with magnitudeAt within 0.3dB.
    ToneBank bank;
    bank.setSampleRate (48000.0);
    bank.setTone (3);
    const float probes[3] = {400.0f, 1000.0f, 5000.0f};
    for (float f : probes)
    {
        const float expected = bank.magnitudeAt (f);
        const float measured = sineRmsDb (bank, 48000.0, f);
        std::printf ("tone 3 @ %.0fHz: magnitudeAt %+.3fdB sine %+.3fdB\n", f, expected, measured);
        REQUIRE (std::fabs (measured - expected) < 0.3f);
    }
}

void checkRateInvariance ()
{
    const float probes[7] = {40.0f, 100.0f, 400.0f, 1000.0f, 4000.0f, 10000.0f, 15000.0f};
    for (int tone = 1; tone <= 6; ++tone)
    {
        ToneBank a;
        a.setSampleRate (44100.0);
        a.setTone (tone);
        ToneBank b;
        b.setSampleRate (48000.0);
        b.setTone (tone);
        for (float f : probes)
            REQUIRE (std::fabs (a.magnitudeAt (f) - b.magnitudeAt (f)) <= 0.1f);
    }
    std::puts ("rate invariance 44.1k/48k within 0.1dB ok");
}

} // namespace

int main ()
{
    DenseCurve csv;
    REQUIRE (loadDenseCsv (TONE_CSV_PATH, csv));
    std::printf ("loaded %u dense CSV points from %s\n", (unsigned)csv.freqHz.size(), TONE_CSV_PATH);

    checkBypassFlat();
    checkIRShapes();
    checkHeaderOracle (csv);
    checkDenseCsv (csv);
    checkLowEndCsv (csv);
    checkTone2NotchDepth();
    checkTone4DipPresent();
    checkProcessSampleAgreement();
    checkRateInvariance();
    if (gFailures == 0)
        std::puts ("ToneBankTest: all checks passed");
    else
        std::printf ("ToneBankTest: %d FAILURES\n", gFailures);
    return gFailures == 0 ? 0 : 1;
}
