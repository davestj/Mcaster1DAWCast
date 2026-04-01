// DAWCast Editor Studio — Forensic-Grade Single-File Audio/Video Editor
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "EditorStudioWindow.h"
#include "../DAWCast/codec/FFmpegCodec.h"
#include "../DAWCast/codec/WavCodec.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QDebug>

#ifdef HAVE_AVFORMAT
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}
#endif

namespace dawcast::editor {

EditorStudioWindow::EditorStudioWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("DAWCast Editor Studio"));
    setMinimumSize(1100, 700);
    resize(1400, 900);
    setAcceptDrops(true);

    createCentralWidget();
    createSpectralDock();
    createInfoPanel();
    createVideoPanel();
    createMenuBar();
    createToolBar();

    // Status bar
    statusBar()->showMessage(QStringLiteral("Ready"));

    // Connect signals
    connect(m_waveformView, &ForensicWaveformView::fileLoaded,
            this, &EditorStudioWindow::onFileLoaded);
    connect(m_waveformView, &ForensicWaveformView::selectionChanged,
            this, &EditorStudioWindow::onSelectionChanged);
    connect(m_waveformView, &ForensicWaveformView::positionChanged,
            this, &EditorStudioWindow::onPositionChanged);
    connect(m_waveformView, &ForensicWaveformView::zoomChanged,
            this, &EditorStudioWindow::onZoomChanged);
    connect(m_waveformView, &ForensicWaveformView::markersUpdated,
            this, &EditorStudioWindow::onMarkersUpdated);

    connect(m_spectralPanel, &SpectralAnalysisPanel::markerClicked,
            this, [this](const DetectionMarker& marker) {
                // Jump waveform to marker position
                m_waveformView->setSelection(marker.positionSamples,
                                              marker.positionSamples + 4096);
            });
    connect(m_spectralPanel, &SpectralAnalysisPanel::analysisComplete,
            this, &EditorStudioWindow::onMarkersUpdated);

    connect(m_markerList, &QListWidget::currentRowChanged,
            this, &EditorStudioWindow::onMarkerListClicked);
}

EditorStudioWindow::~EditorStudioWindow() = default;

// ── Central widget ─────────────────────────────────────────────────────────

void EditorStudioWindow::createCentralWidget()
{
    m_waveformView = new ForensicWaveformView(this);
    setCentralWidget(m_waveformView);
}

// ── Spectral dock (bottom) ─────────────────────────────────────────────────

void EditorStudioWindow::createSpectralDock()
{
    m_spectralPanel = new SpectralAnalysisPanel();
    m_spectralPanel->setMinimumHeight(150);

    m_spectralDock = new QDockWidget(QStringLiteral("Spectral Analysis"), this);
    m_spectralDock->setWidget(m_spectralPanel);
    m_spectralDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, m_spectralDock);
}

// ── Info panel (right) ─────────────────────────────────────────────────────

void EditorStudioWindow::createInfoPanel()
{
    auto* container = new QWidget();
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(8, 8, 8, 8);

    // File info section
    auto* fileInfoHeader = new QLabel(QStringLiteral("<b>File Info</b>"));
    m_fileInfoLabel = new QLabel(QStringLiteral("No file loaded"));
    m_fileInfoLabel->setWordWrap(true);
    m_fileInfoLabel->setTextFormat(Qt::PlainText);

    // Selection info section
    auto* selInfoHeader = new QLabel(QStringLiteral("<b>Selection</b>"));
    m_selectionInfoLabel = new QLabel(QStringLiteral("No selection"));
    m_selectionInfoLabel->setWordWrap(true);

    // Markers list
    auto* markersHeader = new QLabel(QStringLiteral("<b>Detection Markers</b>"));
    m_markerList = new QListWidget();
    m_markerList->setAlternatingRowColors(true);
    m_markerList->setMaximumWidth(320);

    layout->addWidget(fileInfoHeader);
    layout->addWidget(m_fileInfoLabel);
    layout->addSpacing(12);
    layout->addWidget(selInfoHeader);
    layout->addWidget(m_selectionInfoLabel);
    layout->addSpacing(12);
    layout->addWidget(markersHeader);
    layout->addWidget(m_markerList, 1);

    m_infoDock = new QDockWidget(QStringLiteral("Info && Markers"), this);
    m_infoDock->setWidget(container);
    m_infoDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
    m_infoDock->setMinimumWidth(220);
    addDockWidget(Qt::RightDockWidgetArea, m_infoDock);
}

// ── Video panel (right, below info) ────────────────────────────────────────

void EditorStudioWindow::createVideoPanel()
{
    m_videoFrameLabel = new QLabel();
    m_videoFrameLabel->setAlignment(Qt::AlignCenter);
    m_videoFrameLabel->setMinimumSize(200, 150);
    m_videoFrameLabel->setText(QStringLiteral("No video"));
    m_videoFrameLabel->setStyleSheet(
        QStringLiteral("QLabel { background: #1a1a1e; color: #666; }"));

    m_videoScroll = new QScrollArea();
    m_videoScroll->setWidget(m_videoFrameLabel);
    m_videoScroll->setWidgetResizable(true);

    m_videoDock = new QDockWidget(QStringLiteral("Video Frames"), this);
    m_videoDock->setWidget(m_videoScroll);
    m_videoDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, m_videoDock);
    m_videoDock->hide(); // Hidden until a video file is opened
}

// ── Menu bar ───────────────────────────────────────────────────────────────

void EditorStudioWindow::createMenuBar()
{
    auto* mb = menuBar();

    // ── File ──
    auto* fileMenu = mb->addMenu(QStringLiteral("&File"));
    fileMenu->addAction(QStringLiteral("&Open..."), this, &EditorStudioWindow::onOpen,
                        QKeySequence::Open);
    fileMenu->addAction(QStringLiteral("Save &As..."), this, &EditorStudioWindow::onSaveAs,
                        QKeySequence::SaveAs);
    fileMenu->addAction(QStringLiteral("&Export..."), this, &EditorStudioWindow::onExport,
                        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E));
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("&Close"), this, &EditorStudioWindow::onClose,
                        QKeySequence::Close);

    // ── Edit ──
    auto* editMenu = mb->addMenu(QStringLiteral("&Edit"));
    editMenu->addAction(QStringLiteral("&Undo"), this, [](){},
                        QKeySequence::Undo);
    editMenu->addAction(QStringLiteral("&Redo"), this, [](){},
                        QKeySequence::Redo);
    editMenu->addSeparator();
    editMenu->addAction(QStringLiteral("Cu&t"), this, [](){},
                        QKeySequence::Cut);
    editMenu->addAction(QStringLiteral("&Copy"), this, [](){},
                        QKeySequence::Copy);
    editMenu->addAction(QStringLiteral("&Paste"), this, [](){},
                        QKeySequence::Paste);
    editMenu->addSeparator();
    editMenu->addAction(QStringLiteral("Select &All"), this,
                        &EditorStudioWindow::onSelectAll, QKeySequence::SelectAll);

    // ── View ──
    auto* viewMenu = mb->addMenu(QStringLiteral("&View"));
    viewMenu->addAction(QStringLiteral("Zoom &In"), this,
                        &EditorStudioWindow::onZoomIn,
                        QKeySequence(Qt::CTRL | Qt::Key_Plus));
    viewMenu->addAction(QStringLiteral("Zoom &Out"), this,
                        &EditorStudioWindow::onZoomOut,
                        QKeySequence(Qt::CTRL | Qt::Key_Minus));
    viewMenu->addAction(QStringLiteral("Zoom to &Fit"), this,
                        &EditorStudioWindow::onZoomToFit,
                        QKeySequence(Qt::CTRL | Qt::Key_0));
    viewMenu->addAction(QStringLiteral("Zoom to &Selection"), this,
                        &EditorStudioWindow::onZoomToSelection,
                        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    viewMenu->addSeparator();
    viewMenu->addAction(m_spectralDock->toggleViewAction());
    viewMenu->addAction(m_infoDock->toggleViewAction());
    viewMenu->addAction(m_videoDock->toggleViewAction());

    // ── Analysis ──
    auto* analysisMenu = mb->addMenu(QStringLiteral("&Analysis"));
    analysisMenu->addAction(QStringLiteral("&Spectral View"), this,
                            &EditorStudioWindow::onSpectralView);
    analysisMenu->addAction(QStringLiteral("&Frequency Detect..."), this,
                            &EditorStudioWindow::onFrequencyDetect);
    analysisMenu->addAction(QStringLiteral("&Phase Analysis"), this,
                            &EditorStudioWindow::onPhaseAnalysis);
    analysisMenu->addAction(QStringLiteral("&Anomaly Scan"), this,
                            &EditorStudioWindow::onAnomalyScan);

    // ── Forensic ──
    auto* forensicMenu = mb->addMenu(QStringLiteral("F&orensic"));
    forensicMenu->addAction(QStringLiteral("&EVP Detection"), this,
                            &EditorStudioWindow::onEVPDetection);
    forensicMenu->addAction(QStringLiteral("&Infrasonic Scan"), this,
                            &EditorStudioWindow::onInfrasonicScan);
    forensicMenu->addAction(QStringLiteral("&Ultrasonic Scan"), this,
                            &EditorStudioWindow::onUltrasonicScan);
    forensicMenu->addSeparator();
    forensicMenu->addAction(QStringLiteral("&Paranormal Mode"), this,
                            &EditorStudioWindow::onParanormalMode);
    forensicMenu->addAction(QStringLiteral("&Orb Detection"), this,
                            &EditorStudioWindow::onOrbDetection);

    // ── Help ──
    auto* helpMenu = mb->addMenu(QStringLiteral("&Help"));
    helpMenu->addAction(QStringLiteral("&About DAWCast Editor Studio"), this, [this]() {
        QMessageBox::about(this, QStringLiteral("About DAWCast Editor Studio"),
            QStringLiteral(
                "<h3>DAWCast Editor Studio 1.0.0-alpha</h3>"
                "<p>Forensic-grade single-file audio/video editor.</p>"
                "<p>Part of the Mcaster1DAWCast suite.</p>"
                "<p>Copyright &copy; 2026 David St. John</p>"
                "<p>Licensed under GPL-2.0-or-later</p>"));
    });
}

// ── Toolbar ────────────────────────────────────────────────────────────────

void EditorStudioWindow::createToolBar()
{
    auto* toolbar = addToolBar(QStringLiteral("Main"));
    toolbar->setMovable(false);
    toolbar->setIconSize(QSize(20, 20));

    // Open
    toolbar->addAction(QStringLiteral("Open"), this, &EditorStudioWindow::onOpen);

    toolbar->addSeparator();

    // Transport controls
    auto* rewindAction = toolbar->addAction(QStringLiteral("|<"), this, [this]() {
        if (m_waveformView->hasFile()) {
            m_waveformView->stop();
        }
    });
    rewindAction->setToolTip(QStringLiteral("Rewind to start"));

    m_playAction = toolbar->addAction(QStringLiteral("Play"), this, [this]() {
        m_waveformView->play();
    });
    m_playAction->setToolTip(QStringLiteral("Play (Space)"));

    m_pauseAction = toolbar->addAction(QStringLiteral("Pause"), this, [this]() {
        m_waveformView->pause();
    });
    m_pauseAction->setToolTip(QStringLiteral("Pause"));

    m_stopAction = toolbar->addAction(QStringLiteral("Stop"), this, [this]() {
        m_waveformView->stop();
    });
    m_stopAction->setToolTip(QStringLiteral("Stop"));

    toolbar->addSeparator();

    // Zoom controls
    toolbar->addAction(QStringLiteral("Zoom +"), this, &EditorStudioWindow::onZoomIn);
    toolbar->addAction(QStringLiteral("Zoom -"), this, &EditorStudioWindow::onZoomOut);
    toolbar->addAction(QStringLiteral("Fit"), this, &EditorStudioWindow::onZoomToFit);
    toolbar->addAction(QStringLiteral("Sel"), this, &EditorStudioWindow::onZoomToSelection);
}

// ── File operations ────────────────────────────────────────────────────────

void EditorStudioWindow::openFile(const QString& path)
{
    if (path.isEmpty()) return;

    statusBar()->showMessage(QStringLiteral("Loading: ") + path);
    QApplication::processEvents();

    m_currentFilePath = path;
    m_waveformView->loadFile(path);

    // Check if file has video (by extension heuristic and FFmpeg probe)
    QFileInfo fi(path);
    QString ext = fi.suffix().toLower();
    bool mayBeVideo = (ext == QLatin1String("mp4") || ext == QLatin1String("mov") ||
                       ext == QLatin1String("avi") || ext == QLatin1String("mkv") ||
                       ext == QLatin1String("webm") || ext == QLatin1String("flv") ||
                       ext == QLatin1String("wmv") || ext == QLatin1String("m4v"));
    if (mayBeVideo) {
        decodeVideoFrames(path);
    } else {
        m_hasVideo = false;
        m_videoFrames.clear();
        m_videoDock->hide();
    }
}

void EditorStudioWindow::onOpen()
{
    QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Open Media File"),
        QString(),
        QStringLiteral(
            "All Media Files (*.wav *.mp3 *.flac *.ogg *.opus *.aac *.m4a *.wma "
            "*.aiff *.mp4 *.mov *.avi *.mkv *.webm *.flv);;"
            "Audio Files (*.wav *.mp3 *.flac *.ogg *.opus *.aac *.m4a *.wma *.aiff);;"
            "Video Files (*.mp4 *.mov *.avi *.mkv *.webm *.flv);;"
            "All Files (*)"));

    if (!path.isEmpty()) {
        openFile(path);
    }
}

void EditorStudioWindow::onSaveAs()
{
    if (!m_waveformView->hasFile()) return;

    QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Save As"),
        QString(),
        QStringLiteral("WAV Files (*.wav);;All Files (*)"));

    if (path.isEmpty()) return;

    dawcast::AudioBuffer buf;
    buf.data       = const_cast<float*>(m_waveformView->audioData());
    buf.frames     = static_cast<int>(m_waveformView->audioFrames());
    buf.channels   = m_waveformView->audioChannels();
    buf.sampleRate = m_waveformView->audioSampleRate();

    dawcast::WavCodec wav;
    if (wav.encode(buf, path, 32)) {
        statusBar()->showMessage(QStringLiteral("Saved: ") + path, 3000);
    } else {
        QMessageBox::warning(this, QStringLiteral("Save Failed"),
                             QStringLiteral("Could not save file to: ") + path);
    }

    // Do NOT delete buf.data — it belongs to the waveform view
    buf.data = nullptr;
}

void EditorStudioWindow::onExport()
{
    if (!m_waveformView->hasFile()) return;

    // For now, export selected region or full file as WAV
    QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Export Audio"),
        QString(),
        QStringLiteral(
            "WAV Files (*.wav);;FLAC Files (*.flac);;MP3 Files (*.mp3);;All Files (*)"));

    if (path.isEmpty()) return;

    // Determine export range
    int64_t startSample = 0;
    int64_t endSample = m_waveformView->audioFrames();
    if (m_waveformView->hasSelection()) {
        startSample = m_waveformView->selectionStart();
        endSample   = m_waveformView->selectionEnd();
    }

    int channels = m_waveformView->audioChannels();
    int64_t exportFrames = endSample - startSample;

    // Create export buffer
    dawcast::AudioBuffer buf;
    buf.data = const_cast<float*>(
        m_waveformView->audioData() + startSample * channels);
    buf.frames     = static_cast<int>(exportFrames);
    buf.channels   = channels;
    buf.sampleRate = m_waveformView->audioSampleRate();

    QFileInfo fi(path);
    QString ext = fi.suffix().toLower();

    bool ok = false;
    if (ext == QLatin1String("wav")) {
        dawcast::WavCodec wav;
        ok = wav.encode(buf, path, 32);
    } else {
        // Use FFmpegCodec for other formats
        dawcast::FFmpegCodec codec;
        QString codecName;
        int bitrate = 192;
        if (ext == QLatin1String("flac"))
            codecName = QStringLiteral("flac");
        else if (ext == QLatin1String("mp3"))
            codecName = QStringLiteral("libmp3lame");
        else
            codecName = QStringLiteral("pcm_f32le");

        ok = codec.encode(buf, path, codecName, bitrate);
    }

    buf.data = nullptr; // Do NOT free — owned by waveform view

    if (ok) {
        statusBar()->showMessage(QStringLiteral("Exported: ") + path, 3000);
    } else {
        QMessageBox::warning(this, QStringLiteral("Export Failed"),
                             QStringLiteral("Could not export to: ") + path);
    }
}

void EditorStudioWindow::onClose()
{
    close();
}

// ── Edit actions ───────────────────────────────────────────────────────────

void EditorStudioWindow::onSelectAll()
{
    if (m_waveformView->hasFile()) {
        m_waveformView->setSelection(0, m_waveformView->audioFrames());
    }
}

// ── View actions ───────────────────────────────────────────────────────────

void EditorStudioWindow::onZoomIn()      { m_waveformView->zoomIn(); }
void EditorStudioWindow::onZoomOut()     { m_waveformView->zoomOut(); }
void EditorStudioWindow::onZoomToFit()   { m_waveformView->zoomToFit(); }
void EditorStudioWindow::onZoomToSelection() { m_waveformView->zoomToSelection(); }

// ── Analysis actions ───────────────────────────────────────────────────────

void EditorStudioWindow::onSpectralView()
{
    m_waveformView->setOverlayMode(OverlayMode::SpectralView);
    statusBar()->showMessage(QStringLiteral("Spectral View overlay enabled"), 2000);
}

void EditorStudioWindow::onFrequencyDetect()
{
    if (!m_waveformView->hasFile()) return;

    bool ok = false;
    double minFreq = QInputDialog::getDouble(
        this, QStringLiteral("Frequency Detection"),
        QStringLiteral("Minimum frequency (Hz):"),
        200.0, 0.5, 100000.0, 1, &ok);
    if (!ok) return;

    double maxFreq = QInputDialog::getDouble(
        this, QStringLiteral("Frequency Detection"),
        QStringLiteral("Maximum frequency (Hz):"),
        4000.0, 0.5, 100000.0, 1, &ok);
    if (!ok) return;

    double threshold = QInputDialog::getDouble(
        this, QStringLiteral("Frequency Detection"),
        QStringLiteral("Threshold (dB):"),
        -40.0, -120.0, 0.0, 1, &ok);
    if (!ok) return;

    statusBar()->showMessage(QStringLiteral("Running frequency detection..."));
    QApplication::processEvents();

    auto markers = m_spectralPanel->runFrequencyDetection(minFreq, maxFreq, threshold);
    m_waveformView->setMarkers(markers);
    m_waveformView->setOverlayMode(OverlayMode::FrequencyDetect);
    updateMarkersList();

    statusBar()->showMessage(
        QString("Frequency detection complete: %1 markers found").arg(markers.size()),
        3000);
}

void EditorStudioWindow::onPhaseAnalysis()
{
    m_waveformView->setOverlayMode(OverlayMode::PhaseAnalysis);
    updateMarkersList();
    statusBar()->showMessage(
        QString("Phase analysis: %1 anomalies found")
            .arg(m_waveformView->markers().size()), 3000);
}

void EditorStudioWindow::onAnomalyScan()
{
    if (!m_waveformView->hasFile()) return;

    statusBar()->showMessage(QStringLiteral("Running anomaly scan..."));
    QApplication::processEvents();

    auto markers = m_spectralPanel->runAnomalyDetection();
    m_waveformView->setMarkers(markers);
    m_waveformView->setOverlayMode(OverlayMode::AnomalyScan);
    updateMarkersList();

    statusBar()->showMessage(
        QString("Anomaly scan complete: %1 anomalies found").arg(markers.size()),
        3000);
}

// ── Forensic actions ───────────────────────────────────────────────────────

void EditorStudioWindow::onEVPDetection()
{
    if (!m_waveformView->hasFile()) return;

    statusBar()->showMessage(QStringLiteral("Running EVP detection..."));
    QApplication::processEvents();

    auto markers = m_spectralPanel->runEVPDetection();
    m_waveformView->setMarkers(markers);
    m_waveformView->setOverlayMode(OverlayMode::EVPDetection);
    updateMarkersList();

    statusBar()->showMessage(
        QString("EVP detection complete: %1 potential EVPs found")
            .arg(markers.size()), 3000);
}

void EditorStudioWindow::onInfrasonicScan()
{
    if (!m_waveformView->hasFile()) return;

    statusBar()->showMessage(QStringLiteral("Running infrasonic scan (0.5-20 Hz)..."));
    QApplication::processEvents();

    auto markers = m_spectralPanel->runInfrasonicScan();
    m_waveformView->setMarkers(markers);
    m_waveformView->setOverlayMode(OverlayMode::InfrasonicScan);
    updateMarkersList();

    statusBar()->showMessage(
        QString("Infrasonic scan complete: %1 events found")
            .arg(markers.size()), 3000);
}

void EditorStudioWindow::onUltrasonicScan()
{
    if (!m_waveformView->hasFile()) return;

    statusBar()->showMessage(QStringLiteral("Running ultrasonic scan (18kHz+)..."));
    QApplication::processEvents();

    auto markers = m_spectralPanel->runUltrasonicScan();
    m_waveformView->setMarkers(markers);
    m_waveformView->setOverlayMode(OverlayMode::UltrasonicScan);
    updateMarkersList();

    statusBar()->showMessage(
        QString("Ultrasonic scan complete: %1 events found")
            .arg(markers.size()), 3000);
}

void EditorStudioWindow::onParanormalMode()
{
    if (!m_waveformView->hasFile()) return;

    statusBar()->showMessage(
        QStringLiteral("PARANORMAL MODE: Running all forensic detectors..."));
    QApplication::processEvents();

    m_waveformView->setOverlayMode(OverlayMode::ParanormalMode);
    updateMarkersList();

    int count = m_waveformView->markers().size();
    statusBar()->showMessage(
        QString("PARANORMAL MODE active: %1 total anomalies detected").arg(count),
        5000);
}

void EditorStudioWindow::onOrbDetection()
{
    if (m_videoFrames.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Orb Detection"),
            QStringLiteral("No video frames available. Open a video file first."));
        return;
    }

    statusBar()->showMessage(QStringLiteral("Running orb detection on video frames..."));
    QApplication::processEvents();

    ForensicDetector detector;
    auto markers = detector.detectOrbs(m_videoFrames, 70.0);

    m_waveformView->setMarkers(markers);
    m_waveformView->setOverlayMode(OverlayMode::OrbDetection);
    updateMarkersList();

    statusBar()->showMessage(
        QString("Orb detection complete: %1 potential orbs found")
            .arg(markers.size()), 5000);
}

// ── Signal handlers ────────────────────────────────────────────────────────

void EditorStudioWindow::onFileLoaded(const QString& path, int64_t frames,
                                       int channels, int sampleRate)
{
    Q_UNUSED(path)

    // Share audio data with spectral panel
    m_spectralPanel->setAudioData(m_waveformView->audioData(),
                                   frames, channels, sampleRate);

    updateFileInfo();
    statusBar()->showMessage(QStringLiteral("File loaded: ") +
                             QFileInfo(path).fileName(), 3000);
}

void EditorStudioWindow::onSelectionChanged(int64_t /*startSample*/,
                                             int64_t /*endSample*/)
{
    updateSelectionInfo();
}

void EditorStudioWindow::onPositionChanged(int64_t samplePosition)
{
    m_spectralPanel->setPosition(samplePosition);

    // Update video frame if video is loaded
    if (m_hasVideo && !m_videoFrames.isEmpty() && m_waveformView->audioSampleRate() > 0) {
        double timeSeconds = static_cast<double>(samplePosition) /
            static_cast<double>(m_waveformView->audioSampleRate());
        // Approximate frame index (assume 30fps if unknown)
        int frameIdx = static_cast<int>(timeSeconds * 30.0);
        frameIdx = qBound(0, frameIdx, static_cast<int>(m_videoFrames.size()) - 1);
        if (frameIdx != m_currentVideoFrame) {
            m_currentVideoFrame = frameIdx;
            QPixmap pix = QPixmap::fromImage(
                m_videoFrames[frameIdx].scaled(
                    m_videoFrameLabel->width(), m_videoFrameLabel->height(),
                    Qt::KeepAspectRatio, Qt::SmoothTransformation));
            m_videoFrameLabel->setPixmap(pix);
        }
    }
}

void EditorStudioWindow::onZoomChanged(double samplesPerPixel)
{
    QString zoomText;
    if (samplesPerPixel < 1.0) {
        zoomText = QString("%1x sample zoom").arg(1.0 / samplesPerPixel, 0, 'f', 1);
    } else {
        zoomText = QString("%1 samples/px").arg(samplesPerPixel, 0, 'f', 0);
    }
    statusBar()->showMessage(QStringLiteral("Zoom: ") + zoomText, 1500);
}

void EditorStudioWindow::onMarkersUpdated(const QList<DetectionMarker>& markers)
{
    Q_UNUSED(markers)
    updateMarkersList();
}

void EditorStudioWindow::onMarkerListClicked(int row)
{
    auto allMarkers = m_waveformView->markers();
    allMarkers.append(m_spectralPanel->markers());

    if (row >= 0 && row < allMarkers.size()) {
        const auto& marker = allMarkers.at(row);
        if (marker.frameIndex >= 0 && m_hasVideo) {
            // Video marker — show that frame
            int fi = qBound(0, marker.frameIndex,
                           static_cast<int>(m_videoFrames.size()) - 1);
            m_currentVideoFrame = fi;
            QPixmap pix = QPixmap::fromImage(
                m_videoFrames[fi].scaled(
                    m_videoFrameLabel->width(), m_videoFrameLabel->height(),
                    Qt::KeepAspectRatio, Qt::SmoothTransformation));
            m_videoFrameLabel->setPixmap(pix);
        } else {
            // Audio marker — navigate waveform
            m_waveformView->setSelection(
                marker.positionSamples,
                marker.positionSamples + 4096);
        }
    }
}

// ── Info updates ───────────────────────────────────────────────────────────

void EditorStudioWindow::updateFileInfo()
{
    if (!m_waveformView->hasFile()) {
        m_fileInfoLabel->setText(QStringLiteral("No file loaded"));
        setWindowTitle(QStringLiteral("DAWCast Editor Studio"));
        return;
    }

    QFileInfo fi(m_waveformView->filePath());
    int64_t frames = m_waveformView->audioFrames();
    int channels   = m_waveformView->audioChannels();
    int sr         = m_waveformView->audioSampleRate();
    double duration = static_cast<double>(frames) / static_cast<double>(sr);

    QString info = QString(
        "File: %1\n"
        "Size: %2\n"
        "Duration: %3\n"
        "Sample Rate: %4 Hz\n"
        "Channels: %5\n"
        "Total Frames: %6\n"
        "Bit Depth: 32-bit float (decoded)")
        .arg(fi.fileName())
        .arg(formatFileSize(fi.size()))
        .arg(formatTime(frames, sr))
        .arg(sr)
        .arg(channels)
        .arg(frames);

    if (m_hasVideo) {
        info += QString("\nVideo: %1 frames decoded")
                    .arg(m_videoFrames.size());
    }

    m_fileInfoLabel->setText(info);
    setWindowTitle(QStringLiteral("DAWCast Editor Studio - ") + fi.fileName());
}

void EditorStudioWindow::updateSelectionInfo()
{
    if (!m_waveformView->hasSelection()) {
        m_selectionInfoLabel->setText(QStringLiteral("No selection"));
        return;
    }

    int64_t start = m_waveformView->selectionStart();
    int64_t end   = m_waveformView->selectionEnd();
    int sr        = m_waveformView->audioSampleRate();
    int64_t len   = end - start;

    QString info = QString(
        "Start: %1 (%2)\n"
        "End: %3 (%4)\n"
        "Length: %5 (%6 samples)")
        .arg(formatTime(start, sr))
        .arg(start)
        .arg(formatTime(end, sr))
        .arg(end)
        .arg(formatTime(len, sr))
        .arg(len);

    m_selectionInfoLabel->setText(info);
}

void EditorStudioWindow::updateMarkersList()
{
    m_markerList->clear();
    auto allMarkers = m_waveformView->markers();
    // Also include spectral panel markers (may overlap, but that is fine for display)

    for (const auto& marker : allMarkers) {
        auto* item = new QListWidgetItem(marker.label);
        item->setForeground(marker.color);
        m_markerList->addItem(item);
    }
}

// ── Video frame decoding ───────────────────────────────────────────────────

void EditorStudioWindow::decodeVideoFrames(const QString& path)
{
#ifdef HAVE_AVFORMAT
    m_videoFrames.clear();
    m_hasVideo = false;

    AVFormatContext* fmtCtx = nullptr;
    int ret = avformat_open_input(&fmtCtx, path.toUtf8().constData(), nullptr, nullptr);
    if (ret < 0) return;

    ret = avformat_find_stream_info(fmtCtx, nullptr);
    if (ret < 0) {
        avformat_close_input(&fmtCtx);
        return;
    }

    int videoIdx = av_find_best_stream(fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (videoIdx < 0) {
        avformat_close_input(&fmtCtx);
        return;
    }

    AVStream* stream = fmtCtx->streams[videoIdx];
    const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) {
        avformat_close_input(&fmtCtx);
        return;
    }

    AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecCtx, stream->codecpar);
    if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmtCtx);
        return;
    }

    // Set up scaler to convert to RGB32
    SwsContext* swsCtx = sws_getContext(
        codecCtx->width, codecCtx->height, codecCtx->pix_fmt,
        codecCtx->width, codecCtx->height, AV_PIX_FMT_RGB32,
        SWS_BILINEAR, nullptr, nullptr, nullptr);

    AVFrame* frame = av_frame_alloc();
    AVFrame* rgbFrame = av_frame_alloc();
    AVPacket* pkt = av_packet_alloc();

    // Allocate RGB buffer
    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB32,
                                            codecCtx->width, codecCtx->height, 1);
    std::vector<uint8_t> rgbBuffer(static_cast<size_t>(numBytes));
    av_image_fill_arrays(rgbFrame->data, rgbFrame->linesize,
                         rgbBuffer.data(), AV_PIX_FMT_RGB32,
                         codecCtx->width, codecCtx->height, 1);

    // Decode up to 500 frames for analysis (limit memory)
    int maxFrames = 500;
    int decoded = 0;

    while (av_read_frame(fmtCtx, pkt) >= 0 && decoded < maxFrames) {
        if (pkt->stream_index != videoIdx) {
            av_packet_unref(pkt);
            continue;
        }

        if (avcodec_send_packet(codecCtx, pkt) >= 0) {
            while (avcodec_receive_frame(codecCtx, frame) >= 0 && decoded < maxFrames) {
                sws_scale(swsCtx, frame->data, frame->linesize,
                          0, codecCtx->height,
                          rgbFrame->data, rgbFrame->linesize);

                QImage img(rgbFrame->data[0],
                           codecCtx->width, codecCtx->height,
                           rgbFrame->linesize[0],
                           QImage::Format_RGB32);
                m_videoFrames.append(img.copy()); // deep copy
                ++decoded;
                av_frame_unref(frame);
            }
        }
        av_packet_unref(pkt);
    }

    av_packet_free(&pkt);
    av_frame_free(&rgbFrame);
    av_frame_free(&frame);
    sws_freeContext(swsCtx);
    avcodec_free_context(&codecCtx);
    avformat_close_input(&fmtCtx);

    if (!m_videoFrames.isEmpty()) {
        m_hasVideo = true;
        m_currentVideoFrame = 0;
        m_videoDock->show();
        QPixmap pix = QPixmap::fromImage(
            m_videoFrames[0].scaled(320, 240, Qt::KeepAspectRatio,
                                    Qt::SmoothTransformation));
        m_videoFrameLabel->setPixmap(pix);
        updateFileInfo();
    }
#else
    Q_UNUSED(path)
#endif
}

// ── Utility ────────────────────────────────────────────────────────────────

QString EditorStudioWindow::formatTime(int64_t samples, int sampleRate) const
{
    if (sampleRate <= 0) return QStringLiteral("0:00.000");
    double seconds = static_cast<double>(samples) / static_cast<double>(sampleRate);
    int mins = static_cast<int>(seconds) / 60;
    double secs = seconds - mins * 60.0;
    return QString("%1:%2")
        .arg(mins)
        .arg(secs, 6, 'f', 3, QLatin1Char('0'));
}

QString EditorStudioWindow::formatFileSize(qint64 bytes) const
{
    if (bytes < 1024)
        return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024 * 1024 * 1024)
        return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    return QString("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}

// ── Drag & drop ────────────────────────────────────────────────────────────

void EditorStudioWindow::closeEvent(QCloseEvent* event)
{
    m_waveformView->stop();
    event->accept();
}

void EditorStudioWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void EditorStudioWindow::dropEvent(QDropEvent* event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    if (!urls.isEmpty()) {
        openFile(urls.first().toLocalFile());
    }
}

} // namespace dawcast::editor
