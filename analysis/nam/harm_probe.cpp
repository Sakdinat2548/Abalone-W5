// Throwaway probe: THD (%) vs frequency (10 Hz - 20 kHz) through Boost 10
// (+30 dB) into the REAL ColorStage, one row per input gain. Coherent DFT
// (integer-cycle freqs, 1 s @ 48 kHz), H2..H8 over H1. Not part of the repo.
#include "C:/Users/<you>/Code/Abalone-W5/src/dsp/ColorStage.h"

#include <cmath>
#include <cstdio>

static constexpr double kFs = 48000.0;
static constexpr int kTotal = 48000;
static constexpr int kMaxHarm = 8;
static constexpr double kPi = 3.14159265358979323846;

static double thdAt(ColorStage& color, double freqHz, double amp)
{
    double magSq[kMaxHarm + 1] = {};
    for (int h = 1; h <= kMaxHarm; ++h)
    {
        double re = 0.0, im = 0.0;
        for (int n = 0; n < kTotal; ++n)
        {
            const float x = static_cast<float>(
                amp * std::sin(2.0 * kPi * freqHz * n / kFs));
            const double y = static_cast<double>(color.processSample(x));
            const double a = 2.0 * kPi * freqHz * h * n / kFs;
            re += y * std::cos(a);
            im += y * std::sin(a);
        }
        const double m = 2.0 * std::hypot(re, im) / kTotal;
        magSq[h] = m * m;
    }
    if (!(magSq[1] > 0.0))
        return 0.0;
    double rest = 0.0;
    for (int h = 2; h <= kMaxHarm; ++h)
        rest += magSq[h];
    return std::sqrt(rest / magSq[1]) * 100.0;
}

int main()
{
    ColorStage color;
    color.setEnabled(true);
    const double boostLin = std::pow(10.0, 30.0 / 20.0); // Boost step 10.
    const double freqs[] = {10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000};
    const double gains[] = {-30.0, -20.0, -10.0, 0.0};

    std::printf("freqHz");
    for (double g : gains)
        std::printf(",in%+.0fdBFS", g);
    std::printf("\n");
    for (double f : freqs)
    {
        std::printf("%.0f", f);
        for (double g : gains)
            std::printf(",%.4f", thdAt(color, f, std::pow(10.0, g / 20.0) * boostLin));
        std::printf("\n");
    }

    std::printf("# level sweep at 1 kHz\n# inDbFS,THDpct\n");
    for (double g = -50.0; g <= 0.0; g += 1.0)
        std::printf("%.0f,%.4f\n", g,
                    thdAt(color, 1000.0, std::pow(10.0, g / 20.0) * boostLin));
    return 0;
}
