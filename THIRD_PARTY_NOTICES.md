# Third-Party Notices

This project is licensed under **AGPL-3.0-only**. JUCE is used under the **AGPLv3 option** of its dual license (AGPLv3 / commercial), which keeps the project's licensing consistent.

| Component | Version | License | How used | Notes |
|---|---|---|---|---|
| [JUCE](https://github.com/juce-framework/JUCE) | 9.0.5 (tag pinned as git submodule; gitlink commit `d60b7a78030661574a615b586d9325ba7a7fa7dd`) | Dual AGPLv3 / commercial — **used under AGPLv3** (see [JUCE/LICENSE.md](JUCE/LICENSE.md)) | Plugin framework: VST3 adapter, audio/GUI plumbing, build integration | Bundles the VST3 SDK; never commit inside `JUCE/` |
| [Steinberg VST3 SDK](https://github.com/steinbergmedia/vst3sdk) | Bundled inside JUCE | MIT-style Steinberg license (as of VST3 3.8) | Underlying VST3 interface definitions used by JUCE's wrapper | "VST" is a registered trademark of [Steinberg Media Technologies GmbH](https://www.steinberg.net/); VST3 trademarks and logo are property of Steinberg |
| [pluginval](https://github.com/Tracktion/pluginval) | 1.0.4 | Tracktion license (GPLv3-compatible per project) | CI validation tool only — **not distributed with the plugin** | Downloaded by CI from official releases; never bundled into the artifact |

**Rule:** no dependency, asset or preset may be added without adding a row to this table with its origin, version and license, before merging.
