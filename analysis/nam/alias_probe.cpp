// Throwaway probe: worst folded-alias line level (dBFS) for high
// fundamentals at -18 dB RMS input, Boost 1/6/10, 1x raw ColorStage.
// Measures H5..H10 at their folded positions via coherent DFT.
#include "C:/Users/<you>/Code/Abalone-W5/src/dsp/ColorStage.h"

#include <cmath>
#include <cstdio>
#include <vector>

static constexpr double kFs = 48000.0;
static constexpr int kTotal = 48000;
static constexpr double kPi = 3.14159265358979323846;

static double binMag(const std::vector<double>& y, double freqHz)
{
    double re = 0.0, im = 0.0;
    for (int n = 0; n < kTotal; ++n)
    {
        const double a = 2.0 * kPi * freqHz * n / kFs;
        re += y[n] * std::cos(a);
        im += y[n] * std::sin(a);
    }
    return 2.0 * std::hypot(re, im) / kTotal;
}

int main()
{
    ColorStage color;
    color.setEnabled(true);
    const double freqs[] = {3000.0, 5000.0, 8000.0};
    const double boosts[] = {3.0, 18.0, 30.0};
    const double ampBase = std::pow(10.0, (-18.0 + 3.0103) / 20.0);

    for (double f : freqs)
    {
        for (double b : boosts)
        {
            const double amp = ampBase * std::pow(10.0, b / 20.0);
            std::vector<double> y(kTotal);
            for (int n = 0; n < kTotal; ++n)
            {
                const float x = static_cast<float>(
                    amp * std::sin(2.0 * kPi * f * n / kFs));
                y[n] = static_cast<double>(color.processSample(x));
            }
            double mean = 0.0;
            for (double v : y)
                mean += v;
            mean /= kTotal;
            for (double& v : y)
                v -= mean;
            double worst = 1e-12;
            for (int h = 5; h <= 10; ++h)
            {
                double fh = f * h;
                double folded = std::fabs(std::fmod(fh + 24000.0, 48000.0) - 24000.0);
                bool coincides = false;
                for (int g = 1; g <= 10 && f * g < 24000.0; ++g)
                    if (std::fabs(folded - f * g) < 1.0)
                        coincides = true;
                if (coincides)
                    continue;
                const double m = binMag(y, folded);
                if (m > worst)
                    worst = m;
            }
            const double fund = binMag(y, f);
            std::printf("f=%.0f boost=%+.0f worstAlias=%.1fdBFS (%.0f rel fund)\n", f, b,
                        20.0 * std::log10(worst), 20.0 * std::log10(worst / fund));
        }
    }
    return 0;
}
