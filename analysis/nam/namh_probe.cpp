// Throwaway probe: 1 s 440 Hz -10 dBFS through the REAL ProcessorChain at
// Boost 3 and 7 (tone 0, highcut off, trim 0, 1x). Raw samples to stdout.
#include "C:/Users/<you>/Code/Abalone-W5/src/dsp/ProcessorChain.h"

#include <cmath>
#include <cstdio>

static constexpr double kFs = 48000.0;
static constexpr double kPi = 3.14159265358979323846;

static void dump(int step, const char* label)
{
    ProcessorChain chain;
    chain.setSampleRate(kFs);
    chain.setBoostStep(step);
    chain.setTone(0);
    chain.setHighcut(false);
    chain.setOsFactor(1);
    chain.setTrimDb(0.0f);
    const double amp = std::pow(10.0, -10.0 / 20.0);
    std::printf("# %s\n", label);
    for (int n = 0; n < 48000; ++n)
    {
        const float x = static_cast<float>(
            amp * std::sin(2.0 * kPi * 440.0 * n / kFs));
        std::printf("%.7f\n", static_cast<double>(chain.processSample(x)));
    }
}

int main()
{
    dump(3, "ourB3");
    dump(7, "ourB7");
    return 0;
}
