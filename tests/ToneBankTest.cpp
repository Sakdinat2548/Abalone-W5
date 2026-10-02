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

// Oracle-consistency table (measured 2026-10-02): the Task 2 eye-read header
// and the machine-digitized CSV disagree by >1dB at 14 points, e.g. T2@400Hz
// (header -8.5dB vs CSV -12.0dB) and T5@400Hz (-0.5dB vs +1.4dB). The
// independently-fitted JSON reference SOS cascades (max err 0.08-0.44dB vs
// the CSV) reproduce the CSV values at those points, so the eye-read is the
// outlier. No physical filter can sit within +/-1dB of both oracles where
// they differ by up to 3.5dB, so each header point passes when it lands
// within +/-1dB of EITHER oracle. Task 8 IR comparison adjudicates.
void checkHeaderOracle (const DenseCurve& csv)
{
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
            REQUIRE (t.freqHz >= 40.0f && t.freqHz <= 15000.0f);
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
        const float f = 600.0f * static_cast<float> (std::pow (2.0, i / 200.0)); // 600..1200Hz
        const float m = bank.magnitudeAt (f);
        if (m < deepest)
        {
            deepest = m;
            deepFreq = f;
        }
    }
    std::printf ("tone 2 notch tip %+.2fdB at %.1fHz\n", deepest, deepFreq);
    REQUIRE (deepest < -12.0f);
}

void checkTone4DipPresent ()
{
    // Chart dip (~-3.5dB near 6kHz) falls between oracle points; assert the
    // fitted shape carries a dip in 4-8kHz without pinning its exact tip.
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
    checkHeaderOracle (csv);
    checkDenseCsv (csv);
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
