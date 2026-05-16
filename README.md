<div align="center">

<img src="image_resources/icon_128x128.png" alt="Mcaster1DAWCast" width="96" height="96">

# Mcaster1DAWCast

**Multi-Channel DAW for Broadcasting, Webcasting, Podcasting & Video Editing**

[![Status](https://img.shields.io/badge/status-alpha--preview-FF6B35?style=for-the-badge&logo=git&logoColor=white)](#beta-status)
[![Version](https://img.shields.io/badge/version-v1.0.0--alpha-blue?style=for-the-badge)](https://github.com/davestj/Mcaster1DAWCast/releases)
[![License](https://img.shields.io/badge/license-GPL--2.0--or--later-blue?style=for-the-badge)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-macOS%20%7C%20Windows%20%7C%20Linux-lightgrey?style=for-the-badge&logo=apple&logoColor=white)](https://github.com/davestj/Mcaster1DAWCast)
[![Qt](https://img.shields.io/badge/Qt-6.8%2B-41CD52?style=for-the-badge&logo=qt&logoColor=white)](https://www.qt.io)
[![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)](https://en.cppreference.com/w/cpp/17)
[![FFmpeg](https://img.shields.io/badge/FFmpeg-6.0%2B-007808?style=for-the-badge&logo=ffmpeg&logoColor=white)](https://ffmpeg.org)
[![PortAudio](https://img.shields.io/badge/PortAudio-19.7%2B-8B0000?style=for-the-badge)](http://www.portaudio.com)

<br>

*A Cubase + Sony Vegas + Sound Forge mashup purpose-built for broadcasters, webcasters, and podcasters.*

[**Quick Start**](#quick-start) · [**Features**](#features) · [**Architecture**](#architecture) · [**Documentation**](#documentation)

</div>

---

## Beta Status

> [!WARNING]
> **Alpha Preview — Not Yet for Production Use**
>
> Mcaster1DAWCast is currently in **alpha preview** for early testers and contributors. The codebase builds cleanly, the app launches, and the core DAW pipeline is exercised daily — but several subsystems are still being wired up. Set expectations accordingly:
>
> ### ✅ Working today (95%+ complete)
> - Multi-track audio recording & playback (PortAudio engine)
> - 32-strip mixer with VU meters, faders, panning, bus routing
> - **MC1 plugin library** — 52 plugins, 83 factory presets, all flagship-grade QPainter dialogs
> - Mcaster1Tuner VST3 plugin (YIN pitch detection, 29 tuning presets)
> - Audio export — WAV / FLAC / MP3 / AAC / Opus / Vorbis
> - Project save/load, undo/redo, media library, theme system
> - Podcast metadata + chapter editing, RSS 2.0 generation
> - Whisper transcription engine (requires `whisper-cpp` on PATH)
>
> ### 🚧 Known incomplete (hidden or disabled in early builds)
> - **Audio device picker** — CoreAudio enumeration is stubbed (`platform/device_discovery.mm`); use system default I/O for now
> - **Video export pipeline** — `MuxerDemuxer` and `VideoEncoder` produce no playable file yet
> - **RTMP live streaming** — audio stream allocation is unfinished in `broadcast/RTMPStreamer`
> - **AudioUnit plugin processing** — AU bundles enumerate but audio doesn't flow (VST3 hosting works)
> - **Automation lanes** — collected & persisted in projects, but `PlaybackEngine` doesn't yet read them at run time
>
> Report bugs / regressions at [GitHub Issues](https://github.com/davestj/Mcaster1DAWCast/issues). Beta testers welcome — see [Quick Start](#quick-start) below.

---

## What It Does

Mcaster1DAWCast is a multi-track digital audio workstation with integrated video editing, live broadcast output, and podcast publishing -- all in a single application. It combines the multitrack audio editing of a professional DAW with the NLE video timeline of a broadcast editor and the loudness-managed export pipeline of a podcast production tool.

```
                          ┌──────────────────────────┐
                          │    Mcaster1DAWCast        │
                          │    Multi-Channel DAW      │
                          └────────────┬─────────────┘
                                       │
             ┌─────────────────────────┼─────────────────────────┐
             │                         │                         │
    ┌────────▼────────┐     ┌─────────▼─────────┐    ┌─────────▼─────────┐
    │  Audio Tracks    │     │  Video Tracks      │    │  Graphics Layer   │
    │  (up to 32)      │     │  (H.264/VP9/AV1)   │    │  Lower Thirds     │
    │  WAV/FLAC/MP3/   │     │  Transitions &      │    │  Ticker Crawl     │
    │  AAC/Opus/Vorbis │     │  Compositing        │    │  Watermarks       │
    └────────┬────────┘     └─────────┬─────────┘    └─────────┬─────────┘
             │                         │                         │
             └─────────────────────────┼─────────────────────────┘
                                       │
                              ┌────────▼────────┐
                              │   Mixer Bus      │
                              │   32-strip       │
                              │   DSP Chain      │
                              │   Metering       │
                              └────────┬────────┘
                                       │
                      ┌────────────────┼────────────────┐
                      │                │                │
             ┌────────▼──────┐  ┌──────▼───────┐  ┌────▼──────────┐
             │  Master Bus    │  │  Live Stream  │  │  Podcast      │
             │  Export        │  │  Icecast /    │  │  RSS 2.0      │
             │  MP4/MKV/WebM  │  │  DNAS / RTMP  │  │  Chapters     │
             │  WAV/FLAC/MP3  │  │  Broadcast    │  │  Multi-plat   │
             └───────────────┘  └──────────────┘  └───────────────┘
```

---

## Features

<details open>
<summary><strong>Multi-Track Audio Engine</strong></summary>
<br>

- **PortAudio backend** -- CoreAudio (macOS), WASAPI/ASIO (Windows), PulseAudio/PipeWire (Linux)
- **32 simultaneous mixer strips** with per-track volume, pan, mute, solo, and record-arm
- **Lock-free SPSC ring buffers** -- cache-line aligned, < 1ms latency at 48kHz
- **Full codec support** via FFmpeg + native libraries: WAV, AIFF, FLAC, MP3, AAC, Opus, Vorbis, WavPack, Speex, AMR, GSM, TwoLAME (MP2)
- **High-quality resampling** with libsoxr (SoX Resampler) or FFmpeg swresample fallback
- **libsndfile integration** for direct PCM file I/O alongside FFmpeg decode
- Float32 native processing at 44.1 / 48 / 96 / 192 kHz, mono through 7.1 surround

</details>

<details open>
<summary><strong>Video Editing</strong></summary>
<br>

- **FFmpeg-powered decode/encode** pipeline: H.264 (x264), VP8/VP9 (libvpx), Theora, AV1 (libaom / SVT-AV1 / dav1d)
- **Hardware-accelerated** via VideoToolbox (macOS), NVENC/VAAPI (Linux), QuickSync (Windows)
- **10 video transitions**: Cross Dissolve, Dip to Black, Dip to White, Wipe Left, Push Slide, Zoom Through, Luma Key, Chroma Key, Glitch Cut, Stinger Overlay
- **Video compositing** with layer ordering, opacity, and blend modes
- **Subtitle rendering** with libass (ASS/SSA), FreeType text overlays, FriBidi bidirectional layout, libzvbi teletext
- **Resolution support** from 480p to 4K UHD at frame rates up to 120 fps
- **Container export**: MP4, MKV, WebM, OGG, AVI, MOV, TS, FLV

</details>

<details open>
<summary><strong>Broadcasting & Webcasting</strong></summary>
<br>

- **Live recording** with multi-channel capture and punch-in/out
- **Stream output** to Icecast, Mcaster1DNAS, and RTMP (YouTube Live, Twitch, Facebook)
- **Broadcast clock** with segue timing, cue points, and tally light signaling
- **Real-time DSP chain** on the master bus for on-air processing
- **OpenSSL / SecureTransport** for HTTPS stream connections
- **yt-dlp integration** for YouTube/Vimeo URL resolution and media import

</details>

<details open>
<summary><strong>Podcasting</strong></summary>
<br>

- **Chapter editor** with ID3v2 chapter frames and MP4 chapter atoms
- **RSS 2.0 feed generator** with iTunes/Apple Podcasts namespace support
- **Loudness metering** (EBU R128 / ATSC A/85) with integrated LUFS targeting
- **Show notes editor** with rich text and HTML export
- **Metadata editor** with full ID3v2 / Vorbis Comment / MP4 atom support via TagLib
- **Multi-platform publishing** export presets for Apple Podcasts, Spotify, Google Podcasts, and custom feeds
- **Episode templates** with season/episode numbering, artwork, and keyword tagging

</details>

<details open>
<summary><strong>Broadcast Graphics</strong></summary>
<br>

- **Lower thirds** with animated reveal, name/title fields, and custom styling
- **Ticker crawl** for scrolling news/alert text with configurable speed and direction
- **Callout overlays** for annotating video with pointers, boxes, and highlights
- **Watermark engine** with position, opacity, and scaling controls
- **Chroma key** and **luma key** compositing for green-screen workflows
- **Graphics renderer** composites all overlay layers onto the video output in real time

</details>

<details open>
<summary><strong>Effects & DSP</strong></summary>
<br>

**Audio Effects (10 built-in):**

| Effect | Description |
|:---|:---|
| Parametric EQ | Fully parametric multi-band equalizer with Q, gain, frequency per band |
| 31-Band Graphic EQ | ISO 1/3-octave graphic equalizer |
| Compressor | Threshold, ratio, attack, release, knee, makeup gain |
| Limiter | Brick-wall look-ahead limiter with ceiling control |
| Noise Gate | Threshold, attack, hold, release, range |
| De-Esser | Frequency-selective sibilance reduction |
| Sonic Enhancer | Harmonic exciter and stereo widening |
| Reverb | Algorithmic room/hall/plate reverb |
| AGC | Automatic gain control for level normalization |
| Normalizer | Peak and RMS normalization |

**Video Transitions (10 built-in):**

| Transition | Description |
|:---|:---|
| Cross Dissolve | Linear alpha blend between two clips |
| Dip to Black | Fade out to black, fade in from black |
| Dip to White | Fade out to white, fade in from white |
| Wipe Left | Horizontal wipe reveal from right to left |
| Push Slide | Outgoing clip slides off, incoming slides on |
| Zoom Through | Zoom into outgoing clip, reveal incoming |
| Luma Key | Composite based on luminance channel |
| Chroma Key | Green/blue-screen keying with spill suppression |
| Glitch Cut | Digital glitch/distortion transition effect |
| Stinger Overlay | Animated overlay transition (broadcast bumpers) |

</details>

<details open>
<summary><strong>Theme Engine</strong></summary>
<br>

- **3 included themes**: Default (native platform), Dark Studio (deep blue-black DAW), Broadcast Pro (high-contrast on-air)
- **YAML-driven theme definitions** with full color palette, button style, knob style, and meter style
- **QSS stylesheet** per theme for pixel-level control over every widget
- **Custom theme support** -- drop a folder into `themes/` with `theme.yaml` + `style.qss`
- **Custom widgets**: Beveled buttons, embossed rotary knobs, gradient/segmented VU meters

</details>

---

## Supported Formats

### Audio Codecs

| Codec | Decode | Encode | Library | Notes |
|:---|:---:|:---:|:---|:---|
| PCM (WAV/AIFF) | Yes | Yes | Built-in / libsndfile | 8/16/24/32-bit int, 32/64-bit float |
| FLAC | Yes | Yes | libFLAC / FFmpeg | Lossless, up to 24-bit/192kHz |
| MP3 | Yes | Yes | libmpg123 / LAME / FFmpeg | CBR/VBR, up to 320kbps |
| MP2 | Yes | Yes | TwoLAME / FFmpeg | Broadcast standard (DAB/DVB) |
| AAC (LC/HE/HEv2) | Yes | Yes | FDK-AAC / FFmpeg | M4A/MP4 container |
| Opus | Yes | Yes | libopus / FFmpeg | Low-latency, 6kbps--510kbps |
| Vorbis | Yes | Yes | libvorbis / FFmpeg | OGG container |
| WavPack | Yes | Yes | libwavpack / FFmpeg | Lossless + lossy hybrid |
| Speex | Yes | Yes | libspeex / FFmpeg | Voice-optimized codec |
| AMR-NB/WB | Yes | Yes | OpenCore AMR / FFmpeg | Narrowband/wideband speech |
| GSM 06.10 | Yes | Yes | libgsm / FFmpeg | Telephony codec |
| ALAC | Yes | Yes | FFmpeg | Apple Lossless Audio Codec |
| AC3 / E-AC3 | Yes | Yes | FFmpeg | Dolby Digital |
| DTS | Yes | -- | FFmpeg | Decode only |
| TrueAudio (TTA) | Yes | Yes | FFmpeg | Lossless codec |
| Musepack (MPC) | Yes | -- | FFmpeg | Decode only |
| Monkey's Audio (APE) | Yes | -- | FFmpeg | Decode only |
| WMA / WMA Pro | Yes | Yes | FFmpeg | Windows Media Audio |
| RealAudio | Yes | -- | FFmpeg | Decode only |
| PCM A-law / mu-law | Yes | Yes | FFmpeg | Telephony PCM variants |

### Video Codecs

| Codec | Decode | Encode | Library | Notes |
|:---|:---:|:---:|:---|:---|
| H.264 / AVC | Yes | Yes | x264 / FFmpeg / VideoToolbox | Main/High profile, up to 4K |
| H.265 / HEVC | Yes | Yes | FFmpeg / VideoToolbox | 8/10-bit, HDR support |
| VP8 | Yes | Yes | libvpx / FFmpeg | WebM container |
| VP9 | Yes | Yes | libvpx / FFmpeg | WebM container, 10-bit |
| AV1 | Yes | Yes | libaom / SVT-AV1 / dav1d | Next-gen, royalty-free |
| Theora | Yes | Yes | libtheora / FFmpeg | OGG container |
| MPEG-2 | Yes | Yes | FFmpeg | Broadcast standard |
| MPEG-4 Part 2 | Yes | Yes | FFmpeg | DivX/Xvid compatible |
| ProRes | Yes | Yes | FFmpeg / VideoToolbox | 422 Proxy through 4444 XQ |
| DNxHD / DNxHR | Yes | Yes | FFmpeg | Avid editing codec |
| MJPEG | Yes | Yes | FFmpeg | Motion JPEG |
| FFV1 | Yes | Yes | FFmpeg | Lossless archival codec |
| Huffyuv | Yes | Yes | FFmpeg | Lossless intermediate |
| WebP (animated) | Yes | Yes | libwebp / FFmpeg | Animated image sequences |
| GIF | Yes | Yes | FFmpeg | Animated GIF export |

### Container Formats

| Container | Read | Write | Typical Use |
|:---|:---:|:---:|:---|
| MP4 / M4A / M4V | Yes | Yes | Universal distribution |
| MKV / MKA | Yes | Yes | High-quality archival |
| WebM | Yes | Yes | Web streaming (VP9/AV1 + Opus) |
| OGG / OGA | Yes | Yes | Open-source distribution |
| AVI | Yes | Yes | Legacy Windows |
| MOV | Yes | Yes | Apple / Final Cut Pro interchange |
| MPEG-TS (.ts) | Yes | Yes | Broadcast transport stream |
| FLV | Yes | Yes | RTMP streaming |
| WAV / AIFF | Yes | Yes | Uncompressed PCM audio |
| FLAC | Yes | Yes | Lossless single-file |
| MP3 | Yes | Yes | Ubiquitous lossy audio |
| MXF | Yes | Yes | Broadcast interchange |
| ASF / WMV / WMA | Yes | -- | Windows Media |
| 3GP / 3G2 | Yes | Yes | Mobile media |
| RM / RMVB | Yes | -- | RealMedia (decode only) |

### Subtitle Formats

| Format | Read | Write | Renderer |
|:---|:---:|:---:|:---|
| SRT (SubRip) | Yes | Yes | Built-in |
| ASS / SSA | Yes | Yes | libass |
| WebVTT | Yes | Yes | Built-in |
| Teletext | Yes | -- | libzvbi |
| DVB Subtitle | Yes | -- | FFmpeg |
| PGS (Blu-ray) | Yes | -- | FFmpeg |
| TTML | Yes | Yes | Built-in |

---

## Platform Support

| Platform | Audio Backend | Video Accel | GUI | Status |
|:---:|:---:|:---:|:---:|:---:|
| **macOS 13+** | CoreAudio via PortAudio | VideoToolbox | Qt 6 Widgets | Active (v1.0.0-alpha) |
| **Windows 10+** | WASAPI / ASIO via PortAudio | QuickSync / NVENC | Qt 6 Widgets | Planned |
| **Linux** | PulseAudio / PipeWire via PortAudio | VAAPI / NVENC | Qt 6 Widgets | Planned |

---

## Quick Start

### Prerequisites

```bash
# macOS -- install all build + runtime dependencies
brew install autoconf automake autoconf-archive pkg-config \
    qt@6 portaudio ffmpeg taglib sqlite libyaml \
    lame libvorbis flac opus fdk-aac libsndfile sox \
    x264 libvpx theora aom svt-av1 dav1d webp \
    libass freetype fontconfig fribidi openssl yt-dlp
```

---

## Tech Stack

<div align="center">

#### Build & Language

![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?style=flat-square&logo=cplusplus&logoColor=white)
![Autotools](https://img.shields.io/badge/Build-Autotools-A42E2B?style=flat-square&logo=gnu&logoColor=white)
![GNU Make](https://img.shields.io/badge/GNU%20Make-required-A42E2B?style=flat-square&logo=gnu&logoColor=white)
![pkg-config](https://img.shields.io/badge/pkg--config-required-555?style=flat-square)
![Clang](https://img.shields.io/badge/Clang-LLVM-262D3A?style=flat-square&logo=llvm&logoColor=white)
![GCC](https://img.shields.io/badge/GCC-supported-F34B7D?style=flat-square&logo=gnu&logoColor=white)

#### UI Framework

![Qt 6.8+](https://img.shields.io/badge/Qt-6.8%2B-41CD52?style=flat-square&logo=qt&logoColor=white)
![QtCore](https://img.shields.io/badge/QtCore-6.8-41CD52?style=flat-square&logo=qt&logoColor=white)
![QtGui](https://img.shields.io/badge/QtGui-6.8-41CD52?style=flat-square&logo=qt&logoColor=white)
![QtWidgets](https://img.shields.io/badge/QtWidgets-6.8-41CD52?style=flat-square&logo=qt&logoColor=white)
![QtNetwork](https://img.shields.io/badge/QtNetwork-6.8-41CD52?style=flat-square&logo=qt&logoColor=white)
![QtConcurrent](https://img.shields.io/badge/QtConcurrent-6.8-41CD52?style=flat-square&logo=qt&logoColor=white)
![QtMultimedia](https://img.shields.io/badge/QtMultimedia-6.8-41CD52?style=flat-square&logo=qt&logoColor=white)
![QtSvg](https://img.shields.io/badge/QtSvg-6.8-41CD52?style=flat-square&logo=qt&logoColor=white)

#### Audio Engine & Codecs

![PortAudio 19.7+](https://img.shields.io/badge/PortAudio-19.7%2B-8B0000?style=flat-square)
![LAME](https://img.shields.io/badge/LAME-MP3-2E86AB?style=flat-square)
![FLAC](https://img.shields.io/badge/FLAC-lossless-2E86AB?style=flat-square)
![Vorbis](https://img.shields.io/badge/Vorbis-OGG-2E86AB?style=flat-square)
![Opus](https://img.shields.io/badge/Opus-codec-2E86AB?style=flat-square)
![fdk-aac](https://img.shields.io/badge/fdk--aac-AAC-2E86AB?style=flat-square)
![libsndfile](https://img.shields.io/badge/libsndfile-PCM-2E86AB?style=flat-square)
![SoX](https://img.shields.io/badge/SoX-resample-2E86AB?style=flat-square)

#### Video Engine & Codecs

![FFmpeg 6.0+](https://img.shields.io/badge/FFmpeg-6.0%2B-007808?style=flat-square&logo=ffmpeg&logoColor=white)
![x264](https://img.shields.io/badge/x264-H.264-007808?style=flat-square)
![libvpx](https://img.shields.io/badge/libvpx-VP8%2FVP9-007808?style=flat-square)
![Theora](https://img.shields.io/badge/Theora-OGG-007808?style=flat-square)
![libaom](https://img.shields.io/badge/libaom-AV1-007808?style=flat-square)
![SVT-AV1](https://img.shields.io/badge/SVT--AV1-encoder-007808?style=flat-square)
![dav1d](https://img.shields.io/badge/dav1d-AV1%20decoder-007808?style=flat-square)
![libwebp](https://img.shields.io/badge/libwebp-WebP-007808?style=flat-square)

#### Subtitles & Text Rendering

![libass](https://img.shields.io/badge/libass-ASS%2FSSA-9B59B6?style=flat-square)
![FreeType](https://img.shields.io/badge/FreeType-fonts-9B59B6?style=flat-square)
![Fontconfig](https://img.shields.io/badge/Fontconfig-system%20fonts-9B59B6?style=flat-square)
![FriBidi](https://img.shields.io/badge/FriBidi-RTL-9B59B6?style=flat-square)
![libzvbi](https://img.shields.io/badge/libzvbi-VBI%2Fteletext-9B59B6?style=flat-square)

#### Plugin Hosting

![VST3 SDK](https://img.shields.io/badge/VST3%20SDK-3.7.14-E1A227?style=flat-square)
![VSTGUI](https://img.shields.io/badge/VSTGUI-editor-E1A227?style=flat-square)
![AudioUnit](https://img.shields.io/badge/AudioUnit-macOS-000000?style=flat-square&logo=apple&logoColor=white)
![CoreAudio](https://img.shields.io/badge/CoreAudio-macOS-000000?style=flat-square&logo=apple&logoColor=white)
![MC1 Plugins](https://img.shields.io/badge/MC1%20Plugins-52%20bundled-6F42C1?style=flat-square)

#### AI & Tooling

![Whisper.cpp](https://img.shields.io/badge/whisper.cpp-transcription-412991?style=flat-square&logo=openai&logoColor=white)
![yt-dlp](https://img.shields.io/badge/yt--dlp-media%20fetch-FF0000?style=flat-square&logo=youtube&logoColor=white)

#### Data, Config & Metadata

![SQLite](https://img.shields.io/badge/SQLite-3.24%2B-003B57?style=flat-square&logo=sqlite&logoColor=white)
![TagLib](https://img.shields.io/badge/TagLib-ID3%2FFLAC%2Ftags-555?style=flat-square)
![libyaml](https://img.shields.io/badge/libyaml-parser-CB171E?style=flat-square&logo=yaml&logoColor=white)
![yaml-cpp](https://img.shields.io/badge/yaml--cpp-presets-CB171E?style=flat-square&logo=yaml&logoColor=white)

#### Security & Distribution

![OpenSSL](https://img.shields.io/badge/OpenSSL-3.x-721412?style=flat-square&logo=openssl&logoColor=white)
![codesign](https://img.shields.io/badge/Apple-codesign-000000?style=flat-square&logo=apple&logoColor=white)
![DMG](https://img.shields.io/badge/macOS-DMG-000000?style=flat-square&logo=apple&logoColor=white)
![PKG](https://img.shields.io/badge/macOS-PKG-000000?style=flat-square&logo=apple&logoColor=white)
![NSIS](https://img.shields.io/badge/Windows-NSIS-0078D6?style=flat-square&logo=windows&logoColor=white)
![DEB/RPM](https://img.shields.io/badge/Linux-DEB%2FRPM-FCC624?style=flat-square&logo=linux&logoColor=black)

</div>

---

### Build

```bash
# Clone
git clone https://github.com/davestj/Mcaster1DAWCast.git
cd Mcaster1DAWCast

# Bootstrap autotools
./autogen.sh

# Configure with macOS Qt 6 GUI
./configure --enable-macos-gui

# Build (all cores)
make -j$(sysctl -n hw.ncpu)

# Launch
open build/Mcaster1DAWCast.app
# or run directly:
./src/DAWCast/mcaster1dawcast
```

### Package for Distribution

```bash
# Build .app bundle
make -C src/DAWCast app-bundle

# Build signed DMG
make -C src/DAWCast dmg SIGN_ID="Developer ID Application: Your Name (TEAMID)"

# Build signed PKG installer
make -C src/DAWCast pkg SIGN_ID="Developer ID Installer: Your Name (TEAMID)"

# Full release pipeline (sign + notarize + staple)
make -C src/DAWCast release SIGN_ID="Developer ID Application: Your Name (TEAMID)"
```

---

## Architecture

<details>
<summary><strong>Project Structure</strong></summary>

```
Mcaster1DAWCast/
├── configure.ac                          # Autoconf -- library detection, feature flags
├── Makefile.am                           # Top-level automake
├── autogen.sh                            # Bootstrap script (autoreconf wrapper)
├── src/
│   ├── Makefile.am                       # Subdirectory automake
│   └── DAWCast/
│       ├── Makefile.am                   # Main build -- sources, MOC rules, packaging
│       ├── main.cpp                      # Application entry point
│       ├── app.h / app.cpp               # Application singleton, module init
│       ├── core/                         # Interfaces and framework
│       │   ├── IModule.h                 # Module lifecycle interface
│       │   ├── IEffectUnit.h             # Audio effect plugin interface
│       │   ├── IVideoEffect.h            # Video effect plugin interface
│       │   ├── IPlugin.h                 # Generic plugin interface
│       │   ├── AudioBuffer.h             # Float32 multi-channel audio buffer
│       │   ├── VideoFrame.h              # Video frame container
│       │   ├── MediaItem.h               # Timeline media item descriptor
│       │   ├── ModuleRegistry.cpp/h      # Runtime module discovery and init
│       │   ├── UndoManager.cpp/h         # Undo/redo command stack
│       │   └── ProjectManager.cpp/h      # Project file I/O (JSON + SQLite)
│       ├── audio_engine/                 # Real-time audio pipeline
│       │   ├── AudioEngine.cpp/h         # PortAudio stream management
│       │   ├── AudioMixer.cpp/h          # 32-strip summing mixer
│       │   ├── AudioResampler.cpp/h      # libsoxr / swresample wrapper
│       │   ├── AudioWorker.cpp/h         # Threaded decode worker
│       │   └── RingBuffer.h              # Lock-free SPSC ring buffer
│       ├── video_engine/                 # FFmpeg video pipeline
│       │   ├── VideoDecoder.cpp/h        # Frame-accurate decode
│       │   ├── VideoEncoder.cpp/h        # Multi-codec encode
│       │   ├── VideoMixer.cpp/h          # Layer compositing
│       │   ├── VideoRenderer.cpp/h       # OpenGL/Metal preview render
│       │   ├── VideoFrame.cpp            # Frame pool management
│       │   ├── SubtitleRenderer.cpp/h    # libass / FreeType subtitle burn-in
│       │   └── MuxerDemuxer.cpp/h        # Container mux/demux (MP4, MKV, etc.)
│       ├── timeline/                     # Non-destructive timeline model
│       │   ├── Timeline.cpp/h            # Master timeline state
│       │   ├── AudioTrack.cpp/h          # Audio track with clip list
│       │   ├── VideoTrack.cpp/h          # Video track with clip list
│       │   ├── Clip.cpp/h                # Audio/video clip (non-destructive)
│       │   ├── Region.cpp/h              # Loop region / selection
│       │   ├── Marker.cpp/h              # Named position markers
│       │   ├── Automation.cpp/h          # Parameter automation lanes
│       │   ├── CrossfadeCalc.cpp/h       # Crossfade shape computation
│       │   └── TimelineCommands.cpp/h    # Undoable timeline operations
│       ├── dsp/                          # Built-in audio effects
│       │   ├── DspChain.cpp/h            # Ordered effect chain manager
│       │   ├── Biquad.h                  # Biquad filter coefficients
│       │   ├── ParametricEQ.cpp/h        # Multi-band parametric EQ
│       │   ├── GraphicEQ31.cpp/h         # 31-band graphic EQ
│       │   ├── Compressor.cpp/h          # Dynamic range compressor
│       │   ├── Limiter.cpp/h             # Brick-wall limiter
│       │   ├── NoiseGate.cpp/h           # Noise gate
│       │   ├── DeEsser.cpp/h             # Sibilance reducer
│       │   ├── SonicEnhancer.cpp/h       # Harmonic exciter
│       │   ├── Reverb.cpp/h              # Algorithmic reverb
│       │   ├── AGC.cpp/h                 # Automatic gain control
│       │   └── Normalizer.cpp/h          # Peak/RMS normalizer
│       ├── vfx/                          # Built-in video effects & transitions
│       │   ├── VideoEffectChain.cpp/h    # Ordered video effect chain
│       │   ├── CrossDissolve.cpp/h       # Alpha blend transition
│       │   ├── DipToBlack.cpp/h          # Black fade transition
│       │   ├── DipToWhite.cpp/h          # White fade transition
│       │   ├── WipeLeft.cpp/h            # Horizontal wipe
│       │   ├── PushSlide.cpp/h           # Slide push transition
│       │   ├── ZoomThrough.cpp/h         # Zoom-based transition
│       │   ├── LumaKey.cpp/h             # Luminance keying
│       │   ├── ChromaKey.cpp/h           # Chroma (green screen) keying
│       │   ├── GlitchCut.cpp/h           # Digital glitch transition
│       │   └── StingerOverlay.cpp/h      # Animated overlay (bumper)
│       ├── podcast/                      # Podcast production pipeline
│       │   ├── ChapterEditor.cpp/h       # Chapter mark editing
│       │   ├── MetadataEditor.cpp/h      # Tag editing (ID3/Vorbis/MP4)
│       │   ├── RSSGenerator.cpp/h        # Podcast RSS 2.0 feed builder
│       │   ├── PodcastExporter.cpp/h     # Loudness-targeted export
│       │   └── ShowNotesEditor.cpp/h     # Rich text show notes
│       ├── broadcast/                    # Live broadcast & recording
│       │   ├── LiveRecorder.cpp/h        # Multi-channel live capture
│       │   ├── StreamEncoder.cpp/h       # Icecast/DNAS/RTMP stream output
│       │   ├── BroadcastClock.cpp/h      # Segue timing and cue points
│       │   └── TallyLight.cpp/h          # On-air tally signal controller
│       ├── codec/                        # Audio codec wrappers
│       │   ├── CodecRegistry.cpp/h       # Codec discovery and factory
│       │   ├── WavCodec.cpp/h            # PCM WAV read/write
│       │   ├── FlacCodec.cpp/h           # FLAC encode/decode
│       │   ├── Mp3Codec.cpp/h            # MP3 (mpg123 + LAME)
│       │   ├── AacCodec.cpp/h            # AAC (FDK-AAC)
│       │   ├── OpusCodec.cpp/h           # Opus encode/decode
│       │   ├── VorbisCodec.cpp/h         # Vorbis encode/decode
│       │   └── FFmpegCodec.cpp/h         # Generic FFmpeg fallback
│       ├── graphics/                     # Broadcast graphics overlays
│       │   ├── OverlayEngine.cpp/h       # Layer compositor for overlays
│       │   ├── LowerThird.cpp/h          # Lower-third name straps
│       │   ├── TickerCrawl.cpp/h         # Scrolling ticker text
│       │   ├── Callout.cpp/h             # Annotation callouts
│       │   ├── Watermark.cpp/h           # Logo/watermark overlay
│       │   └── GraphicsRenderer.cpp/h    # GPU-accelerated overlay render
│       ├── widgets/                      # Qt 6 GUI widgets
│       │   ├── MainWindow.cpp/h          # Central window, docking, menus
│       │   ├── TimelineWidget.cpp/h      # Timeline ruler + track lanes
│       │   ├── TrackHeaderWidget.cpp/h   # Track name, arm, solo, mute
│       │   ├── WaveformView.cpp/h        # Zoomable waveform display
│       │   ├── VideoPreview.cpp/h        # Real-time video preview
│       │   ├── MixerWidget.cpp/h         # Mixing console (32 strips)
│       │   ├── VUMeterWidget.cpp/h       # Peak + RMS metering bars
│       │   ├── TransportBar.cpp/h        # Play/stop/record/rewind
│       │   ├── EffectsRackWidget.cpp/h   # Insert effects GUI
│       │   ├── MediaBrowser.cpp/h        # File/asset browser
│       │   ├── ChapterWidget.cpp/h       # Chapter list GUI
│       │   ├── MetadataPanel.cpp/h       # Tag metadata panel
│       │   ├── ExportDialog.cpp/h        # Export settings dialog
│       │   ├── PreferencesDialog.cpp/h   # Application preferences
│       │   ├── ProjectSettingsDialog.cpp/h # Project settings
│       │   ├── SpectrumWidget.cpp/h      # Real-time spectrum analyzer
│       │   ├── BevelButton.cpp/h         # Custom beveled push button
│       │   ├── EmbossedKnob.cpp/h        # Custom embossed rotary knob
│       │   ├── ThemeEngine.cpp/h         # YAML theme loader + QSS applicator
│       │   └── AboutDialog.cpp/h         # About/credits dialog
│       ├── platform/                     # OS-specific backends
│       │   ├── compat.h                  # Cross-platform compatibility macros
│       │   ├── worker_pool.cpp/h         # Thread pool for async work
│       │   ├── device_discovery.h/.mm    # macOS CoreAudio device enumeration
│       │   ├── macos_audio.h/.mm         # macOS AudioToolbox integration
│       │   └── macos_video.h/.mm         # macOS AVFoundation/VideoToolbox
│       ├── config/                       # Configuration & logging
│       │   ├── AppConfig.cpp/h           # User preferences persistence
│       │   ├── YamlPresets.cpp/h         # DSP/export preset loader
│       │   └── DebugLogger.cpp/h         # Timestamped logging subsystem
│       └── resources/
│           └── mcaster1dawcast.qrc       # Qt resource bundle
├── configs/                              # Preset configurations
│   ├── default_project.json              # New project template
│   ├── dsp_presets/                      # DSP chain presets (YAML)
│   │   ├── broadcast_chain.yaml
│   │   ├── music_master.yaml
│   │   ├── podcast_voice.yaml
│   │   └── spoken_word.yaml
│   └── export_profiles/                  # Export format presets (YAML)
│       ├── broadcast_wav.yaml
│       ├── podcast_aac.yaml
│       ├── podcast_mp3.yaml
│       ├── webm_vp9.yaml
│       └── youtube_1080p.yaml
├── themes/                               # UI theme packs
│   ├── Default/                          # Native platform theme
│   │   ├── theme.yaml
│   │   └── style.qss
│   ├── DarkStudio/                       # Deep blue-black DAW theme
│   │   ├── theme.yaml
│   │   └── style.qss
│   └── BroadcastPro/                     # High-contrast on-air theme
│       ├── theme.yaml
│       └── style.qss
├── build/                                # Build output (.app, .dmg, .pkg)
├── docs/                                 # Documentation
├── tests/                                # Unit and integration tests
├── scripts/                              # Build/packaging scripts
├── installer/                            # Installer resources
└── image_resources/                      # Icons and artwork
```

</details>

<details>
<summary><strong>Module Architecture</strong></summary>

```
┌─────────────────────────────────────────────────────────────────────┐
│                         App Singleton                               │
│  ModuleRegistry · ProjectManager · UndoManager · AppConfig          │
└────────┬──────────────────────────────────────────────┬─────────────┘
         │                                              │
   ┌─────▼──────────┐                           ┌──────▼──────────┐
   │  Audio Engine   │                           │  Video Engine   │
   │  AudioEngine    │◄── RingBuffer ──►         │  VideoDecoder   │
   │  AudioMixer     │                           │  VideoEncoder   │
   │  AudioResampler │                           │  VideoMixer     │
   │  AudioWorker    │                           │  VideoRenderer  │
   │  CodecRegistry  │                           │  MuxerDemuxer   │
   └────────┬────────┘                           │  SubtitleRender │
            │                                    └──────┬──────────┘
            │                                           │
   ┌────────▼────────────────────────────────────────────▼──────────┐
   │                         Timeline                                │
   │  AudioTrack · VideoTrack · Clip · Region · Marker · Automation  │
   │  CrossfadeCalc · TimelineCommands (undoable)                    │
   └────────┬─────────────────────────────────────────────┬─────────┘
            │                                             │
   ┌────────▼────────┐   ┌──────────┐   ┌───────────────▼──────────┐
   │  DSP Chain       │   │ VFX Chain│   │  Graphics / Broadcast     │
   │  ParametricEQ    │   │ Dissolve │   │  OverlayEngine            │
   │  GraphicEQ31     │   │ Wipe     │   │  LowerThird · TickerCrawl │
   │  Compressor      │   │ ChromaKey│   │  StreamEncoder            │
   │  Limiter · Gate  │   │ LumaKey  │   │  BroadcastClock           │
   │  DeEsser · Reverb│   │ Stinger  │   │  LiveRecorder · TallyLight│
   │  AGC · Normalizer│   │ GlitchCut│   │  PodcastExporter · RSS    │
   └─────────────────┘   └──────────┘   └──────────────────────────┘
```

</details>

---

## Documentation

| Document | Description |
|:---|:---|
| [PLAN.md](PLAN.md) | Architecture decisions, phase roadmap, module breakdown |
| [configs/default_project.json](configs/default_project.json) | Default project template -- shows all configurable fields |
| [configs/dsp_presets/](configs/dsp_presets/) | Built-in DSP processing chain presets |
| [configs/export_profiles/](configs/export_profiles/) | Audio/video export format presets |

---

## Mcaster1 Ecosystem

Mcaster1DAWCast is part of the Mcaster1 broadcast and media production suite:

| Component | Purpose | DAWCast Integration |
|:---|:---|:---|
| **[Mcaster1AMP](../Mcaster1AMP/)** | Broadcast-grade media player | Source audio tracks, preview playback |
| **[Mcaster1AudioPipe](../Mcaster1AudioPipe/)** | Virtual audio routing (CoreAudio HAL) | Route DAWCast output to other apps |
| **[Mcaster1DSPEncoder](../Mcaster1DSPEncoder/)** | Real-time DSP + stream encoder | Encode DAWCast master bus for broadcast |
| **[mcaster1dnas](../mcaster1dnas/)** | DNAS streaming server | Stream target for live broadcast output |
| **[Mcaster1Studio](../Mcaster1Studio/)** | All-in-one broadcast suite | Shared codec/DSP libraries |
| **[Mcaster1KeySmith](../Mcaster1KeySmith/)** | License key management | Product activation |
| **[Mcaster1MailCaster](../Mcaster1MailCaster/)** | Email/notification system | Publish notifications |
| **[Mcaster1SyncIt](../Mcaster1SyncIt/)** | File synchronization | Project sync between workstations |
| **[Mcaster1InstallSystem](../Mcaster1InstallSystem/)** | Installer builder | DMG/PKG/EXE packaging |
| **[Mcaster1FileCaster](../Mcaster1FileCaster/)** | File distribution | Distribute rendered media |

```
┌──────────────┐     AudioPipe     ┌───────────────┐     encoded      ┌────────────┐
│ Mcaster1     │ ─────────────────▶│ Mcaster1      │ ────────────────▶│ Mcaster1   │
│ DAWCast      │  virtual device   │ DSPEncoder    │  Icecast/RTMP    │ DNAS       │
│ (Production) │                   │ (Broadcast)   │                  │ (Server)   │
└──────────────┘                   └───────────────┘                  └────────────┘
```

---

## License

Copyright &copy; 2026 [David St. John](mailto:davestj@gmail.com)

Released under the **GPL-2.0-or-later** license. See [LICENSE](LICENSE) for details.
