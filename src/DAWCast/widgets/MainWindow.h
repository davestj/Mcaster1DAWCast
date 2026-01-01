// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QMainWindow>
#include <QMenuBar>
#include <QToolBar>
#include <QDockWidget>
#include <QSplitter>
#include <QAction>

namespace dawcast::widgets {

class TimelineWidget;
class MixerWidget;
class VideoPreview;
class MediaBrowser;
class EffectsRackWidget;
class TransportBar;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    // File
    void newProject();
    void openProject();
    void saveProject();
    void saveProjectAs();
    void exportProject();

    // Edit
    void undo();
    void redo();
    void cut();
    void copy();
    void paste();
    void deleteSelected();

    // View
    void toggleMixer();
    void toggleVideoPreview();
    void toggleMediaBrowser();
    void toggleEffectsRack();

    // Track
    void addAudioTrack();
    void addVideoTrack();
    void removeTrack();

    // Podcast
    void openChapterEditor();
    void openMetadata();
    void exportEpisode();
    void generateRSS();

    // Broadcast
    void startRecording();
    void stopRecording();
    void startStreaming();
    void stopStreaming();

    // Help
    void showAbout();
    void showDocumentation();

    // Transport
    void onPlay();
    void onStop();
    void onRecord();

private:
    void setupMenus();
    void setupToolbars();
    void setupCentralWidget();
    void setupDockWidgets();
    void setupStatusBar();
    void setupConnections();

    // Central
    QSplitter*          m_centralSplitter = nullptr;
    TimelineWidget*     m_timeline        = nullptr;

    // Dock widget contents
    MixerWidget*        m_mixer         = nullptr;
    VideoPreview*       m_videoPreview  = nullptr;
    MediaBrowser*       m_mediaBrowser  = nullptr;
    EffectsRackWidget*  m_effectsRack   = nullptr;
    TransportBar*       m_transportBar  = nullptr;

    // Menu bar & toolbar
    QMenuBar*   m_menuBar   = nullptr;
    QToolBar*   m_toolBar   = nullptr;

    // Dock widgets
    QDockWidget* m_mixerDock        = nullptr;
    QDockWidget* m_videoDock        = nullptr;
    QDockWidget* m_mediaBrowserDock = nullptr;
    QDockWidget* m_effectsDock      = nullptr;

    // View menu toggle actions (to sync checkmarks with dock visibility)
    QAction* m_actToggleMixer        = nullptr;
    QAction* m_actToggleVideo        = nullptr;
    QAction* m_actToggleMediaBrowser = nullptr;
    QAction* m_actToggleEffectsRack  = nullptr;

    QString m_projectPath;
};

} // namespace dawcast::widgets
