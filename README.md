# Auralis

Open-source VST3 audio enhancement plugin built with C++20 and JUCE. It provides a modular DSP pipeline for shaping audio through equalization, bass enhancement, clarity, dynamics, ambience and stereo processing. The project prioritizes real-time safety, reproducible builds, transparent development and community contributions.

## Status

v0.2.0. Implemented: master gain, 4-band EQ, bass enhancer, clarity exciter, compressor, FDN reverb, stereo enhancer, lookahead limiter, spectrum analyzer with meters, presets (factory + user JSON), A/B, global intensity and a resizable JUCE GUI. Host target: FL Studio. Remaining: host-side polish and release engineering.

## Features

- Fixed DSP chain: `Gain → EQ → Bass → Clarity → Dynamics → Ambience → Stereo → Limiter`
- Global **Intensity** control (0 = neutral, 1 = as set), per-parameter host automation (38 stable IDs)
- **Presets**: 3 factory presets + user presets as editable `.auralis` JSON files with `schema_version`
- **A/B** comparison and one-click reset, both persisted in the host state
- Spectrum view (32 log bands, input/output) with RMS meters and stereo correlation
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

## License

This project is licensed under **AGPL-3.0-only** (see [LICENSE](LICENSE)). JUCE is used under the AGPLv3 option of its dual license — see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).
