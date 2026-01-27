// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "MainWindow.h"
#include "TimelineWidget.h"
#include "MixerWidget.h"
#include "VideoPreview.h"
#include "MediaBrowser.h"
#include "EffectsRackWidget.h"
#include "TransportBar.h"
#include "LUFSMeterWidget.h"
#include "PreferencesDialog.h"
#include "../ai/AIPanel.h"

#include "../timeline/Timeline.h"
#include "../timeline/AudioTrack.h"
#include "../audio_engine/AudioEngine.h"
#include "../audio_engine/AudioMixer.h"
#include "../audio_engine/PlaybackEngine.h"
#include "../audio_engine/MultitrackRecorder.h"
#include "../audio_engine/Metronome.h"
#include "../audio_engine/WaveformCache.h"
#include "../audio_engine/ExportEngine.h"
#include "../video_engine/VideoPlaybackController.h"
#include "../config/AppConfig.h"
#include "ExportDialog.h"
#include "BatchEncoderDialog.h"
#include "MassTagEditor.h"
#include "AboutDialog.h"
#include "StreamingDialog.h"
#include "../broadcast/RTMPStreamer.h"

#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QInputDialog>
#include <QStatusBar>
#include <QSplitter>
#include <QApplication>
#include <QKeySequence>
#include <QDesktopServices>
#include <QUrl>
#include <QStringList>
#include <QProgressDialog>
#include <QLineEdit>

namespace dawcast::widgets {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Mcaster1DAWCast \u2014 Untitled Project"));
    setMinimumSize(1024, 700);

    setupMenus();
    setupToolbars();
    setupCentralWidget();
    setupDockWidgets();
    setupStatusBar();
    setupAudioPipeline();
    setupVideoPlayback();
    setupRTMPStreamer();
    setupConnections();

    // Restore window geometry, dock layout, and splitter state from last session
    restoreWindowState();
}

MainWindow::~MainWindow() = default;

// ── Close Event (save window state) ────────────────────────────────────────

void MainWindow::closeEvent(QCloseEvent* event)
{
    saveWindowState();
    QMainWindow::closeEvent(event);
}

// ── Menu Bar ────────────────────────────────────────────────────────────────

void MainWindow::setupMenus()
{
    m_menuBar = menuBar();

    // ── File ────────────────────────────────────────────────────────────
    auto* fileMenu = m_menuBar->addMenu(tr("&File"));

    auto* actNew = fileMenu->addAction(tr("&New Project"));
    actNew->setShortcut(QKeySequence::New);
    connect(actNew, &QAction::triggered, this, &MainWindow::newProject);

    auto* actOpen = fileMenu->addAction(tr("&Open Project..."));
    actOpen->setShortcut(QKeySequence::Open);
    connect(actOpen, &QAction::triggered, this, qOverload<>(&MainWindow::openProject));

    auto* actClose = fileMenu->addAction(tr("&Close Project"));
    actClose->setShortcut(QKeySequence(tr("Ctrl+W")));
    connect(actClose, &QAction::triggered, this, [this] {
        if (m_playbackEngine && m_playbackEngine->isPlaying())
            m_playbackEngine->stop();
        if (m_timelineModel) {
            while (m_timelineModel->trackCount() > 0)
                m_timelineModel->removeTrack(0);
            m_timelineModel->setPlayhead(0);
        }
        m_projectPath.clear();
        m_projectName.clear();
        setWindowTitle(QStringLiteral("Mcaster1DAWCast"));
        if (m_timeline) m_timeline->update();
        statusBar()->showMessage(tr("Project closed"), 3000);
    });

    fileMenu->addSeparator();

    auto* actSave = fileMenu->addAction(tr("&Save Project"));
    actSave->setShortcut(QKeySequence::Save);
    connect(actSave, &QAction::triggered, this, &MainWindow::saveProject);

    auto* actSaveAs = fileMenu->addAction(tr("Save Project &As..."));
    actSaveAs->setShortcut(QKeySequence::SaveAs);
    connect(actSaveAs, &QAction::triggered, this, &MainWindow::saveProjectAs);

    fileMenu->addSeparator();

    // ── Recent Projects submenu ────────────────────────────────────────
    m_recentFilesMenu = fileMenu->addMenu(tr("Recent Projects"));
    for (int i = 0; i < kMaxRecentFiles; ++i) {
        auto* action = new QAction(this);
        action->setVisible(false);
        connect(action, &QAction::triggered, this, &MainWindow::openRecentFile);
        m_recentFileActions.append(action);
        m_recentFilesMenu->addAction(action);
    }
    m_recentFilesMenu->addSeparator();
    auto* actClearRecent = m_recentFilesMenu->addAction(tr("Clear Recent"));
    connect(actClearRecent, &QAction::triggered, this, &MainWindow::clearRecentFiles);
    updateRecentFilesMenu();

    fileMenu->addSeparator();

    auto* actExport = fileMenu->addAction(tr("&Export..."));
    actExport->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E));
    connect(actExport, &QAction::triggered, this, &MainWindow::exportProject);

    auto* actBatchEncode = fileMenu->addAction(tr("&Batch Encoder..."));
    actBatchEncode->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_B));
    connect(actBatchEncode, &QAction::triggered, this, &MainWindow::openBatchEncoder);

    fileMenu->addSeparator();

    auto* actQuit = fileMenu->addAction(tr("&Quit"));
    actQuit->setShortcut(QKeySequence::Quit);
    connect(actQuit, &QAction::triggered, qApp, &QApplication::quit);

    // ── Edit ────────────────────────────────────────────────────────────
    auto* editMenu = m_menuBar->addMenu(tr("&Edit"));

    auto* actUndo = editMenu->addAction(tr("&Undo"));
    actUndo->setShortcut(QKeySequence::Undo);
    connect(actUndo, &QAction::triggered, this, &MainWindow::undo);

    auto* actRedo = editMenu->addAction(tr("&Redo"));
    actRedo->setShortcut(QKeySequence::Redo);
    connect(actRedo, &QAction::triggered, this, &MainWindow::redo);

    editMenu->addSeparator();

    auto* actCut = editMenu->addAction(tr("Cu&t"));
    actCut->setShortcut(QKeySequence::Cut);
    connect(actCut, &QAction::triggered, this, &MainWindow::cut);

    auto* actCopy = editMenu->addAction(tr("&Copy"));
    actCopy->setShortcut(QKeySequence::Copy);
    connect(actCopy, &QAction::triggered, this, &MainWindow::copy);

    auto* actPaste = editMenu->addAction(tr("&Paste"));
    actPaste->setShortcut(QKeySequence::Paste);
    connect(actPaste, &QAction::triggered, this, &MainWindow::paste);

    auto* actDelete = editMenu->addAction(tr("&Delete"));
    actDelete->setShortcut(QKeySequence::Delete);
    connect(actDelete, &QAction::triggered, this, &MainWindow::deleteSelected);

    editMenu->addSeparator();

    auto* actPreferences = editMenu->addAction(tr("&Preferences..."));
    actPreferences->setShortcut(QKeySequence::Preferences);
    actPreferences->setMenuRole(QAction::PreferencesRole);  // macOS puts this in app menu
    connect(actPreferences, &QAction::triggered, this, [this] {
        auto* dlg = new PreferencesDialog(this);
        if (m_audioEngine)
            dlg->setAudioEngine(m_audioEngine);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->exec();
    });

    // ── View ────────────────────────────────────────────────────────────
    auto* viewMenu = m_menuBar->addMenu(tr("&View"));

    m_actToggleMixer = viewMenu->addAction(tr("&Mixer"));
    m_actToggleMixer->setCheckable(true);
    m_actToggleMixer->setChecked(true);
    connect(m_actToggleMixer, &QAction::triggered, this, &MainWindow::toggleMixer);

    m_actToggleVideo = viewMenu->addAction(tr("Video &Preview"));
    m_actToggleVideo->setCheckable(true);
    m_actToggleVideo->setChecked(true);
    connect(m_actToggleVideo, &QAction::triggered, this, &MainWindow::toggleVideoPreview);

    m_actToggleMediaBrowser = viewMenu->addAction(tr("Media &Browser"));
    m_actToggleMediaBrowser->setCheckable(true);
    m_actToggleMediaBrowser->setChecked(false); // initially hidden
    connect(m_actToggleMediaBrowser, &QAction::triggered, this, &MainWindow::toggleMediaBrowser);

    m_actToggleEffectsRack = viewMenu->addAction(tr("&Effects Rack"));
    m_actToggleEffectsRack->setCheckable(true);
    m_actToggleEffectsRack->setChecked(true);
    connect(m_actToggleEffectsRack, &QAction::triggered, this, &MainWindow::toggleEffectsRack);

    m_actToggleLUFS = viewMenu->addAction(tr("&LUFS Meter"));
    m_actToggleLUFS->setCheckable(true);
    m_actToggleLUFS->setChecked(true);
    connect(m_actToggleLUFS, &QAction::triggered, this, [this]() {
        if (m_lufsDock) m_lufsDock->setVisible(!m_lufsDock->isVisible());
    });

    m_actToggleAI = viewMenu->addAction(tr("&AI Assistant"));
    m_actToggleAI->setCheckable(true);
    m_actToggleAI->setChecked(true);
    connect(m_actToggleAI, &QAction::triggered, this, &MainWindow::toggleAIPanel);

    // ── Track ───────────────────────────────────────────────────────────
    auto* trackMenu = m_menuBar->addMenu(tr("&Track"));

    auto* actAddAudio = trackMenu->addAction(tr("Add &Audio Track"));
    actAddAudio->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A));
    connect(actAddAudio, &QAction::triggered, this, &MainWindow::addAudioTrack);

    auto* actAddVideo = trackMenu->addAction(tr("Add &Video Track"));
    actAddVideo->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V));
    connect(actAddVideo, &QAction::triggered, this, &MainWindow::addVideoTrack);

    auto* actAddMidi = trackMenu->addAction(tr("Add &MIDI Track"));
    actAddMidi->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_M));
    connect(actAddMidi, &QAction::triggered, this, &MainWindow::addMidiTrack);

    auto* actRemoveTrack = trackMenu->addAction(tr("&Remove Track"));
    connect(actRemoveTrack, &QAction::triggered, this, &MainWindow::removeTrack);

    // ── Podcast ─────────────────────────────────────────────────────────
    auto* podcastMenu = m_menuBar->addMenu(tr("&Podcast"));

    auto* actChapter = podcastMenu->addAction(tr("&Chapter Editor"));
    connect(actChapter, &QAction::triggered, this, &MainWindow::openChapterEditor);

    auto* actMetadata = podcastMenu->addAction(tr("&Metadata"));
    connect(actMetadata, &QAction::triggered, this, &MainWindow::openMetadata);

    auto* actExportEp = podcastMenu->addAction(tr("&Export Episode"));
    connect(actExportEp, &QAction::triggered, this, &MainWindow::exportEpisode);

    auto* actRSS = podcastMenu->addAction(tr("&Generate RSS"));
    connect(actRSS, &QAction::triggered, this, &MainWindow::generateRSS);

    // ── Broadcast ───────────────────────────────────────────────────────
    auto* broadcastMenu = m_menuBar->addMenu(tr("&Broadcast"));

    auto* actStartRec = broadcastMenu->addAction(tr("Start &Recording"));
    connect(actStartRec, &QAction::triggered, this, &MainWindow::startRecording);

    auto* actStopRec = broadcastMenu->addAction(tr("Stop R&ecording"));
    connect(actStopRec, &QAction::triggered, this, &MainWindow::stopRecording);

    broadcastMenu->addSeparator();

    auto* actStreamLive = broadcastMenu->addAction(tr("Stream &Live..."));
    actStreamLive->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));
    connect(actStreamLive, &QAction::triggered, this, &MainWindow::openStreamingDialog);

    broadcastMenu->addSeparator();

    auto* actStartStream = broadcastMenu->addAction(tr("Start &Streaming"));
    connect(actStartStream, &QAction::triggered, this, &MainWindow::startStreaming);

    auto* actStopStream = broadcastMenu->addAction(tr("S&top Streaming"));
    connect(actStopStream, &QAction::triggered, this, &MainWindow::stopStreaming);

    // ── Transport ──────────────────────────────────────────────────────
    auto* transportMenu = m_menuBar->addMenu(tr("T&ransport"));

    auto* actMetronome = transportMenu->addAction(tr("&Metronome"));
    actMetronome->setCheckable(true);
    actMetronome->setChecked(false);
    actMetronome->setShortcut(QKeySequence(Qt::Key_M));
    connect(actMetronome, &QAction::toggled, this, [this](bool on) {
        if (m_playbackEngine && m_playbackEngine->metronome()) {
            m_playbackEngine->metronome()->setEnabled(on);
        }
    });

    transportMenu->addSeparator();

    auto* actCountIn = transportMenu->addAction(tr("&Count-In (1 Bar)"));
    actCountIn->setCheckable(true);
    actCountIn->setChecked(false);
    connect(actCountIn, &QAction::toggled, this, [this](bool on) {
        if (m_playbackEngine && m_playbackEngine->metronome()) {
            m_playbackEngine->metronome()->setCountIn(on ? 1 : 0);
        }
    });

    // ── Tools ───────────────────────────────────────────────────────────
    auto* toolsMenu = m_menuBar->addMenu(tr("&Tools"));

    auto* actMassTag = toolsMenu->addAction(tr("Mass Tag &Editor..."));
    actMassTag->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T));
    connect(actMassTag, &QAction::triggered, this, &MainWindow::openMassTagEditor);

    // ── Help ────────────────────────────────────────────────────────────
    auto* helpMenu = m_menuBar->addMenu(tr("&Help"));

    auto* actAbout = helpMenu->addAction(tr("&About Mcaster1DAWCast"));
    connect(actAbout, &QAction::triggered, this, &MainWindow::showAbout);

    auto* actDocs = helpMenu->addAction(tr("&Documentation"));
    connect(actDocs, &QAction::triggered, this, &MainWindow::showDocumentation);
}

// ── Toolbar ─────────────────────────────────────────────────────────────────

void MainWindow::setupToolbars()
{
    m_toolBar = addToolBar(tr("Transport"));
    m_toolBar->setMovable(false);
    m_toolBar->setFloatable(false);

    m_transportBar = new TransportBar(this);
    m_toolBar->addWidget(m_transportBar);
}

// ── Central Widget ──────────────────────────────────────────────────────────

void MainWindow::setupCentralWidget()
{
    m_centralSplitter = new QSplitter(Qt::Vertical, this);
    m_centralSplitter->setChildrenCollapsible(false);

    m_timeline = new TimelineWidget(this);
    m_timeline->setMinimumHeight(200);

    m_centralSplitter->addWidget(m_timeline);
    m_centralSplitter->setStretchFactor(0, 1);

    setCentralWidget(m_centralSplitter);
}

// ── Dock Widgets ────────────────────────────────────────────────────────────

void MainWindow::setupDockWidgets()
{
    // Bottom dock — Mixer
    m_mixerDock = new QDockWidget(tr("Mixer"), this);
    m_mixerDock->setObjectName(QStringLiteral("MixerDock"));
    m_mixerDock->setMinimumHeight(120);
    m_mixer = new MixerWidget(m_mixerDock);
    m_mixerDock->setWidget(m_mixer);
    addDockWidget(Qt::BottomDockWidgetArea, m_mixerDock);

    // Right dock — Video Preview
    m_videoDock = new QDockWidget(tr("Video Preview"), this);
    m_videoDock->setObjectName(QStringLiteral("VideoPreviewDock"));
    m_videoDock->setMinimumSize(240, 180);
    m_videoPreview = new VideoPreview(m_videoDock);
    m_videoDock->setWidget(m_videoPreview);
    addDockWidget(Qt::RightDockWidgetArea, m_videoDock);

    // Left dock — Media Browser (initially hidden)
    m_mediaBrowserDock = new QDockWidget(tr("Media Browser"), this);
    m_mediaBrowserDock->setObjectName(QStringLiteral("MediaBrowserDock"));
    m_mediaBrowserDock->setMinimumWidth(200);
    m_mediaBrowser = new MediaBrowser(m_mediaBrowserDock);
    m_mediaBrowserDock->setWidget(m_mediaBrowser);
    addDockWidget(Qt::LeftDockWidgetArea, m_mediaBrowserDock);
    m_mediaBrowserDock->hide();

    // Right dock — Effects Rack (tabified with Video Preview)
    m_effectsDock = new QDockWidget(tr("Effects Rack"), this);
    m_effectsDock->setObjectName(QStringLiteral("EffectsRackDock"));
    m_effectsDock->setMinimumSize(240, 180);
    m_effectsRack = new EffectsRackWidget(m_effectsDock);
    m_effectsDock->setWidget(m_effectsRack);
    addDockWidget(Qt::RightDockWidgetArea, m_effectsDock);
    tabifyDockWidget(m_videoDock, m_effectsDock);

    // Right dock — LUFS Meter
    m_lufsDock = new QDockWidget(tr("LUFS Meter"), this);
    m_lufsDock->setObjectName(QStringLiteral("LUFSMeterDock"));
    m_lufsDock->setMinimumSize(80, 200);
    m_lufsMeter = new LUFSMeterWidget(m_lufsDock);
    m_lufsDock->setWidget(m_lufsMeter);
    addDockWidget(Qt::RightDockWidgetArea, m_lufsDock);

    // Right dock — AI Panel (tabified with Effects Rack)
    m_aiDock = new QDockWidget(tr("AI Assistant"), this);
    m_aiDock->setObjectName(QStringLiteral("AIDock"));
    m_aiDock->setMinimumSize(240, 180);
    m_aiPanel = new dawcast::ai::AIPanel(m_aiDock);
    m_aiDock->setWidget(m_aiPanel);
    addDockWidget(Qt::RightDockWidgetArea, m_aiDock);
    tabifyDockWidget(m_effectsDock, m_aiDock);

    // Raise Video Preview as the default visible tab on the right
    m_videoDock->raise();
}

// ── Status Bar ──────────────────────────────────────────────────────────────

void MainWindow::setupStatusBar()
{
    statusBar()->showMessage(tr("Ready"));
}

// ── Audio Pipeline ─────────────────────────────────────────────────────────

void MainWindow::setupAudioPipeline()
{
    // Create the data model timeline (distinct from the TimelineWidget)
    m_timelineModel = new dawcast::Timeline(this);
    m_timelineModel->setSampleRate(48000);

    // Give the TimelineWidget its data model
    m_timeline->setTimeline(m_timelineModel);

    // Audio mixer (volume / pan / mute / solo per strip)
    m_audioMixer = new dawcast::AudioMixer(this);

    // Audio engine (PortAudio I/O)
    m_audioEngine = new dawcast::AudioEngine(this);
    m_audioEngine->setSampleRate(48000);
    m_audioEngine->setBufferSize(512);
    m_audioEngine->setMixer(m_audioMixer);

    // Restore saved device selections from AppConfig
    auto* cfg = dawcast::config::AppConfig::instance();
    if (cfg) {
        int outDev = cfg->value(QStringLiteral("audio/outputDeviceIndex"), -1).toInt();
        int inDev  = cfg->value(QStringLiteral("audio/inputDeviceIndex"), -1).toInt();
        if (outDev >= 0) m_audioEngine->setOutputDevice(outDev);
        if (inDev >= 0)  m_audioEngine->setInputDevice(inDev);
    }

    // Multitrack recorder (records input to armed tracks)
    m_recorder = new dawcast::MultitrackRecorder(this);
    m_recorder->setTimeline(m_timelineModel);
    m_recorder->setAudioEngine(m_audioEngine);

    // Playback engine (reads timeline clips -> feeds mixer -> PortAudio)
    m_playbackEngine = new dawcast::PlaybackEngine(this);
    m_playbackEngine->setTimeline(m_timelineModel);
    m_playbackEngine->setAudioEngine(m_audioEngine);
    m_playbackEngine->setRecorder(m_recorder);

    // Tell the audio engine about the playback engine so the callback
    // calls processBlock() before mixer->process().
    m_audioEngine->setPlaybackEngine(m_playbackEngine);

    // Start the audio engine so PortAudio is ready when the user hits Play
    m_audioEngine->start();
}

// ── Video Playback ─────────────────────────────────────────────────────────

void MainWindow::setupVideoPlayback()
{
    m_videoPlaybackController = new dawcast::VideoPlaybackController(this);
    m_videoPlaybackController->setVideoPreview(m_videoPreview);
    m_videoPlaybackController->setTimeline(m_timelineModel);
}

// ── Signal/Slot Connections ─────────────────────────────────────────────────

void MainWindow::setupConnections()
{
    // Transport bar signals
    connect(m_transportBar, &TransportBar::playClicked, this, &MainWindow::onPlay);
    connect(m_transportBar, &TransportBar::stopClicked, this, &MainWindow::onStop);
    connect(m_transportBar, &TransportBar::recordClicked, this, &MainWindow::onRecord);
    connect(m_transportBar, &TransportBar::pauseClicked, this, [this]() {
        if (m_playbackEngine) m_playbackEngine->pause();
        if (m_videoPlaybackController) m_videoPlaybackController->stop();
        m_transportBar->setPlaying(false);
        statusBar()->showMessage(tr("Paused"), 2000);
    });
    connect(m_transportBar, &TransportBar::rewindClicked, this, [this]() {
        if (m_playbackEngine) {
            m_playbackEngine->seekTo(0);
            if (m_timelineModel) m_timelineModel->setPlayhead(0);
        }
    });

    // Metronome toggle and tempo from TransportBar -> PlaybackEngine::Metronome
    connect(m_transportBar, &TransportBar::metronomeToggled, this, [this](bool on) {
        if (m_playbackEngine && m_playbackEngine->metronome()) {
            m_playbackEngine->metronome()->setEnabled(on);
            statusBar()->showMessage(on ? tr("Metronome enabled")
                                        : tr("Metronome disabled"), 2000);
        }
    });
    connect(m_transportBar, &TransportBar::tempoChanged, this, [this](double bpm) {
        if (m_playbackEngine && m_playbackEngine->metronome()) {
            m_playbackEngine->metronome()->setTempo(bpm);
        }
    });

    // Beat indicator: Metronome::beat fires on the audio thread, so use
    // Qt::QueuedConnection to marshal to the GUI thread safely.
    if (m_playbackEngine && m_playbackEngine->metronome()) {
        connect(m_playbackEngine->metronome(), &dawcast::Metronome::beat,
                m_transportBar, &TransportBar::flashBeat,
                Qt::QueuedConnection);
    }

    // PlaybackEngine -> TransportBar time display and TimelineWidget playhead
    if (m_playbackEngine) {
        connect(m_playbackEngine, &dawcast::PlaybackEngine::positionChanged,
                this, [this](int64_t samples) {
            int sr = m_audioEngine ? m_audioEngine->sampleRate() : 48000;
            m_transportBar->setPosition(samples, sr);
            // TimelineWidget repaints automatically when Timeline::playheadChanged fires
        });
        connect(m_playbackEngine, &dawcast::PlaybackEngine::playbackStopped,
                this, [this]() {
            m_transportBar->setPlaying(false);
        });
        connect(m_playbackEngine, &dawcast::PlaybackEngine::playbackStarted,
                this, [this]() {
            m_transportBar->setPlaying(true);
        });
    }

    // TimelineWidget playhead clicks -> seek PlaybackEngine + VideoPlaybackController
    connect(m_timeline, &TimelineWidget::playheadMoved,
            this, [this](int64_t pos) {
        if (m_playbackEngine) {
            m_playbackEngine->seekTo(pos);
        }
        if (m_videoPlaybackController && m_timelineModel) {
            int sr = m_timelineModel->sampleRate();
            if (sr <= 0) sr = 48000;
            double timeSec = static_cast<double>(pos) / sr;
            m_videoPlaybackController->seekTo(timeSec);
        }
    });

    // Sync dock visibility with View menu toggle actions
    connect(m_mixerDock, &QDockWidget::visibilityChanged,
            m_actToggleMixer, &QAction::setChecked);
    connect(m_videoDock, &QDockWidget::visibilityChanged,
            m_actToggleVideo, &QAction::setChecked);
    connect(m_mediaBrowserDock, &QDockWidget::visibilityChanged,
            m_actToggleMediaBrowser, &QAction::setChecked);
    connect(m_effectsDock, &QDockWidget::visibilityChanged,
            m_actToggleEffectsRack, &QAction::setChecked);
    connect(m_lufsDock, &QDockWidget::visibilityChanged,
            m_actToggleLUFS, &QAction::setChecked);
    connect(m_aiDock, &QDockWidget::visibilityChanged,
            m_actToggleAI, &QAction::setChecked);

    // LUFS metering — feed from audio engine buffer-processed signal
    if (m_audioEngine && m_lufsMeter) {
        connect(m_audioEngine, &dawcast::AudioEngine::bufferProcessed,
                m_lufsMeter, [this]() {
            // In a full implementation, LUFS measurements would be computed
            // from the master output buffer in the audio callback and
            // posted to the GUI thread via atomic variables. For now, the
            // LUFSMeterWidget receives values via its public setters.
        });
    }

    // Reset LUFS meter when playback starts/stops
    if (m_playbackEngine && m_lufsMeter) {
        connect(m_playbackEngine, &dawcast::PlaybackEngine::playbackStarted,
                m_lufsMeter, &LUFSMeterWidget::reset);
    }

    // Pre-cache waveforms when files are imported or double-clicked in Media Browser
    connect(m_mediaBrowser, &MediaBrowser::fileDoubleClicked,
            this, [](const QString& path) {
        dawcast::WaveformCache::instance()->requestWaveform(path);
    });

    connect(m_mediaBrowser, &MediaBrowser::importRequested,
            this, [](const QStringList& paths) {
        for (const QString& path : paths) {
            dawcast::WaveformCache::instance()->requestWaveform(path);
        }
    });
}

// ── File Slots ──────────────────────────────────────────────────────────────

void MainWindow::newProject()
{
    // Prompt for project name
    bool ok = false;
    QString projectName = QInputDialog::getText(
        this, tr("New Project"),
        tr("Project name:"),
        QLineEdit::Normal,
        tr("Untitled Project"),
        &ok);

    if (!ok || projectName.trimmed().isEmpty())
        return;

    projectName = projectName.trimmed();

    // Stop playback if running
    if (m_playbackEngine && m_playbackEngine->isPlaying())
        m_playbackEngine->stop();

    // Clear existing timeline
    if (m_timelineModel) {
        while (m_timelineModel->trackCount() > 0)
            m_timelineModel->removeTrack(0);
        m_timelineModel->setPlayhead(0);
    }

    // Add a default audio track so the user has something to work with
    if (m_timelineModel) {
        dawcast::AudioTrack* track = m_timelineModel->addAudioTrack();
        if (track)
            track->setName(tr("Audio 1"));
    }

    m_projectPath.clear();
    m_projectName = projectName;
    setWindowTitle(QStringLiteral("Mcaster1DAWCast \u2014 ") + projectName);

    // Update UI
    if (m_timeline) m_timeline->update();
    if (m_transportBar) {
        m_transportBar->setPosition(0, m_timelineModel ? m_timelineModel->sampleRate() : 48000);
        m_transportBar->setDuration(0, m_timelineModel ? m_timelineModel->sampleRate() : 48000);
    }

    statusBar()->showMessage(tr("New project created — Audio 1 track ready"), 3000);
}

void MainWindow::openProject()
{
    auto* config = dawcast::config::AppConfig::instance();
    QString lastDir = config->value(QStringLiteral("file/lastOpenDir")).toString();

    QString path = QFileDialog::getOpenFileName(
        this, tr("Open Project"),
        lastDir,
        tr("DAWCast Projects (*.dawcast);;All Files (*)"));

    if (!path.isEmpty()) {
        openProject(path);
    }
}

void MainWindow::openProject(const QString& path)
{
    if (path.isEmpty()) return;

    m_projectPath = path;
    QFileInfo fi(path);
    setWindowTitle(QStringLiteral("Mcaster1DAWCast \u2014 %1").arg(fi.baseName()));
    statusBar()->showMessage(tr("Opened: %1").arg(fi.fileName()), 5000);

    addToRecentFiles(path);

    // Remember directory for next file dialog
    auto* config = dawcast::config::AppConfig::instance();
    config->setValue(QStringLiteral("file/lastOpenDir"), fi.absolutePath());
    config->save();
}

void MainWindow::saveProject()
{
    if (m_projectPath.isEmpty()) {
        saveProjectAs();
        return;
    }
    statusBar()->showMessage(tr("Project saved"), 3000);
}

void MainWindow::saveProjectAs()
{
    auto* config = dawcast::config::AppConfig::instance();
    QString lastDir = config->value(QStringLiteral("file/lastOpenDir")).toString();

    QString path = QFileDialog::getSaveFileName(
        this, tr("Save Project As"),
        lastDir,
        tr("DAWCast Projects (*.dawcast);;All Files (*)"));

    if (!path.isEmpty()) {
        m_projectPath = path;
        QFileInfo fi(path);
        setWindowTitle(QStringLiteral("Mcaster1DAWCast \u2014 %1").arg(fi.baseName()));
        statusBar()->showMessage(tr("Project saved as: %1").arg(fi.fileName()), 5000);

        addToRecentFiles(path);

        config->setValue(QStringLiteral("file/lastOpenDir"), fi.absolutePath());
        config->save();
    }
}

void MainWindow::exportProject()
{
    runExportPipeline();
}

void MainWindow::openBatchEncoder()
{
    BatchEncoderDialog dlg(this);
    dlg.exec();
}

// ── Edit Slots ──────────────────────────────────────────────────────────────

void MainWindow::undo()
{
    statusBar()->showMessage(tr("Undo"), 2000);
}

void MainWindow::redo()
{
    statusBar()->showMessage(tr("Redo"), 2000);
}

void MainWindow::cut()
{
    statusBar()->showMessage(tr("Cut"), 2000);
}

void MainWindow::copy()
{
    statusBar()->showMessage(tr("Copy"), 2000);
}

void MainWindow::paste()
{
    statusBar()->showMessage(tr("Paste"), 2000);
}

void MainWindow::deleteSelected()
{
    statusBar()->showMessage(tr("Delete"), 2000);
}

// ── View Slots ──────────────────────────────────────────────────────────────

void MainWindow::toggleMixer()
{
    m_mixerDock->setVisible(!m_mixerDock->isVisible());
}

void MainWindow::toggleVideoPreview()
{
    m_videoDock->setVisible(!m_videoDock->isVisible());
}

void MainWindow::toggleMediaBrowser()
{
    m_mediaBrowserDock->setVisible(!m_mediaBrowserDock->isVisible());
}

void MainWindow::toggleEffectsRack()
{
    m_effectsDock->setVisible(!m_effectsDock->isVisible());
}

void MainWindow::toggleAIPanel()
{
    m_aiDock->setVisible(!m_aiDock->isVisible());
}

// ── Track Slots ─────────────────────────────────────────────────────────────

void MainWindow::addAudioTrack()
{
    if (m_timelineModel) {
        dawcast::AudioTrack* track = m_timelineModel->addAudioTrack();
        Q_UNUSED(track)
        // TimelineWidget repaints via Timeline::trackAdded signal
        m_timeline->update();

        // Update transport bar with timeline duration
        int sr = m_audioEngine ? m_audioEngine->sampleRate() : 48000;
        m_transportBar->setDuration(m_timelineModel->duration(), sr);
    }
    statusBar()->showMessage(tr("Audio track added"), 3000);
}

void MainWindow::addVideoTrack()
{
    statusBar()->showMessage(tr("Video track added"), 3000);
}

void MainWindow::addMidiTrack()
{
    if (m_timelineModel) {
        m_timelineModel->addMidiTrack();
        m_timeline->update();
    }
    statusBar()->showMessage(tr("MIDI track added"), 3000);
}

void MainWindow::removeTrack()
{
    statusBar()->showMessage(tr("Track removed"), 3000);
}

// ── Podcast Slots ───────────────────────────────────────────────────────────

void MainWindow::openChapterEditor()
{
    statusBar()->showMessage(tr("Chapter Editor opened"), 3000);
}

void MainWindow::openMetadata()
{
    statusBar()->showMessage(tr("Metadata editor opened"), 3000);
}

void MainWindow::exportEpisode()
{
    statusBar()->showMessage(tr("Episode export not yet implemented"), 3000);
}

void MainWindow::generateRSS()
{
    statusBar()->showMessage(tr("RSS generation not yet implemented"), 3000);
}

// ── Broadcast Slots ─────────────────────────────────────────────────────────

void MainWindow::startRecording()
{
    statusBar()->showMessage(tr("Recording started"), 3000);
}

void MainWindow::stopRecording()
{
    statusBar()->showMessage(tr("Recording stopped"), 3000);
}

void MainWindow::startStreaming()
{
    if (m_rtmpStreamer && !m_rtmpStreamer->isStreaming()) {
        m_rtmpStreamer->startStreaming();
    }
    statusBar()->showMessage(tr("Streaming started"), 3000);
}

void MainWindow::stopStreaming()
{
    if (m_rtmpStreamer && m_rtmpStreamer->isStreaming()) {
        m_rtmpStreamer->stopStreaming();
    }
    statusBar()->showMessage(tr("Streaming stopped"), 3000);
}

void MainWindow::openStreamingDialog()
{
    if (!m_rtmpStreamer) {
        QMessageBox::warning(this, tr("Streaming"),
                             tr("RTMP streaming engine is not available.\n"
                                "Ensure FFmpeg (libavformat) support is enabled."));
        return;
    }

    if (!m_streamingDialog) {
        m_streamingDialog = new StreamingDialog(m_rtmpStreamer, this);
        m_streamingDialog->setAttribute(Qt::WA_DeleteOnClose);

        connect(m_streamingDialog, &StreamingDialog::streamingStarted,
                this, [this]() {
            updateOnAirStatus(true);
            statusBar()->showMessage(tr("LIVE -- Streaming"), 0);
        });

        connect(m_streamingDialog, &StreamingDialog::streamingStopped,
                this, [this]() {
            updateOnAirStatus(false);
            statusBar()->showMessage(tr("Stream ended"), 5000);
        });

        connect(m_streamingDialog, &QDialog::destroyed,
                this, [this]() {
            m_streamingDialog = nullptr;
        });
    }

    m_streamingDialog->show();
    m_streamingDialog->raise();
    m_streamingDialog->activateWindow();
}

// ── RTMP Streamer Setup ───────────────────────────────────────────────────

void MainWindow::setupRTMPStreamer()
{
    m_rtmpStreamer = new dawcast::RTMPStreamer(this);

    // Wire streamer to PlaybackEngine so the audio callback feeds it
    if (m_playbackEngine) {
        m_playbackEngine->setRTMPStreamer(m_rtmpStreamer);
    }

    // On-air status label for the status bar
    m_onAirStatusLabel = new QLabel(this);
    m_onAirStatusLabel->setStyleSheet(
        QStringLiteral("QLabel { color: #ff2020; font-weight: bold; "
                        "padding: 0 8px; }"));
    m_onAirStatusLabel->hide();
    statusBar()->addPermanentWidget(m_onAirStatusLabel);
}

void MainWindow::updateOnAirStatus(bool live)
{
    if (!m_onAirStatusLabel) return;

    if (live) {
        m_onAirStatusLabel->setText(tr("LIVE"));
        m_onAirStatusLabel->show();
    } else {
        m_onAirStatusLabel->hide();
    }
}

// ── Tools Slots ────────────────────────────────────────────────────────────

void MainWindow::openMassTagEditor()
{
    auto* editor = new MassTagEditor(this);
    editor->setAttribute(Qt::WA_DeleteOnClose);
    editor->show();
}

// ── Help Slots ──────────────────────────────────────────────────────────────

void MainWindow::showAbout()
{
    AboutDialog dlg(this);
    dlg.exec();
}

void MainWindow::showDocumentation()
{
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://mcaster1.com/dawcast/docs")));
}

// ── Transport Slots ─────────────────────────────────────────────────────────

void MainWindow::onPlay()
{
    if (m_playbackEngine) {
        m_playbackEngine->play();
    }
    if (m_videoPlaybackController) {
        // Sync video start time to the current playhead position
        if (m_timelineModel) {
            int sr = m_timelineModel->sampleRate();
            if (sr <= 0) sr = 48000;
            double timeSec = static_cast<double>(m_timelineModel->playhead()) / sr;
            m_videoPlaybackController->seekTo(timeSec);
        }
        m_videoPlaybackController->play();
    }
    statusBar()->showMessage(tr("Playing"), 2000);
}

void MainWindow::onStop()
{
    // If we are recording, stop the recorder first
    if (m_recorder && m_recorder->isRecording()) {
        m_recorder->stopRecording();
        m_transportBar->setRecording(false);
    }

    if (m_playbackEngine) {
        m_playbackEngine->stop();
    }
    if (m_videoPlaybackController) {
        m_videoPlaybackController->stop();
    }
    statusBar()->showMessage(tr("Stopped"), 2000);
}

void MainWindow::onRecord()
{
    if (!m_recorder) return;

    if (m_recorder->isRecording()) {
        // Stop recording
        m_recorder->stopRecording();
        m_transportBar->setRecording(false);
        statusBar()->showMessage(tr("Recording stopped"), 3000);
        return;
    }

    // Check if any tracks are armed
    if (!hasArmedTracks()) {
        QMessageBox::information(this, tr("No Armed Tracks"),
            tr("Arm one or more audio tracks to start recording.\n"
               "Click the record-arm button (R) on a track header."));
        return;
    }

    // Start playback simultaneously so the playhead advances
    if (m_playbackEngine && !m_playbackEngine->isPlaying()) {
        m_playbackEngine->play();
    }

    m_recorder->clearTargets(); // Let startRecording scan for armed tracks
    m_recorder->startRecording();
    m_transportBar->setRecording(true);
    statusBar()->showMessage(tr("Recording..."), 0);
}

bool MainWindow::hasArmedTracks() const
{
    if (!m_timelineModel) return false;

    int count = m_timelineModel->trackCount();
    for (int i = 0; i < count; ++i) {
        auto* audioTrack = qobject_cast<dawcast::AudioTrack*>(m_timelineModel->track(i));
        if (audioTrack && audioTrack->isRecordArmed()) {
            return true;
        }
    }
    return false;
}

void MainWindow::updateRecordButtonState()
{
    // Future enhancement: disable the record button and show a tooltip
    // when no tracks are armed. Connect to track arm-state changes.
}

// ── Recent Files ───────────────────────────────────────────────────────────

void MainWindow::updateRecentFilesMenu()
{
    auto* config = dawcast::config::AppConfig::instance();
    QStringList files = config->value(QStringLiteral("file/recentFiles")).toStringList();

    int numRecent = qMin(files.size(), kMaxRecentFiles);
    for (int i = 0; i < numRecent; ++i) {
        QFileInfo fi(files[i]);
        QString text = QStringLiteral("&%1  %2").arg(i + 1).arg(fi.fileName());
        m_recentFileActions[i]->setText(text);
        m_recentFileActions[i]->setData(files[i]);
        m_recentFileActions[i]->setVisible(true);
        m_recentFileActions[i]->setToolTip(files[i]);
    }

    for (int i = numRecent; i < kMaxRecentFiles; ++i) {
        m_recentFileActions[i]->setVisible(false);
    }

    m_recentFilesMenu->setEnabled(numRecent > 0);
}

void MainWindow::addToRecentFiles(const QString& path)
{
    auto* config = dawcast::config::AppConfig::instance();
    QStringList files = config->value(QStringLiteral("file/recentFiles")).toStringList();

    // Remove duplicates, then prepend the new path
    files.removeAll(path);
    files.prepend(path);

    // Cap at max
    while (files.size() > kMaxRecentFiles) {
        files.removeLast();
    }

    config->setValue(QStringLiteral("file/recentFiles"), files);
    config->save();

    updateRecentFilesMenu();
}

void MainWindow::openRecentFile()
{
    auto* action = qobject_cast<QAction*>(sender());
    if (action) {
        QString path = action->data().toString();
        if (!path.isEmpty()) {
            openProject(path);
        }
    }
}

void MainWindow::clearRecentFiles()
{
    auto* config = dawcast::config::AppConfig::instance();
    config->setValue(QStringLiteral("file/recentFiles"), QStringList());
    config->save();
    updateRecentFilesMenu();
    statusBar()->showMessage(tr("Recent files cleared"), 3000);
}

// ── Window State Persistence ───────────────────────────────────────────────

void MainWindow::saveWindowState()
{
    auto* config = dawcast::config::AppConfig::instance();
    config->setValue(QStringLiteral("window/geometry"),
                     QString::fromLatin1(saveGeometry().toBase64()));
    config->setValue(QStringLiteral("window/state"),
                     QString::fromLatin1(saveState().toBase64()));
    config->save();
}

void MainWindow::restoreWindowState()
{
    auto* config = dawcast::config::AppConfig::instance();

    QByteArray geo = QByteArray::fromBase64(
        config->value(QStringLiteral("window/geometry"), QString())
            .toString().toLatin1());
    QByteArray state = QByteArray::fromBase64(
        config->value(QStringLiteral("window/state"), QString())
            .toString().toLatin1());

    if (!geo.isEmpty()) restoreGeometry(geo);
    if (!state.isEmpty()) restoreState(state);
}

// ── Export Pipeline ───────────────────────────────────────────────────────

void MainWindow::runExportPipeline()
{
    // Stop playback before exporting
    if (m_playbackEngine && m_playbackEngine->isPlaying()) {
        m_playbackEngine->stop();
    }

    // Show the ExportDialog to collect settings
    ExportDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    ExportSettings settings = dlg.exportSettings();

    // Build the ExportEngine config from dialog settings
    dawcast::ExportEngine::ExportConfig config;
    config.audioCodec   = settings.audioCodec;
    config.audioBitrate = settings.audioBitrate;
    config.sampleRate   = settings.sampleRate;
    config.channels     = 2;  // Stereo by default

    if (settings.audioOnly) {
        config.videoCodec.clear();
    } else {
        config.videoCodec   = settings.videoCodec;
        config.videoBitrate = 5000;
        config.videoWidth   = settings.videoWidth;
        config.videoHeight  = settings.videoHeight;
        config.videoFps     = settings.videoFps;
    }

    // Determine output path -- use the outputPathEdit from the dialog,
    // or fall back to a file dialog
    // The ExportDialog stores the path in a QLineEdit; extract it
    // via the settings. If empty, ask the user.
    QString outputPath;
    // Try to find the output path from the dialog's line edit
    auto* pathEdit = dlg.findChild<QLineEdit*>();
    if (pathEdit && !pathEdit->text().isEmpty()) {
        outputPath = pathEdit->text();
    }

    if (outputPath.isEmpty()) {
        QString filter;
        if (settings.audioOnly) {
            filter = tr("Audio Files (*.mp3 *.aac *.wav *.flac *.ogg *.opus);;All Files (*)");
        } else {
            filter = tr("Video Files (*.mp4 *.mkv *.webm *.avi);;All Files (*)");
        }
        outputPath = QFileDialog::getSaveFileName(
            this, tr("Export"), QString(), filter);
        if (outputPath.isEmpty())
            return;
    }

    config.outputPath = outputPath;

    // Determine container from the file extension
    QFileInfo fi(outputPath);
    config.container = fi.suffix().toLower();

    // Create the export engine
    m_exportEngine = new dawcast::ExportEngine(this);

    // Show a progress dialog
    auto* progressDlg = new QProgressDialog(
        tr("Exporting..."), tr("Cancel"), 0, 100, this);
    progressDlg->setWindowModality(Qt::WindowModal);
    progressDlg->setMinimumDuration(0);
    progressDlg->setValue(0);

    // Connect signals
    connect(m_exportEngine, &dawcast::ExportEngine::progress,
            progressDlg, &QProgressDialog::setValue);

    connect(m_exportEngine, &dawcast::ExportEngine::finished,
            this, [this, progressDlg](const QString& path) {
        progressDlg->close();
        progressDlg->deleteLater();

        QMessageBox::information(
            this, tr("Export Complete"),
            tr("Successfully exported to:\n%1").arg(path));

        statusBar()->showMessage(
            tr("Export complete: %1").arg(QFileInfo(path).fileName()), 5000);

        m_exportEngine->deleteLater();
        m_exportEngine = nullptr;
    });

    connect(m_exportEngine, &dawcast::ExportEngine::error,
            this, [this, progressDlg](const QString& message) {
        progressDlg->close();
        progressDlg->deleteLater();

        QMessageBox::warning(
            this, tr("Export Error"), message);

        statusBar()->showMessage(tr("Export failed"), 3000);

        m_exportEngine->deleteLater();
        m_exportEngine = nullptr;
    });

    connect(progressDlg, &QProgressDialog::canceled,
            m_exportEngine, &dawcast::ExportEngine::cancelExport);

    // Start the export
    statusBar()->showMessage(tr("Exporting..."));
    m_exportEngine->startExport(m_timelineModel, config);
}

} // namespace dawcast::widgets
