# Changelog

All notable changes to Mcaster1DAWCast will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0-alpha] - 2026-04-02

### Added
- Project scaffold with complete directory structure
- Autotools build system (autogen.sh, configure.ac, Makefile.am)
- Core module interfaces (IModule, IEffectUnit, IVideoEffect)
- Audio engine skeleton (PortAudio, AudioMixer, RingBuffer)
- Video engine skeleton (FFmpeg decode/encode, VideoMixer, SubtitleRenderer)
- Timeline model (tracks, clips, regions, markers, automation)
- 10 audio DSP effects (ParametricEQ, GraphicEQ31, Compressor, Limiter, NoiseGate, DeEsser, SonicEnhancer, Reverb, AGC, Normalizer)
- 10 video transitions (CrossDissolve, DipToBlack, DipToWhite, WipeLeft, PushSlide, ZoomThrough, LumaKey, ChromaKey, GlitchCut, StingerOverlay)
- Podcast production module (chapters, metadata, RSS, export)
- Broadcast module (live recorder, stream encoder, broadcast clock)
- Codec wrappers (WAV, FLAC, MP3, AAC, Opus, Vorbis, FFmpeg generic)
- Broadcast graphics (overlays, lower thirds, ticker, callouts, watermarks)
- Qt 6 widget set (MainWindow, Timeline, Mixer, VideoPreview, Transport, etc.)
- Theme engine with 3 themes (Default, Dark Studio, Broadcast Pro)
- DSP preset library (broadcast chain, podcast voice, music master, spoken word)
- Export profiles (podcast MP3, podcast AAC, broadcast WAV, YouTube 1080p, WebM VP9)
- Cross-platform installer setup (macOS PKG/DMG, Windows NSIS, Linux DEB/RPM)
- Project documentation (PLAN.md, README.md, docs/PLANNING.html, docs/index.html)
- JSON project file format (.dawcast)
- Debug logging with stack traces
