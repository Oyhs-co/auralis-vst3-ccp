# Auralis

Open-source VST3 audio enhancement plugin built with C++20 and JUCE. It provides a modular DSP pipeline for shaping audio through equalization, bass enhancement, clarity, dynamics, ambience and stereo processing. The project prioritizes real-time safety, reproducible builds, transparent development and community contributions.

## Status

Early development. Slice 0 (compilable CMake + JUCE project) and Slice 1 (passthrough + master gain prototype) are in progress; features are added and validated slice by slice. Host target: FL Studio.

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

The VST3 artifact is generated at `build/Auralis_artefacts/Release/VST3/Auralis.vst3`. Copy it to `C:\Program Files\Common Files\VST3` so FL Studio can scan and load it.

## License

This project is licensed under **AGPL-3.0-only** (see [LICENSE](LICENSE)). JUCE is used under the AGPLv3 option of its dual license — see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).
