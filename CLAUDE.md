# CLAUDE.md — Mcaster1DAWCast

**Maintainer:** Dave St. John <davestj@gmail.com>
**Repo root:** `/Users/dstjohn/dev/01_mcaster1.com/Mcaster1DAWCast/`
**Server:** `mediacast1-one` (15.204.91.208) — OVH US-West

---

## What This Is

Mcaster1DAWCast is a full multi-channel DAW for broadcasting, webcasting, podcasting, and video editing. C++17, Qt 6, Autotools build system. Part of the Mcaster1 ecosystem.

## Build

```bash
./autogen.sh
./configure --enable-macos-gui
rm -f src/DAWCast/mcaster1dawcast   # always force relink
make -C src/DAWCast -j$(sysctl -n hw.ncpu)
codesign --force --sign "Developer ID Application: David St John (FCA38UPLY3)" --timestamp=none src/DAWCast/mcaster1dawcast
```

Binary: `src/DAWCast/mcaster1dawcast`

## Directory Structure

```
src/DAWCast/              All C++ source (14 module subdirectories)
  dsp/mc1/                MC1 plugin ecosystem (52 plugins)
    patchbay/dsp/         Header-only DSP engines (fx_*.h)
    fx_ui/                Custom QPainter flagship dialogs
    Mc1EffectRegistry.h   Plugin catalog (display name / category / id / factory)
    Mc1DialogFactory.h    ID -> dialog dispatch
    Mc1EffectAdapter.h    mc1dsp::DspEffect -> dawcast::IEffectUnit bridge
plugins/                  Standalone VST3 plugins (CMake build)
  Mcaster1Tuner/          Chromatic tuner VST3 with custom VSTGUI editor
themes/                   QSS theme pack (Default — light theme only)
configs/                  YAML presets + JSON project templates
installer/                macOS PKG/DMG, Windows NSIS, Linux DEB/RPM
docs/                     PLANNING.html + index.html
tests/                    Unit test stubs
scripts/                  Build helper scripts
image_resources/          App icon SVG source
```

## Settings / Data Storage

All app data lives under `~/.mcaster1/Mcaster1DAWCast/` via centralized `AppConfig` helpers:

| Method | Path |
|--------|------|
| `AppConfig::appDataDir()` | `~/.mcaster1/Mcaster1DAWCast/` |
| `AppConfig::presetsDir()` | `.../presets/<effect-id>/factory/` or `.../custom/` |
| `AppConfig::trackPresetsDir()` | `.../track_presets/` |
| `AppConfig::whisperModelsDir()` | `.../whisper-models/` |
| `AppConfig::pluginDataDir(name)` | `.../plugins/<name>/` |
| `AppConfig::mediaLibraryPath()` | `.../media_library.json` |

Never hardcode `~/.mcaster1/` — always use `AppConfig::` helpers.

## MC1 Plugin Library (52 plugins)

| Category | Count | Plugins |
|----------|-------|---------|
| MC1 EQ | 3 | Parametric 10-band, Dual 15-band, Graphic 31-band |
| MC1 Dynamics | 2 | Compressor/Gate/Limiter, Broadcast AGC |
| MC1 Lexicon | 5 | 224, PCM 70, PCM 96, 480L, MPX 1 |
| MC1 Podcast | 9 | Voice Lift, Plosive Killer, Mouth Click, Bleed Suppressor, Phone Line, Remote Restorer, Loudness Match, Stinger Bed, Vodcast Lipsync |
| MC1 Studios | 5 | Signal Hill A, Tidemark A/B/Vault, Granite Hall A |
| MC1 BBE | 5 | 882i, D82, H82, L82, Mach 3 Bass |
| MC1 dbx | 9 | 286s, 166xs, 676, 580, 266xs, 560A, 520, 510, 530 |
| MC1 Flagship | 1 | Vocal Producer Pro (pitch correction + channel strip + tape delay + reverb) |
| MC1 Analyzer | 1 | Topline Key Finder (Krumhansl-Schmuckler key detection) |
| MC1 Channel Strip | 2 | Xenyx Preamp, Analog (Tube Preamp, CasterTube, Mic Modeler) |

**83 factory presets** generated via `PresetManager::ensureFactoryPresets()`.

All dialogs have: preset load/save/delete, resizable windows, bypass, reset, status LCD.

## Adding a New MC1 DSP Plugin

1. Create `dsp/mc1/patchbay/dsp/fx_myeffect.h` inheriting from `mc1dsp::DspEffect`
2. Create `dsp/mc1/fx_ui/MyEffectDialog.h` with Q_OBJECT subclass + QPainter hero widget
3. Add include + catalog entry in `Mc1EffectRegistry.h`
4. Add dialog dispatch in `Mc1DialogFactory.h`
5. Add create() + availableEffects() in `effect_factory.h`
6. Add factory presets in `preset_manager.cpp`
7. Add headers to `Makefile.am` SOURCES
8. Add MOC rule: `moc_mc1_MyEffectDialog.cpp` (prefixed `moc_mc1_*` to avoid case-insensitive collisions)
9. Add to nodist_SOURCES + BUILT_SOURCES
10. `rm -f mcaster1dawcast && make -j$(sysctl -n hw.ncpu)`

## VST3 Plugin Development

VST3 plugins live in `plugins/<PluginName>/` with their own CMake build:

```bash
cd plugins/Mcaster1Tuner
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target Mcaster1Tuner
codesign --force --deep --sign "Developer ID Application: David St John (FCA38UPLY3)" --timestamp=none build/VST3/Release/Mcaster1Tuner.vst3
cp -r build/VST3/Release/Mcaster1Tuner.vst3 ~/Library/Audio/Plug-Ins/VST3/
```

VST3 SDK fetched via CMake FetchContent (v3.7.14). VSTGUI enabled for custom editors.

## Source Modules (`src/DAWCast/`)

| Module | Purpose |
|--------|---------|
| `core/` | Interfaces (IModule, IEffectUnit, IVideoEffect), ProjectManager, UndoManager, MediaLibrary |
| `audio_engine/` | PortAudio engine, AudioMixer (32 strips), SPSC RingBuffer, PlaybackEngine |
| `video_engine/` | FFmpeg VideoDecoder/Encoder, SubtitleRenderer, MuxerDemuxer |
| `timeline/` | Timeline model, AudioTrack, VideoTrack, Clip, Automation, TempoMap |
| `dsp/` | 10 legacy DSP effects + MC1 plugin ecosystem (52 plugins) |
| `vfx/` | 10 video transitions (dissolve, wipe, chroma key, etc.) |
| `podcast/` | ChapterEditor, MetadataEditor, RSSGenerator, PodcastExporter |
| `broadcast/` | LiveRecorder, StreamEncoder, BroadcastClock, RTMPStreamer |
| `codec/` | WAV, FLAC, MP3, AAC, Opus, Vorbis, FFmpeg wrappers |
| `graphics/` | LowerThird, TickerCrawl, Callout, Watermark overlays |
| `widgets/` | Qt6 UI (MainWindow, Timeline, Mixer, BevelButton, VUMeter, ThemeEngine) |
| `plugins/` | PluginScanner (VST3/AU discovery), PluginHost |
| `platform/` | macOS CoreAudio/AVFoundation bridges, thread pool |
| `config/` | AppConfig (JSON + centralized paths), YamlPresets, DebugLogger |
| `ai/` | AIEngine, TranscriptionEngine (Whisper), AIAssistant, AIPanel |

## Namespace

All code lives in `namespace dawcast`. Widget code uses `namespace dawcast::widgets`. MC1 DSP plugins use `namespace mc1dsp`. No deep sub-namespaces.

## Key Patterns

- **MC1 DSP plugins** inherit from `mc1dsp::DspEffect` — header-only, RT-safe, lock-free C++17
- **Composite plugins** own sub-effects as member objects (not pointers), chain in process()
- **Macro parameters**: 10-14 user params -> 60+ sub-plugin params via recompute()
- **Synthesized IRs**: addReflection() + applyExpDecay() — no shipped IR files
- **Mc1EffectAdapter** bridges mc1dsp::DspEffect -> dawcast::IEffectUnit
- **MOC prefix**: all MC1 dialog MOC outputs use `moc_mc1_*` prefix (case-insensitive filesystem)
- **Qt MOC** rules are explicit in `src/DAWCast/Makefile.am`
- **FFmpeg code** wrapped in `#ifdef HAVE_AVFORMAT` with `extern "C" { }` for headers
- **Theme**: Default light theme only (dark themes removed — user has diabetic eyes)
- **Plugin dialogs**: VST-style — each plugin keeps its own look, does NOT inherit host theme

## Key Rules

1. **Autotools build for DAWCast** — CMake for VST3 plugins only
2. **Always force relink** — `rm -f mcaster1dawcast` before `make` to avoid stale binaries
3. **Namespace is flat** — `dawcast::ClassName`, not `dawcast::module::ClassName`
4. **No allocations in audio callback** — DSP process() must be RT-safe
5. **FFmpeg is required** — DAWCast needs it for video
6. **Light theme only** — never add dark themes (accessibility requirement)
7. **Top-tier graphics** — every MC1 plugin gets a custom QPainter hero widget, no generic dialogs
8. **Studio tribute names** — Tidemark, Granite Hall, Signal Hill — never use real studio brand names
9. **Settings DRY** — all paths through `AppConfig::` helpers, never hardcode `~/.mcaster1/`
10. **Codesign** — `Developer ID Application: David St John (FCA38UPLY3)`

## Dependencies

Required: Qt6 (Core/Gui/Widgets/Network/Concurrent/Multimedia), PortAudio, FFmpeg, TagLib, SQLite3, libyaml, yaml-cpp

Optional: LAME, mpg123, Vorbis, FLAC, Opus, fdk-aac, x264, libvpx, Theora, AV1 codecs, libass, FreeType, OpenSSL, Qt6Svg, Qt6Multimedia
