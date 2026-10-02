# AGENTS.md — Abalone W5 v1 (read first)

U5-flavored clean DI VST3. JUCE 8 biquads/gain only. No WDF, no oversampling v1.

## Commands (verbatim)

- Configure: `cmake -B build -G "Visual Studio 16 2019" -A x64`
- Build: `cmake --build build --target AbaloneW5_VST3 --config Release`
- Tests: `ctest --test-dir build --output-on-failure`
- Format gate: `clang-format --dry-run --Werror "src/*.cpp" "src/*.h"` clean every commit

## Toolchain

- MSVC 2019 BuildTools; JUCE 8.0.15 FetchContent (`GIT_TAG 8.0.15`); C++17.
- MinGW hard-blocked by JUCE (Ruling 5). Static MSVC runtime (/MT) for Sonar.
- Never commit Tone3000 IR WAVs (T3K license); test locally only.

## Repo map

- `src/` PluginProcessor/Editor; `src/dsp/` per-stage DSP (planned).
- `tests/` per-stage plain-CTest asserts + oracle vs manual curves; `analysis/` IR FFT scripts.
- `docs/` refs. Root: `.clang-format` `.clang-tidy` `.clangd` `CMakeLists.txt`.
- VST3 Windows only; 44.1k + 48k must pass.

## DSP chain (fixed order)

`Boost -> DC-block -> Tone -> Color -> HighCut -> Trim + LED`

- Boost: Choice 1-10, 3dB/step (~+3 to +30dB). Clean float, no hard clip.
- DC-block: 5Hz input. Tone: bypass (TONE0) + 1-6 biquads, default Tone 3, 10ms xfade.
- Color: fixed subtle tanh/2nd-harmonic ~0.1% THD at +10dB, bypassable for test.
- HighCut: on/off, -3dB at 8kHz, 1-pole min-phase. Trim + SIGNAL LED at -2dB.
- APVTS: boost (Choice 1-10), tone (Choice Bypass,1-6), highcut (Bool), output (dB trim).

## TDD + lint rules

- Tests before audio code: bypass flat 5Hz-100kHz ±0.5dB; tones ±1dB 40Hz-15kHz;
  boost +3dB/step no clip; highcut -3dB @8kHz; THD ~0.1%; IR shape agreement.
- Flat bypass when tone=bypass, highcut=off, boost=min. No clicks on tone switch.

## References

- Spec: `C:\Users\kluis\.opencode\plan\u5-spec.md`
- Sauce: `C:\Users\kluis\Downloads\AbaloneU55SAUCE`
- Tone3000: https://www.tone3000.com/tones/avalon_u5-36172
- Thread: https://www.freestompboxes.org/viewtopic.php?t=3273
