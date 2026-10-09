// Throwaway: engaged-flat sweep of the REAL shipped chain (tilt trio).
#include "C:/Users/<you>/Code/Abalone-W5/src/dsp/ProcessorChain.h"

#include <cmath>
#include <cstdio>

int main()
{
    const double freqs[] = {20, 50, 100, 200, 500, 1000, 2000, 5000, 8000, 10000, 15000, 20000};
    for (double f : freqs)
    {
        ProcessorChain c;
        c.setSampleRate(48000.0);
        c.setBoostStep(1);
        c.setTone(0);
        c.setHighcut(false);
        c.setTrimDb(0.0f);
        double si = 0.0, so = 0.0;
        for (int n = 0; n < 96000; ++n)
        {
            const float x = 0.5f * static_cast<float>(std::sin(2.0 * 3.14159265358979 * f * n / 48000.0));
            const float y = c.processSample(x);
            if (n >= 48000)
            {
                si += (double)x * x;
                so += (double)y * y;
            }
        }
        std::printf("%.0f %+.3f\n", f, 10.0 * std::log10(so / si));
    }
    return 0;
}
