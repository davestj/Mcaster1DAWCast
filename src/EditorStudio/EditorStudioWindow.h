// DAWCast Editor Studio — Forensic-Grade Single-File Audio/Video Editor
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QListWidget>
#include <QDockWidget>
#include <QToolBar>
#include <QAction>
#include <QMenu>
#include <QSplitter>
#include <QStatusBar>
#include <QScrollArea>
#include <QList>
#include <QImage>

#include "ForensicWaveformView.h"
#include "SpectralAnalysisPanel.h"
#include "ForensicDetector.h"
#include "PlayerControls.h"

namespace dawcast::editor {

/// Main window for DAWCast Editor Studio — forensic-grade single-file editor.
/// Provides full menu bar, toolbar with transport controls, central waveform view
/// with extreme zoom, bottom spectrogram dock, right-side info/markers panel,
/// and optional video frame viewer for video files.
class EditorStudioWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit EditorStudioWindow(QWidget* parent = nullptr);
    ~EditorStudioWindow() override;

    /// Open a media file for analysis.
    void openFile(const QString& path);

protected:
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    // ── File actions ─────────────────────────────────────────────────────
    void onOpen();
    void onSaveAs();
    void onExport();
    void onClose();

    // ── Edit actions ─────────────────────────────────────────────────────
    void onSelectAll();

    // ── View actions ─────────────────────────────────────────────────────
    void onZoomIn();
    void onZoomOut();
    void onZoomToFit();
    void onZoomToSelection();

    // ── Analysis actions ─────────────────────────────────────────────────
    void onSpectralView();
    void onFrequencyDetect();
    void onPhaseAnalysis();
    void onAnomalyScan();

    // ── Forensic actions ─────────────────────────────────────────────────
    void onEVPDetection();
    void onInfrasonicScan();
    void onUltrasonicScan();
    void onParanormalMode();
    void onOrbDetection();

    // ── Internal ─────────────────────────────────────────────────────────
    void onFileLoaded(const QString& path, int64_t frames, int channels, int sampleRate);
    void onSelectionChanged(int64_t startSample, int64_t endSample);
    void onPositionChanged(int64_t samplePosition);
    void onZoomChanged(double samplesPerPixel);
    void onMarkersUpdated(const QList<DetectionMarker>& markers);
    void onMarkerListClicked(int row);

private:
    void createMenuBar();
    void createToolBar();
    void createCentralWidget();
    void createSpectralDock();
    void createInfoPanel();
    void createVideoPanel();
    void updateFileInfo();
    void updateSelectionInfo();
    void updateMarkersList();
    void decodeVideoFrames(const QString& path);

    QString formatTime(int64_t samples, int sampleRate) const;
    QString formatFileSize(qint64 bytes) const;

    // ── Core widgets ─────────────────────────────────────────────────────
    ForensicWaveformView*  m_waveformView   = nullptr;
    SpectralAnalysisPanel* m_spectralPanel   = nullptr;
    PlayerControls*        m_playerControls  = nullptr;

    // ── Docks ────────────────────────────────────────────────────────────
    QDockWidget* m_spectralDock = nullptr;
    QDockWidget* m_infoDock     = nullptr;
    QDockWidget* m_videoDock    = nullptr;

    // ── Right panel: info + markers ──────────────────────────────────────
    QLabel*      m_fileInfoLabel     = nullptr;
    QLabel*      m_selectionInfoLabel = nullptr;
    QListWidget* m_markerList        = nullptr;

    // ── Video frame viewer ───────────────────────────────────────────────
    QLabel*      m_videoFrameLabel = nullptr;
    QScrollArea* m_videoScroll     = nullptr;
    QList<QImage> m_videoFrames;
    int           m_currentVideoFrame = 0;

    // ── State ────────────────────────────────────────────────────────────
    QString m_currentFilePath;
    bool    m_hasVideo = false;
};

} // namespace dawcast::editor
