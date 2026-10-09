// Throwaway probe: (1) DC offset out of the REAL ColorStage + settled mean
// after a textbook 1-pole 2 Hz HP (the post-color blocker); (2) 5 kHz @
// Boost 10 raw spectrum samples for alias census; (3) 440+660 Hz two-tone
// spectrum for IMD. Sections separated by "# <name>" headers.
#include "C:/Users/<you>/Code/Abalone-W5/src/dsp/ColorStage.h"

#include <cmath>
#include <cstdio>
#include <vector>

static constexpr double kFs = 48000.0;
static constexpr double kPi = 3.14159265358979323846;

int main()
{
    ColorStage color;
    color.setEnabled(true);
    const double boostLin = std::pow(10.0, 30.0 / 20.0); // Boost 10.

    // (1) DC: -10 dB RMS 440 Hz, 2 s. Pre-block mean vs settled 2 Hz HP mean.
    {
        const double amp = std::pow(10.0, (-10.0 + 3.0103) / 20.0) * boostLin;
        const double a = std::exp(-2.0 * kPi * 2.0 / kFs); // 1-pole HP @2Hz
        double sumPre = 0.0, sumPost = 0.0, y = 0.0, xp = 0.0;
        long n = 0, m = 0;
        for (int i = 0; i < 96000; ++i)
        {
            const float x = static_cast<float>(
                amp * std::sin(2.0 * kPi * 440.0 * i / kFs));
            const double c = static_cast<double>(color.processSample(x));
            sumPre += c;
            ++n;
            y = a * (y + c - xp);
            xp = c;
            if (i >= 72000)
            {
                sumPost += y;
                ++m;
            }
        }
        std::printf("# dc\npreHPmean=%.6f postHPmean=%.6f\n", sumPre / n, sumPost / m);
    }

    // (2) Alias census: 5 kHz -10 dB RMS, 1 s raw samples.
    {
        const double amp = std::pow(10.0, (-10.0 + 3.0103) / 20.0) * boostLin;
        std::printf("# alias5k\n");
        for (int i = 0; i < 48000; ++i)
        {
            const float x = static_cast<float>(
                amp * std::sin(2.0 * kPi * 5000.0 * i / kFs));
            std::printf("%.7f\n", static_cast<double>(color.processSample(x)));
        }
    }

    // (3) IMD: 440 + 660 Hz, each -16 dB RMS (sum ~-13), 1 s raw samples.
    {
        const double amp = std::pow(10.0, (-16.0 + 3.0103) / 20.0) * boostLin;
        std::printf("# imd\n");
        for (int i = 0; i < 48000; ++i)
        {
            const double s = std::sin(2.0 * kPi * 440.0 * i / kFs)
                           + std::sin(2.0 * kPi * 660.0 * i / kFs);
            const float x = static_cast<float>(amp * s);
            std::printf("%.7f\n", static_cast<double>(color.processSample(x)));
        }
    }
    return 0;
}
