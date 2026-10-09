// Throwaway probe: 5 kHz -10 dB RMS through the REAL ProcessorChain
// (Boost 10, tone bypass, highcut off, trim -30 unity) at OS 1x/2x/4x.
// Fresh chain per factor; 2 s run, last 1 s dumped. "# 1x" style headers.
#include "C:/Users/<you>/Code/Abalone-W5/src/dsp/ProcessorChain.h"

#include <cmath>
#include <cstdio>

static constexpr double kFs = 48000.0;
static constexpr double kPi = 3.14159265358979323846;

static void dump(int factor, const char* label)
{
    ProcessorChain chain;
    chain.setSampleRate(kFs);
    chain.setBoostStep(10);
    chain.setTone(0);
    chain.setHighcut(false);
    chain.setOsFactor(factor);
    chain.setTrimDb(-30.0f);
    const double amp = std::pow(10.0, (-10.0 + 3.0103) / 20.0);
    std::printf("# %s\n", label);
    for (int n = 0; n < 96000; ++n)
    {
        const float x = static_cast<float>(
            amp * std::sin(2.0 * kPi * 5000.0 * n / kFs));
        const float y = chain.processSample(x);
        if (n >= 48000)
            std::printf("%.7f\n", static_cast<double>(y));
    }
}

int main()
{
    dump(1, "os1x");
    dump(2, "os2x");
    dump(4, "os4x");
    return 0;
}
