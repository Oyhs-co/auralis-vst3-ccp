# AGENTS.md — Rules for AI agents and contributors

Rules in this file are enforceable and checked in review. "Do" and "Never" mean exactly that. The plan document is the source of truth for anything not covered here.

---

## 1. What this project is

Auralis is an open-source VST3 audio effect plugin (C++20, JUCE 9.0.5 pinned as a submodule, CMake, Windows/FL Studio first). The plan document `Plan_Auralis_VST3_Cpp_JUCE_OpenSource.md` is the source of truth for architecture, slices and scope. Work progresses by vertical slices (0–12); **never implement ahead of the current slice**, and never scaffold directories for slices that have not started.

Slice map (plan §7 — each slice defines its own includes/excludes/acceptance):

| Slice | Delivers |
|---|---|
| 0 | Compilable CMake + JUCE project, CI, VST3 artifact |
| 1 | Passthrough, Master Gain, bypass, automation, state |
| 2 | DSP core contracts, pipeline, prepared buffers |
| 3–5 | EQ → Bass Enhancement → Clarity |
| 6–9 | Dynamics → Ambience → Stereo → Limiter |
| 10–12 | Analyzer/metering → Presets/A-B/GUI → Integration & distribution |

Initial DSP chain (plan §5.2): `Input → EQ → Bass → Clarity → Dynamics → Ambience → Stereo → Limiter → Output`. Fixed, modular chain — no dynamic graph until the plan says so.

---

## 2. Hard architecture boundaries

| Layer | Directory | May depend on | Must never include |
|---|---|---|---|
| JUCE plugin adapter | `Source/plugin` | domain, parameters, state, JUCE modules | Must not contain DSP algorithms or preset logic |
| Plugin domain | `Source/domain` | parameters, state, `modules/audio_core` contract | Any `juce_*` header, files, UI |
| Parameters | `Source/parameters` | (self) | `juce_audio_processors`, `juce_gui_basics`, UI classes, files |
| State | `Source/state` | parameters | `juce_audio_processors`, `juce_gui_basics`, audio processing, files |
| DSP processors | `modules/dsp/**` | `modules/audio_core` contract | `juce_audio_processors`, `juce_gui_basics`, any JUCE Component, files, registry, network |
| DSP pipeline | `modules/dsp_pipeline` | `modules/dsp`, `modules/audio_core` | `juce_audio_processors`, `juce_gui_basics`, host logic |
| audio_core contract | `modules/audio_core` | (self) | VST3, GUI, file I/O |
| Presets | `Source/presets` | parameters, state | Audio processing, files (only in non-RT host calls) |
| Analysis/telemetry | `Source/analysis`, `modules/telemetry` | `modules/audio_core` | Anything that locks or blocks the audio thread |
| GUI | `Source/ui` | JUCE, parameters | Direct audio buffer manipulation |

Hard rules:

- **Do** include `juce_*` headers only in `Source/plugin` (and `Source/ui` once it exists). **Never** include them anywhere under `Source/domain`, `Source/parameters`, `Source/state`, `modules/dsp/**`, `modules/dsp_pipeline/**` or `modules/audio_core/**`.
- DSP code must never include `juce_audio_processors`, `juce_gui_basics`, or any JUCE component class.
- `juce::AudioBuffer` may be referenced in DSP only through the internal `audio_core` contract once it exists. Until then, DSP operates on plain `float*` / spans — no JUCE buffer types.
- No layer may do file, Windows registry, or network I/O. Preset and state file I/O happens only in non-real-time host callbacks (`getStateInformation` / `setStateInformation`), never in the audio path.
- DSP and domain code must compile and test without loading the host or opening the GUI.

---

## 3. Real-time audio rules (every PR touching the audio path)

Applies to `processBlock` and everything it calls:

- [ ] No heap allocation or deallocation (no `new`, `malloc`, `std::vector::resize`, `std::string`, exceptions).
- [ ] No mutexes, locks, or waits between threads.
- [ ] No file or registry access.
- [ ] No message thread calls (`MessageManager::callAsync`, etc.).
- [ ] No logging.
- [ ] No unbounded loops; loop bounds derive from block size / known config.
- [ ] Parameters are read once per block via cached `std::atomic` pointers (e.g. `APVTS::getRawParameterValue` cached in `prepareToPlay`), not looked up by string per block.
- [ ] Audible parameters (gains, wet/dry) are smoothed (`SmoothedValue` or equivalent) — no parameter jumps straight into the signal.
- [ ] No work in the audio thread that can be done in `prepareToPlay` (buffer allocation, filter setup, sample-rate config).
- [ ] No NaN/inf can propagate: process with `float`, guard feedback paths.

Thread communication (plan §4.4) — when audio ↔ GUI/control exchange is needed:

- **Do** use atomics for simple shared values; SPSC lock-free queues for messages; pre-allocated buffers for telemetry.
- **Do** define the policy for full queues and discarded data up front.
- **Never** assume an `AudioProcessorValueTreeState` read/write/listener is real-time safe by default — verify each operation and transfer values to the DSP engine through a non-blocking mechanism.

---

## 4. Anti-blob rules (the core rules of this repo)

- **One class per file pair.** `X.h` / `X.cpp`, file named after the class. Guideline: ~300 lines per file; if a file grows past it, **split by responsibility before adding more code**.
- **No god objects.** One class, one responsibility. `PluginProcessor` only adapts JUCE ↔ domain — it contains no DSP algorithms, no preset logic, no UI logic.
- **No dumping grounds.** Never create `Utils.h`, `Misc.h`, `Helpers.h`, `Common.h`. Helpers live beside the code that uses them and are named for what they do (`GainSmoother.h`, not `AudioUtils.h`).
- **New DSP effect = new directory.** `modules/dsp/<effect>/` implementing the processor contract (`prepare` / `process` / `reset`), wired into the pipeline in exactly one place. **Never** add an effect inline inside `PluginProcessor` or inside an existing effect.
- **No speculative code.** No abstractions for anticipated needs, no config options nobody asked for, no "for later" stubs (YAGNI). If a deliberate simplification cuts a real corner, mark it:
  ```cpp
  // shortcut: <the limit>, <when to upgrade>
  ```
- **Parameter IDs are stable API.** Strings like `"gain"`, `"bypass"` are never renamed or reused. Only append new IDs. Changing a range or default requires a note in the PR description.
- **Dead code is deleted.** Commented-out code, commented debug prints, unused members and unused includes are removed, not left in. Review history preserves them.
- **Comments explain why, once.** One line for the "why" the code cannot show. The code itself shows the "what". No narrating comments (`// increment i`).
- **No premature structure.** Directories and CMake targets appear when the current slice needs them, not before (plan §6 is a map, not a pre-creation checklist).

---

## 5. Build, test and validation

Exact commands (PowerShell, repo root):

```powershell
git submodule update --init --recursive
cmake --preset msvc
cmake --build --preset release
pluginval --strictness-level 8 --validate build/Auralis_artefacts/Release/VST3/Auralis.vst3
```

**Definition of done for any change:**

1. Builds clean with `cmake --preset msvc` + `cmake --build --preset release`.
2. Zero new warnings (`juce_recommended_warning_flags` is enabled — warnings are not optional).
3. `pluginval` passes at strictness level 8.
4. Mono **and** stereo paths still work (test both, at variable buffer sizes).
5. If the change touches DSP: no NaN/inf output, sample rates 44.1/48/88.2/96 kHz still pass.

Global quality criteria (plan §10) — these hold for every PR, not just audio-path PRs:

| Area | Criterion |
|---|---|
| Channels | Mono and stereo both work |
| Sample rates | 44.1, 48, 88.2, 96 kHz |
| Buffers | Variable block sizes accepted |
| Numeric errors | No NaN/inf propagation |
| Parameters | Stable IDs, automatable |
| Latency | Measured and declared (setLatencySamples) when a change introduces it |
| State | Save/restore still verifiable |
| Distribution | Reproducible build, validated artifact |

---

## 6. Dependencies and licensing

- Every new dependency, asset, icon, sample or preset needs a row in `THIRD_PARTY_NOTICES.md` (origin, version, license, conditions) **before** merging — not after.
- License must be compatible with **AGPL-3.0-only**. Permissive (MIT/BSD) is fine; GPL-incompatible or unknown-provenance code is not. No copyleft-incompatible assets.
- Every new source file starts with:
  ```cpp
  // SPDX-License-Identifier: AGPL-3.0-only
  ```
- **Never** modify or commit inside `JUCE/` — it is a pinned submodule. Change the pinned commit deliberately, never by accident inside another PR.
- **Never** commit `build/`, IDE files (`.vs/`, `.vscode/`, `*.user`), or binary artifacts.

Licensing notes (plan §3.4): JUCE modules are used under AGPLv3 (no commercial JUCE license, no closed edition); VST3 SDK ≥ 3.8 is MIT — keep its copyright notices; distribution of binaries requires publishing the corresponding source.

---

## 7. Repository hygiene

- **Do not** create directories ahead of the slice that needs them. `modules/dsp/limiter/` appears in slice 9, not in slice 1.
- Branch naming: `feature/*`, `fix/*`, `docs/*` (e.g. `feature/master-gain`, `fix/audio-buffer-boundary`).
- Commits: small, one logical change, descriptive message ("Add smoothed master gain parameter", not "update").
- **Do not** edit `.qodo/`, `.plans/`, or `Plan_Auralis_VST3_Cpp_JUCE_OpenSource.md` without explicit instruction.
- PRs describe: the change, motivation, tests run, and CPU/latency impact when relevant (plan §18.2).

---

## 8. When unsure

1. Read the plan document section for the current slice (§7 defines each slice's includes, excludes and acceptance criteria).
2. If the plan is silent, prefer the **smaller change** that satisfies the current slice.
3. If two options are equal in size, take the one correct on edge cases (variable block size, mono, NaN guard).
4. Still unsure? Ask in the PR description. **Never invent scope** — new features belong in the plan first.
