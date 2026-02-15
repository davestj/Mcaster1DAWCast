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
#include <QLabel>

#include "../core/ViewModeManager.h"

namespace dawcast {
class Timeline;
class AudioEngine;
class AudioMixer;
class PlaybackEngine;
class MultitrackRecorder;
class VideoPlaybackController;
class ExportEngine;
class RTMPStreamer;
class ViewModeManager;
class UndoManager;
class WorkspaceManager;
}

namespace dawcast::ai { class AIPanel; }

namespace dawcast::widgets {

class TimelineWidget;
class TrackHeaderPanel;
class MixerWidget;
class VideoPreview;
class MediaBrowser;
class MediaLibraryWidget;
class EffectsRackWidget;
class TransportBar;
class LUFSMeterWidget;
class StreamingDialog;
class SidebarNav;
class ActionBar;
class MasterStrip;
class ViewModeSwitcher;
class ChapterWidget;
class MetadataPanel;
class SpectrumWidget;
class PianoRollWidget;
class ScriptReaderPanel;
class PedalboardWidget;
class StreamMonitorPanel;

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
    void openBatchEncoder();

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
    void toggleAIPanel();

    // View Modes
    void applyViewMode(dawcast::ViewModeManager::Mode mode);

    // Workspace Profiles
    void saveWorkspace();
    void manageWorkspaces();
    void loadWorkspaceProfile(const QString& name);

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
    void openStreamingDialog();

    // Action bar
    void loadFromLibrary();
    void openProjectBrowser();

    // Tools
    void openMassTagEditor();

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
    void setupSidebar();
    void setupCentralWidget();
    void setupDockWidgets();
    void setupStatusBar();
    void setupConnections();
    void setupViewModes();
    void setupUndoRedo();
    void rebuildWorkspaceMenu();

    // Recent files
    void updateRecentFilesMenu();
    void addToRecentFiles(const QString& path);
    static constexpr int kMaxRecentFiles = 10;

    // Window state persistence
    void saveWindowState();
    void restoreWindowState();

    QString m_projectName;

    // Central
    QSplitter*          m_centralSplitter = nullptr;
    QSplitter*          m_timelineSplitter = nullptr;
    TrackHeaderPanel*   m_trackHeaders     = nullptr;
    TimelineWidget*     m_timeline         = nullptr;

    // Dock widget contents
    MixerWidget*        m_mixer            = nullptr;
    VideoPreview*       m_videoPreview     = nullptr;
    MediaBrowser*       m_mediaBrowser     = nullptr;
    MediaLibraryWidget* m_mediaLibrary     = nullptr;
    EffectsRackWidget*  m_effectsRack      = nullptr;
    TransportBar*       m_transportBar  = nullptr;
    LUFSMeterWidget*    m_lufsMeter     = nullptr;
    dawcast::ai::AIPanel* m_aiPanel     = nullptr;
    SidebarNav*         m_sidebarNav    = nullptr;
    ActionBar*          m_actionBar     = nullptr;
    MasterStrip*        m_masterStrip   = nullptr;

    // Menu bar & toolbar
    QMenuBar*   m_menuBar   = nullptr;
    QToolBar*   m_toolBar   = nullptr;

    // Dock widgets
    QDockWidget* m_mixerDock        = nullptr;
    QDockWidget* m_videoDock        = nullptr;
    QDockWidget* m_mediaBrowserDock = nullptr;
    QDockWidget* m_effectsDock      = nullptr;
    QDockWidget* m_lufsDock         = nullptr;
    QDockWidget* m_aiDock           = nullptr;

    // View Mode system
    ViewModeSwitcher*    m_viewModeSwitcher   = nullptr;
    QToolBar*            m_viewModeToolBar    = nullptr;
    QMenu*               m_viewModeMenu       = nullptr;
    QList<QAction*>      m_viewModeActions;

    // Mode-specific dock widgets
    QDockWidget* m_chapterDock       = nullptr;
    QDockWidget* m_metadataDock      = nullptr;
    QDockWidget* m_spectrumDock      = nullptr;
    QDockWidget* m_pianoRollDock     = nullptr;
    QDockWidget* m_scriptReaderDock  = nullptr;
    QDockWidget* m_pedalboardDock    = nullptr;
    QDockWidget* m_streamMonitorDock = nullptr;

    // Mode-specific widget contents
    ChapterWidget*       m_chapterWidget     = nullptr;
    MetadataPanel*       m_metadataPanel     = nullptr;
    SpectrumWidget*      m_spectrumWidget    = nullptr;
    PianoRollWidget*     m_pianoRoll         = nullptr;
    ScriptReaderPanel*   m_scriptReader      = nullptr;
    PedalboardWidget*    m_pedalboard        = nullptr;
    StreamMonitorPanel*  m_streamMonitor     = nullptr;

    // View menu toggle actions (to sync checkmarks with dock visibility)
    QAction* m_actToggleMixer        = nullptr;
    QAction* m_actToggleVideo        = nullptr;
    QAction* m_actToggleMediaBrowser = nullptr;
    QAction* m_actToggleEffectsRack  = nullptr;
    QAction* m_actToggleLUFS         = nullptr;
    QAction* m_actToggleAI           = nullptr;

    // Recent files menu
    QMenu*          m_recentFilesMenu = nullptr;
    QList<QAction*> m_recentFileActions;

    QString m_projectPath;

    // Audio pipeline
    Timeline*                  m_timelineModel          = nullptr;
    AudioEngine*               m_audioEngine            = nullptr;
    AudioMixer*                m_audioMixer             = nullptr;
    PlaybackEngine*            m_playbackEngine         = nullptr;
    MultitrackRecorder*        m_recorder               = nullptr;
    VideoPlaybackController*   m_videoPlaybackController = nullptr;

    void setupAudioPipeline();
    void setupVideoPlayback();
    void runExportPipeline();
    void updateRecordButtonState();
    bool hasArmedTracks() const;

    ExportEngine* m_exportEngine = nullptr;

    // RTMP streaming
    RTMPStreamer*     m_rtmpStreamer      = nullptr;
    StreamingDialog*  m_streamingDialog   = nullptr;
    QLabel*           m_onAirStatusLabel  = nullptr;

    void setupRTMPStreamer();
    void updateOnAirStatus(bool live);

    // Undo/Redo
    UndoManager* m_undoManager = nullptr;
    QAction*     m_actUndo     = nullptr;
    QAction*     m_actRedo     = nullptr;

    // Workspace profiles
    QMenu*               m_workspaceLoadMenu = nullptr;
    QList<QAction*>      m_workspaceShortcuts;
};

} // namespace dawcast::widgets
