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
#include <QCloseEvent>
#include <QList>
#include <QMenu>

namespace dawcast {
class Timeline;
class AudioEngine;
class AudioMixer;
class PlaybackEngine;
class VideoPlaybackController;
class ExportEngine;
}

namespace dawcast::widgets {

class TimelineWidget;
class MixerWidget;
class VideoPreview;
class MediaBrowser;
class EffectsRackWidget;
class TransportBar;
class LUFSMeterWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    // File
    void newProject();
    void openProject();
    void openProject(const QString& path);
    void saveProject();
    void saveProjectAs();
    void exportProject();

    // Recent files
    void openRecentFile();
    void clearRecentFiles();

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
    void addMidiTrack();
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

    // Recent files
    void updateRecentFilesMenu();
    void addToRecentFiles(const QString& path);
    static constexpr int kMaxRecentFiles = 10;

    // Window state persistence
    void saveWindowState();
    void restoreWindowState();

    // Central
    QSplitter*          m_centralSplitter = nullptr;
    TimelineWidget*     m_timeline        = nullptr;

    // Dock widget contents
    MixerWidget*        m_mixer         = nullptr;
    VideoPreview*       m_videoPreview  = nullptr;
    MediaBrowser*       m_mediaBrowser  = nullptr;
    EffectsRackWidget*  m_effectsRack   = nullptr;
    TransportBar*       m_transportBar  = nullptr;
    LUFSMeterWidget*    m_lufsMeter     = nullptr;

    // Menu bar & toolbar
    QMenuBar*   m_menuBar   = nullptr;
    QToolBar*   m_toolBar   = nullptr;

    // Dock widgets
    QDockWidget* m_mixerDock        = nullptr;
    QDockWidget* m_videoDock        = nullptr;
    QDockWidget* m_mediaBrowserDock = nullptr;
    QDockWidget* m_effectsDock      = nullptr;
    QDockWidget* m_lufsDock         = nullptr;

    // View menu toggle actions (to sync checkmarks with dock visibility)
    QAction* m_actToggleMixer        = nullptr;
    QAction* m_actToggleVideo        = nullptr;
    QAction* m_actToggleMediaBrowser = nullptr;
    QAction* m_actToggleEffectsRack  = nullptr;
    QAction* m_actToggleLUFS         = nullptr;

    // Recent files menu
    QMenu*          m_recentFilesMenu = nullptr;
    QList<QAction*> m_recentFileActions;

    QString m_projectPath;

    // Audio pipeline
    Timeline*                  m_timelineModel          = nullptr;
    AudioEngine*               m_audioEngine            = nullptr;
    AudioMixer*                m_audioMixer             = nullptr;
    PlaybackEngine*            m_playbackEngine         = nullptr;
    VideoPlaybackController*   m_videoPlaybackController = nullptr;

    void setupAudioPipeline();
    void setupVideoPlayback();
    void runExportPipeline();

    ExportEngine* m_exportEngine = nullptr;
};

} // namespace dawcast::widgets
