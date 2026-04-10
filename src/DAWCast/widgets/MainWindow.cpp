// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "MainWindow.h"
#include "TimelineWidget.h"
#include "TrackHeaderPanel.h"
#include "MixerWidget.h"
#include "VideoPreview.h"
#include "MediaBrowser.h"
#include "MediaLibraryWidget.h"
#include "LibraryTableModel.h"
#include "EffectsRackWidget.h"
#include "TransportBar.h"
#include "LUFSMeterWidget.h"
#include "SidebarNav.h"
#include "ActionBar.h"
#include "MasterStrip.h"
#include "PreferencesDialog.h"
#include "ViewModeSwitcher.h"
#include "ImportAudioDialog.h"
#include "ChapterWidget.h"
#include "MetadataPanel.h"
#include "SpectrumWidget.h"
#include "PianoRollWidget.h"
#include "ScriptReaderPanel.h"
#include "PedalboardWidget.h"
#include "StreamMonitorPanel.h"
#include "MarkerListWidget.h"
#include "../ai/AIPanel.h"
#include "../core/ViewModeManager.h"

#include "../timeline/Timeline.h"
#include "../timeline/Marker.h"
#include "../timeline/AudioTrack.h"
#include "../timeline/Clip.h"
#include "../audio_engine/AudioEngine.h"
#include "../audio_engine/AudioMixer.h"
#include "../audio_engine/PlaybackEngine.h"
#include "../audio_engine/MultitrackRecorder.h"
#include "../audio_engine/BusRouter.h"
#include "../audio_engine/AudioBus.h"
#include "../audio_engine/Metronome.h"
#include "../audio_engine/WaveformCache.h"
#include "../audio_engine/ExportEngine.h"
#include "../video_engine/VideoPlaybackController.h"
#include "../core/MediaLibrary.h"
#include "../config/AppConfig.h"
#include "ExportDialog.h"
#include "BatchEncoderDialog.h"
#include "MassTagEditor.h"
#include "AboutDialog.h"
#include "ImportAudioDialog.h"
#include "StreamingDialog.h"
#include "../broadcast/RTMPStreamer.h"
#include "../core/UndoManager.h"
#include "../core/ProjectManager.h"
#include "../core/WorkspaceManager.h"

#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <QMenuBar>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QInputDialog>
#include <QStatusBar>
#include <QScrollBar>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
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
    // Sidebar nav removed — view modes will handle workspace layouts
    setupCentralWidget();
    setupDockWidgets();
    setupStatusBar();
    setupAudioPipeline();
    setupVideoPlayback();
    setupRTMPStreamer();
    setupConnections();
    setupViewModes();
    setupUndoRedo();

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

    auto* actImportAudio = fileMenu->addAction(tr("&Import Audio..."));
    actImportAudio->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_I));
    connect(actImportAudio, &QAction::triggered, this, [this] {
        QStringList files = QFileDialog::getOpenFileNames(this,
            tr("Import Audio Files"), QString(),
            tr("Audio Files (*.wav *.aiff *.aif *.mp3 *.flac *.ogg *.opus *.aac *.m4a);;"
               "All Files (*)"));
        if (files.isEmpty()) return;

        // Check preference for showing import dialog
        bool showDialog = true;
        auto* cfg = dawcast::config::AppConfig::instance();
        if (cfg)
            showDialog = cfg->value(QStringLiteral("audio/showImportDialog"), true).toBool();

        if (showDialog) {
            auto* dlg = new ImportAudioDialog(files, this);
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            if (dlg->exec() != QDialog::Accepted)
                return;
            // Import options are available via dlg->options()
            // Actual import logic to be wired to the timeline engine
        }

        statusBar()->showMessage(
            tr("Imported %n file(s)", "", files.size()), 5000);
    });

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

    m_actUndo = editMenu->addAction(tr("&Undo"));
    m_actUndo->setShortcut(QKeySequence::Undo);
    connect(m_actUndo, &QAction::triggered, this, &MainWindow::undo);

    m_actRedo = editMenu->addAction(tr("&Redo"));
    m_actRedo->setShortcut(QKeySequence::Redo);
    connect(m_actRedo, &QAction::triggered, this, &MainWindow::redo);

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

    // ── Edit > Split / Ripple ──────────────────────────────────────────
    auto* actSplit = editMenu->addAction(tr("Split at Playhead"));
    actSplit->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(actSplit, &QAction::triggered, this, &MainWindow::splitAtPlayhead);

    auto* actRipple = editMenu->addAction(tr("Ripple Edit Mode"));
    actRipple->setCheckable(true);
    actRipple->setChecked(false);
    connect(actRipple, &QAction::triggered, this, &MainWindow::toggleRippleMode);

    editMenu->addSeparator();

    // ── Edit > Zoom ────────────────────────────────────────────────────
    auto* actZoomIn = editMenu->addAction(tr("Zoom &In"));
    actZoomIn->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Equal));
    connect(actZoomIn, &QAction::triggered, this, [this] {
        if (m_timeline) m_timeline->zoomIn();
    });

    auto* actZoomOut = editMenu->addAction(tr("Zoom &Out"));
    actZoomOut->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus));
    connect(actZoomOut, &QAction::triggered, this, [this] {
        if (m_timeline) m_timeline->zoomOut();
    });

    auto* actZoomFit = editMenu->addAction(tr("Zoom to &Fit"));
    actZoomFit->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(actZoomFit, &QAction::triggered, this, [this] {
        if (m_timeline) m_timeline->zoomToFit();
    });

    auto* actZoomSel = editMenu->addAction(tr("Zoom to &Selection"));
    actZoomSel->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z));
    connect(actZoomSel, &QAction::triggered, this, [this] {
        if (m_timeline) m_timeline->zoomToSelection();
    });

    editMenu->addSeparator();

    // ── Edit > Solo Mode ─────────────────────────────────────────────
    auto* soloModeMenu = editMenu->addMenu(tr("Solo &Mode"));

    auto* actSoloInPlace = soloModeMenu->addAction(tr("Solo in Place (SIP)"));
    actSoloInPlace->setCheckable(true);
    actSoloInPlace->setChecked(true);

    auto* actSoloInFront = soloModeMenu->addAction(tr("Solo in Front (SIF)"));
    actSoloInFront->setCheckable(true);

    auto* soloModeGroup = new QActionGroup(this);
    soloModeGroup->setExclusive(true);
    soloModeGroup->addAction(actSoloInPlace);
    soloModeGroup->addAction(actSoloInFront);

    connect(actSoloInPlace, &QAction::triggered, this, [this]() {
        if (m_audioMixer)
            m_audioMixer->setSoloMode(dawcast::AudioMixer::SoloInPlace);
    });
    connect(actSoloInFront, &QAction::triggered, this, [this]() {
        if (m_audioMixer)
            m_audioMixer->setSoloMode(dawcast::AudioMixer::SoloInFront);
    });

    editMenu->addSeparator();

    auto* actPreferences = editMenu->addAction(tr("&Preferences..."));
    actPreferences->setShortcut(QKeySequence::Preferences);
    actPreferences->setMenuRole(QAction::PreferencesRole);  // macOS puts this in app menu
    connect(actPreferences, &QAction::triggered, this, [this] {
        auto* dlg = new PreferencesDialog(this);
        if (m_audioEngine)
            dlg->setAudioEngine(m_audioEngine);
        if (m_audioMixer)
            dlg->setAudioMixer(m_audioMixer);
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

    m_actToggleMediaBrowser = viewMenu->addAction(tr("Media &Library"));
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

    m_actToggleMarkerList = viewMenu->addAction(tr("Marker &List"));
    m_actToggleMarkerList->setCheckable(true);
    m_actToggleMarkerList->setChecked(false);
    connect(m_actToggleMarkerList, &QAction::triggered, this, [this]() {
        if (m_markerListDock) m_markerListDock->setVisible(!m_markerListDock->isVisible());
    });

    viewMenu->addSeparator();

    // ── View > Mode submenu ────────────────────────────────────────────
    m_viewModeMenu = viewMenu->addMenu(tr("&Mode"));

    auto* mgr = dawcast::ViewModeManager::instance();

    struct ModeEntry {
        dawcast::ViewModeManager::Mode mode;
        const char* shortcut;
    };
    static const ModeEntry modes[] = {
        { dawcast::ViewModeManager::Podcaster,    "Ctrl+1" },
        { dawcast::ViewModeManager::Producer,     "Ctrl+2" },
        { dawcast::ViewModeManager::DJLive,       "Ctrl+3" },
        { dawcast::ViewModeManager::StudioArtist, "Ctrl+4" },
        { dawcast::ViewModeManager::VoiceOver,    "Ctrl+5" },
        { dawcast::ViewModeManager::GuitarFX,     "Ctrl+6" },
    };

    auto* modeGroup = new QActionGroup(this);
    modeGroup->setExclusive(true);

    for (const auto& entry : modes) {
        QAction* act = m_viewModeMenu->addAction(mgr->modeName(entry.mode));
        act->setCheckable(true);
        act->setShortcut(QKeySequence(QString::fromLatin1(entry.shortcut)));
        act->setStatusTip(mgr->modeDescription(entry.mode));
        act->setData(static_cast<int>(entry.mode));
        modeGroup->addAction(act);

        connect(act, &QAction::triggered, this, [this, entry]() {
            dawcast::ViewModeManager::instance()->setMode(entry.mode);
        });

        // Check the currently active mode
        if (entry.mode == mgr->currentMode())
            act->setChecked(true);

        m_viewModeActions.append(act);
    }

    viewMenu->addSeparator();

    // ── View > Workspace Profiles ──────────────────────────────────────
    auto* actSaveWorkspace = viewMenu->addAction(tr("Save Workspace..."));
    actSaveWorkspace->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_S));
    connect(actSaveWorkspace, &QAction::triggered, this, &MainWindow::saveWorkspace);

    m_workspaceLoadMenu = viewMenu->addMenu(tr("Load Workspace"));
    rebuildWorkspaceMenu();

    auto* actManageWorkspaces = viewMenu->addAction(tr("Manage Workspaces..."));
    connect(actManageWorkspaces, &QAction::triggered, this, &MainWindow::manageWorkspaces);

    // Factory preset shortcuts: Cmd+Shift+1..6
    {
        auto* wsMgr = dawcast::WorkspaceManager::instance();
        QStringList names = wsMgr->profileNames();
        int shortcutIdx = 0;
        static const Qt::Key shortcutKeys[] = {
            Qt::Key_1, Qt::Key_2, Qt::Key_3,
            Qt::Key_4, Qt::Key_5, Qt::Key_6
        };
        for (const QString& name : names) {
            if (!wsMgr->profile(name).isFactory) continue;
            if (shortcutIdx >= 6) break;

            auto* wsAct = new QAction(name, this);
            wsAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | shortcutKeys[shortcutIdx]));
            connect(wsAct, &QAction::triggered, this, [this, name] {
                loadWorkspaceProfile(name);
            });
            addAction(wsAct);  // Register shortcut globally on the window
            m_workspaceShortcuts.append(wsAct);
            ++shortcutIdx;
        }

        // Rebuild menu whenever profiles change
        connect(wsMgr, &dawcast::WorkspaceManager::profilesChanged,
                this, &MainWindow::rebuildWorkspaceMenu);
    }

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

    // Transport + workspace selector in one toolbar row
    auto* tbContainer = new QWidget(this);
    auto* tbLayout = new QHBoxLayout(tbContainer);
    tbLayout->setContentsMargins(0, 0, 4, 0);
    tbLayout->setSpacing(8);

    m_transportBar = new TransportBar(this);
    tbLayout->addWidget(m_transportBar, 1);

    m_viewModeSwitcher = new ViewModeSwitcher(this);
    m_viewModeSwitcher->setMaximumWidth(210);
    tbLayout->addWidget(m_viewModeSwitcher, 0);

    m_toolBar->addWidget(tbContainer);
}

// ── Sidebar Navigation (removed — replaced by View Mode system) ────────────

void MainWindow::setupSidebar()
{
    // Sidebar removed. View Modes will handle workspace layouts:
    // - Podcaster, Producer, DJ/Live, Studio, Voice Over, Guitar FX
    // Each mode configures visible panels, dock layout, and tool focus.
}

// ── Central Widget ──────────────────────────────────────────────────────────

void MainWindow::setupCentralWidget()
{
    // Central content: timeline + track headers + master strip + action bar
    auto* centralContainer = new QWidget(this);
    auto* centralLayout = new QVBoxLayout(centralContainer);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);

    m_centralSplitter = new QSplitter(Qt::Vertical, centralContainer);
    m_centralSplitter->setChildrenCollapsible(false);

    // Horizontal splitter: track headers (left) + timeline waveform area (right)
    m_timelineSplitter = new QSplitter(Qt::Horizontal, m_centralSplitter);
    m_timelineSplitter->setChildrenCollapsible(false);

    m_trackHeaders = new TrackHeaderPanel(m_timelineSplitter);
    m_timeline = new TimelineWidget(m_timelineSplitter);
    m_timeline->setMinimumHeight(200);

    m_timelineSplitter->addWidget(m_trackHeaders);
    m_timelineSplitter->addWidget(m_timeline);
    m_timelineSplitter->setStretchFactor(0, 0);  // headers: fixed width
    m_timelineSplitter->setStretchFactor(1, 1);  // timeline: stretches

    m_centralSplitter->addWidget(m_timelineSplitter);
    m_centralSplitter->setStretchFactor(0, 1);

    centralLayout->addWidget(m_centralSplitter, 1);

    // Master strip — full width between timeline and action bar
    m_masterStrip = new MasterStrip(centralContainer);
    centralLayout->addWidget(m_masterStrip);

    // Action bar — full width at the bottom of the central area
    m_actionBar = new ActionBar(centralContainer);
    centralLayout->addWidget(m_actionBar);

    setCentralWidget(centralContainer);
}

// ── Dock Widgets ────────────────────────────────────────────────────────────

void MainWindow::setupDockWidgets()
{
    // Bottom dock — Mixer
    m_mixerDock = new QDockWidget(tr("Mixer"), this);
    m_mixerDock->setObjectName(QStringLiteral("MixerDock"));
    m_mixerDock->setToolTip(tr("Mixer - Per-track volume, pan, mute, solo, and bus routing controls"));
    m_mixerDock->setMinimumHeight(120);
    m_mixer = new MixerWidget(m_mixerDock);
    m_mixerDock->setWidget(m_mixer);
    addDockWidget(Qt::BottomDockWidgetArea, m_mixerDock);

    // Right dock — Video Preview
    m_videoDock = new QDockWidget(tr("Video Preview"), this);
    m_videoDock->setObjectName(QStringLiteral("VideoPreviewDock"));
    m_videoDock->setToolTip(tr("Video Preview - Live preview of video tracks synchronized with playback"));
    m_videoDock->setMinimumSize(240, 180);
    m_videoPreview = new VideoPreview(m_videoDock);
    m_videoDock->setWidget(m_videoPreview);
    addDockWidget(Qt::RightDockWidgetArea, m_videoDock);

    // Left dock — Media Library (replaces basic Media Browser)
    m_mediaBrowserDock = new QDockWidget(tr("Media Library"), this);
    m_mediaBrowserDock->setObjectName(QStringLiteral("MediaBrowserDock"));
    m_mediaBrowserDock->setToolTip(tr("Media Library - Browse, search, and import audio and video files"));
    m_mediaBrowserDock->setMinimumWidth(280);
    m_mediaLibrary = new MediaLibraryWidget(m_mediaBrowserDock);
    m_mediaBrowser = m_mediaLibrary->fileBrowser();  // keep ref for backward compat
    m_mediaBrowserDock->setWidget(m_mediaLibrary);
    addDockWidget(Qt::LeftDockWidgetArea, m_mediaBrowserDock);
    m_mediaBrowserDock->hide();

    // Right dock — Effects Rack (tabified with Video Preview)
    m_effectsDock = new QDockWidget(tr("Effects Rack"), this);
    m_effectsDock->setObjectName(QStringLiteral("EffectsRackDock"));
    m_effectsDock->setToolTip(tr("Effects Rack - Add and configure audio effects for the selected track"));
    m_effectsDock->setMinimumSize(240, 180);
    m_effectsRack = new EffectsRackWidget(m_effectsDock);
    m_effectsDock->setWidget(m_effectsRack);
    addDockWidget(Qt::RightDockWidgetArea, m_effectsDock);
    tabifyDockWidget(m_videoDock, m_effectsDock);

    // Right dock — LUFS Meter
    m_lufsDock = new QDockWidget(tr("LUFS Meter"), this);
    m_lufsDock->setObjectName(QStringLiteral("LUFSMeterDock"));
    m_lufsDock->setToolTip(tr("LUFS Meter - Real-time loudness measurement for broadcast standards compliance"));
    m_lufsDock->setMinimumSize(80, 200);
    m_lufsMeter = new LUFSMeterWidget(m_lufsDock);
    m_lufsDock->setWidget(m_lufsMeter);
    addDockWidget(Qt::RightDockWidgetArea, m_lufsDock);

    // Right dock — AI Panel (tabified with Effects Rack)
    m_aiDock = new QDockWidget(tr("AI Assistant"), this);
    m_aiDock->setObjectName(QStringLiteral("AIDock"));
    m_aiDock->setToolTip(tr("AI Assistant - AI-powered tools for transcription, noise removal, and content suggestions"));
    m_aiDock->setMinimumSize(240, 180);
    m_aiPanel = new dawcast::ai::AIPanel(m_aiDock);
    m_aiDock->setWidget(m_aiPanel);
    addDockWidget(Qt::RightDockWidgetArea, m_aiDock);
    tabifyDockWidget(m_effectsDock, m_aiDock);

    // Raise Video Preview as the default visible tab on the right
    m_videoDock->raise();

    // ── Mode-specific dock widgets (initially hidden) ──────────────────

    // Right dock — Chapter Widget (Podcaster mode)
    m_chapterDock = new QDockWidget(tr("Chapters"), this);
    m_chapterDock->setObjectName(QStringLiteral("ChapterDock"));
    m_chapterDock->setToolTip(tr("Chapters - Define chapter markers for podcast episodes"));
    m_chapterDock->setMinimumSize(200, 150);
    m_chapterWidget = new ChapterWidget(m_chapterDock);
    m_chapterDock->setWidget(m_chapterWidget);
    addDockWidget(Qt::RightDockWidgetArea, m_chapterDock);
    m_chapterDock->hide();

    // Right dock — Metadata Panel (Podcaster mode)
    m_metadataDock = new QDockWidget(tr("Metadata"), this);
    m_metadataDock->setObjectName(QStringLiteral("MetadataDock"));
    m_metadataDock->setToolTip(tr("Metadata - Edit ID3 tags, show notes, and episode information"));
    m_metadataDock->setMinimumSize(200, 150);
    m_metadataPanel = new MetadataPanel(m_metadataDock);
    m_metadataDock->setWidget(m_metadataPanel);
    addDockWidget(Qt::RightDockWidgetArea, m_metadataDock);
    tabifyDockWidget(m_chapterDock, m_metadataDock);
    m_metadataDock->hide();

    // Bottom dock — Spectrum Analyzer (Producer / Guitar FX modes)
    m_spectrumDock = new QDockWidget(tr("Spectrum Analyzer"), this);
    m_spectrumDock->setObjectName(QStringLiteral("SpectrumDock"));
    m_spectrumDock->setToolTip(tr("Spectrum Analyzer - Real-time frequency analysis of the audio output"));
    m_spectrumDock->setMinimumSize(200, 120);
    m_spectrumWidget = new SpectrumWidget(m_spectrumDock);
    m_spectrumDock->setWidget(m_spectrumWidget);
    addDockWidget(Qt::BottomDockWidgetArea, m_spectrumDock);
    m_spectrumDock->hide();

    // Bottom dock — Piano Roll (Studio Artist mode)
    m_pianoRollDock = new QDockWidget(tr("Piano Roll"), this);
    m_pianoRollDock->setObjectName(QStringLiteral("PianoRollDock"));
    m_pianoRollDock->setToolTip(tr("Piano Roll - MIDI note editor for composing and editing MIDI tracks"));
    m_pianoRollDock->setMinimumSize(300, 150);
    m_pianoRoll = new PianoRollWidget(m_pianoRollDock);
    m_pianoRollDock->setWidget(m_pianoRoll);
    addDockWidget(Qt::BottomDockWidgetArea, m_pianoRollDock);
    m_pianoRollDock->hide();

    // Left dock — Script Reader (Voice Over mode)
    m_scriptReaderDock = new QDockWidget(tr("Script Reader"), this);
    m_scriptReaderDock->setObjectName(QStringLiteral("ScriptReaderDock"));
    m_scriptReaderDock->setToolTip(tr("Script Reader - Scrolling teleprompter for voice-over recording sessions"));
    m_scriptReaderDock->setMinimumSize(280, 200);
    m_scriptReader = new ScriptReaderPanel(m_scriptReaderDock);
    m_scriptReaderDock->setWidget(m_scriptReader);
    addDockWidget(Qt::LeftDockWidgetArea, m_scriptReaderDock);
    m_scriptReaderDock->hide();

    // Bottom dock — Pedalboard (Guitar FX mode)
    m_pedalboardDock = new QDockWidget(tr("Pedalboard"), this);
    m_pedalboardDock->setObjectName(QStringLiteral("PedalboardDock"));
    m_pedalboardDock->setToolTip(tr("Pedalboard - Virtual guitar effects pedalboard with drag-and-drop effect chain"));
    m_pedalboardDock->setMinimumSize(400, 280);
    m_pedalboard = new PedalboardWidget(m_pedalboardDock);
    m_pedalboardDock->setWidget(m_pedalboard);
    addDockWidget(Qt::BottomDockWidgetArea, m_pedalboardDock);
    m_pedalboardDock->hide();

    // Right dock — Stream Monitor (DJ/Live mode)
    m_streamMonitorDock = new QDockWidget(tr("Stream Monitor"), this);
    m_streamMonitorDock->setObjectName(QStringLiteral("StreamMonitorDock"));
    m_streamMonitorDock->setToolTip(tr("Stream Monitor - Live stream status, bitrate, viewer count, and connection health"));
    m_streamMonitorDock->setMinimumSize(240, 300);
    m_streamMonitor = new StreamMonitorPanel(m_streamMonitorDock);
    m_streamMonitorDock->setWidget(m_streamMonitor);
    addDockWidget(Qt::RightDockWidgetArea, m_streamMonitorDock);
    m_streamMonitorDock->hide();

    // Bottom dock — Marker List (tabified with Mixer)
    m_markerListDock = new QDockWidget(tr("Marker List"), this);
    m_markerListDock->setObjectName(QStringLiteral("MarkerListDock"));
    m_markerListDock->setToolTip(tr("Marker List - View and navigate to all timeline markers"));
    m_markerListDock->setMinimumSize(200, 120);
    m_markerList = new MarkerListWidget(m_markerListDock);
    m_markerListDock->setWidget(m_markerList);
    addDockWidget(Qt::BottomDockWidgetArea, m_markerListDock);
    tabifyDockWidget(m_mixerDock, m_markerListDock);
    m_mixerDock->raise();   // Keep mixer as the default visible bottom tab
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

    // Give the TimelineWidget and TrackHeaderPanel their data model
    m_timeline->setTimeline(m_timelineModel);
    m_trackHeaders->setTimeline(m_timelineModel);

    // Give the MarkerListWidget its data model
    m_markerList->setTimeline(m_timelineModel);

    // Wire "+" button in the track header panel to add an audio track
    connect(m_trackHeaders, &TrackHeaderPanel::addTrackRequested,
            this, &MainWindow::addAudioTrack);
    connect(m_trackHeaders, &TrackHeaderPanel::duplicateTrackRequested,
            this, &MainWindow::duplicateTrack);
    connect(m_trackHeaders, &TrackHeaderPanel::deleteTrackRequested,
            this, [this](int trackIndex) {
        if (m_timelineModel) {
            m_timelineModel->removeTrack(trackIndex);
            statusBar()->showMessage(tr("Track deleted"), 3000);
        }
    });
    connect(m_trackHeaders, &TrackHeaderPanel::trackMoveRequested,
            this, [this](int from, int to) {
        if (m_timelineModel) {
            m_timelineModel->moveTrack(from, to);
        }
    });

    // Track selection -> effects rack. When the user clicks a track header
    // we swap the EffectsRackWidget over to that track's DspChain so that
    // Add Effect / Bypass / Remove all operate on the selected track.
    connect(m_trackHeaders, &TrackHeaderPanel::trackSelected,
            this, [this](int trackIndex) {
        if (!m_timelineModel || !m_effectsRack) return;
        auto* track = qobject_cast<dawcast::AudioTrack*>(
            m_timelineModel->track(trackIndex));
        if (!track) return;
        m_effectsRack->setDspChain(track->effectChain());
        statusBar()->showMessage(
            tr("Effects rack bound to track %1").arg(trackIndex + 1), 2000);
    });

    // Project manager — handles save/load serialization to/from JSON
    m_projectManager = new dawcast::ProjectManager(this);
    m_projectManager->setTimeline(m_timelineModel);

    // Audio mixer (volume / pan / mute / solo per strip)
    m_audioMixer = new dawcast::AudioMixer(this);

    // Bus router (master + sub-groups + sends). Created here so the master
    // fader on the MixerWidget has somewhere to send its volume changes
    // and so the PlaybackEngine can route per-track audio through buses.
    m_busRouter = new dawcast::BusRouter(this);

    // Hand the AudioMixer to the MixerWidget so fader/pan/mute/solo
    // changes and VU meter polling are wired up. The mixer widget adds
    // one strip per AudioMixer strip on setMixer() and the PlaybackEngine
    // grows the mixer's strip count as tracks are added.
    if (m_mixer) {
        m_mixer->setBusRouter(m_busRouter);
        m_mixer->setMixer(m_audioMixer);
    }

    // Rebuild mixer widget strips whenever tracks are added/removed so the
    // fader/VU columns stay in sync with the timeline. The AudioMixer's
    // strip count is otherwise only advanced lazily by PlaybackEngine on
    // play(), but we want the mixer dock to reflect new tracks immediately.
    connect(m_timelineModel, &dawcast::Timeline::trackAdded,
            this, [this](int) {
        if (!m_audioMixer || !m_timelineModel) return;
        while (m_audioMixer->stripCount() < m_timelineModel->trackCount()) {
            m_audioMixer->addStrip();
        }
        if (m_mixer) m_mixer->setMixer(m_audioMixer);
    });
    connect(m_timelineModel, &dawcast::Timeline::trackRemoved,
            this, [this](int idx) {
        if (m_audioMixer) m_audioMixer->removeStrip(idx);
        if (m_mixer && m_audioMixer) m_mixer->setMixer(m_audioMixer);
    });

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
    m_playbackEngine->setBusRouter(m_busRouter);

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
        int sr = m_audioEngine ? m_audioEngine->sampleRate() : 48000;
        m_transportBar->setPosition(0, sr);
    });

    // Fast-forward -> jump to end of timeline
    connect(m_transportBar, &TransportBar::fastForwardClicked, this, [this]() {
        if (m_timelineModel) {
            int64_t dur = m_timelineModel->duration();
            if (dur > 0) {
                m_timelineModel->setPlayhead(dur);
                if (m_playbackEngine) m_playbackEngine->seekTo(dur);
                int sr = m_audioEngine ? m_audioEngine->sampleRate() : 48000;
                m_transportBar->setPosition(dur, sr);
            }
        }
    });

    // Zoom slider -> TimelineWidget
    connect(m_transportBar, &TransportBar::zoomChanged, this, [this](int pxPerSec) {
        if (m_timeline) {
            m_timeline->setZoom(static_cast<float>(pxPerSec) / 100.0f);
        }
    });

    // Automation write mode (status bar feedback for now)
    connect(m_transportBar, &TransportBar::automationWriteToggled, this, [this](bool on) {
        statusBar()->showMessage(on ? tr("Automation write enabled")
                                    : tr("Automation write disabled"), 2000);
    });

    // Ripple edit mode toggle from transport bar
    connect(m_transportBar, &TransportBar::rippleModeToggled, this, [this](bool enabled) {
        if (m_timeline) {
            m_timeline->setRippleMode(enabled);
            statusBar()->showMessage(
                enabled ? tr("Ripple Edit: ON") : tr("Ripple Edit: OFF"), 2000);
        }
    });

    // Crossfade mode
    connect(m_transportBar, &TransportBar::crossfadeModeChanged, this, [this](int mode) {
        static const char* modeNames[] = { "Auto", "Manual", "Off" };
        if (mode >= 0 && mode <= 2) {
            statusBar()->showMessage(
                tr("Crossfade mode: %1").arg(QString::fromLatin1(modeNames[mode])), 2000);
        }
    });

    // Snap mode selector -> TimelineWidget
    connect(m_transportBar, &TransportBar::snapModeChanged, this, [this](int mode) {
        if (m_timeline) {
            m_timeline->setSnapMode(static_cast<TimelineWidget::SnapMode>(mode));
        }
        static const char* snapNames[] = { "Off", "Beat", "Bar", "Second", "Half Second", "Frame" };
        if (mode >= 0 && mode <= 5) {
            statusBar()->showMessage(
                tr("Snap: %1").arg(QString::fromLatin1(snapNames[mode])), 2000);
        }
    });

    // Buses button
    connect(m_transportBar, &TransportBar::busesClicked, this, [this]() {
        if (m_mixerDock) {
            m_mixerDock->setVisible(true);
            m_mixerDock->raise();
        }
        statusBar()->showMessage(tr("Audio buses"), 2000);
    });

    // ── Marker navigation: Prev / Next ────────────────────────────────
    connect(m_transportBar, &TransportBar::prevMarkerClicked, this, [this]() {
        if (!m_timelineModel) return;
        int64_t pos = m_timelineModel->playhead();
        int idx = m_timelineModel->previousMarkerIndex(pos);
        if (idx >= 0) {
            int64_t target = m_timelineModel->marker(idx).position();
            m_timelineModel->setPlayhead(target);
            if (m_playbackEngine) m_playbackEngine->seekTo(target);
        } else {
            // No previous marker — jump to start
            m_timelineModel->setPlayhead(0);
            if (m_playbackEngine) m_playbackEngine->seekTo(0);
        }
    });

    connect(m_transportBar, &TransportBar::nextMarkerClicked, this, [this]() {
        if (!m_timelineModel) return;
        int64_t pos = m_timelineModel->playhead();
        int idx = m_timelineModel->nextMarkerIndex(pos);
        if (idx >= 0) {
            int64_t target = m_timelineModel->marker(idx).position();
            m_timelineModel->setPlayhead(target);
            if (m_playbackEngine) m_playbackEngine->seekTo(target);
        } else {
            // No next marker — jump to end
            int64_t dur = m_timelineModel->duration();
            if (dur > 0) {
                m_timelineModel->setPlayhead(dur);
                if (m_playbackEngine) m_playbackEngine->seekTo(dur);
            }
        }
    });

    // ── Loop toggle ───────────────────────────────────────────────────
    connect(m_transportBar, &TransportBar::loopToggled, this, [this](bool enabled) {
        if (!m_timelineModel) return;
        if (enabled && m_timelineModel->loopEnd() <= m_timelineModel->loopStart()) {
            // No loop region set — default to full timeline
            m_timelineModel->setLoopStart(0);
            int64_t dur = m_timelineModel->duration();
            m_timelineModel->setLoopEnd(dur > 0 ? dur : m_timelineModel->sampleRate() * 10);
        }
        m_timelineModel->setLoopEnabled(enabled);
        statusBar()->showMessage(enabled ? tr("Loop enabled") : tr("Loop disabled"), 2000);
    });

    // ── Marker list panel navigation ──────────────────────────────────
    connect(m_markerList, &MarkerListWidget::markerSelected, this, [this](int64_t pos) {
        if (m_timelineModel) m_timelineModel->setPlayhead(pos);
        if (m_playbackEngine) m_playbackEngine->seekTo(pos);
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

    // TimelineWidget selection changes -> TransportBar selection display
    connect(m_timeline, &TimelineWidget::selectionChanged,
            this, [this](int64_t start, int64_t end) {
        int sr = m_audioEngine ? m_audioEngine->sampleRate() : 48000;
        m_transportBar->setSelection(start, end, sr);
    });

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
    connect(m_markerListDock, &QDockWidget::visibilityChanged,
            m_actToggleMarkerList, &QAction::setChecked);

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

    // ── Action Bar signals ───────────────────────────────────────────────
    connect(m_actionBar, &ActionBar::addTrackClicked,
            this, &MainWindow::addAudioTrack);
    connect(m_actionBar, &ActionBar::loadFromLibraryClicked,
            this, &MainWindow::loadFromLibrary);
    connect(m_actionBar, &ActionBar::exportMixdownClicked,
            this, &MainWindow::exportProject);
    connect(m_actionBar, &ActionBar::projectsClicked,
            this, &MainWindow::openProjectBrowser);
    connect(m_actionBar, &ActionBar::saveProjectClicked,
            this, &MainWindow::saveProject);

    // ── Master Strip signals ───────────────────────────────────────────
    // Feed master fader changes to the AudioMixer (strip 0 reserved for master
    // output in a future update; for now we log the value)
    connect(m_masterStrip, &MasterStrip::levelChanged,
            this, [this](float db) {
        // Route master fader to the BusRouter's master bus
        if (m_playbackEngine && m_playbackEngine->busRouter()) {
            auto* master = m_playbackEngine->busRouter()->masterBus();
            if (master) master->setVolume(db);
        }
    });

    // Pre-cache waveforms when files are double-clicked in Media Library
    connect(m_mediaLibrary, &MediaLibraryWidget::fileDoubleClicked,
            this, [](const QString& path) {
        dawcast::WaveformCache::instance()->requestWaveform(path);
    });

    connect(m_mediaLibrary, &MediaLibraryWidget::importRequested,
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

    bool loaded = false;
    if (m_projectManager) {
        loaded = m_projectManager->openProject(path);
    }

    if (loaded) {
        m_projectPath = path;
        QFileInfo fi(path);
        setWindowTitle(QStringLiteral("Mcaster1DAWCast \u2014 %1").arg(fi.baseName()));
        statusBar()->showMessage(tr("Opened: %1").arg(fi.fileName()), 5000);

        addToRecentFiles(path);

        // Refresh the timeline widget and transport bar
        m_timeline->update();
        if (m_timelineModel) {
            int sr = m_audioEngine ? m_audioEngine->sampleRate() : 48000;
            m_transportBar->setDuration(m_timelineModel->duration(), sr);
            m_transportBar->setPosition(m_timelineModel->playhead(), sr);
        }

        // Remember directory for next file dialog
        auto* config = dawcast::config::AppConfig::instance();
        config->setValue(QStringLiteral("file/lastOpenDir"), fi.absolutePath());
        config->save();
    } else {
        statusBar()->showMessage(tr("Failed to open: %1").arg(path), 5000);
    }
}

void MainWindow::saveProject()
{
    if (m_projectPath.isEmpty()) {
        saveProjectAs();
        return;
    }

    if (m_projectManager) {
        if (m_projectManager->saveProjectAs(m_projectPath)) {
            statusBar()->showMessage(tr("Project saved"), 3000);
        } else {
            statusBar()->showMessage(tr("Failed to save project!"), 5000);
        }
    }
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
        bool saved = false;
        if (m_projectManager) {
            saved = m_projectManager->saveProjectAs(path);
        }

        if (saved) {
            m_projectPath = path;
            QFileInfo fi(path);
            setWindowTitle(QStringLiteral("Mcaster1DAWCast \u2014 %1").arg(fi.baseName()));
            statusBar()->showMessage(tr("Project saved as: %1").arg(fi.fileName()), 5000);

            addToRecentFiles(path);

            config->setValue(QStringLiteral("file/lastOpenDir"), fi.absolutePath());
            config->save();
        } else {
            statusBar()->showMessage(tr("Failed to save project!"), 5000);
        }
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
    if (m_undoManager) {
        m_undoManager->undo();
    }
    statusBar()->showMessage(tr("Undo"), 2000);
}

void MainWindow::redo()
{
    if (m_undoManager) {
        m_undoManager->redo();
    }
    statusBar()->showMessage(tr("Redo"), 2000);
}

void MainWindow::cut()
{
    if (m_timeline) {
        m_timeline->cutSelectedClips();
    }
    statusBar()->showMessage(tr("Cut"), 2000);
}

void MainWindow::copy()
{
    if (m_timeline) {
        m_timeline->copySelectedClips();
    }
    statusBar()->showMessage(tr("Copy"), 2000);
}

void MainWindow::paste()
{
    if (m_timeline) {
        m_timeline->pasteClips();
    }
    statusBar()->showMessage(tr("Paste"), 2000);
}

void MainWindow::deleteSelected()
{
    if (m_timeline) {
        m_timeline->deleteSelectedClips();
    }
    statusBar()->showMessage(tr("Delete"), 2000);
}

void MainWindow::splitAtPlayhead()
{
    if (m_timeline) {
        m_timeline->splitAtPlayhead();
    }
    statusBar()->showMessage(tr("Split at Playhead"), 2000);
}

void MainWindow::toggleRippleMode()
{
    if (m_timeline) {
        m_timeline->setRippleMode(!m_timeline->rippleMode());
        statusBar()->showMessage(
            m_timeline->rippleMode() ? tr("Ripple Edit: ON") : tr("Ripple Edit: OFF"),
            2000);
    }
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

// ── Action Bar Slots ───────────────────────────────────────────────────────

void MainWindow::loadFromLibrary()
{
    // Show the media library if hidden, then let the user pick files
    if (m_mediaBrowserDock && !m_mediaBrowserDock->isVisible()) {
        m_mediaBrowserDock->show();
        m_mediaBrowserDock->raise();
    }
    statusBar()->showMessage(tr("Media Library opened — drag files to the timeline"), 3000);
}

void MainWindow::openProjectBrowser()
{
    // Open the project directory via a standard file dialog for now
    openProject();
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

void MainWindow::duplicateTrack(int trackIndex)
{
    if (!m_timelineModel) return;
    if (trackIndex < 0 || trackIndex >= m_timelineModel->trackCount()) return;

    QObject* srcTrackObj = m_timelineModel->track(trackIndex);
    auto* srcAudio = qobject_cast<dawcast::AudioTrack*>(srcTrackObj);
    if (!srcAudio) {
        statusBar()->showMessage(tr("Only audio tracks can be duplicated"), 3000);
        return;
    }

    // Create a new audio track
    auto* newTrack = m_timelineModel->addAudioTrack();
    newTrack->setName(srcAudio->name() + QStringLiteral(" - Copy"));
    newTrack->setVolume(srcAudio->volumeDb());
    newTrack->setPan(srcAudio->pan());
    newTrack->setMuted(srcAudio->isMuted());
    newTrack->setSolo(srcAudio->isSolo());

    // Copy all clips (new Clip objects referencing the same source files)
    for (int c = 0; c < srcAudio->clipCount(); ++c) {
        dawcast::Clip* srcClip = srcAudio->clip(c);
        if (!srcClip) continue;

        auto* newClip = new dawcast::Clip(newTrack);
        newClip->setSourcePath(srcClip->sourcePath());
        newClip->setSourceIn(srcClip->sourceIn());
        newClip->setSourceOut(srcClip->sourceOut());
        newClip->setTimelinePosition(srcClip->timelinePosition());
        newClip->setGain(srcClip->gain());
        newClip->setFadeIn(srcClip->fadeIn());
        newClip->setFadeOut(srcClip->fadeOut());
        newClip->setGainEnvelope(srcClip->gainEnvelope());

        newTrack->addClip(newClip);
    }

    statusBar()->showMessage(tr("Track duplicated: %1").arg(newTrack->name()), 3000);
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

// ── View Mode System ──────────────────────────────────────────────────────

void MainWindow::setupViewModes()
{
    auto* mgr = dawcast::ViewModeManager::instance();

    // Wire the ViewModeSwitcher button clicks to the manager
    connect(m_viewModeSwitcher, &ViewModeSwitcher::modeSelected,
            mgr, &dawcast::ViewModeManager::setMode);

    // Wire the manager's mode changes to the workspace layout applicator
    connect(mgr, &dawcast::ViewModeManager::modeChanged,
            this, &MainWindow::applyViewMode);

    // Wire the stream monitor panel to the RTMP streamer
    if (m_rtmpStreamer && m_streamMonitor)
        m_streamMonitor->setRTMPStreamer(m_rtmpStreamer);

    // Wire stream monitor start/stop to the existing streaming methods
    connect(m_streamMonitor, &StreamMonitorPanel::startStreamRequested,
            this, &MainWindow::startStreaming);
    connect(m_streamMonitor, &StreamMonitorPanel::stopStreamRequested,
            this, &MainWindow::stopStreaming);

    // Wire chapter widget to the timeline model
    if (m_chapterWidget && m_timelineModel)
        m_chapterWidget->setTimeline(m_timelineModel);

    // Apply the current (persisted) mode immediately
    applyViewMode(mgr->currentMode());
}

void MainWindow::applyViewMode(dawcast::ViewModeManager::Mode mode)
{
    auto* mgr = dawcast::ViewModeManager::instance();
    auto layout = mgr->layoutForMode(mode);

    // ── Show/hide existing dock widgets ────────────────────────────────
    if (m_mixerDock)          m_mixerDock->setVisible(layout.showMixer);
    if (m_videoDock)          m_videoDock->setVisible(layout.showVideoPreview);
    if (m_mediaBrowserDock)   m_mediaBrowserDock->setVisible(layout.showMediaBrowser);
    if (m_effectsDock)        m_effectsDock->setVisible(layout.showEffectsRack);
    if (m_lufsDock)           m_lufsDock->setVisible(layout.showLUFSMeter);
    if (m_aiDock)             m_aiDock->setVisible(layout.showAIPanel);

    // ── Show/hide mode-specific dock widgets ───────────────────────────
    if (m_chapterDock)        m_chapterDock->setVisible(layout.showChapterWidget);
    if (m_metadataDock)       m_metadataDock->setVisible(layout.showMetadataPanel);
    if (m_spectrumDock)       m_spectrumDock->setVisible(layout.showSpectrumWidget);
    if (m_pianoRollDock)      m_pianoRollDock->setVisible(layout.showPianoRoll);
    if (m_scriptReaderDock)   m_scriptReaderDock->setVisible(layout.showScriptReader);
    if (m_pedalboardDock)     m_pedalboardDock->setVisible(layout.showPedalboard);
    if (m_streamMonitorDock)  m_streamMonitorDock->setVisible(layout.showStreamMonitor);

    // ── Sync View menu toggle actions with actual dock visibility ──────
    if (m_actToggleMixer)        m_actToggleMixer->setChecked(layout.showMixer);
    if (m_actToggleVideo)        m_actToggleVideo->setChecked(layout.showVideoPreview);
    if (m_actToggleMediaBrowser) m_actToggleMediaBrowser->setChecked(layout.showMediaBrowser);
    if (m_actToggleEffectsRack)  m_actToggleEffectsRack->setChecked(layout.showEffectsRack);
    if (m_actToggleLUFS)         m_actToggleLUFS->setChecked(layout.showLUFSMeter);
    if (m_actToggleAI)           m_actToggleAI->setChecked(layout.showAIPanel);

    // ── Sync View > Mode menu radio buttons ────────────────────────────
    for (QAction* act : m_viewModeActions) {
        act->setChecked(act->data().toInt() == static_cast<int>(mode));
    }

    // ── Arrange docks for specific modes ───────────────────────────────
    switch (mode) {

    case dawcast::ViewModeManager::Podcaster:
        // Chapters + Metadata on the right, LUFS alongside
        if (m_chapterDock && m_lufsDock) {
            addDockWidget(Qt::RightDockWidgetArea, m_chapterDock);
            addDockWidget(Qt::RightDockWidgetArea, m_metadataDock);
            tabifyDockWidget(m_chapterDock, m_metadataDock);
            m_chapterDock->raise();
        }
        break;

    case dawcast::ViewModeManager::Producer:
        // Full layout — spectrum at bottom, tabified with mixer
        if (m_spectrumDock && m_mixerDock) {
            addDockWidget(Qt::BottomDockWidgetArea, m_spectrumDock);
            tabifyDockWidget(m_mixerDock, m_spectrumDock);
            m_mixerDock->raise();
        }
        break;

    case dawcast::ViewModeManager::DJLive:
        // Stream monitor on the right
        if (m_streamMonitorDock) {
            addDockWidget(Qt::RightDockWidgetArea, m_streamMonitorDock);
        }
        break;

    case dawcast::ViewModeManager::StudioArtist:
        // Piano roll at the bottom, tabified with mixer
        if (m_pianoRollDock && m_mixerDock) {
            addDockWidget(Qt::BottomDockWidgetArea, m_pianoRollDock);
            tabifyDockWidget(m_mixerDock, m_pianoRollDock);
            m_mixerDock->raise();
        }
        break;

    case dawcast::ViewModeManager::VoiceOver:
        // Script reader on the left, effects rack on the right
        if (m_scriptReaderDock) {
            addDockWidget(Qt::LeftDockWidgetArea, m_scriptReaderDock);
        }
        break;

    case dawcast::ViewModeManager::GuitarFX:
        // Pedalboard at the bottom, spectrum analyzer alongside
        if (m_pedalboardDock) {
            addDockWidget(Qt::BottomDockWidgetArea, m_pedalboardDock);
        }
        if (m_spectrumDock && m_pedalboardDock) {
            addDockWidget(Qt::BottomDockWidgetArea, m_spectrumDock);
            tabifyDockWidget(m_pedalboardDock, m_spectrumDock);
            m_pedalboardDock->raise();
        }
        break;
    }

    // ── Update window title to include the mode name ───────────────────
    QString modeLabel = mgr->modeName(mode);
    QString projectLabel = m_projectName.isEmpty()
        ? QStringLiteral("Untitled Project") : m_projectName;
    setWindowTitle(QStringLiteral("Mcaster1DAWCast \u2014 %1 [%2]")
                       .arg(projectLabel, modeLabel));

    // ── Update status bar ──────────────────────────────────────────────
    statusBar()->showMessage(
        tr("View mode: %1 \u2014 %2")
            .arg(modeLabel, mgr->modeDescription(mode)),
        5000);
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
        // Update the transport bar to reflect the current playhead position
        // (first stop keeps position, second stop returns to 0)
        int sr = m_audioEngine ? m_audioEngine->sampleRate() : 48000;
        m_transportBar->setPosition(m_playbackEngine->currentPosition(), sr);
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

// ── Undo/Redo Wiring ─────────────────────────────────────────────────────────

void MainWindow::setupUndoRedo()
{
    m_undoManager = new dawcast::UndoManager(this);

    // Update menu text when the undo/redo stack description changes
    connect(m_undoManager, &dawcast::UndoManager::undoTextChanged,
            this, [this](const QString& text) {
        if (m_actUndo) {
            m_actUndo->setText(text.isEmpty() ? tr("&Undo")
                                              : tr("&Undo %1").arg(text));
            m_actUndo->setEnabled(m_undoManager->canUndo());
        }
    });

    connect(m_undoManager, &dawcast::UndoManager::redoTextChanged,
            this, [this](const QString& text) {
        if (m_actRedo) {
            m_actRedo->setText(text.isEmpty() ? tr("&Redo")
                                              : tr("&Redo %1").arg(text));
            m_actRedo->setEnabled(m_undoManager->canRedo());
        }
    });

    // Initial state: disable if nothing to undo/redo
    if (m_actUndo) m_actUndo->setEnabled(m_undoManager->canUndo());
    if (m_actRedo) m_actRedo->setEnabled(m_undoManager->canRedo());
}

// ── Workspace Profile Methods ───────────────────────────────────────────────

void MainWindow::rebuildWorkspaceMenu()
{
    if (!m_workspaceLoadMenu) return;

    m_workspaceLoadMenu->clear();

    auto* wsMgr = dawcast::WorkspaceManager::instance();
    QStringList names = wsMgr->profileNames();

    bool addedFactory = false;
    for (const QString& name : names) {
        auto p = wsMgr->profile(name);

        if (addedFactory && !p.isFactory) {
            // Add separator between factory and user profiles
            m_workspaceLoadMenu->addSeparator();
            addedFactory = false;  // Only once
        }

        auto* act = m_workspaceLoadMenu->addAction(name);
        if (p.isFactory) {
            QFont f = act->font();
            f.setItalic(true);
            act->setFont(f);
            addedFactory = true;
        }

        connect(act, &QAction::triggered, this, [this, name] {
            loadWorkspaceProfile(name);
        });
    }

    m_workspaceLoadMenu->setEnabled(!names.isEmpty());
}

void MainWindow::saveWorkspace()
{
    bool ok = false;
    QString name = QInputDialog::getText(
        this, tr("Save Workspace"),
        tr("Workspace name:"),
        QLineEdit::Normal,
        QString(), &ok);

    if (!ok || name.trimmed().isEmpty())
        return;

    name = name.trimmed();

    auto* wsMgr = dawcast::WorkspaceManager::instance();
    wsMgr->saveProfile(name, this);

    statusBar()->showMessage(
        tr("Workspace \"%1\" saved").arg(name), 3000);
}

void MainWindow::loadWorkspaceProfile(const QString& name)
{
    auto* wsMgr = dawcast::WorkspaceManager::instance();
    wsMgr->loadProfile(name, this);

    statusBar()->showMessage(
        tr("Workspace \"%1\" loaded").arg(name), 3000);
}

void MainWindow::manageWorkspaces()
{
    auto* wsMgr = dawcast::WorkspaceManager::instance();
    QStringList names = wsMgr->profileNames();

    // Simple management dialog: list profiles with rename/delete options
    QStringList items;
    for (const QString& name : names) {
        auto p = wsMgr->profile(name);
        items << (p.isFactory ? name + tr(" [Factory]") : name);
    }

    bool ok = false;
    QString selected = QInputDialog::getItem(
        this, tr("Manage Workspaces"),
        tr("Select a workspace to manage:"),
        items, 0, false, &ok);

    if (!ok || selected.isEmpty())
        return;

    // Strip " [Factory]" suffix to get the real name
    QString realName = selected;
    realName.remove(tr(" [Factory]"));

    auto profile = wsMgr->profile(realName);

    if (profile.isFactory) {
        QMessageBox::information(this, tr("Factory Preset"),
            tr("Factory presets cannot be renamed or deleted."));
        return;
    }

    // Show action choices
    QStringList actions;
    actions << tr("Rename") << tr("Delete") << tr("Cancel");
    bool actionOk = false;
    QString action = QInputDialog::getItem(
        this, tr("Manage \"%1\"").arg(realName),
        tr("Action:"), actions, 0, false, &actionOk);

    if (!actionOk || action == tr("Cancel"))
        return;

    if (action == tr("Rename")) {
        bool renameOk = false;
        QString newName = QInputDialog::getText(
            this, tr("Rename Workspace"),
            tr("New name:"), QLineEdit::Normal,
            realName, &renameOk);
        if (renameOk && !newName.trimmed().isEmpty()) {
            wsMgr->renameProfile(realName, newName.trimmed());
            statusBar()->showMessage(
                tr("Workspace renamed to \"%1\"").arg(newName.trimmed()), 3000);
        }
    } else if (action == tr("Delete")) {
        auto confirm = QMessageBox::question(
            this, tr("Delete Workspace"),
            tr("Delete workspace \"%1\"?").arg(realName),
            QMessageBox::Yes | QMessageBox::No);
        if (confirm == QMessageBox::Yes) {
            wsMgr->deleteProfile(realName);
            statusBar()->showMessage(
                tr("Workspace \"%1\" deleted").arg(realName), 3000);
        }
    }
}

} // namespace dawcast::widgets
