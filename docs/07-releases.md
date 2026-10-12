# 07 — Branches, nightlies, releases

## Two lines, different jobs

- **`main`** is the shop window: releases only. Merging into it is a
  small ceremony (PR, no force-pushes, no deletions, tags point into it
  — never rewrite it).
- **`dev`** is the workbench: integration, directly pushable, protected
  from deletion and force-pushes. Nightlies build from here.

Anything bigger than a typo rides a short branch: DSP work merges direct
to `dev` by word; main-bound changes (workflows, installer) go by PR to
`main`, then sync back. Branches die after merge.

## Nightly

The workflow file lives on `main` (cron only runs there) but builds
`dev`'s HEAD: 02:00 UTC, all three OSes, full gates, then the rolling
`nightly` tag force-moves to whatever just passed and a prerelease
publishes (`Abalone-W5-nightly-*.zip` plus the Windows Setup exe, UI stamped `vX-nightly`). Quiet
night? The guard compares SHAs and goes back to sleep. You can also fire
it by hand from Actions any time. Prereleases never take the Latest slot,
so the sidebar always shows the newest stable.

## Stable releases

1. Work lands on `dev`, nightlies prove it.
2. PR `dev` → `main`, merge.
3. Tag `vX.Y.Z` on main, builds fire, notes get handwritten.

Zips carry their version (`Abalone-W5-v1.2.1-windows.zip`, …): Windows
gets VST3, macOS VST3+AU, Linux VST3 — plus an unsigned Windows Setup exe
(Inno Setup, folder page defaulting to the spec VST3 path). The Standalone
is a dev-testing tool and never ships. The version lives in exactly one place
(`project()` in CMakeLists, numeric because hosts demand numbers);
`-dev`/`-nightly` are display-only badges from
`ABALONEW5_VERSION_SUFFIX`.

## KVR listing (by hand, not CI)

Free listing, plain-human copy, Apple Silicon checked, per-OS
requirements spelled out. Every number in it has to match gated evidence
— it once claimed "under 0.2% THD" against a 0.38% measurement, now
~0.5%, fixed with each design change. The nightlies get one line
pointing at the prerelease.

## Working style

The human habits behind this repo live in `AGENTS.md` — the same file
the agents read. Explicit word before publishing anything, pictures
before commits, licensed files stay out. These chapters teach *why* the
plugin is built this way; AGENTS.md holds *how* to work on it.
