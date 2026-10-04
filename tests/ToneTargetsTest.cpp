// Abalone W5 - U5-inspired clean bass DI.
// Copyright (C) 2026 Sakdinat2548.
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "../analysis/tone_targets.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <optional>
#include <vector>

namespace
{

std::optional<ToneTarget> findPoint (int tone, float freqHz)
{
    for (const ToneTarget& t : getToneTargets())
    {
        if (t.tone == tone && std::fabs (t.freqHz - freqHz) < 0.001f)
            return t;
    }

    return std::nullopt;
}

void checkOracleShape ()
{
    const std::vector<ToneTarget> targets = getToneTargets();
    assert (targets.size() == 60);

    for (int tone = 1; tone <= 6; ++tone)
    {
        int count = 0;
        for (const ToneTarget& t : targets)
        {
            if (t.tone != tone)
                continue;
            ++count;
            assert (t.freqHz >= 40.0f && t.freqHz <= 15000.0f);
            assert (t.db <= 6.0f && t.db >= -24.0f);
        }
        assert (count == 10);
    }
}

void checkTone3Scoop ()
{
    const std::optional<ToneTarget> mid = findPoint (3, 400.0f);
    const std::optional<ToneTarget> high = findPoint (3, 10000.0f);
    assert (mid.has_value() && high.has_value());
    assert (mid->db < -2.0f);
    assert (high->db > -1.0f);
}

void checkTone2Notch ()
{
    const std::optional<ToneTarget> notch = findPoint (2, 700.0f);
    assert (notch.has_value());
    assert (notch->db < -12.0f);
}

} // namespace

int main ()
{
    checkOracleShape();
    checkTone3Scoop();
    checkTone2Notch();
    std::puts ("ToneTargetsTest: all checks passed");
    return 0;
}
