// Throwaway probe: capture 1 s of 440 Hz sine through Boost 10 (+30 dB)
// into the REAL ColorStage at two input levels. Raw samples to stdout:
// "# <label>" header lines then one float per line.
#include "C:/Users/<you>/Code/Abalone-W5/src/dsp/ColorStage.h"

#include <cmath>
#include <cstdio>

static constexpr double kFs = 48000.0;
static constexpr int kTotal = 48000;
static constexpr double kPi = 3.14159265358979323846;

static void dump(double freqHz, double boostDb, double rmsDb, const char* label)
{
    ColorStage color;
    color.setEnabled(true);
    // Sine peak sits 3.0103 dB above RMS: peak amp from the RMS spec.
    const double amp = std::pow(10.0, (rmsDb + 3.0103) / 20.0) * std::pow(10.0, boostDb / 20.0);
    const double trimLin = std::pow(10.0, -boostDb / 20.0); // unity staging
    std::printf("# %s\n", label);
    for (int n = 0; n < kTotal; ++n)
    {
        const float x = static_cast<float>(
            amp * std::sin(2.0 * kPi * freqHz * n / kFs));
        std::printf("%.7f\n", static_cast<double>(color.processSample(x)) * trimLin);
    }
}

int main()
{
    dump(440.0, 18.0, -10.0, "A4_-10dBRMS_B6");
    dump(440.0, 18.0, -18.0, "A4_-18dBRMS_B6");
    dump(349.23, 3.0, -10.0, "F4_-10dBRMS_B1");
    dump(349.23, 3.0, -18.0, "F4_-18dBRMS_B1");
    dump(493.88, 30.0, -10.0, "B4_-10dBRMS_B10");
    dump(493.88, 30.0, -18.0, "B4_-18dBRMS_B10");
    return 0;
}
