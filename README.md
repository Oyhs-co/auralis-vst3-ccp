# Auralis

Open-source VST3 audio enhancement plugin built with C++20 and JUCE. It provides a modular DSP pipeline for shaping audio through equalization, bass enhancement, clarity, dynamics, ambience and stereo processing. The project prioritizes real-time safety, reproducible builds, transparent development and community contributions.

## Status

v0.2.0. Implemented: master gain, 4-band EQ, bass enhancer, clarity exciter, compressor, FDN reverb, stereo enhancer, lookahead limiter, spectrum analyzer with meters, presets (factory + user JSON), A/B, global intensity and a resizable JUCE GUI. Host target: FL Studio. Remaining: host-side polish and release engineering.

## Features

- Fixed DSP chain: `Gain → EQ → Bass → Clarity → Dynamics → Ambience → Stereo → Limiter`
- Global **Intensity** control (0 = neutral, 1 = as set), per-parameter host automation (49 stable IDs, including per-module and per-band enables)
- **True bypass**: the whole chain returns the dry signal (latency-compensated, click-free via parameter smoothing)
- **Presets**: 3 factory presets + user presets as editable `.auralis` JSON files with `schema_version`
- **A/B** comparison and one-click reset, both persisted in the host state
- Spectrum view (32 log bands, input/output) with RMS meters, stereo correlation and compressor/limiter gain-reduction readouts
- Lookahead limiter reports its latency to the host; reverb reports its tail length

## Requirements

- Windows 10/11 x64
- Visual Studio 2022 with the **Desktop development with C++** workload
- CMake ≥ 3.22
- Git

## Build

```powershell
git submodule update --init --recursive
cmake --preset msvc
cmake --build --preset release
```

The VST3 artifact is generated at `build/Auralis_artefacts/Release/VST3/Auralis.vst3`. Install it with:

```powershell
powershell -File tools\install.ps1
```

Then rescan plugins in FL Studio.

## Documentation

Full documentation lives in [`docs/`](docs/README.md). Start here:

- [`docs/USER-GUIDE.md`](docs/USER-GUIDE.md) — install, usage in FL Studio, use cases, FAQ
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — layers, C4 diagrams, thread model
- [`docs/DSP-ALGORITHMS.md`](docs/DSP-ALGORITHMS.md) — signal processing math per module
- [`docs/PARAMETERS.md`](docs/PARAMETERS.md) — stable parameter reference (IDs, ranges, defaults)
- [`docs/BUILD-TEST-RELEASE.md`](docs/BUILD-TEST-RELEASE.md) — build, tests, CI, packaging
- [`docs/TROUBLESHOOTING.md`](docs/TROUBLESHOOTING.md) — support playbook and diagnostics

## License

This project is licensed under **AGPL-3.0-only** (see [LICENSE](LICENSE)). JUCE is used under the AGPLv3 option of its dual license — see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).
