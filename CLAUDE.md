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
make -C src/DAWCast -j$(sysctl -n hw.ncpu)
```

Binary: `src/DAWCast/mcaster1dawcast`

## Directory Structure

```
src/DAWCast/          ← All C++ source (14 module subdirectories)
themes/               ← QSS theme packs (Default, DarkStudio, BroadcastPro)
configs/              ← YAML presets + JSON project templates
installer/            ← macOS PKG/DMG, Windows NSIS, Linux DEB/RPM
docs/                 ← PLANNING.html + index.html
tests/                ← Unit test stubs
scripts/              ← Build helper scripts
image_resources/      ← App icon SVG source
```

## Source Modules (`src/DAWCast/`)

| Module | Purpose |
|--------|---------|
| `core/` | Interfaces (IModule, IEffectUnit, IVideoEffect), ProjectManager, UndoManager |
| `audio_engine/` | PortAudio engine, AudioMixer (32 strips), SPSC RingBuffer |
| `video_engine/` | FFmpeg VideoDecoder/Encoder, SubtitleRenderer, MuxerDemuxer |
| `timeline/` | Timeline model, AudioTrack, VideoTrack, Clip, Automation |
| `dsp/` | 10 DSP effects (EQ, compressor, limiter, gate, reverb, etc.) |
| `vfx/` | 10 video transitions (dissolve, wipe, chroma key, etc.) |
| `podcast/` | ChapterEditor, MetadataEditor, RSSGenerator, PodcastExporter |
| `broadcast/` | LiveRecorder, StreamEncoder, BroadcastClock |
| `codec/` | WAV, FLAC, MP3, AAC, Opus, Vorbis, FFmpeg wrappers |
| `graphics/` | LowerThird, TickerCrawl, Callout, Watermark overlays |
| `widgets/` | Qt6 UI (MainWindow, Timeline, Mixer, BevelButton, VUMeter, ThemeEngine) |
| `platform/` | macOS CoreAudio/AVFoundation bridges, thread pool |
| `config/` | AppConfig (JSON), YamlPresets (libyaml), DebugLogger |
| `resources/` | Qt resource file (.qrc), icons |

## Namespace

All code lives in `namespace dawcast`. Widget code uses `namespace dawcast::widgets`. No sub-namespaces for other modules — everything is flat `dawcast::ClassName`.

## Key Patterns

- **DSP effects** inherit from `dawcast::IEffectUnit` — process float* in-place, atomic parameters
- **Video effects** inherit from `dawcast::IVideoEffect` — process QImage frames
- **Qt MOC** rules are explicit in `src/DAWCast/Makefile.am` — every Q_OBJECT header needs a moc rule
- **FFmpeg code** wrapped in `#ifdef HAVE_AVFORMAT` with `extern "C" { }` for headers
- **Codec wrappers** use native libs when available, fallback to FFmpegCodec
- **Theme YAML** colors are substituted into QSS via `${variable_name}` patterns

## Adding a New Source File

1. Create `.h` and `.cpp` in the appropriate subdirectory
2. Add both to `mcaster1dawcast_SOURCES` in `src/DAWCast/Makefile.am`
3. If the header has `Q_OBJECT`, add a MOC rule, and add `moc_ClassName.cpp` to `nodist_mcaster1dawcast_SOURCES` and `BUILT_SOURCES`

## Adding a New DSP Effect

1. Create `dsp/MyEffect.h` and `dsp/MyEffect.cpp` inheriting from `IEffectUnit`
2. Add to Makefile.am sources
3. Register in EffectsRackWidget's add-effect menu

## Dependencies

Required: Qt6 (Core/Gui/Widgets/Network/Concurrent/Multimedia), PortAudio, FFmpeg, TagLib, SQLite3, libyaml

Optional: LAME, mpg123, Vorbis, FLAC, Opus, fdk-aac, x264, libvpx, Theora, AV1 codecs, libass, FreeType, OpenSSL

## Key Rules

1. **Autotools build** — not CMake. All build changes go in configure.ac + Makefile.am
2. **Namespace is flat** — `dawcast::ClassName`, not `dawcast::module::ClassName`
3. **No allocations in audio callback** — DSP process() must be RT-safe
4. **FFmpeg is required** — unlike AMP where it's optional, DAWCast needs it for video
5. **Theme engine** — custom widgets read colors from `ThemeEngine::color()`, not hardcoded
