# PLAN.md -- Mcaster1DAWCast Architecture & Roadmap

**Version:** 1.0.0-alpha
**Author:** David St. John <davestj@gmail.com>
**Last Updated:** 2026-04-02

---

## 1. Project Vision

### What Is DAWCast?

Mcaster1DAWCast is a multi-channel digital audio workstation with integrated video editing, live broadcast output, and podcast publishing. It combines the multitrack audio editing and mixing of Steinberg Cubase, the NLE video timeline of Sony Vegas Pro, and the destructive/non-destructive audio editing of Sony Sound Forge -- purpose-built for the broadcast, webcasting, and podcasting workflow.

### Target Users

- **Radio broadcasters** who produce pre-recorded shows with music, voice, and jingles on a timeline with segue timing and broadcast clock integration
- **Webcasters** who stream live or record-to-file with real-time DSP processing and Icecast/RTMP output
- **Podcasters** who need chapter editing, RSS feed generation, loudness metering, and multi-platform publishing in one tool
- **Video producers** working on broadcast packages, webcasts, and podcast video episodes who need lower thirds, tickers, and overlay graphics alongside their audio mix
- **Content creators** who want a single application for audio production, video editing, and live streaming without juggling OBS, Audacity, Premiere, and Hindenburg

### Market Position

DAWCast fills the gap between consumer-grade tools (GarageBand, Audacity) and heavyweight general-purpose DAWs (Pro Tools, Logic) or NLEs (Premiere, DaVinci Resolve). It is opinionated toward broadcast workflows: segue timing, loudness compliance, stream output, and RSS publishing are first-class features rather than afterthoughts or plugins.

It is part of the Mcaster1 ecosystem and integrates natively with Mcaster1AudioPipe (virtual audio routing), Mcaster1DSPEncoder (broadcast encoding), and Mcaster1DNAS (streaming server).

---

## 2. Architecture Decisions

### Why Autotools (not CMake)?

- **Consistency with mcaster1dnas** -- the DNAS streaming server uses autotools, and DAWCast shares codec and DSP code paths. A common build system reduces friction when sharing code.
- **Fine-grained optional dependency control** -- autotools `PKG_CHECK_MODULES` with `AC_MSG_NOTICE` fallbacks cleanly handle 20+ optional codec libraries (LAME, Vorbis, FLAC, Opus, FDK-AAC, x264, libvpx, libaom, etc.) without CMake's `find_package` complexity.
- **configure.ac is the canonical dependency manifest** -- the entire library matrix (required vs optional, decode-only vs encode+decode) is documented in one file that also generates the build.
- **Proven at scale** -- FFmpeg, GStreamer, and most of the audio/video libraries DAWCast links against use autotools. Interoperability is seamless.

### Why Qt 6 Widgets (not QML)?

- **Desktop-native controls** -- DAWCast is a professional desktop application. QWidgets provide native menus, dock widgets, splitters, and keyboard shortcuts that QML would require reimplementing.
- **Custom painting performance** -- Waveform views, VU meters, spectrum analyzers, and timeline lanes use `QPainter` with hardware-backed rendering. QWidgets' paint event model maps directly to this use case.
- **Theme engine via QSS** -- Qt Style Sheets allow per-widget styling without rebuilding. The three included themes (Default, Dark Studio, Broadcast Pro) are pure YAML + QSS, no code changes.
- **Ecosystem consistency** -- Mcaster1AMP, Mcaster1AudioPipe, and Mcaster1DSPEncoder all use Qt 6 Widgets. Shared widget code (beveled buttons, embossed knobs, VU meters) is directly reusable.
- **Qt6 Multimedia** -- `QMediaPlayer`, `QVideoWidget`, and `QAudioOutput` provide platform-native media playback alongside PortAudio for low-latency audio engine work.

### Why PortAudio?

- **Cross-platform with one API** -- CoreAudio (macOS), WASAPI/ASIO (Windows), PulseAudio/PipeWire/ALSA (Linux) behind a single callback interface.
- **Low-latency callback model** -- PortAudio's `PaStreamCallback` runs on a real-time audio thread, which maps directly to the SPSC ring buffer architecture for glitch-free mixing.
- **Device enumeration** -- PortAudio enumerates all audio devices including Mcaster1AudioPipe virtual devices with no special-case code.
- **Proven in professional audio** -- Used by Audacity, MuseScore, and dozens of pro-audio applications.

### Why FFmpeg?

- **Universal codec support** -- A single library provides decode and encode for every audio and video format DAWCast needs. Optional native libraries (x264, libvpx, FDK-AAC, etc.) plug in as FFmpeg external encoders for higher quality.
- **Container mux/demux** -- `libavformat` handles MP4, MKV, WebM, OGG, AVI, MOV, TS, FLV, MXF -- every container format the broadcast workflow requires.
- **Hardware acceleration** -- FFmpeg's `hwaccel` API surfaces VideoToolbox (macOS), NVENC (NVIDIA), VAAPI (Linux), and QuickSync (Intel) transparently.
- **Frame-accurate seeking** -- Essential for non-destructive editing. FFmpeg's keyframe + packet-level seeking enables sample-accurate audio and frame-accurate video positioning.
- **Subtitle support** -- `libavcodec` decodes SRT, ASS, WebVTT, DVB, PGS, and TTML; paired with libass for styled rendering.

### Why Lock-Free SPSC Ring Buffers?

- **Zero-contention audio path** -- The audio callback thread and the decode worker thread never compete for a lock. `memory_order_acquire/release` atomics on read/write cursors are the only synchronization.
- **Cache-line alignment** -- 128-byte alignment prevents false sharing on Apple Silicon (M-series) and Intel CPUs. Each ring buffer's read cursor, write cursor, and data pointer reside on separate cache lines.
- **Bounded latency** -- A 4096-sample ring buffer at 48kHz gives ~85ms of buffering. The SPSC design guarantees that the audio callback will never block, even if the decode thread falls behind (it simply plays silence).
- **Battle-tested in Mcaster1AMP** -- The same ring buffer design powers Mcaster1AMP's gapless playback engine. Shared code, shared confidence.

---

## 3. Technology Stack

| Layer | Technology | Version | Purpose |
|:---|:---|:---|:---|
| Language | C++17 | -- | Core application logic |
| Language | Objective-C++ | -- | macOS platform integration (CoreAudio, AVFoundation) |
| Build system | GNU Autotools | autoconf 2.69+ | Configure, compile, install |
| GUI framework | Qt 6 Widgets | 6.8+ | Desktop UI, docking, menus, custom painting |
| GUI modules | Qt6 Core, Gui, Network, Concurrent, Multimedia, Svg, Charts | 6.8+ | Networking, threading, media, SVG icons, analysis |
| Audio I/O | PortAudio | 19.7+ | Cross-platform audio device access |
| Audio/video processing | FFmpeg (libavformat, libavcodec, libswresample, libswscale, libavutil) | 6.0+ | Decode, encode, resample, scale, mux/demux |
| Metadata | TagLib | 1.11+ | ID3v2, Vorbis Comment, MP4 atom read/write |
| Database | SQLite3 | 3.24+ | Project database, media library index |
| Config parsing | libyaml | 0.2+ | Theme and preset YAML loading |
| Resampling | libsoxr | 0.1.3+ | High-quality sample rate conversion (optional) |
| Audio file I/O | libsndfile | 1.0.28+ | Direct PCM file read/write (optional) |
| MP3 decode | libmpg123 | 1.25+ | Optimized MP3 decoding (optional, FFmpeg fallback) |
| MP3 encode | LAME | 3.100+ | MP3 encoding (optional) |
| MP2 encode | TwoLAME | 0.4+ | MPEG Audio Layer 2 encoding (optional) |
| Vorbis | libvorbis + libvorbisenc | 1.3+ | OGG Vorbis encode/decode (optional) |
| FLAC | libFLAC | 1.3+ | FLAC encode/decode (optional) |
| Opus | libopus + libopusenc | 1.3+ | Opus encode/decode (optional) |
| AAC | FDK-AAC | 2.0+ | High-quality AAC encoding (optional) |
| Speex | libspeex + libspeexdsp | 1.2+ | Speex voice codec (optional) |
| WavPack | libwavpack | 5.0+ | WavPack lossless/lossy codec (optional) |
| GSM | libgsm | 1.0+ | GSM 06.10 telephony codec (optional) |
| AMR | OpenCore AMR | 0.1+ | AMR-NB / AMR-WB codecs (optional) |
| H.264 encode | x264 | -- | H.264/AVC encoding (optional) |
| VP8/VP9 encode | libvpx | 1.10+ | VP8/VP9 encoding (optional) |
| Theora | libtheora | 1.1+ | Theora encode/decode (optional) |
| AV1 codec | libaom | 3.0+ | AV1 encode/decode (optional) |
| AV1 encoder | SVT-AV1 | 1.0+ | Fast AV1 encoding (optional) |
| AV1 decoder | dav1d | 1.0+ | Fast AV1 decoding (optional) |
| WebP | libwebp | 1.2+ | WebP image codec (optional) |
| Subtitle rendering | libass | 0.15+ | ASS/SSA styled subtitle rendering (optional) |
| Text rendering | FreeType | 2.10+ | Font rasterization for overlays (optional) |
| Font discovery | Fontconfig | 2.13+ | System font enumeration (optional) |
| Bidi text | FriBidi | 1.0+ | Bidirectional text layout (optional) |
| Teletext | libzvbi | 0.2+ | Teletext subtitle decoding (optional) |
| TLS | OpenSSL | 1.1+ | HTTPS stream connections (optional, SecureTransport fallback) |
| Media import | yt-dlp | -- | YouTube/Vimeo URL resolution (optional external tool) |
| macOS frameworks | CoreAudio, AudioToolbox, AVFoundation, CoreMedia, VideoToolbox, Accelerate, Metal, OpenGL, QuartzCore, CoreImage | -- | Platform audio, video, GPU acceleration |

---

## 4. Phase Roadmap

### DC1 -- Foundation (v0.1.0)

**Goal:** Skeleton application that compiles, launches, and shows an empty main window with the theme engine active.

| Deliverable | Details |
|:---|:---|
| Build system | `configure.ac`, `Makefile.am`, `autogen.sh` with all library detection |
| Application entry | `main.cpp`, `App` singleton, `ModuleRegistry` |
| Main window | `MainWindow` with dock layout, menu bar, status bar |
| Theme engine | `ThemeEngine` loads YAML + QSS, ships 3 themes |
| Custom widgets | `BevelButton`, `EmbossedKnob` base implementations |
| Config | `AppConfig` (JSON preferences), `YamlPresets` loader |
| Logging | `DebugLogger` with timestamped file and console output |
| Project manager | `ProjectManager` with new/open/save/save-as (JSON + SQLite) |
| Undo system | `UndoManager` command stack |
| Platform layer | `compat.h`, `worker_pool`, macOS audio/video bridge stubs |

### DC2 -- Audio Engine (v0.2.0)

**Goal:** Multi-track audio playback through PortAudio with ring-buffer mixing and real-time metering.

| Deliverable | Details |
|:---|:---|
| Audio engine | `AudioEngine` wraps PortAudio stream open/start/stop/close |
| Ring buffer | `RingBuffer<float>` lock-free SPSC, 128-byte cache-line aligned |
| Audio mixer | `AudioMixer` sums up to 32 strips with volume/pan/mute/solo |
| Audio worker | `AudioWorker` threaded decode via FFmpeg/native codecs |
| Codec registry | `CodecRegistry` discovers and instantiates codec wrappers |
| Codec wrappers | `WavCodec`, `FlacCodec`, `Mp3Codec`, `AacCodec`, `OpusCodec`, `VorbisCodec`, `FFmpegCodec` |
| Resampler | `AudioResampler` using libsoxr or swresample |
| Transport bar | `TransportBar` with play/pause/stop/record/rewind/ffwd |
| VU meters | `VUMeterWidget` with peak + RMS + clip indicators |
| Device discovery | `device_discovery.mm` CoreAudio enumeration |

### DC3 -- Recording (v0.3.0)

**Goal:** Multi-channel live recording with punch-in/out, arm-per-track, and monitor modes.

| Deliverable | Details |
|:---|:---|
| Live recorder | `LiveRecorder` multi-channel capture to disk |
| Record-arm | Per-track record-arm state in `AudioTrack` |
| Punch-in/out | Region-based and on-the-fly punch modes |
| Input monitoring | Zero-latency direct monitoring toggle |
| File writer | Real-time safe file writer (WAV/FLAC/MP3/AAC) on worker thread |
| Record metering | Input level metering on armed tracks |

### DC4 -- Editing (v0.4.0)

**Goal:** Non-destructive clip editing on the timeline with split, trim, fade, crossfade, and undo.

| Deliverable | Details |
|:---|:---|
| Timeline model | `Timeline`, `AudioTrack`, `Clip`, `Region`, `Marker` |
| Timeline widget | `TimelineWidget` with ruler, track lanes, clip rendering |
| Track headers | `TrackHeaderWidget` with name, color, arm, solo, mute |
| Waveform view | `WaveformView` zoomable overview + detail waveform |
| Editing operations | Split, trim start/end, move, copy, delete, crossfade |
| Timeline commands | `TimelineCommands` -- all operations are undoable |
| Crossfade engine | `CrossfadeCalc` with linear, equal-power, S-curve shapes |
| Automation | `Automation` lanes for volume, pan, and effect parameters |
| Markers | Named markers with timecode, color, and label |

### DC5 -- Mixer (v0.5.0)

**Goal:** Full mixing console GUI with bus routing, sends, inserts, and master bus.

| Deliverable | Details |
|:---|:---|
| Mixer widget | `MixerWidget` with 32 channel strips |
| Channel strip | Fader, pan knob, mute/solo/record, insert slots, send knobs |
| Bus routing | Submix buses, aux sends, master bus |
| Master strip | Master fader, limiter insert, stereo/5.1/7.1 output |
| Metering | Per-strip peak + RMS meters, master bus K-weighted LUFS meter |
| Spectrum | `SpectrumWidget` real-time FFT analyzer (Qt Charts or custom) |

### DC6 -- Effects (v0.6.0)

**Goal:** Full built-in DSP effect suite and effect rack GUI.

| Deliverable | Details |
|:---|:---|
| DSP chain | `DspChain` ordered insert chain per track/bus |
| Parametric EQ | Multi-band with interactive frequency response display |
| 31-band graphic EQ | ISO 1/3-octave with slider GUI |
| Compressor | Threshold/ratio/attack/release/knee/makeup with gain reduction meter |
| Limiter | Brick-wall look-ahead with ceiling |
| Noise gate | Threshold/attack/hold/release/range |
| De-esser | Band-split sibilance detection and reduction |
| Sonic enhancer | Harmonic exciter + stereo widening |
| Reverb | Algorithmic with room/hall/plate presets |
| AGC | Broadcast-grade automatic gain control |
| Normalizer | Peak and RMS normalization |
| Effects rack | `EffectsRackWidget` drag-and-drop effect ordering |
| DSP presets | Load/save YAML preset files from `configs/dsp_presets/` |

### DC7 -- Video (v0.7.0)

**Goal:** Video track support with FFmpeg decode/encode, timeline compositing, and video preview.

| Deliverable | Details |
|:---|:---|
| Video decoder | `VideoDecoder` frame-accurate FFmpeg decode with hwaccel |
| Video encoder | `VideoEncoder` multi-codec output (H.264, VP9, AV1, ProRes) |
| Video mixer | `VideoMixer` layer compositing with opacity and blend modes |
| Video renderer | `VideoRenderer` OpenGL/Metal preview rendering |
| Video frame | `VideoFrame` pooled frame management |
| Subtitle renderer | `SubtitleRenderer` libass + FreeType burn-in |
| Muxer/demuxer | `MuxerDemuxer` container read/write (MP4, MKV, WebM, etc.) |
| Video track | `VideoTrack` on timeline with clip trim/split/move |
| Video preview | `VideoPreview` widget with scrubbing and playback |
| Export dialog | `ExportDialog` with format/codec/quality presets |
| Export profiles | YAML export profiles from `configs/export_profiles/` |

### DC7b -- Batch Encoder (v0.7.1)

**Goal:** Standalone batch encoding system that lets broadcasters and podcasters queue multiple audio/video files and encode them all with configurable DSP processing, loudness normalization, and output format selection.

| Deliverable | Details |
|:---|:---|
| Batch encoder engine | `BatchEncoder` queues multiple files, decodes via FFmpegCodec, applies DSP chain from YAML presets, runs EBU R128 loudness normalization, resamples, converts channels, and encodes to any supported output codec (MP3, AAC, Opus, FLAC, WAV, Vorbis) |
| DSP preset loading | Loads and applies DSP presets from `configs/dsp_presets/` YAML files (Broadcast Chain, Podcast Voice, Music Master, Spoken Word) — instantiates NoiseGate, DeEsser, ParametricEQ, Compressor, Limiter, SonicEnhancer, AGC, and Normalizer with preset parameters |
| Two-pass loudness normalization | Measures integrated LUFS via the Normalizer's K-weighting measurement pass, then applies linear makeup gain to hit the target (-14 streaming, -16 podcast, -23 EBU R128, -24 ATSC A/85) |
| Sample rate conversion | Uses `AudioResampler` (libswresample / libsoxr) for high-quality rate conversion (22050, 44100, 48000, 96000 Hz) |
| Channel conversion | Mono/stereo downmix and upmix with proper summing and gain compensation |
| Batch encoder dialog | `BatchEncoderDialog` with drag-and-drop file queue table, per-file status/progress, output format/bitrate/sample-rate/channel settings, DSP preset selector, loudness normalization target, output directory and filename pattern, context menu, and overall batch progress bar |
| MainWindow integration | File menu: "Batch Encoder..." (Cmd+B) opens the dialog |

**Key differentiator:** Unlike general-purpose batch converters, DAWCast's batch encoder applies the same broadcast-grade DSP processing chain used in the live broadcast path — noise gate, parametric EQ, compressor, limiter, and loudness normalization — ensuring all encoded files meet broadcast loudness standards without manual per-file processing.

### DC8 -- Broadcasting (v0.8.0)

**Goal:** Live broadcast output to Icecast, Mcaster1DNAS, and RTMP with broadcast clock and tally.

| Deliverable | Details |
|:---|:---|
| Stream encoder | `StreamEncoder` encodes master bus to Icecast/DNAS/RTMP |
| Live recorder | `LiveRecorder` simultaneous record-to-file during broadcast |
| Broadcast clock | `BroadcastClock` with segue timing, cue points, countdown |
| Tally light | `TallyLight` on-air signal controller (GPIO, network, visual) |
| Connection manager | Server list, auto-reconnect, metadata push |
| Stream monitoring | Bitrate, buffer, listener count, connection status |

### DC9 -- Podcasting (v0.9.0)

**Goal:** Complete podcast production pipeline from recording through RSS publishing.

| Deliverable | Details |
|:---|:---|
| Chapter editor | `ChapterEditor` with ID3v2 chapter frames and MP4 chapter atoms |
| Metadata editor | `MetadataEditor` full tag editing via TagLib |
| RSS generator | `RSSGenerator` podcast RSS 2.0 with iTunes namespace |
| Podcast exporter | `PodcastExporter` loudness-targeted export (EBU R128 / ATSC A/85) |
| Show notes editor | `ShowNotesEditor` rich text with HTML export |
| Chapter widget | `ChapterWidget` timeline-integrated chapter list |
| Metadata panel | `MetadataPanel` sidebar tag display/edit |
| Publishing | FTP/SFTP/S3 upload with progress |

### DC10 -- Graphics (v0.10.0)

**Goal:** Broadcast graphics overlay system for lower thirds, tickers, callouts, and watermarks.

| Deliverable | Details |
|:---|:---|
| Overlay engine | `OverlayEngine` manages overlay layer stack |
| Lower third | `LowerThird` animated name strap with title/subtitle fields |
| Ticker crawl | `TickerCrawl` scrolling text with configurable speed/direction |
| Callout | `Callout` annotation overlays with pointers and highlights |
| Watermark | `Watermark` logo overlay with position/opacity/scale |
| Chroma key | `ChromaKey` green/blue screen keying with spill suppression |
| Luma key | `LumaKey` luminance-based compositing |
| Graphics renderer | `GraphicsRenderer` GPU-accelerated overlay compositing |
| Stinger overlay | `StingerOverlay` animated transition bumpers |

### DC11 -- Polish (v0.11.0)

**Goal:** UI polish, keyboard shortcuts, templates, accessibility, and performance optimization.

| Deliverable | Details |
|:---|:---|
| Keyboard shortcuts | Configurable shortcut map with conflict detection |
| Project templates | Podcast, broadcast show, music production, video edit templates |
| Session recovery | Auto-save with crash recovery |
| Performance | Audio engine profiling, memory pool tuning, GPU render optimization |
| Accessibility | Screen reader labels, high-contrast mode, keyboard-only navigation |
| Localization | i18n framework with Qt Linguist, initial English strings |
| Media browser | `MediaBrowser` file browser with preview, favorites, search |
| Recent projects | Recent project list with thumbnail previews |

### DC12 -- Release (v1.0.0)

**Goal:** Production release with installers, CI/CD, documentation, and website integration.

| Deliverable | Details |
|:---|:---|
| macOS installer | Signed + notarized DMG and PKG via `make release` |
| Windows installer | NSIS installer with MSVC/MinGW build |
| Linux packages | DEB (Ubuntu/Debian) and RPM (Fedora/RHEL) packages |
| CI/CD | GitHub Actions: build matrix (macOS ARM64 + Intel, Windows, Ubuntu), test, package |
| Unit tests | Audio engine, codec, DSP, timeline, project manager |
| Integration tests | Full pipeline tests (import -> edit -> export -> verify) |
| Documentation | User manual, keyboard shortcut reference, plugin development guide |
| Website | Product page on mcaster1.com with download links, screenshots, changelog |
| Ansible deployment | `deploy-release.yml` pushes installers to mcaster1.com downloads |

---

## 5. Module Breakdown

| Module | Directory | Files | Purpose |
|:---|:---|:---:|:---|
| **Core** | `core/` | 12 | Interfaces (`IModule`, `IEffectUnit`, `IVideoEffect`, `IPlugin`), data types (`AudioBuffer`, `VideoFrame`, `MediaItem`), `ModuleRegistry`, `UndoManager`, `ProjectManager` |
| **Audio Engine** | `audio_engine/` | 12 | PortAudio stream management, 32-strip mixer, SPSC ring buffer, threaded decode worker, resampler, batch encoder |
| **Video Engine** | `video_engine/` | 14 | FFmpeg decode/encode, layer compositing, OpenGL/Metal preview, subtitle rendering, container mux/demux |
| **Timeline** | `timeline/` | 18 | Non-destructive timeline model: tracks, clips, regions, markers, automation, crossfade, undoable commands |
| **DSP** | `dsp/` | 22 | Audio effects chain: EQ (parametric + graphic), compressor, limiter, gate, de-esser, enhancer, reverb, AGC, normalizer |
| **VFX** | `vfx/` | 22 | Video transitions and keying: dissolve, dip, wipe, push, zoom, chroma key, luma key, glitch, stinger |
| **Podcast** | `podcast/` | 10 | Chapter editing, metadata editing, RSS 2.0 generation, loudness-targeted export, show notes |
| **Broadcast** | `broadcast/` | 8 | Live recording, stream encoding (Icecast/DNAS/RTMP), broadcast clock, tally light |
| **Codec** | `codec/` | 14 | Audio codec wrappers: WAV, FLAC, MP3, AAC, Opus, Vorbis, FFmpeg generic fallback, codec registry |
| **Graphics** | `graphics/` | 12 | Broadcast overlay engine: lower thirds, ticker crawl, callouts, watermarks, GPU renderer |
| **Widgets** | `widgets/` | 44 | Qt 6 GUI: main window, timeline, mixer, VU meters, transport, waveform, video preview, effects rack, batch encoder dialog, dialogs, custom controls, theme engine |
| **Platform** | `platform/` | 8 | OS-specific backends: macOS CoreAudio/AVFoundation/VideoToolbox, cross-platform compat, thread pool |
| **Config** | `config/` | 6 | Application preferences, YAML preset loader, debug logger |

**Total: ~202 source files across 13 modules.**

---

## 6. Risk Assessment

| Risk | Severity | Likelihood | Mitigation |
|:---|:---:|:---:|:---|
| **Audio glitches from lock contention** | High | Medium | SPSC ring buffers with no locks on the audio thread. Decode and GUI run on separate threads. Proven design from Mcaster1AMP. |
| **FFmpeg API instability** | Medium | Medium | Pin to FFmpeg 6.x LTS. Use `libav*` C API directly (not deprecated `avcodec_decode_*`). Wrap all FFmpeg calls in version-guarded macros. |
| **Qt 6 breaking changes** | Medium | Low | Pin minimum to Qt 6.8. Avoid deprecated APIs. Qt LTS releases provide 3-year stability windows. |
| **Video sync drift (A/V desync)** | High | Medium | Use presentation timestamps (PTS) from FFmpeg for both audio and video. Resync on seek. Clock drift detection in the render loop. |
| **macOS code signing / notarization** | Medium | Low | Automated via `make release` with `xcrun notarytool`. Test on each macOS release. Hardened runtime flags in build system. |
| **Cross-platform audio latency** | Medium | Medium | PortAudio abstracts platform differences. Provide per-platform buffer size recommendations. Test with ASIO on Windows for pro-audio latency. |
| **Large project file sizes** | Low | Medium | SQLite for media library index. JSON for project state. External media references (not embedded). Lazy waveform cache generation. |
| **Plugin API stability** | Medium | Low | Define `IEffectUnit` and `IVideoEffect` interfaces early (DC1). Version the plugin ABI. Keep interfaces minimal and additive-only. |
| **Build complexity (20+ optional deps)** | Low | High | `configure.ac` handles all optional libraries with graceful fallback to FFmpeg. `install-deps.sh` script installs everything via Homebrew. |
| **Scope creep** | High | High | Strict phase gating (DC1-DC12). Each phase has a defined deliverable list. Features not in the current phase go to the backlog. |

---

## 7. Integration Points

### Mcaster1AudioPipe

DAWCast's master bus output appears as a PortAudio device selection. When AudioPipe is installed, its virtual devices appear alongside hardware devices. Users select an AudioPipe device as DAWCast's output, routing audio to DSPEncoder or any other application.

**Integration:** Zero code changes. PortAudio device enumeration picks up AudioPipe HAL devices automatically.

### Mcaster1DSPEncoder

DAWCast's `StreamEncoder` module speaks the same Icecast/DNAS/RTMP protocols as DSPEncoder. For users who want DSPEncoder's advanced broadcast processing chain (multiband AGC, stereo tool, composite clipper), they route DAWCast output through AudioPipe to DSPEncoder.

**Integration:** Protocol-level. DAWCast can also connect directly to DNAS for simpler setups.

### Mcaster1DNAS

DAWCast's stream encoder connects directly to DNAS as a source client. DNAS serves the encoded stream to listeners.

**Integration:** HTTP source protocol (Icecast-compatible). DAWCast sends metadata updates (title, artist) alongside the audio stream.

### Mcaster1AMP

AMP can serve as a source for DAWCast -- playing back media files that DAWCast records or processes. In the other direction, DAWCast projects can be rendered and loaded into AMP playlists for automated playout.

**Integration:** File-level (rendered audio/video files) and device-level (AudioPipe routing).

### Mcaster1Studio

Studio is the all-in-one broadcast suite that bundles AMP, DSPEncoder, and DNAS functionality. DAWCast shares codec wrappers, DSP algorithms, and the theme engine with Studio.

**Integration:** Shared source code (codec/, dsp/, widgets/ThemeEngine).

### Mcaster1InstallSystem

InstallSystem builds the DMG, PKG, and EXE installers for DAWCast releases.

**Integration:** `make release` produces the signed .app bundle. InstallSystem wraps it with branding, EULA, and distribution packaging.

### mcaster1.com Website

The product page (`/dawcast/` or `/dawcast.php`) hosts download links, screenshots, changelog, and documentation. Release artifacts are uploaded via Ansible `deploy-release.yml`.

**Integration:** Ansible role uploads versioned builds to `/var/www/mcaster1.com/html/dawcast/downloads/`.

---

## 8. Testing Strategy

### Unit Tests

| Module | Test Scope | Framework |
|:---|:---|:---|
| `audio_engine/RingBuffer` | SPSC correctness: single-threaded fill/drain, concurrent producer/consumer, overflow/underflow, cache-line alignment verification | Qt Test / Google Test |
| `audio_engine/AudioMixer` | Summing accuracy: mono/stereo/5.1, volume/pan, mute/solo, clipping behavior | Qt Test |
| `dsp/*` | Per-effect impulse response, frequency response, gain staging, bypass correctness | Qt Test |
| `vfx/*` | Per-transition frame interpolation at 0%, 50%, 100% progress | Qt Test |
| `codec/*` | Round-trip encode/decode: WAV, FLAC, MP3, AAC, Opus, Vorbis with bit-exact or PSNR threshold | Qt Test |
| `timeline/Timeline` | Track add/remove, clip split/trim/move/delete, undo/redo, marker CRUD | Qt Test |
| `timeline/CrossfadeCalc` | Linear, equal-power, S-curve shapes at boundary values | Qt Test |
| `podcast/RSSGenerator` | XML output validation against RSS 2.0 + iTunes namespace schema | Qt Test |
| `config/YamlPresets` | Preset load/save round-trip, missing field defaults, malformed YAML handling | Qt Test |
| `core/ProjectManager` | New/open/save/save-as, version migration, corrupt file recovery | Qt Test |

### Integration Tests

| Test | Scope |
|:---|:---|
| **Import-Edit-Export** | Import WAV + MP4, add to timeline, apply DSP, apply video transition, export to MP4, verify A/V sync and codec correctness with ffprobe |
| **Podcast Pipeline** | Record voice, add chapters, set metadata, export MP3 with chapters, generate RSS, validate feed XML |
| **Broadcast Pipeline** | Start stream encoder, connect to local Icecast, stream 30s of audio, verify listener can decode |
| **Theme Switch** | Load each of the 3 themes, verify no widget rendering crashes, verify color palette application |
| **Stress Test** | 32 audio tracks + 4 video tracks + DSP chains, verify no audio glitches over 5-minute playback |

### CI Matrix

| OS | Compiler | Qt | Architecture | Run |
|:---|:---|:---|:---|:---|
| macOS 14 (Sonoma) | Apple Clang 15 | 6.8 | ARM64 (Apple Silicon) | Build + unit tests + integration |
| macOS 13 (Ventura) | Apple Clang 14 | 6.8 | x86_64 (Intel) | Build + unit tests |
| Ubuntu 24.04 | GCC 13 | 6.8 | x86_64 | Build + unit tests |
| Windows 11 | MSVC 2022 | 6.8 | x86_64 | Build + unit tests |

### Test Execution

```bash
# Run all unit tests
make check

# Run a specific test suite
./tests/test_ringbuffer
./tests/test_dsp
./tests/test_codecs
./tests/test_timeline

# Run integration tests (requires ffmpeg, icecast in PATH)
make integration-test

# Generate coverage report
make coverage
```

---

## Appendix: File Counts by Phase

| Phase | New Files | Cumulative |
|:---:|:---:|:---:|
| DC1 Foundation | ~45 | 45 |
| DC2 Audio Engine | ~25 | 70 |
| DC3 Recording | ~10 | 80 |
| DC4 Editing | ~30 | 110 |
| DC5 Mixer | ~15 | 125 |
| DC6 Effects | ~25 | 150 |
| DC7 Video | ~20 | 170 |
| DC8 Broadcasting | ~10 | 180 |
| DC9 Podcasting | ~12 | 192 |
| DC10 Graphics | ~14 | 206 |
| DC11 Polish | ~10 | 216 |
| DC12 Release | ~15 | 231 |
