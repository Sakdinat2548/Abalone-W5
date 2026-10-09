// Throwaway probe: static transfer (peak-out vs peak-in, dB) through the
// REAL Boost gain + ColorStage, no trim — compressor-curve style. Shows
// where each boost setting bends into the tanh knee.
#include "C:/Users/<you>/Code/Abalone-W5/src/dsp/ColorStage.h"

#include <cmath>
#include <cstdio>

static constexpr double kFs = 48000.0;
static constexpr int kTotal = 48000;
static constexpr double kPi = 3.14159265358979323846;

static double peakOut(ColorStage& color, double boostDb, double inPeakDb)
{
    const double amp = std::pow(10.0, inPeakDb / 20.0) * std::pow(10.0, boostDb / 20.0);
    const double trimLin = std::pow(10.0, -boostDb / 20.0); // unity staging
    double peak = 0.0;
    for (int n = 0; n < kTotal; ++n)
    {
        const float x = static_cast<float>(
            amp * std::sin(2.0 * kPi * 100.0 * n / kFs));
        const double a = std::fabs(static_cast<double>(color.processSample(x)) * trimLin);
        if (a > peak)
            peak = a;
    }
    return 20.0 * std::log10(peak);
}

int main()
{
    ColorStage color;
    color.setEnabled(true);
    std::printf("inDb,B1,B6,B10\n");
    for (double inDb = -60.0; inDb <= 0.0; inDb += 1.0)
        std::printf("%.0f,%.3f,%.3f,%.3f\n", inDb,
                    peakOut(color, 3.0, inDb),
                    peakOut(color, 18.0, inDb),
                    peakOut(color, 30.0, inDb));
    return 0;
}
