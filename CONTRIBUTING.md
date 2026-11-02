# Contributing to Auralis

## Before you code

- Open an **issue first** for any significant change (new feature, architecture change, new dependency) so it can be discussed against the plan before work starts. Small fixes go straight to PRs.
- Read [AGENTS.md](AGENTS.md) — its rules are enforced in review.
- Branch naming: `feature/*`, `fix/*`, `docs/*`.

## Pull requests

A PR must describe:

1. The change.
2. The motivation.
3. The tests you ran (build, pluginval, mono/stereo, buffer sizes).

Include CPU/latency impact notes when the change touches the audio path.

## Code rules (summary)

- **One class per file pair** (`X.h`/`X.cpp`, named after the class). Guideline: ~300 lines per file; split by responsibility before growing past it.
- **New DSP must not include JUCE headers** — DSP lives in `modules/dsp/**` and depends only on the `audio_core` contract. JUCE includes are allowed only in `Source/plugin` (and `Source/ui`). See [AGENTS.md](AGENTS.md) for the full boundary table and real-time rules.
- Every new source file starts with:
  ```cpp
  // SPDX-License-Identifier: AGPL-3.0-only
  ```
- Never modify anything inside `JUCE/` — it is a pinned submodule.
- Every new dependency, asset or preset needs a row in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) with origin, version and license **before** merging.

## License

Contributions are licensed under **AGPL-3.0-only**, the same license as the project.

## Sign-off

We recommend the [Developer Certificate of Origin](https://developercertificate.org/) (DCO): commit with `git commit -s` so your commits carry a `Signed-off-by` line confirming you have the right to submit the change.
