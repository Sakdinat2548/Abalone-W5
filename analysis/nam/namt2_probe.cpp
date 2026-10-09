// Throwaway probe: stepped-sine gain (2 s, last 1 s RMS) + 440 Hz raw
// capture through the REAL ProcessorChain, Tone 2, Boost 4, highcut off,
// trim 0, 1x. Freqs on stdin for FR; "RAW" line switches to raw dump.
#include "C:/Users/<you>/Code/Abalone-W5/src/dsp/ProcessorChain.h"

#include <cmath>
#include <cstdio>
#include <string>

static constexpr double kFs = 48000.0;
static constexpr double kPi = 3.14159265358979323846;

int main()
{
    ProcessorChain chain;
    chain.setSampleRate(kFs);
    chain.setBoostStep(4);
    chain.setTone(2);
    chain.setHighcut(false);
    chain.setOsFactor(1);
    chain.setTrimDb(0.0f);
    char line[64];
    while (std::fgets(line, sizeof(line), stdin) != nullptr)
    {
        std::string s(line);
        if (s.rfind("RAW", 0) == 0)
        {
            std::printf("# ourT2B4\n");
            const double amp = std::pow(10.0, -10.0 / 20.0);
            for (int n = 0; n < 48000; ++n)
            {
                const float x = static_cast<float>(
                    amp * std::sin(2.0 * kPi * 440.0 * n / kFs));
                std::printf("%.7f\n", static_cast<double>(chain.processSample(x)));
            }
        }
        else
        {
            const double f = std::atof(line.c_str());
            double sIn = 0.0, sOut = 0.0;
            for (int n = 0; n < 96000; ++n)
            {
                const float x = static_cast<float>(
                    std::pow(10.0, -20.0 / 20.0) * std::sin(2.0 * kPi * f * n / kFs));
                const float y = chain.processSample(x);
                if (n >= 48000)
                {
                    sIn += (double)x * x;
                    sOut += (double)y * y;
                }
            }
            std::printf("%.1f %+.3f\n", f, 10.0 * std::log10(sOut / (sIn > 0.0 ? sIn : 1.0)));
        }
    }
    return 0;
}
