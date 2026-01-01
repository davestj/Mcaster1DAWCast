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

#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QStatusBar>
#include <QSplitter>
#include <QApplication>
#include <QKeySequence>
#include <QDesktopServices>
#include <QUrl>

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
    setupConnections();
}

MainWindow::~MainWindow() = default;

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
    connect(actOpen, &QAction::triggered, this, &MainWindow::openProject);

    auto* actSave = fileMenu->addAction(tr("&Save Project"));
    actSave->setShortcut(QKeySequence::Save);
    connect(actSave, &QAction::triggered, this, &MainWindow::saveProject);

    auto* actSaveAs = fileMenu->addAction(tr("Save Project &As..."));
    actSaveAs->setShortcut(QKeySequence::SaveAs);
    connect(actSaveAs, &QAction::triggered, this, &MainWindow::saveProjectAs);

    auto* actExport = fileMenu->addAction(tr("&Export..."));
    actExport->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E));
    connect(actExport, &QAction::triggered, this, &MainWindow::exportProject);

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

    // ── Track ───────────────────────────────────────────────────────────
    auto* trackMenu = m_menuBar->addMenu(tr("&Track"));

    auto* actAddAudio = trackMenu->addAction(tr("Add &Audio Track"));
    actAddAudio->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A));
    connect(actAddAudio, &QAction::triggered, this, &MainWindow::addAudioTrack);

    auto* actAddVideo = trackMenu->addAction(tr("Add &Video Track"));
    actAddVideo->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V));
    connect(actAddVideo, &QAction::triggered, this, &MainWindow::addVideoTrack);

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

    auto* actStartStream = broadcastMenu->addAction(tr("Start &Streaming"));
    connect(actStartStream, &QAction::triggered, this, &MainWindow::startStreaming);

    auto* actStopStream = broadcastMenu->addAction(tr("S&top Streaming"));
    connect(actStopStream, &QAction::triggered, this, &MainWindow::stopStreaming);

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

    // Raise Video Preview as the default visible tab on the right
    m_videoDock->raise();
}

// ── Status Bar ──────────────────────────────────────────────────────────────

void MainWindow::setupStatusBar()
{
    statusBar()->showMessage(tr("Ready"));
}

// ── Signal/Slot Connections ─────────────────────────────────────────────────

void MainWindow::setupConnections()
{
    // Transport bar signals
    connect(m_transportBar, &TransportBar::playClicked, this, &MainWindow::onPlay);
    connect(m_transportBar, &TransportBar::stopClicked, this, &MainWindow::onStop);
    connect(m_transportBar, &TransportBar::recordClicked, this, &MainWindow::onRecord);

    // Sync dock visibility with View menu toggle actions
    connect(m_mixerDock, &QDockWidget::visibilityChanged,
            m_actToggleMixer, &QAction::setChecked);
    connect(m_videoDock, &QDockWidget::visibilityChanged,
            m_actToggleVideo, &QAction::setChecked);
    connect(m_mediaBrowserDock, &QDockWidget::visibilityChanged,
            m_actToggleMediaBrowser, &QAction::setChecked);
    connect(m_effectsDock, &QDockWidget::visibilityChanged,
            m_actToggleEffectsRack, &QAction::setChecked);
}

// ── File Slots ──────────────────────────────────────────────────────────────

void MainWindow::newProject()
{
    m_projectPath.clear();
    setWindowTitle(QStringLiteral("Mcaster1DAWCast \u2014 Untitled Project"));
    statusBar()->showMessage(tr("New project created"), 3000);
}

void MainWindow::openProject()
{
    QString path = QFileDialog::getOpenFileName(
        this, tr("Open Project"),
        QString(),
        tr("DAWCast Projects (*.dawcast);;All Files (*)"));

    if (!path.isEmpty()) {
        m_projectPath = path;
        QFileInfo fi(path);
        setWindowTitle(QStringLiteral("Mcaster1DAWCast \u2014 %1").arg(fi.baseName()));
        statusBar()->showMessage(tr("Opened: %1").arg(fi.fileName()), 5000);
    }
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
    QString path = QFileDialog::getSaveFileName(
        this, tr("Save Project As"),
        QString(),
        tr("DAWCast Projects (*.dawcast);;All Files (*)"));

    if (!path.isEmpty()) {
        m_projectPath = path;
        QFileInfo fi(path);
        setWindowTitle(QStringLiteral("Mcaster1DAWCast \u2014 %1").arg(fi.baseName()));
        statusBar()->showMessage(tr("Project saved as: %1").arg(fi.fileName()), 5000);
    }
}

void MainWindow::exportProject()
{
    statusBar()->showMessage(tr("Export not yet implemented"), 3000);
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

// ── Track Slots ─────────────────────────────────────────────────────────────

void MainWindow::addAudioTrack()
{
    statusBar()->showMessage(tr("Audio track added"), 3000);
}

void MainWindow::addVideoTrack()
{
    statusBar()->showMessage(tr("Video track added"), 3000);
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
    statusBar()->showMessage(tr("Streaming started"), 3000);
}

void MainWindow::stopStreaming()
{
    statusBar()->showMessage(tr("Streaming stopped"), 3000);
}

// ── Help Slots ──────────────────────────────────────────────────────────────

void MainWindow::showAbout()
{
    QMessageBox::about(
        this,
        tr("About Mcaster1DAWCast"),
        tr("<h3>Mcaster1DAWCast 1.0.0-alpha</h3>"
           "<p>Multi-Channel DAW for Broadcasting</p>"
           "<p>Copyright &copy; 2026 David St. John</p>"
           "<p><a href=\"https://mcaster1.com\">mcaster1.com</a></p>"));
}

void MainWindow::showDocumentation()
{
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://mcaster1.com/dawcast/docs")));
}

// ── Transport Slots ─────────────────────────────────────────────────────────

void MainWindow::onPlay()
{
    statusBar()->showMessage(tr("Playing"), 2000);
}

void MainWindow::onStop()
{
    statusBar()->showMessage(tr("Stopped"), 2000);
}

void MainWindow::onRecord()
{
    statusBar()->showMessage(tr("Recording"), 2000);
}

} // namespace dawcast::widgets
