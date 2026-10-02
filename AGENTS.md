# AGENTS.md — Abalone W5 v1 (read first)

U5-flavored clean DI VST3. JUCE 8 biquads/gain only. No WDF, no oversampling v1.

## Commands (verbatim)

- Configure: `cmake -B build -G "Visual Studio 16 2019" -A x64`
- Build: `cmake --build build --target AbaloneW5_VST3 --config Release`
- Tests: `ctest --test-dir build --output-on-failure`
- Format gate: `clang-format --dry-run --Werror "src/*.cpp" "src/*.h" "src/dsp/*.h" "tests/*.cpp"` clean every commit

## Toolchain

- MSVC 2019 BuildTools; JUCE 8.0.15 FetchContent (`GIT_TAG 8.0.15`); C++17.
- MinGW hard-blocked by JUCE (Ruling 5). Static MSVC runtime (/MT) for Sonar.
- Never commit Tone3000 IR WAVs (T3K license); test locally only.

## Repo map

- `src/` PluginProcessor/Editor; `src/dsp/` per-stage DSP (planned).
- `tests/` per-stage plain-CTest asserts + oracle vs manual curves; `analysis/` IR FFT scripts.
- `docs/` refs. Root: `.clang-format` `.clang-tidy` `.clangd` `CMakeLists.txt`.
- VST3 Windows only; 44.1k + 48k must pass.

## IDE (clangd) setup — one-time

- The VS generator in `build/` never emits `compile_commands.json`, so clangd
  uses a secondary Ninja+MSVC scratch dir `build-ide/` (gitignored, never commit).
- One-time setup (from repo root, `cmd`, NOT a MinGW shell — JUCE hard-rejects
  MinGW gcc on PATH, so force `CC=cl`/`CXX=cl`):
  `call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 && set CC=cl && set CXX=cl && C:\msys64\ucrt64\bin\cmake.exe -B build-ide -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM=C:\msys64\ucrt64\bin\ninja.exe`
  (note: `VsDevCmd.bat` is directly under `Common7\Tools\`, no `vsdevcmd\` subdir).
- `.clangd` points at `build-ide` and adds MSVC 14.29.30133 + WinSDK 10.0.19041.0
  includes explicitly (clangd 22 auto-detection falls back to stale VS8/9/10
  paths; use combined `-I<path>` form — split `-isystem <path>` breaks its
  `--driver-mode=cl` arg parsing). Refresh versions after toolchain updates.
- Verify: `C:\msys64\ucrt64\bin\clangd.exe --check=src\PluginProcessor.cpp`
  must end `All checks completed, 0 errors`. Zed cannot be driven headlessly;
  MSYS2 clangd 22 `--check` is the accepted proxy.

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
- Test integrity: MSVC Release defines NDEBUG which kills bare `assert()` — every CTest target must use the `add_dsp_test()` helper so asserts stay live.

## References

- Spec: `C:\Users\kluis\.opencode\plan\u5-spec.md`
- Sauce: `C:\Users\kluis\Downloads\AbaloneU55SAUCE`
- Tone3000: https://www.tone3000.com/tones/avalon_u5-36172
- Thread: https://www.freestompboxes.org/viewtopic.php?t=3273
