// Throwaway: C6 1046.5 Hz -18 dBFS peak through REAL chain, Boost 6,
// tone bypass, highcut off, trim 0, 1x. Dumps last 1 s of 2 s run.
#include "C:/Users/<you>/Code/Abalone-W5/src/dsp/ProcessorChain.h"

#include <cmath>
#include <cstdio>

int main()
{
    ProcessorChain chain;
    chain.setSampleRate(48000.0);
    chain.setBoostStep(6);
    chain.setTone(0);
    chain.setHighcut(false);
    chain.setOsFactor(1);
    chain.setTrimDb(0.0f);
    const double amp = std::pow(10.0, -18.0 / 20.0);
    for (int n = 0; n < 96000; ++n)
    {
        const float x = static_cast<float>(
            amp * std::sin(2.0 * 3.14159265358979 * 1046.5 * n / 48000.0));
        const float y = chain.processSample(x);
        if (n >= 48000)
            std::printf("%.7f\n", static_cast<double>(y));
    }
    return 0;
}
