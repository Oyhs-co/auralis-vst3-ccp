## Change

<!-- What changes and why. One paragraph. -->

## Tests

<!-- Commands run and their result: cmake --preset msvc, cmake --build --preset release, ctest, pluginval -->

## Real-time / CPU / latency impact

<!-- Required if Source/modules audio paths changed: allocations, locks, per-sample cost, declared latency -->

## Checklist

- [ ] Builds clean, zero new warnings
- [ ] `ctest` passes (all suites)
- [ ] `pluginval --strictness-level 8` passes
- [ ] Mono AND stereo paths verified
- [ ] New dependency/asset added a row in THIRD_PARTY_NOTICES.md
- [ ] New source files have the SPDX header
- [ ] Parameter IDs only appended (never renamed), if parameters changed