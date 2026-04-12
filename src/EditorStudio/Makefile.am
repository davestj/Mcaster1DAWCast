# Makefile.am — DAWCast Editor Studio (forensic single-file editor)
#
# Standalone application sharing DSP, codec, and FFmpeg libraries with the main DAW.
# Build: make -C src/EditorStudio -j$(sysctl -n hw.ncpu)
# Output: dawcast-editor-studio (Mach-O ARM64)

bin_PROGRAMS = dawcast-editor-studio

# ── DAWCast source root (for shared codec, dsp, core headers) ───────────────
DAWCAST_SRC = $(srcdir)/../DAWCast

# ── Common flags ─────────────────────────────────────────────────────────────
AM_CPPFLAGS = \
    -Wno-implicit-function-declaration \
    -I$(srcdir) \
    -I$(DAWCAST_SRC) \
    -I$(DAWCAST_SRC)/core \
    -I$(DAWCAST_SRC)/codec \
    -I$(DAWCAST_SRC)/dsp \
    -I$(DAWCAST_SRC)/audio_engine \
    -I$(DAWCAST_SRC)/video_engine \
    -std=c++17 \
    -DHAVE_PORTAUDIO \
    -DHAVE_TAGLIB \
    -DHAVE_SQLITE3 \
    $(QT6CORE_CFLAGS) \
    $(QT6GUI_CFLAGS) \
    $(QT6WIDGETS_CFLAGS) \
    $(QT6NETWORK_CFLAGS) \
    $(QT6CONCURRENT_CFLAGS) \
    $(PORTAUDIO_CFLAGS) \
    $(AVFORMAT_CFLAGS) \
    $(TAGLIB_CFLAGS) \
    $(SQLITE3_CFLAGS) \
    $(LIBYAML_CFLAGS)

dawcast_editor_studio_LDADD = \
    $(QT6WIDGETS_LIBS) \
    $(QT6CORE_LIBS) \
    $(QT6GUI_LIBS) \
    $(QT6NETWORK_LIBS) \
    $(QT6CONCURRENT_LIBS) \
    $(PORTAUDIO_LIBS) \
    $(AVFORMAT_LIBS) \
    $(TAGLIB_LIBS) \
    $(SQLITE3_LIBS) \
    $(LIBYAML_LIBS) \
    $(MACOS_FRAMEWORKS)

# ── Optional codec flags (mirrors DAWCast/Makefile.am) ───────────────────────
if HAVE_MPG123
AM_CPPFLAGS += $(MPG123_CFLAGS) -DHAVE_MPG123
dawcast_editor_studio_LDADD += $(MPG123_LIBS)
endif

if HAVE_AVFORMAT
AM_CPPFLAGS += -DHAVE_AVFORMAT
endif

if HAVE_LAME
AM_CPPFLAGS += $(LAME_CFLAGS) -DHAVE_LAME
dawcast_editor_studio_LDADD += $(LAME_LIBS)
endif

if HAVE_VORBIS
AM_CPPFLAGS += $(VORBIS_CFLAGS) -DHAVE_VORBIS
dawcast_editor_studio_LDADD += $(VORBIS_LIBS)
endif

if HAVE_FLAC
AM_CPPFLAGS += $(FLAC_CFLAGS) -DHAVE_FLAC
dawcast_editor_studio_LDADD += $(FLAC_LIBS)
endif

if HAVE_OPUS
AM_CPPFLAGS += $(OPUS_CFLAGS) -DHAVE_OPUS
dawcast_editor_studio_LDADD += $(OPUS_LIBS)
endif

if HAVE_FDKAAC
AM_CPPFLAGS += $(FDKAAC_CFLAGS) -DHAVE_FDKAAC
dawcast_editor_studio_LDADD += $(FDKAAC_LIBS)
endif

if HAVE_OPENSSL
AM_CPPFLAGS += $(OPENSSL_CFLAGS) -DHAVE_OPENSSL
dawcast_editor_studio_LDADD += $(OPENSSL_LIBS)
endif

if HAVE_QT6MULTIMEDIA
AM_CPPFLAGS += $(QT6MULTIMEDIA_CFLAGS) -DHAVE_QT6MULTIMEDIA
dawcast_editor_studio_LDADD += $(QT6MULTIMEDIA_LIBS)
endif

if HAVE_QT6SVG
AM_CPPFLAGS += $(QT6SVG_CFLAGS) -DHAVE_QT6SVG
dawcast_editor_studio_LDADD += $(QT6SVG_LIBS)
endif

# ── MOC rules (Qt Meta-Object Compiler) ─────────────────────────────────────
MOC = $(QT6_MOC)
MOCFLAGS = $(QT6CORE_CFLAGS) $(QT6WIDGETS_CFLAGS) $(QT6GUI_CFLAGS)
MOCFLAGS_CLEAN = $(filter-out -f%,$(MOCFLAGS))

moc_EditorStudioWindow.cpp: $(srcdir)/EditorStudioWindow.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_ForensicWaveformView.cpp: $(srcdir)/ForensicWaveformView.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_SpectralAnalysisPanel.cpp: $(srcdir)/SpectralAnalysisPanel.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_ForensicDetector.cpp: $(srcdir)/ForensicDetector.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_PlayerControls.cpp: $(srcdir)/PlayerControls.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

# ── Source files ─────────────────────────────────────────────────────────────
# Editor Studio's own sources + shared codec/core sources from DAWCast
dawcast_editor_studio_SOURCES = \
    main.cpp \
    EditorStudioWindow.h \
    EditorStudioWindow.cpp \
    ForensicWaveformView.h \
    ForensicWaveformView.cpp \
    SpectralAnalysisPanel.h \
    SpectralAnalysisPanel.cpp \
    ForensicDetector.h \
    ForensicDetector.cpp \
    PlayerControls.h \
    PlayerControls.cpp \
    $(DAWCAST_SRC)/core/AudioBuffer.h \
    $(DAWCAST_SRC)/core/VideoFrame.h \
    $(DAWCAST_SRC)/dsp/FFT.h \
    $(DAWCAST_SRC)/codec/FFmpegCodec.h \
    $(DAWCAST_SRC)/codec/FFmpegCodec.cpp \
    $(DAWCAST_SRC)/codec/WavCodec.h \
    $(DAWCAST_SRC)/codec/WavCodec.cpp

# ── MOC-generated sources ───────────────────────────────────────────────────
nodist_dawcast_editor_studio_SOURCES = \
    moc_EditorStudioWindow.cpp \
    moc_ForensicWaveformView.cpp \
    moc_SpectralAnalysisPanel.cpp \
    moc_ForensicDetector.cpp \
    moc_PlayerControls.cpp

# ── Build ordering ──────────────────────────────────────────────────────────
BUILT_SOURCES = \
    moc_EditorStudioWindow.cpp \
    moc_ForensicWaveformView.cpp \
    moc_SpectralAnalysisPanel.cpp \
    moc_ForensicDetector.cpp \
    moc_PlayerControls.cpp

# ── Clean generated files ──────────────────────────────────────────────────
CLEANFILES = moc_*.cpp

.PHONY: all-local
