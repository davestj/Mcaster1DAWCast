# Makefile.am — Mcaster1DAWCast Qt 6 DAW / NLE build
#
# Podcast & broadcast DAW with timeline, DSP, video engine, and graphics overlay.
# Build: make -C src/DAWCast -j$(sysctl -n hw.ncpu)
# Output: mcaster1dawcast (Mach-O ARM64)

bin_PROGRAMS = mcaster1dawcast

# ── Common flags ─────────────────────────────────────────────────────────────
VST3SDK = $(srcdir)/../../plugins/Mcaster1Tuner/build/_deps/vst3sdk-src

AM_CPPFLAGS = \
    -Wno-implicit-function-declaration \
    -I$(srcdir) \
    -I$(srcdir)/core \
    -I$(srcdir)/audio_engine \
    -I$(srcdir)/video_engine \
    -I$(srcdir)/timeline \
    -I$(srcdir)/dsp \
    -I$(srcdir)/dsp/mc1 \
    -I$(srcdir)/vfx \
    -I$(srcdir)/podcast \
    -I$(srcdir)/broadcast \
    -I$(srcdir)/codec \
    -I$(srcdir)/graphics \
    -I$(srcdir)/widgets \
    -I$(srcdir)/plugins \
    -I$(srcdir)/platform \
    -I$(srcdir)/config \
    -I$(srcdir)/ai \
    -I$(VST3SDK) \
    -DHAVE_VST3 \
    -DRELEASE \
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
    $(LIBYAML_CFLAGS) \
    $(YAMLCPP_CFLAGS)

AM_OBJCXXFLAGS = $(AM_CPPFLAGS)

mcaster1dawcast_LDADD = \
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
    $(YAMLCPP_LIBS) \
    $(MACOS_FRAMEWORKS)

# ── Codec flags (optional, added per-codec) ──────────────────────────────────
if HAVE_MPG123
AM_CPPFLAGS += $(MPG123_CFLAGS) -DHAVE_MPG123
mcaster1dawcast_LDADD += $(MPG123_LIBS)
endif

if HAVE_AVFORMAT
AM_CPPFLAGS += -DHAVE_AVFORMAT
endif

if HAVE_LAME
AM_CPPFLAGS += $(LAME_CFLAGS) -DHAVE_LAME
mcaster1dawcast_LDADD += $(LAME_LIBS)
endif

if HAVE_VORBIS
AM_CPPFLAGS += $(VORBIS_CFLAGS) -DHAVE_VORBIS
mcaster1dawcast_LDADD += $(VORBIS_LIBS)
endif

if HAVE_FLAC
AM_CPPFLAGS += $(FLAC_CFLAGS) -DHAVE_FLAC
mcaster1dawcast_LDADD += $(FLAC_LIBS)
endif

if HAVE_OPUS
AM_CPPFLAGS += $(OPUS_CFLAGS) -DHAVE_OPUS
mcaster1dawcast_LDADD += $(OPUS_LIBS)
endif

if HAVE_FDKAAC
AM_CPPFLAGS += $(FDKAAC_CFLAGS) -DHAVE_FDKAAC
mcaster1dawcast_LDADD += $(FDKAAC_LIBS)
endif

if HAVE_OPENSSL
AM_CPPFLAGS += $(OPENSSL_CFLAGS) -DHAVE_OPENSSL
mcaster1dawcast_LDADD += $(OPENSSL_LIBS)
endif

if HAVE_QT6MULTIMEDIA
AM_CPPFLAGS += $(QT6MULTIMEDIA_CFLAGS) -DHAVE_QT6MULTIMEDIA
mcaster1dawcast_LDADD += $(QT6MULTIMEDIA_LIBS)
endif

if HAVE_QT6SVG
AM_CPPFLAGS += $(QT6SVG_CFLAGS) -DHAVE_QT6SVG
mcaster1dawcast_LDADD += $(QT6SVG_LIBS)
endif

# ── Objective-C++ (.mm) compilation rule ─────────────────────────────────────
SUFFIXES = .mm
.mm.o:
	$(OBJCXX) $(AM_OBJCXXFLAGS) $(OBJCXXFLAGS) -fobjc-arc -c -o $@ $<

# ── MOC rules (Qt Meta-Object Compiler) ──────────────────────────────────────
MOC = $(QT6_MOC)
MOCFLAGS = $(QT6CORE_CFLAGS) $(QT6WIDGETS_CFLAGS) $(QT6GUI_CFLAGS)
# Strip -f flags from MOCFLAGS (MOC misinterprets -f as force-include)
MOCFLAGS_CLEAN = $(filter-out -f%,$(MOCFLAGS))

# -- app --
moc_app.cpp: $(srcdir)/app.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

# -- core --
moc_ModuleRegistry.cpp: $(srcdir)/core/ModuleRegistry.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_UndoManager.cpp: $(srcdir)/core/UndoManager.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_ProjectManager.cpp: $(srcdir)/core/ProjectManager.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_ViewModeManager.cpp: $(srcdir)/core/ViewModeManager.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_WorkspaceManager.cpp: $(srcdir)/core/WorkspaceManager.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_MediaLibrary.cpp: $(srcdir)/core/MediaLibrary.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

# -- audio_engine --
moc_AudioEngine.cpp: $(srcdir)/audio_engine/AudioEngine.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_AudioMixer.cpp: $(srcdir)/audio_engine/AudioMixer.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_AudioWorker.cpp: $(srcdir)/audio_engine/AudioWorker.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_WaveformCache.cpp: $(srcdir)/audio_engine/WaveformCache.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_PlaybackEngine.cpp: $(srcdir)/audio_engine/PlaybackEngine.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_MultitrackRecorder.cpp: $(srcdir)/audio_engine/MultitrackRecorder.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_TrackBouncer.cpp: $(srcdir)/audio_engine/TrackBouncer.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_Metronome.cpp: $(srcdir)/audio_engine/Metronome.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_AudioBus.cpp: $(srcdir)/audio_engine/AudioBus.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_BusRouter.cpp: $(srcdir)/audio_engine/BusRouter.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

# -- video_engine --
moc_VideoDecoder.cpp: $(srcdir)/video_engine/VideoDecoder.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_VideoEncoder.cpp: $(srcdir)/video_engine/VideoEncoder.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_VideoMixer.cpp: $(srcdir)/video_engine/VideoMixer.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_VideoRenderer.cpp: $(srcdir)/video_engine/VideoRenderer.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_SubtitleRenderer.cpp: $(srcdir)/video_engine/SubtitleRenderer.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_MuxerDemuxer.cpp: $(srcdir)/video_engine/MuxerDemuxer.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_VideoPlaybackController.cpp: $(srcdir)/video_engine/VideoPlaybackController.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

# -- timeline --
moc_Timeline.cpp: $(srcdir)/timeline/Timeline.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_AudioTrack.cpp: $(srcdir)/timeline/AudioTrack.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_VideoTrack.cpp: $(srcdir)/timeline/VideoTrack.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_Clip.cpp: $(srcdir)/timeline/Clip.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_Automation.cpp: $(srcdir)/timeline/Automation.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_MidiClip.cpp: $(srcdir)/timeline/MidiClip.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_MidiTrack.cpp: $(srcdir)/timeline/MidiTrack.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_TrackGroup.cpp: $(srcdir)/timeline/TrackGroup.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_TempoMap.cpp: $(srcdir)/timeline/TempoMap.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

# -- plugins --
moc_PluginScanner.cpp: $(srcdir)/plugins/PluginScanner.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_PluginHost.cpp: $(srcdir)/plugins/PluginHost.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

# -- dsp --
moc_DspChain.cpp: $(srcdir)/dsp/DspChain.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

# -- vfx --
moc_VideoEffectChain.cpp: $(srcdir)/vfx/VideoEffectChain.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

# -- podcast --
moc_ChapterEditor.cpp: $(srcdir)/podcast/ChapterEditor.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_MetadataEditor.cpp: $(srcdir)/podcast/MetadataEditor.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_RSSGenerator.cpp: $(srcdir)/podcast/RSSGenerator.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_PodcastExporter.cpp: $(srcdir)/podcast/PodcastExporter.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_ShowNotesEditor.cpp: $(srcdir)/podcast/ShowNotesEditor.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

# -- broadcast --
moc_LiveRecorder.cpp: $(srcdir)/broadcast/LiveRecorder.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_StreamEncoder.cpp: $(srcdir)/broadcast/StreamEncoder.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_BroadcastClock.cpp: $(srcdir)/broadcast/BroadcastClock.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_TallyLight.cpp: $(srcdir)/broadcast/TallyLight.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_RTMPStreamer.cpp: $(srcdir)/broadcast/RTMPStreamer.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

# -- codec --
moc_CodecRegistry.cpp: $(srcdir)/codec/CodecRegistry.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

# -- graphics --
moc_OverlayEngine.cpp: $(srcdir)/graphics/OverlayEngine.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_LowerThird.cpp: $(srcdir)/graphics/LowerThird.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_TickerCrawl.cpp: $(srcdir)/graphics/TickerCrawl.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_Callout.cpp: $(srcdir)/graphics/Callout.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_Watermark.cpp: $(srcdir)/graphics/Watermark.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_GraphicsRenderer.cpp: $(srcdir)/graphics/GraphicsRenderer.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

# -- widgets --
moc_MainWindow.cpp: $(srcdir)/widgets/MainWindow.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_TimelineWidget.cpp: $(srcdir)/widgets/TimelineWidget.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_TimelineScrollBar.cpp: $(srcdir)/widgets/TimelineScrollBar.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_TrackHeaderWidget.cpp: $(srcdir)/widgets/TrackHeaderWidget.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_TrackHeaderPanel.cpp: $(srcdir)/widgets/TrackHeaderPanel.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_WaveformView.cpp: $(srcdir)/widgets/WaveformView.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_VideoPreview.cpp: $(srcdir)/widgets/VideoPreview.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_MixerWidget.cpp: $(srcdir)/widgets/MixerWidget.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_VUMeterWidget.cpp: $(srcdir)/widgets/VUMeterWidget.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_TransportBar.cpp: $(srcdir)/widgets/TransportBar.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_EffectsRackWidget.cpp: $(srcdir)/widgets/EffectsRackWidget.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_MediaBrowser.cpp: $(srcdir)/widgets/MediaBrowser.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_MediaLibraryWidget.cpp: $(srcdir)/widgets/MediaLibraryWidget.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_LibraryTableModel.cpp: $(srcdir)/widgets/LibraryTableModel.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_ChapterWidget.cpp: $(srcdir)/widgets/ChapterWidget.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_MetadataPanel.cpp: $(srcdir)/widgets/MetadataPanel.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_ExportDialog.cpp: $(srcdir)/widgets/ExportDialog.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_PreferencesDialog.cpp: $(srcdir)/widgets/PreferencesDialog.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_ProjectSettingsDialog.cpp: $(srcdir)/widgets/ProjectSettingsDialog.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_SpectrumWidget.cpp: $(srcdir)/widgets/SpectrumWidget.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_BevelButton.cpp: $(srcdir)/widgets/BevelButton.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_EmbossedKnob.cpp: $(srcdir)/widgets/EmbossedKnob.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_ThemeEngine.cpp: $(srcdir)/widgets/ThemeEngine.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_AboutDialog.cpp: $(srcdir)/widgets/AboutDialog.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_PianoRollWidget.cpp: $(srcdir)/widgets/PianoRollWidget.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_ParametricEQDialog.cpp: $(srcdir)/widgets/ParametricEQDialog.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_LUFSMeterWidget.cpp: $(srcdir)/widgets/LUFSMeterWidget.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_SplashScreen.cpp: $(srcdir)/widgets/SplashScreen.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_TimeStretchDialog.cpp: $(srcdir)/widgets/TimeStretchDialog.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_CrossfadeEditorDialog.cpp: $(srcdir)/widgets/CrossfadeEditorDialog.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

# -- audio_engine (ExportEngine, BatchEncoder) --
moc_ExportEngine.cpp: $(srcdir)/audio_engine/ExportEngine.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_BatchEncoder.cpp: $(srcdir)/audio_engine/BatchEncoder.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

# -- widgets (BatchEncoderDialog) --
moc_BatchEncoderDialog.cpp: $(srcdir)/widgets/BatchEncoderDialog.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

# -- widgets (MassTagEditor) --
moc_MassTagEditor.cpp: $(srcdir)/widgets/MassTagEditor.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_TagTableModel.cpp: $(srcdir)/widgets/TagTableModel.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_FilenameToTagDialog.cpp: $(srcdir)/widgets/FilenameToTagDialog.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_TagToFilenameDialog.cpp: $(srcdir)/widgets/TagToFilenameDialog.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_StreamingDialog.cpp: $(srcdir)/widgets/StreamingDialog.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_ActionBar.cpp: $(srcdir)/widgets/ActionBar.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_MasterStrip.cpp: $(srcdir)/widgets/MasterStrip.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_SidebarNav.cpp: $(srcdir)/widgets/SidebarNav.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_ViewModeSwitcher.cpp: $(srcdir)/widgets/ViewModeSwitcher.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_ScriptReaderPanel.cpp: $(srcdir)/widgets/ScriptReaderPanel.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_PedalboardWidget.cpp: $(srcdir)/widgets/PedalboardWidget.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_StreamMonitorPanel.cpp: $(srcdir)/widgets/StreamMonitorPanel.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_TrackColorDialog.cpp: $(srcdir)/widgets/TrackColorDialog.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_MarkerListWidget.cpp: $(srcdir)/widgets/MarkerListWidget.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_ImportAudioDialog.cpp: $(srcdir)/widgets/ImportAudioDialog.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

# -- config --
moc_AppConfig.cpp: $(srcdir)/config/AppConfig.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

# -- mc1 (Mediacast One) DSP plugin UI dialogs (Q_OBJECT) --
# Prefixed `moc_mc1_*` to avoid case-insensitive filesystem collisions
# with DAWCast's existing `moc_ParametricEQDialog.cpp` etc.
moc_mc1_ParametricEqDialog.cpp: $(srcdir)/dsp/mc1/fx_ui/ParametricEqDialog.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_mc1_DualEq15Dialog.cpp: $(srcdir)/dsp/mc1/fx_ui/DualEq15Dialog.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_mc1_GraphicEq31Dialog.cpp: $(srcdir)/dsp/mc1/fx_ui/GraphicEq31Dialog.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_mc1_CompressorDialog.cpp: $(srcdir)/dsp/mc1/fx_ui/CompressorDialog.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_mc1_Dbx166xsDialog.cpp: $(srcdir)/dsp/mc1/fx_ui/Dbx166xsDialog.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_mc1_BroadcastAgcDialog.cpp: $(srcdir)/dsp/mc1/fx_ui/BroadcastAgcDialog.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_mc1_DbxVoiceDialog.cpp: $(srcdir)/dsp/mc1/fx_ui/DbxVoiceDialog.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_mc1_XenyxPreampDialog.cpp: $(srcdir)/dsp/mc1/fx_ui/XenyxPreampDialog.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_mc1_SonicEnhancerDialog.cpp: $(srcdir)/dsp/mc1/fx_ui/SonicEnhancerDialog.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_mc1_TubePreampDialog.cpp: $(srcdir)/dsp/mc1/fx_ui/TubePreampDialog.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_mc1_CasterTubeDialog.cpp: $(srcdir)/dsp/mc1/fx_ui/CasterTubeDialog.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_mc1_MicModelerDialog.cpp: $(srcdir)/dsp/mc1/fx_ui/MicModelerDialog.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_mc1_Lexicon224Dialog.cpp: $(srcdir)/dsp/mc1/fx_ui/Lexicon224Dialog.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_mc1_LexiconPcm70Dialog.cpp: $(srcdir)/dsp/mc1/fx_ui/LexiconPcm70Dialog.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_mc1_LexiconPcm96Dialog.cpp: $(srcdir)/dsp/mc1/fx_ui/LexiconPcm96Dialog.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_mc1_Lexicon480LDialog.cpp: $(srcdir)/dsp/mc1/fx_ui/Lexicon480LDialog.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_mc1_LexiconMpx1Dialog.cpp: $(srcdir)/dsp/mc1/fx_ui/LexiconMpx1Dialog.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_mc1_PodcastPluginDialog.cpp: $(srcdir)/dsp/mc1/fx_ui/PodcastPluginDialog.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_mc1_MusicStudioDialogs.cpp: $(srcdir)/dsp/mc1/fx_ui/MusicStudioDialogs.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_mc1_BBESonicSweetDialogs.cpp: $(srcdir)/dsp/mc1/fx_ui/BBESonicSweetDialogs.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_mc1_DbxDialogs.cpp: $(srcdir)/dsp/mc1/fx_ui/DbxDialogs.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_mc1_FlagshipDialogs.cpp: $(srcdir)/dsp/mc1/fx_ui/FlagshipDialogs.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_mc1_SignalHillStudioDialog.cpp: $(srcdir)/dsp/mc1/fx_ui/SignalHillStudioDialog.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_mc1_RackKnob.cpp: $(srcdir)/dsp/mc1/fx_ui/RackKnob.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_mc1_RackMeter.cpp: $(srcdir)/dsp/mc1/fx_ui/RackMeter.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_mc1_EqCurveWidget.cpp: $(srcdir)/dsp/mc1/fx_ui/EqCurveWidget.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

# -- ai --
moc_AIEngine.cpp: $(srcdir)/ai/AIEngine.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_TranscriptionEngine.cpp: $(srcdir)/ai/TranscriptionEngine.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_AIAssistant.cpp: $(srcdir)/ai/AIAssistant.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

moc_AIPanel.cpp: $(srcdir)/ai/AIPanel.h
	$(MOC) $(MOCFLAGS_CLEAN) $< -o $@

# ── RCC rules (Qt Resource Compiler) ─────────────────────────────────────────
RCC = $(QT6_RCC)

qrc_mcaster1dawcast.cpp: $(srcdir)/resources/mcaster1dawcast.qrc
	$(RCC) $< -o $@ --name mcaster1dawcast

# ── Source files ─────────────────────────────────────────────────────────────
mcaster1dawcast_SOURCES = \
    main.cpp \
    app.h \
    app.cpp \
    core/IModule.h \
    core/IEffectUnit.h \
    core/IVideoEffect.h \
    core/IPlugin.h \
    core/AudioBuffer.h \
    core/VideoFrame.h \
    core/MediaItem.h \
    core/ModuleRegistry.h \
    core/ModuleRegistry.cpp \
    core/UndoManager.h \
    core/UndoManager.cpp \
    core/ProjectManager.h \
    core/ProjectManager.cpp \
    core/MediaLibrary.h \
    core/MediaLibrary.cpp \
    core/ViewModeManager.h \
    core/ViewModeManager.cpp \
    core/WorkspaceManager.h \
    core/WorkspaceManager.cpp \
    audio_engine/AudioEngine.h \
    audio_engine/AudioEngine.cpp \
    audio_engine/AudioMixer.h \
    audio_engine/AudioMixer.cpp \
    audio_engine/AudioResampler.h \
    audio_engine/AudioResampler.cpp \
    audio_engine/RingBuffer.h \
    audio_engine/AudioWorker.h \
    audio_engine/AudioWorker.cpp \
    audio_engine/WaveformCache.h \
    audio_engine/WaveformCache.cpp \
    audio_engine/PlaybackEngine.h \
    audio_engine/PlaybackEngine.cpp \
    audio_engine/MultitrackRecorder.h \
    audio_engine/MultitrackRecorder.cpp \
    audio_engine/TrackBouncer.h \
    audio_engine/TrackBouncer.cpp \
    audio_engine/AudioClipReader.h \
    audio_engine/AudioClipReader.cpp \
    audio_engine/ExportEngine.h \
    audio_engine/ExportEngine.cpp \
    audio_engine/BatchEncoder.h \
    audio_engine/BatchEncoder.cpp \
    audio_engine/Metronome.h \
    audio_engine/Metronome.cpp \
    audio_engine/AudioBus.h \
    audio_engine/AudioBus.cpp \
    audio_engine/BusRouter.h \
    audio_engine/BusRouter.cpp \
    video_engine/VideoDecoder.h \
    video_engine/VideoDecoder.cpp \
    video_engine/VideoEncoder.h \
    video_engine/VideoEncoder.cpp \
    video_engine/VideoMixer.h \
    video_engine/VideoMixer.cpp \
    video_engine/VideoRenderer.h \
    video_engine/VideoRenderer.cpp \
    video_engine/SubtitleRenderer.h \
    video_engine/SubtitleRenderer.cpp \
    video_engine/VideoFrame.cpp \
    video_engine/MuxerDemuxer.h \
    video_engine/MuxerDemuxer.cpp \
    video_engine/VideoPlaybackController.h \
    video_engine/VideoPlaybackController.cpp \
    timeline/Timeline.h \
    timeline/Timeline.cpp \
    timeline/AudioTrack.h \
    timeline/AudioTrack.cpp \
    timeline/VideoTrack.h \
    timeline/VideoTrack.cpp \
    timeline/Clip.h \
    timeline/Clip.cpp \
    timeline/Region.h \
    timeline/Region.cpp \
    timeline/Marker.h \
    timeline/Marker.cpp \
    timeline/Automation.h \
    timeline/Automation.cpp \
    timeline/CrossfadeCalc.h \
    timeline/CrossfadeCalc.cpp \
    timeline/TimelineCommands.h \
    timeline/TimelineCommands.cpp \
    timeline/MidiEvent.h \
    timeline/MidiClip.h \
    timeline/MidiClip.cpp \
    timeline/MidiTrack.h \
    timeline/MidiTrack.cpp \
    timeline/TrackGroup.h \
    timeline/TrackGroup.cpp \
    timeline/TempoMap.h \
    timeline/TempoMap.cpp \
    dsp/DspChain.h \
    dsp/DspChain.cpp \
    dsp/Biquad.h \
    dsp/ParametricEQ.h \
    dsp/ParametricEQ.cpp \
    dsp/GraphicEQ31.h \
    dsp/GraphicEQ31.cpp \
    dsp/Compressor.h \
    dsp/Compressor.cpp \
    dsp/Limiter.h \
    dsp/Limiter.cpp \
    dsp/NoiseGate.h \
    dsp/NoiseGate.cpp \
    dsp/DeEsser.h \
    dsp/DeEsser.cpp \
    dsp/SonicEnhancer.h \
    dsp/SonicEnhancer.cpp \
    dsp/Reverb.h \
    dsp/Reverb.cpp \
    dsp/AGC.h \
    dsp/AGC.cpp \
    dsp/Normalizer.h \
    dsp/Normalizer.cpp \
    dsp/FFT.h \
    dsp/NoiseReduction.h \
    dsp/NoiseReduction.cpp \
    dsp/TimeStretch.h \
    dsp/TimeStretch.cpp \
    dsp/mc1/Mc1EffectAdapter.h \
    dsp/mc1/Mc1EffectRegistry.h \
    dsp/mc1/Mc1DialogFactory.h \
    dsp/mc1/patchbay/dsp/dsp_effect.h \
    dsp/mc1/patchbay/dsp/effect_factory.h \
    dsp/mc1/patchbay/dsp/preset_manager.h \
    dsp/mc1/patchbay/dsp/preset_manager.cpp \
    dsp/mc1/patchbay/dsp/fx_parametric_eq.h \
    dsp/mc1/patchbay/dsp/fx_dual_eq15.h \
    dsp/mc1/patchbay/dsp/fx_graphic_eq31.h \
    dsp/mc1/patchbay/dsp/fx_compressor.h \
    dsp/mc1/patchbay/dsp/fx_dbx166xs.h \
    dsp/mc1/patchbay/dsp/fx_broadcast_agc.h \
    dsp/mc1/patchbay/dsp/fx_dbx_voice.h \
    dsp/mc1/patchbay/dsp/fx_xenyx_preamp.h \
    dsp/mc1/patchbay/dsp/fx_sonic_enhancer.h \
    dsp/mc1/patchbay/dsp/fx_tube_preamp.h \
    dsp/mc1/patchbay/dsp/fx_castertube.h \
    dsp/mc1/patchbay/dsp/fx_mic_modeler.h \
    dsp/mc1/patchbay/dsp/fx_lexicon_224.h \
    dsp/mc1/patchbay/dsp/fx_lexicon_pcm70.h \
    dsp/mc1/patchbay/dsp/fx_lexicon_pcm96.h \
    dsp/mc1/patchbay/dsp/fx_lexicon_480l.h \
    dsp/mc1/patchbay/dsp/fx_lexicon_mpx1.h \
    dsp/mc1/patchbay/dsp/fx_mc1_voice_lift.h \
    dsp/mc1/patchbay/dsp/fx_mc1_plosive_killer.h \
    dsp/mc1/patchbay/dsp/fx_mc1_mouth_click.h \
    dsp/mc1/patchbay/dsp/fx_mc1_bleed_suppressor.h \
    dsp/mc1/patchbay/dsp/fx_mc1_phone_line.h \
    dsp/mc1/patchbay/dsp/fx_mc1_remote_restorer.h \
    dsp/mc1/patchbay/dsp/fx_mc1_loudness_match.h \
    dsp/mc1/patchbay/dsp/fx_mc1_stinger_bed.h \
    dsp/mc1/patchbay/dsp/fx_mc1_vodcast_lipsync.h \
    dsp/mc1/patchbay/dsp/fx_mc1_signal_hill_a.h \
    dsp/mc1/patchbay/dsp/fx_mc1_tidemark_a.h \
    dsp/mc1/patchbay/dsp/fx_mc1_tidemark_b.h \
    dsp/mc1/patchbay/dsp/fx_mc1_tidemark_vault.h \
    dsp/mc1/patchbay/dsp/fx_mc1_granite_a.h \
    dsp/mc1/patchbay/dsp/fx_mc1_vocal_producer.h \
    dsp/mc1/patchbay/dsp/fx_mc1_key_finder.h \
    dsp/mc1/patchbay/dsp/fx_dbx_676.h \
    dsp/mc1/patchbay/dsp/fx_dbx_580.h \
    dsp/mc1/patchbay/dsp/fx_dbx_266xs.h \
    dsp/mc1/patchbay/dsp/fx_dbx_560a.h \
    dsp/mc1/patchbay/dsp/fx_dbx_520.h \
    dsp/mc1/patchbay/dsp/fx_dbx_510.h \
    dsp/mc1/patchbay/dsp/fx_dbx_530.h \
    dsp/mc1/patchbay/dsp/fx_bbe_d82.h \
    dsp/mc1/patchbay/dsp/fx_bbe_h82.h \
    dsp/mc1/patchbay/dsp/fx_bbe_l82.h \
    dsp/mc1/patchbay/dsp/fx_bbe_mach3.h \
    dsp/mc1/fx_ui/RackKnob.h \
    dsp/mc1/fx_ui/RackMeter.h \
    dsp/mc1/fx_ui/EqCurveWidget.h \
    dsp/mc1/fx_ui/ParametricEqDialog.h \
    dsp/mc1/fx_ui/DualEq15Dialog.h \
    dsp/mc1/fx_ui/GraphicEq31Dialog.h \
    dsp/mc1/fx_ui/CompressorDialog.h \
    dsp/mc1/fx_ui/Dbx166xsDialog.h \
    dsp/mc1/fx_ui/BroadcastAgcDialog.h \
    dsp/mc1/fx_ui/DbxVoiceDialog.h \
    dsp/mc1/fx_ui/XenyxPreampDialog.h \
    dsp/mc1/fx_ui/SonicEnhancerDialog.h \
    dsp/mc1/fx_ui/TubePreampDialog.h \
    dsp/mc1/fx_ui/CasterTubeDialog.h \
    dsp/mc1/fx_ui/MicModelerDialog.h \
    dsp/mc1/fx_ui/Lexicon224Dialog.h \
    dsp/mc1/fx_ui/LexiconPcm70Dialog.h \
    dsp/mc1/fx_ui/LexiconPcm96Dialog.h \
    dsp/mc1/fx_ui/Lexicon480LDialog.h \
    dsp/mc1/fx_ui/LexiconMpx1Dialog.h \
    dsp/mc1/fx_ui/PodcastPluginDialog.h \
    dsp/mc1/fx_ui/SignalHillStudioDialog.h \
    dsp/mc1/fx_ui/MusicStudioDialogs.h \
    dsp/mc1/fx_ui/BBESonicSweetDialogs.h \
    dsp/mc1/fx_ui/DbxDialogs.h \
    dsp/mc1/fx_ui/FlagshipDialogs.h \
    vfx/VideoEffectChain.h \
    vfx/VideoEffectChain.cpp \
    vfx/CrossDissolve.h \
    vfx/CrossDissolve.cpp \
    vfx/DipToBlack.h \
    vfx/DipToBlack.cpp \
    vfx/DipToWhite.h \
    vfx/DipToWhite.cpp \
    vfx/WipeLeft.h \
    vfx/WipeLeft.cpp \
    vfx/PushSlide.h \
    vfx/PushSlide.cpp \
    vfx/ZoomThrough.h \
    vfx/ZoomThrough.cpp \
    vfx/LumaKey.h \
    vfx/LumaKey.cpp \
    vfx/ChromaKey.h \
    vfx/ChromaKey.cpp \
    vfx/GlitchCut.h \
    vfx/GlitchCut.cpp \
    vfx/StingerOverlay.h \
    vfx/StingerOverlay.cpp \
    podcast/ChapterEditor.h \
    podcast/ChapterEditor.cpp \
    podcast/MetadataEditor.h \
    podcast/MetadataEditor.cpp \
    podcast/RSSGenerator.h \
    podcast/RSSGenerator.cpp \
    podcast/PodcastExporter.h \
    podcast/PodcastExporter.cpp \
    podcast/ShowNotesEditor.h \
    podcast/ShowNotesEditor.cpp \
    broadcast/LiveRecorder.h \
    broadcast/LiveRecorder.cpp \
    broadcast/StreamEncoder.h \
    broadcast/StreamEncoder.cpp \
    broadcast/BroadcastClock.h \
    broadcast/BroadcastClock.cpp \
    broadcast/TallyLight.h \
    broadcast/TallyLight.cpp \
    broadcast/RTMPStreamer.h \
    broadcast/RTMPStreamer.cpp \
    codec/CodecRegistry.h \
    codec/CodecRegistry.cpp \
    codec/WavCodec.h \
    codec/WavCodec.cpp \
    codec/FlacCodec.h \
    codec/FlacCodec.cpp \
    codec/Mp3Codec.h \
    codec/Mp3Codec.cpp \
    codec/AacCodec.h \
    codec/AacCodec.cpp \
    codec/OpusCodec.h \
    codec/OpusCodec.cpp \
    codec/VorbisCodec.h \
    codec/VorbisCodec.cpp \
    codec/FFmpegCodec.h \
    codec/FFmpegCodec.cpp \
    codec/TagTransfer.h \
    codec/TagTransfer.cpp \
    graphics/OverlayEngine.h \
    graphics/OverlayEngine.cpp \
    graphics/LowerThird.h \
    graphics/LowerThird.cpp \
    graphics/TickerCrawl.h \
    graphics/TickerCrawl.cpp \
    graphics/Callout.h \
    graphics/Callout.cpp \
    graphics/Watermark.h \
    graphics/Watermark.cpp \
    graphics/GraphicsRenderer.h \
    graphics/GraphicsRenderer.cpp \
    plugins/PluginScanner.h \
    plugins/PluginScanner.cpp \
    plugins/PluginHost.h \
    plugins/PluginHost.cpp \
    plugins/Vst3Host.h \
    plugins/Vst3Host.cpp \
    plugins/Vst3EffectAdapter.h \
    plugins/AuHost.h \
    plugins/AuEffectAdapter.h \
    widgets/MainWindow.h \
    widgets/MainWindow.cpp \
    widgets/TimelineWidget.h \
    widgets/TimelineWidget.cpp \
    widgets/TimelineScrollBar.h \
    widgets/TimelineScrollBar.cpp \
    widgets/TrackHeaderWidget.h \
    widgets/TrackHeaderWidget.cpp \
    widgets/TrackHeaderPanel.h \
    widgets/TrackHeaderPanel.cpp \
    widgets/WaveformView.h \
    widgets/WaveformView.cpp \
    widgets/VideoPreview.h \
    widgets/VideoPreview.cpp \
    widgets/MixerWidget.h \
    widgets/MixerWidget.cpp \
    widgets/VUMeterWidget.h \
    widgets/VUMeterWidget.cpp \
    widgets/TransportBar.h \
    widgets/TransportBar.cpp \
    widgets/EffectsRackWidget.h \
    widgets/EffectsRackWidget.cpp \
    widgets/MediaBrowser.h \
    widgets/MediaBrowser.cpp \
    widgets/MediaLibraryWidget.h \
    widgets/MediaLibraryWidget.cpp \
    widgets/LibraryTableModel.h \
    widgets/LibraryTableModel.cpp \
    widgets/ChapterWidget.h \
    widgets/ChapterWidget.cpp \
    widgets/MetadataPanel.h \
    widgets/MetadataPanel.cpp \
    widgets/ExportDialog.h \
    widgets/ExportDialog.cpp \
    widgets/BatchEncoderDialog.h \
    widgets/BatchEncoderDialog.cpp \
    widgets/PreferencesDialog.h \
    widgets/PreferencesDialog.cpp \
    widgets/ProjectSettingsDialog.h \
    widgets/ProjectSettingsDialog.cpp \
    widgets/SpectrumWidget.h \
    widgets/SpectrumWidget.cpp \
    widgets/BevelButton.h \
    widgets/BevelButton.cpp \
    widgets/EmbossedKnob.h \
    widgets/EmbossedKnob.cpp \
    widgets/ThemeEngine.h \
    widgets/ThemeEngine.cpp \
    widgets/AboutDialog.h \
    widgets/AboutDialog.cpp \
    widgets/PianoRollWidget.h \
    widgets/PianoRollWidget.cpp \
    widgets/ParametricEQDialog.h \
    widgets/ParametricEQDialog.cpp \
    widgets/LUFSMeterWidget.h \
    widgets/LUFSMeterWidget.cpp \
    widgets/SplashScreen.h \
    widgets/SplashScreen.cpp \
    widgets/TimeStretchDialog.h \
    widgets/TimeStretchDialog.cpp \
    widgets/CrossfadeEditorDialog.h \
    widgets/CrossfadeEditorDialog.cpp \
    widgets/MassTagEditor.h \
    widgets/MassTagEditor.cpp \
    widgets/TagTableModel.h \
    widgets/TagTableModel.cpp \
    widgets/FilenameToTagDialog.h \
    widgets/FilenameToTagDialog.cpp \
    widgets/TagToFilenameDialog.h \
    widgets/TagToFilenameDialog.cpp \
    widgets/StreamingDialog.h \
    widgets/StreamingDialog.cpp \
    widgets/ActionBar.h \
    widgets/ActionBar.cpp \
    widgets/MasterStrip.h \
    widgets/MasterStrip.cpp \
    widgets/SidebarNav.h \
    widgets/SidebarNav.cpp \
    widgets/ViewModeSwitcher.h \
    widgets/ViewModeSwitcher.cpp \
    widgets/ScriptReaderPanel.h \
    widgets/ScriptReaderPanel.cpp \
    widgets/PedalboardWidget.h \
    widgets/PedalboardWidget.cpp \
    widgets/StreamMonitorPanel.h \
    widgets/StreamMonitorPanel.cpp \
    widgets/TrackColorDialog.h \
    widgets/TrackColorDialog.cpp \
    widgets/MarkerListWidget.h \
    widgets/MarkerListWidget.cpp \
    widgets/ImportAudioDialog.h \
    widgets/ImportAudioDialog.cpp \
    platform/compat.h \
    platform/worker_pool.h \
    platform/worker_pool.cpp \
    config/AppConfig.h \
    config/AppConfig.cpp \
    config/YamlPresets.h \
    config/YamlPresets.cpp \
    config/DebugLogger.h \
    config/DebugLogger.cpp \
    ai/AIEngine.h \
    ai/AIEngine.cpp \
    ai/TranscriptionEngine.h \
    ai/TranscriptionEngine.cpp \
    ai/AIAssistant.h \
    ai/AIAssistant.cpp \
    ai/AIPanel.h \
    ai/AIPanel.cpp

# ── Conditional macOS platform sources ──────────────────────────────────────
if ENABLE_MACOS_GUI
mcaster1dawcast_SOURCES += \
    platform/macos_audio.h \
    platform/macos_audio.mm \
    platform/macos_video.h \
    platform/macos_video.mm \
    platform/device_discovery.h \
    platform/device_discovery.mm \
    plugins/PluginScannerAU.mm \
    plugins/AuHost.mm \
    plugins/Vst3HostMac.mm \
    $(VST3SDK)/public.sdk/source/vst/hosting/module.cpp \
    $(VST3SDK)/public.sdk/source/vst/hosting/module_mac.mm \
    $(VST3SDK)/public.sdk/source/vst/hosting/hostclasses.cpp \
    $(VST3SDK)/public.sdk/source/vst/hosting/parameterchanges.cpp \
    $(VST3SDK)/public.sdk/source/vst/hosting/pluginterfacesupport.cpp \
    $(VST3SDK)/public.sdk/source/vst/vstinitiids.cpp \
    $(VST3SDK)/public.sdk/source/common/memorystream.cpp \
    $(VST3SDK)/public.sdk/source/common/commoniids.cpp \
    $(VST3SDK)/public.sdk/source/common/commonstringconvert.cpp \
    $(VST3SDK)/public.sdk/source/vst/utility/stringconvert.cpp \
    $(VST3SDK)/base/source/baseiids.cpp \
    $(VST3SDK)/base/source/fobject.cpp \
    $(VST3SDK)/base/source/fbuffer.cpp \
    $(VST3SDK)/base/source/fstring.cpp \
    $(VST3SDK)/base/source/fdebug.cpp \
    $(VST3SDK)/base/source/updatehandler.cpp \
    $(VST3SDK)/base/thread/source/flock.cpp \
    $(VST3SDK)/pluginterfaces/base/coreiids.cpp \
    $(VST3SDK)/pluginterfaces/base/conststringtable.cpp \
    $(VST3SDK)/pluginterfaces/base/funknown.cpp \
    $(VST3SDK)/pluginterfaces/base/ustring.cpp
endif

# ── MOC-generated sources ────────────────────────────────────────────────────
nodist_mcaster1dawcast_SOURCES = \
    moc_app.cpp \
    moc_ModuleRegistry.cpp \
    moc_UndoManager.cpp \
    moc_ProjectManager.cpp \
    moc_MediaLibrary.cpp \
    moc_AudioEngine.cpp \
    moc_AudioMixer.cpp \
    moc_AudioWorker.cpp \
    moc_WaveformCache.cpp \
    moc_PlaybackEngine.cpp \
    moc_Metronome.cpp \
    moc_AudioBus.cpp \
    moc_BusRouter.cpp \
    moc_MultitrackRecorder.cpp \
    moc_TrackBouncer.cpp \
    moc_ExportEngine.cpp \
    moc_BatchEncoder.cpp \
    moc_VideoDecoder.cpp \
    moc_VideoEncoder.cpp \
    moc_VideoMixer.cpp \
    moc_VideoRenderer.cpp \
    moc_SubtitleRenderer.cpp \
    moc_MuxerDemuxer.cpp \
    moc_VideoPlaybackController.cpp \
    moc_Timeline.cpp \
    moc_AudioTrack.cpp \
    moc_VideoTrack.cpp \
    moc_Clip.cpp \
    moc_Automation.cpp \
    moc_MidiClip.cpp \
    moc_MidiTrack.cpp \
    moc_TrackGroup.cpp \
    moc_TempoMap.cpp \
    moc_PluginScanner.cpp \
    moc_PluginHost.cpp \
    moc_DspChain.cpp \
    moc_VideoEffectChain.cpp \
    moc_ChapterEditor.cpp \
    moc_MetadataEditor.cpp \
    moc_RSSGenerator.cpp \
    moc_PodcastExporter.cpp \
    moc_ShowNotesEditor.cpp \
    moc_LiveRecorder.cpp \
    moc_StreamEncoder.cpp \
    moc_BroadcastClock.cpp \
    moc_TallyLight.cpp \
    moc_RTMPStreamer.cpp \
    moc_CodecRegistry.cpp \
    moc_OverlayEngine.cpp \
    moc_LowerThird.cpp \
    moc_TickerCrawl.cpp \
    moc_Callout.cpp \
    moc_Watermark.cpp \
    moc_GraphicsRenderer.cpp \
    moc_MainWindow.cpp \
    moc_TimelineWidget.cpp \
    moc_TimelineScrollBar.cpp \
    moc_TrackHeaderWidget.cpp \
    moc_TrackHeaderPanel.cpp \
    moc_WaveformView.cpp \
    moc_VideoPreview.cpp \
    moc_MixerWidget.cpp \
    moc_VUMeterWidget.cpp \
    moc_TransportBar.cpp \
    moc_EffectsRackWidget.cpp \
    moc_MediaBrowser.cpp \
    moc_MediaLibraryWidget.cpp \
    moc_LibraryTableModel.cpp \
    moc_ChapterWidget.cpp \
    moc_MetadataPanel.cpp \
    moc_ExportDialog.cpp \
    moc_BatchEncoderDialog.cpp \
    moc_PreferencesDialog.cpp \
    moc_ProjectSettingsDialog.cpp \
    moc_SpectrumWidget.cpp \
    moc_BevelButton.cpp \
    moc_EmbossedKnob.cpp \
    moc_ThemeEngine.cpp \
    moc_AboutDialog.cpp \
    moc_PianoRollWidget.cpp \
    moc_ParametricEQDialog.cpp \
    moc_LUFSMeterWidget.cpp \
    moc_SplashScreen.cpp \
    moc_TimeStretchDialog.cpp \
    moc_CrossfadeEditorDialog.cpp \
    moc_MassTagEditor.cpp \
    moc_TagTableModel.cpp \
    moc_FilenameToTagDialog.cpp \
    moc_TagToFilenameDialog.cpp \
    moc_StreamingDialog.cpp \
    moc_ActionBar.cpp \
    moc_MasterStrip.cpp \
    moc_SidebarNav.cpp \
    moc_ViewModeManager.cpp \
    moc_WorkspaceManager.cpp \
    moc_ViewModeSwitcher.cpp \
    moc_ScriptReaderPanel.cpp \
    moc_PedalboardWidget.cpp \
    moc_StreamMonitorPanel.cpp \
    moc_TrackColorDialog.cpp \
    moc_MarkerListWidget.cpp \
    moc_ImportAudioDialog.cpp \
    moc_AppConfig.cpp \
    moc_AIEngine.cpp \
    moc_TranscriptionEngine.cpp \
    moc_AIAssistant.cpp \
    moc_AIPanel.cpp \
    moc_mc1_ParametricEqDialog.cpp \
    moc_mc1_DualEq15Dialog.cpp \
    moc_mc1_GraphicEq31Dialog.cpp \
    moc_mc1_CompressorDialog.cpp \
    moc_mc1_Dbx166xsDialog.cpp \
    moc_mc1_BroadcastAgcDialog.cpp \
    moc_mc1_DbxVoiceDialog.cpp \
    moc_mc1_XenyxPreampDialog.cpp \
    moc_mc1_SonicEnhancerDialog.cpp \
    moc_mc1_TubePreampDialog.cpp \
    moc_mc1_CasterTubeDialog.cpp \
    moc_mc1_MicModelerDialog.cpp \
    moc_mc1_Lexicon224Dialog.cpp \
    moc_mc1_LexiconPcm70Dialog.cpp \
    moc_mc1_LexiconPcm96Dialog.cpp \
    moc_mc1_Lexicon480LDialog.cpp \
    moc_mc1_LexiconMpx1Dialog.cpp \
    moc_mc1_PodcastPluginDialog.cpp \
    moc_mc1_SignalHillStudioDialog.cpp \
    moc_mc1_MusicStudioDialogs.cpp \
    moc_mc1_BBESonicSweetDialogs.cpp \
    moc_mc1_DbxDialogs.cpp \
    moc_mc1_FlagshipDialogs.cpp \
    moc_mc1_RackKnob.cpp \
    moc_mc1_RackMeter.cpp \
    moc_mc1_EqCurveWidget.cpp \
    qrc_mcaster1dawcast.cpp

# ── Build ordering ───────────────────────────────────────────────────────────
BUILT_SOURCES = \
    moc_app.cpp \
    moc_ModuleRegistry.cpp \
    moc_UndoManager.cpp \
    moc_ProjectManager.cpp \
    moc_MediaLibrary.cpp \
    moc_AudioEngine.cpp \
    moc_AudioMixer.cpp \
    moc_AudioWorker.cpp \
    moc_WaveformCache.cpp \
    moc_PlaybackEngine.cpp \
    moc_Metronome.cpp \
    moc_AudioBus.cpp \
    moc_BusRouter.cpp \
    moc_MultitrackRecorder.cpp \
    moc_TrackBouncer.cpp \
    moc_ExportEngine.cpp \
    moc_BatchEncoder.cpp \
    moc_VideoDecoder.cpp \
    moc_VideoEncoder.cpp \
    moc_VideoMixer.cpp \
    moc_VideoRenderer.cpp \
    moc_SubtitleRenderer.cpp \
    moc_MuxerDemuxer.cpp \
    moc_VideoPlaybackController.cpp \
    moc_Timeline.cpp \
    moc_AudioTrack.cpp \
    moc_VideoTrack.cpp \
    moc_Clip.cpp \
    moc_Automation.cpp \
    moc_MidiClip.cpp \
    moc_MidiTrack.cpp \
    moc_TrackGroup.cpp \
    moc_TempoMap.cpp \
    moc_PluginScanner.cpp \
    moc_PluginHost.cpp \
    moc_DspChain.cpp \
    moc_VideoEffectChain.cpp \
    moc_ChapterEditor.cpp \
    moc_MetadataEditor.cpp \
    moc_RSSGenerator.cpp \
    moc_PodcastExporter.cpp \
    moc_ShowNotesEditor.cpp \
    moc_LiveRecorder.cpp \
    moc_StreamEncoder.cpp \
    moc_BroadcastClock.cpp \
    moc_TallyLight.cpp \
    moc_RTMPStreamer.cpp \
    moc_CodecRegistry.cpp \
    moc_OverlayEngine.cpp \
    moc_LowerThird.cpp \
    moc_TickerCrawl.cpp \
    moc_Callout.cpp \
    moc_Watermark.cpp \
    moc_GraphicsRenderer.cpp \
    moc_MainWindow.cpp \
    moc_TimelineWidget.cpp \
    moc_TimelineScrollBar.cpp \
    moc_TrackHeaderWidget.cpp \
    moc_TrackHeaderPanel.cpp \
    moc_WaveformView.cpp \
    moc_VideoPreview.cpp \
    moc_MixerWidget.cpp \
    moc_VUMeterWidget.cpp \
    moc_TransportBar.cpp \
    moc_EffectsRackWidget.cpp \
    moc_MediaBrowser.cpp \
    moc_MediaLibraryWidget.cpp \
    moc_LibraryTableModel.cpp \
    moc_ChapterWidget.cpp \
    moc_MetadataPanel.cpp \
    moc_ExportDialog.cpp \
    moc_BatchEncoderDialog.cpp \
    moc_PreferencesDialog.cpp \
    moc_ProjectSettingsDialog.cpp \
    moc_SpectrumWidget.cpp \
    moc_BevelButton.cpp \
    moc_EmbossedKnob.cpp \
    moc_ThemeEngine.cpp \
    moc_AboutDialog.cpp \
    moc_PianoRollWidget.cpp \
    moc_ParametricEQDialog.cpp \
    moc_LUFSMeterWidget.cpp \
    moc_SplashScreen.cpp \
    moc_TimeStretchDialog.cpp \
    moc_CrossfadeEditorDialog.cpp \
    moc_MassTagEditor.cpp \
    moc_TagTableModel.cpp \
    moc_FilenameToTagDialog.cpp \
    moc_TagToFilenameDialog.cpp \
    moc_StreamingDialog.cpp \
    moc_ActionBar.cpp \
    moc_MasterStrip.cpp \
    moc_SidebarNav.cpp \
    moc_ViewModeManager.cpp \
    moc_WorkspaceManager.cpp \
    moc_ViewModeSwitcher.cpp \
    moc_ScriptReaderPanel.cpp \
    moc_PedalboardWidget.cpp \
    moc_StreamMonitorPanel.cpp \
    moc_TrackColorDialog.cpp \
    moc_MarkerListWidget.cpp \
    moc_ImportAudioDialog.cpp \
    moc_AppConfig.cpp \
    moc_AIEngine.cpp \
    moc_TranscriptionEngine.cpp \
    moc_AIAssistant.cpp \
    moc_AIPanel.cpp \
    moc_mc1_ParametricEqDialog.cpp \
    moc_mc1_DualEq15Dialog.cpp \
    moc_mc1_GraphicEq31Dialog.cpp \
    moc_mc1_CompressorDialog.cpp \
    moc_mc1_Dbx166xsDialog.cpp \
    moc_mc1_BroadcastAgcDialog.cpp \
    moc_mc1_DbxVoiceDialog.cpp \
    moc_mc1_XenyxPreampDialog.cpp \
    moc_mc1_SonicEnhancerDialog.cpp \
    moc_mc1_TubePreampDialog.cpp \
    moc_mc1_CasterTubeDialog.cpp \
    moc_mc1_MicModelerDialog.cpp \
    moc_mc1_Lexicon224Dialog.cpp \
    moc_mc1_LexiconPcm70Dialog.cpp \
    moc_mc1_LexiconPcm96Dialog.cpp \
    moc_mc1_Lexicon480LDialog.cpp \
    moc_mc1_LexiconMpx1Dialog.cpp \
    moc_mc1_PodcastPluginDialog.cpp \
    moc_mc1_SignalHillStudioDialog.cpp \
    moc_mc1_MusicStudioDialogs.cpp \
    moc_mc1_BBESonicSweetDialogs.cpp \
    moc_mc1_DbxDialogs.cpp \
    moc_mc1_FlagshipDialogs.cpp \
    moc_mc1_RackKnob.cpp \
    moc_mc1_RackMeter.cpp \
    moc_mc1_EqCurveWidget.cpp \
    qrc_mcaster1dawcast.cpp

# ── Clean generated files ────────────────────────────────────────────────────
CLEANFILES = moc_*.cpp qrc_*.cpp

# ── macOS Packaging Targets ──────────────────────────────────────────────────
SIGN_ID ?=
APP_SIGN_ID ?= $(SIGN_ID)
PKG_SIGN_ID ?= $(SIGN_ID)
NOTARIZE_PROFILE ?= Mcaster1
APP_BUNDLE = $(abs_top_builddir)/build/Mcaster1DAWCast.app
VERSION_STR = $(shell cat $(abs_top_srcdir)/VERSION.txt 2>/dev/null || echo "0.1.0")
DMG_PATH = $(abs_top_builddir)/build/Mcaster1DAWCast-$(VERSION_STR)-macOS.dmg
PKG_PATH = $(abs_top_builddir)/build/Mcaster1DAWCast-$(VERSION_STR)-macOS.pkg

app-bundle: mcaster1dawcast
	@if test -n "$(APP_SIGN_ID)"; then \
	    $(abs_top_srcdir)/scripts/make-app-bundle.sh --sign "$(APP_SIGN_ID)"; \
	else \
	    $(abs_top_srcdir)/scripts/make-app-bundle.sh; \
	fi

dmg: app-bundle
	$(abs_top_srcdir)/scripts/make-dmg.sh

pkg: app-bundle
	@if test -n "$(PKG_SIGN_ID)"; then \
	    $(abs_top_srcdir)/scripts/make-pkg.sh --sign "$(PKG_SIGN_ID)"; \
	else \
	    $(abs_top_srcdir)/scripts/make-pkg.sh; \
	fi

codesign:
	codesign --force --deep --sign "$(APP_SIGN_ID)" --timestamp --options runtime "$(APP_BUNDLE)"
	codesign --verify --deep --strict "$(APP_BUNDLE)"

notarize:
	@echo "── Notarizing PKG..."
	xcrun notarytool submit "$(PKG_PATH)" --keychain-profile "$(NOTARIZE_PROFILE)" --wait
	@echo "── Notarizing DMG..."
	xcrun notarytool submit "$(DMG_PATH)" --keychain-profile "$(NOTARIZE_PROFILE)" --wait

staple:
	@echo "── Stapling PKG..."
	xcrun stapler staple "$(PKG_PATH)"
	@echo "── Stapling DMG..."
	xcrun stapler staple "$(DMG_PATH)"
	@echo "── Done. Installers ready for distribution."

release: mcaster1dawcast
	$(abs_top_srcdir)/scripts/make-app-bundle.sh --sign "$(APP_SIGN_ID)"
	$(abs_top_srcdir)/scripts/make-pkg.sh --sign "$(PKG_SIGN_ID)"
	$(abs_top_srcdir)/scripts/make-dmg.sh
	@echo "── Submitting PKG for notarization..."
	xcrun notarytool submit "$(PKG_PATH)" --keychain-profile "$(NOTARIZE_PROFILE)" --wait
	@echo "── Submitting DMG for notarization..."
	xcrun notarytool submit "$(DMG_PATH)" --keychain-profile "$(NOTARIZE_PROFILE)" --wait
	@echo "── Stapling tickets..."
	xcrun stapler staple "$(PKG_PATH)"
	xcrun stapler staple "$(DMG_PATH)"
	@echo "── Release build complete. Signed + notarized installers:"
	@echo "  PKG: $(PKG_PATH)"
	@echo "  DMG: $(DMG_PATH)"

.PHONY: app-bundle codesign dmg pkg notarize staple release
