// Throwaway probe: stepped-sine gain through the REAL ProcessorChain.
// Reads freqs (Hz, one per line) from stdin; argv: boostStep trimDb.
// 2 s per freq, last 1 s RMS. Prints "freq gainDb".
#include "C:/Users/<you>/Code/Abalone-W5/src/dsp/ProcessorChain.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

static constexpr double kFs = 48000.0;
static constexpr double kPi = 3.14159265358979323846;

int main(int argc, char** argv)
{
    const int step = argc > 1 ? std::atoi(argv[1]) : 3;
    ProcessorChain chain;
    chain.setSampleRate(kFs);
    chain.setBoostStep(step);
    chain.setTone(0);
    chain.setHighcut(false);
    chain.setOsFactor(1);
    chain.setTrimDb(0.0f);
    double f;
    while (std::scanf("%lf", &f) == 1)
    {
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
        std::printf("%.1f %+.3f\n", f,
                    10.0 * std::log10(sOut / (sIn > 0.0 ? sIn : 1.0)));
    }
    return 0;
}
