// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "BatchEncoderDialog.h"
#include "../audio_engine/BatchEncoder.h"
#include "../config/YamlPresets.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QStandardPaths>

namespace dawcast::widgets {

// Audio file extensions accepted by the batch encoder
static const QStringList kAudioExtensions = {
    QStringLiteral("mp3"),  QStringLiteral("wav"),  QStringLiteral("flac"),
    QStringLiteral("aac"),  QStringLiteral("m4a"),  QStringLiteral("ogg"),
    QStringLiteral("opus"), QStringLiteral("wma"),  QStringLiteral("aiff"),
    QStringLiteral("aif"),  QStringLiteral("mp4"),  QStringLiteral("webm"),
    QStringLiteral("mkv"),  QStringLiteral("avi")
};

static bool isAudioFile(const QString& path)
{
    QString ext = QFileInfo(path).suffix().toLower();
    return kAudioExtensions.contains(ext);
}

// Table column indices
enum Column {
    ColIndex = 0,
    ColStatus,
    ColFilename,
    ColInputFormat,
    ColDuration,
    ColOutputFormat,
    ColBitrate,
    ColDsp,
    ColLufs,
    ColProgress,
    ColCount
};

// ---------------------------------------------------------------------------
// Construction / Destruction
// ---------------------------------------------------------------------------

BatchEncoderDialog::BatchEncoderDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Batch Encoder"));
    setMinimumSize(675, 480);
    setAcceptDrops(true);

    m_encoder = new dawcast::BatchEncoder(this);

    setupUi();

    // Connect encoder signals
    connect(m_encoder, &dawcast::BatchEncoder::jobStarted,
            this, [this](int index) {
        if (index < m_table->rowCount()) {
            m_table->item(index, ColStatus)->setText(tr("Encoding"));
            m_table->item(index, ColStatus)->setIcon(
                style()->standardIcon(QStyle::SP_BrowserReload));
        }
    });

    connect(m_encoder, &dawcast::BatchEncoder::jobProgress,
            this, [this](int index, int percent) {
        if (index < m_table->rowCount()) {
            auto* bar = qobject_cast<QProgressBar*>(
                m_table->cellWidget(index, ColProgress));
            if (bar) bar->setValue(percent);
        }
    });

    connect(m_encoder, &dawcast::BatchEncoder::jobFinished,
            this, [this](int index) {
        if (index < m_table->rowCount()) {
            m_table->item(index, ColStatus)->setText(tr("Done"));
            m_table->item(index, ColStatus)->setIcon(
                style()->standardIcon(QStyle::SP_DialogApplyButton));
        }
    });

    connect(m_encoder, &dawcast::BatchEncoder::jobFailed,
            this, [this](int index, const QString& error) {
        if (index < m_table->rowCount()) {
            m_table->item(index, ColStatus)->setText(tr("Failed"));
            m_table->item(index, ColStatus)->setIcon(
                style()->standardIcon(QStyle::SP_MessageBoxCritical));
            m_table->item(index, ColStatus)->setToolTip(error);
        }
    });

    connect(m_encoder, &dawcast::BatchEncoder::batchProgress,
            this, [this](int completed, int total) {
        m_overallProgress->setMaximum(total);
        m_overallProgress->setValue(completed);
        m_progressLabel->setText(tr("%1 of %2 files").arg(completed).arg(total));
    });

    connect(m_encoder, &dawcast::BatchEncoder::allFinished,
            this, [this]() {
        m_btnEncode->setEnabled(true);
        m_btnCancel->setEnabled(false);
        m_progressLabel->setText(tr("Batch encoding complete"));
    });
}

BatchEncoderDialog::~BatchEncoderDialog() = default;

// ---------------------------------------------------------------------------
// UI Setup
// ---------------------------------------------------------------------------

void BatchEncoderDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);

    // Toolbar
    setupToolbar();
    auto* toolbarLayout = new QHBoxLayout;
    toolbarLayout->addWidget(m_btnAddFiles);
    toolbarLayout->addWidget(m_btnAddFolder);
    toolbarLayout->addWidget(m_btnRemove);
    toolbarLayout->addWidget(m_btnClearAll);
    toolbarLayout->addStretch();
    mainLayout->addLayout(toolbarLayout);

    // File table
    setupFileTable();
    mainLayout->addWidget(m_table, 1);

    // Settings panels side by side
    auto* settingsLayout = new QHBoxLayout;

    setupOutputSettings();
    settingsLayout->addWidget(m_outputGroup, 1);

    setupDspSettings();
    settingsLayout->addWidget(m_dspGroup, 1);

    mainLayout->addLayout(settingsLayout);

    // Bottom bar
    setupBottomBar();
    // Bottom bar layout
    auto* bottomLayout = new QHBoxLayout;
    bottomLayout->addWidget(m_progressLabel);
    bottomLayout->addWidget(m_overallProgress, 1);

    auto* parallelLabel = new QLabel(tr("Parallel:"), this);
    bottomLayout->addWidget(parallelLabel);
    bottomLayout->addWidget(m_parallelSpin);

    bottomLayout->addSpacing(16);
    bottomLayout->addWidget(m_btnEncode);
    bottomLayout->addWidget(m_btnCancel);
    bottomLayout->addWidget(m_btnClose);

    mainLayout->addLayout(bottomLayout);
}

void BatchEncoderDialog::setupToolbar()
{
    m_btnAddFiles = new QPushButton(tr("Add Files..."), this);
    m_btnAddFiles->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
    connect(m_btnAddFiles, &QPushButton::clicked, this, &BatchEncoderDialog::addFiles);

    m_btnAddFolder = new QPushButton(tr("Add Folder..."), this);
    m_btnAddFolder->setIcon(style()->standardIcon(QStyle::SP_DirIcon));
    connect(m_btnAddFolder, &QPushButton::clicked, this, &BatchEncoderDialog::addFolder);

    m_btnRemove = new QPushButton(tr("Remove"), this);
    m_btnRemove->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
    connect(m_btnRemove, &QPushButton::clicked, this, &BatchEncoderDialog::removeSelected);

    m_btnClearAll = new QPushButton(tr("Clear All"), this);
    connect(m_btnClearAll, &QPushButton::clicked, this, &BatchEncoderDialog::clearAll);
}

void BatchEncoderDialog::setupFileTable()
{
    m_table = new QTableWidget(0, ColCount, this);
    m_table->setHorizontalHeaderLabels({
        tr("#"), tr("Status"), tr("Filename"), tr("Format"),
        tr("Duration"), tr("Output"), tr("Bitrate"),
        tr("DSP"), tr("LUFS"), tr("Progress")
    });

    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setAlternatingRowColors(true);
    m_table->setShowGrid(false);
    m_table->verticalHeader()->setVisible(false);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);

    // Column widths
    m_table->setColumnWidth(ColIndex, 35);
    m_table->setColumnWidth(ColStatus, 80);
    m_table->setColumnWidth(ColFilename, 220);
    m_table->setColumnWidth(ColInputFormat, 60);
    m_table->setColumnWidth(ColDuration, 70);
    m_table->setColumnWidth(ColOutputFormat, 70);
    m_table->setColumnWidth(ColBitrate, 65);
    m_table->setColumnWidth(ColDsp, 45);
    m_table->setColumnWidth(ColLufs, 55);
    m_table->horizontalHeader()->setStretchLastSection(true);

    connect(m_table, &QTableWidget::customContextMenuRequested,
            this, &BatchEncoderDialog::showContextMenu);
}

void BatchEncoderDialog::setupOutputSettings()
{
    m_outputGroup = new QGroupBox(tr("Output Settings"), this);
    m_outputGroup->setCheckable(true);
    m_outputGroup->setChecked(true);

    auto* layout = new QFormLayout(m_outputGroup);

    // Format
    m_formatCombo = new QComboBox(m_outputGroup);
    m_formatCombo->addItems({
        QStringLiteral("MP3"), QStringLiteral("AAC"), QStringLiteral("Opus"),
        QStringLiteral("FLAC"), QStringLiteral("WAV"), QStringLiteral("Vorbis"),
        QStringLiteral("OGG")
    });
    layout->addRow(tr("Format:"), m_formatCombo);
    connect(m_formatCombo, &QComboBox::currentTextChanged,
            this, &BatchEncoderDialog::onFormatChanged);

    // Bitrate
    m_bitrateSpin = new QSpinBox(m_outputGroup);
    m_bitrateSpin->setRange(32, 320);
    m_bitrateSpin->setValue(192);
    m_bitrateSpin->setSuffix(QStringLiteral(" kbps"));
    m_bitrateSpin->setSingleStep(32);
    layout->addRow(tr("Bitrate:"), m_bitrateSpin);

    // Sample rate
    m_sampleRateCombo = new QComboBox(m_outputGroup);
    m_sampleRateCombo->addItems({
        tr("Keep Original"), QStringLiteral("22050"),
        QStringLiteral("44100"), QStringLiteral("48000"),
        QStringLiteral("96000")
    });
    m_sampleRateCombo->setCurrentIndex(0);
    layout->addRow(tr("Sample Rate:"), m_sampleRateCombo);

    // Channels
    m_channelsCombo = new QComboBox(m_outputGroup);
    m_channelsCombo->addItems({
        tr("Keep Original"), tr("Mono"), tr("Stereo")
    });
    m_channelsCombo->setCurrentIndex(0);
    layout->addRow(tr("Channels:"), m_channelsCombo);

    // Output directory
    auto* dirLayout = new QHBoxLayout;
    m_outputDirEdit = new QLineEdit(m_outputGroup);
    m_outputDirEdit->setPlaceholderText(tr("Same as input file"));
    dirLayout->addWidget(m_outputDirEdit, 1);

    m_browseDirBtn = new QPushButton(tr("Browse..."), m_outputGroup);
    dirLayout->addWidget(m_browseDirBtn);
    layout->addRow(tr("Output Dir:"), dirLayout);

    connect(m_browseDirBtn, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(
            this, tr("Select Output Directory"),
            m_outputDirEdit->text().isEmpty()
                ? QStandardPaths::writableLocation(QStandardPaths::MusicLocation)
                : m_outputDirEdit->text());
        if (!dir.isEmpty()) {
            m_outputDirEdit->setText(dir);
        }
    });

    // Filename pattern
    m_filenamePatternCombo = new QComboBox(m_outputGroup);
    m_filenamePatternCombo->addItems({
        QStringLiteral("{name}.{ext}"),
        QStringLiteral("{name}_encoded.{ext}"),
        QStringLiteral("{name}_{format}_{bitrate}.{ext}")
    });
    m_filenamePatternCombo->setEditable(true);
    layout->addRow(tr("Filename:"), m_filenamePatternCombo);
}

void BatchEncoderDialog::setupDspSettings()
{
    m_dspGroup = new QGroupBox(tr("DSP Processing"), this);
    m_dspGroup->setCheckable(true);
    m_dspGroup->setChecked(false);

    auto* layout = new QFormLayout(m_dspGroup);

    // Apply DSP chain
    m_applyDspCheck = new QCheckBox(tr("Apply DSP Chain"), m_dspGroup);
    layout->addRow(m_applyDspCheck);

    // Preset dropdown
    m_presetCombo = new QComboBox(m_dspGroup);
    m_presetCombo->addItem(tr("(None)"));

    // Load presets from configs/dsp_presets/
    QString presetDir = QApplication::applicationDirPath()
                        + QStringLiteral("/../../../configs/dsp_presets");
    QStringList presets = config::YamlPresets::listPresets(presetDir);
    for (const QString& preset : presets) {
        QFileInfo fi(preset);
        // Pretty-print the preset name
        QString name = fi.baseName().replace(QChar('_'), QChar(' '));
        // Capitalize first letter of each word
        QStringList words = name.split(QChar(' '));
        for (QString& w : words) {
            if (!w.isEmpty()) w[0] = w[0].toUpper();
        }
        m_presetCombo->addItem(words.join(QChar(' ')), preset);
    }
    // Add built-in names if no files found
    if (presets.isEmpty()) {
        m_presetCombo->addItem(tr("Broadcast Chain"));
        m_presetCombo->addItem(tr("Podcast Voice"));
        m_presetCombo->addItem(tr("Music Master"));
        m_presetCombo->addItem(tr("Spoken Word"));
    }
    layout->addRow(tr("Preset:"), m_presetCombo);

    // Enable/disable preset combo based on checkbox
    m_presetCombo->setEnabled(false);
    connect(m_applyDspCheck, &QCheckBox::toggled,
            m_presetCombo, &QComboBox::setEnabled);

    layout->addRow(new QLabel(QString(), m_dspGroup)); // spacer

    // Loudness normalize
    m_normalizeCheck = new QCheckBox(tr("Loudness Normalize"), m_dspGroup);
    layout->addRow(m_normalizeCheck);

    m_lufsTargetCombo = new QComboBox(m_dspGroup);
    m_lufsTargetCombo->addItem(tr("-14 LUFS (Streaming)"),     -14.0);
    m_lufsTargetCombo->addItem(tr("-16 LUFS (Podcast)"),       -16.0);
    m_lufsTargetCombo->addItem(tr("-23 LUFS (EBU R128)"),      -23.0);
    m_lufsTargetCombo->addItem(tr("-24 LUFS (ATSC A/85)"),     -24.0);
    m_lufsTargetCombo->setCurrentIndex(1);
    layout->addRow(tr("Target:"), m_lufsTargetCombo);

    m_lufsTargetCombo->setEnabled(false);
    connect(m_normalizeCheck, &QCheckBox::toggled,
            m_lufsTargetCombo, &QComboBox::setEnabled);
}

void BatchEncoderDialog::setupBottomBar()
{
    m_overallProgress = new QProgressBar(this);
    m_overallProgress->setRange(0, 100);
    m_overallProgress->setValue(0);
    m_overallProgress->setTextVisible(true);

    m_progressLabel = new QLabel(tr("No files queued"), this);

    m_parallelSpin = new QSpinBox(this);
    m_parallelSpin->setRange(1, 4);
    m_parallelSpin->setValue(1);
    m_parallelSpin->setToolTip(tr("Number of files to encode simultaneously"));

    m_btnEncode = new QPushButton(tr("Encode All"), this);
    m_btnEncode->setDefault(true);
    m_btnEncode->setMinimumWidth(75);
    // Make the Encode button visually prominent
    m_btnEncode->setStyleSheet(
        QStringLiteral("QPushButton { font-weight: bold; padding: 6px 16px; }"));
    connect(m_btnEncode, &QPushButton::clicked,
            this, &BatchEncoderDialog::startEncoding);

    m_btnCancel = new QPushButton(tr("Cancel"), this);
    m_btnCancel->setEnabled(false);
    connect(m_btnCancel, &QPushButton::clicked,
            this, &BatchEncoderDialog::cancelEncoding);

    m_btnClose = new QPushButton(tr("Close"), this);
    connect(m_btnClose, &QPushButton::clicked, this, &QDialog::close);
}

// ---------------------------------------------------------------------------
// Drag & Drop
// ---------------------------------------------------------------------------

void BatchEncoderDialog::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void BatchEncoderDialog::dropEvent(QDropEvent* event)
{
    QStringList paths;
    for (const QUrl& url : event->mimeData()->urls()) {
        QString path = url.toLocalFile();
        if (!path.isEmpty()) {
            QFileInfo fi(path);
            if (fi.isDir()) {
                QDirIterator it(path, QDir::Files, QDirIterator::Subdirectories);
                while (it.hasNext()) {
                    QString filePath = it.next();
                    if (isAudioFile(filePath))
                        paths.append(filePath);
                }
            } else if (isAudioFile(path)) {
                paths.append(path);
            }
        }
    }
    if (!paths.isEmpty()) {
        addFilePaths(paths);
    }
}

// ---------------------------------------------------------------------------
// File Management Slots
// ---------------------------------------------------------------------------

void BatchEncoderDialog::addFiles()
{
    QStringList paths = QFileDialog::getOpenFileNames(
        this, tr("Add Audio Files"),
        QStandardPaths::writableLocation(QStandardPaths::MusicLocation),
        tr("Audio Files (*.mp3 *.wav *.flac *.aac *.m4a *.ogg *.opus *.aiff *.aif *.wma)"
           ";;Video Files (*.mp4 *.webm *.mkv *.avi)"
           ";;All Files (*)"));
    if (!paths.isEmpty()) {
        addFilePaths(paths);
    }
}

void BatchEncoderDialog::addFolder()
{
    QString dir = QFileDialog::getExistingDirectory(
        this, tr("Add Folder"),
        QStandardPaths::writableLocation(QStandardPaths::MusicLocation));
    if (dir.isEmpty()) return;

    QStringList paths;
    QDirIterator it(dir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString path = it.next();
        if (isAudioFile(path))
            paths.append(path);
    }

    if (!paths.isEmpty()) {
        addFilePaths(paths);
    } else {
        QMessageBox::information(this, tr("No Audio Files"),
            tr("No supported audio files found in:\n%1").arg(dir));
    }
}

void BatchEncoderDialog::removeSelected()
{
    QList<int> rows;
    for (auto* item : m_table->selectedItems()) {
        int row = item->row();
        if (!rows.contains(row))
            rows.append(row);
    }
    // Remove in reverse order so indices remain valid
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (int row : rows) {
        m_table->removeRow(row);
        m_encoder->removeJob(row);
    }
    updateJobCount();
}

void BatchEncoderDialog::clearAll()
{
    m_table->setRowCount(0);
    m_encoder->clearJobs();
    m_overallProgress->setValue(0);
    updateJobCount();
}

// ---------------------------------------------------------------------------
// Encoding Slots
// ---------------------------------------------------------------------------

void BatchEncoderDialog::startEncoding()
{
    if (m_encoder->jobCount() == 0) {
        QMessageBox::information(this, tr("No Files"),
            tr("Add files to the queue before encoding."));
        return;
    }

    // Sync all job settings from the UI before starting
    syncJobSettings();

    m_encoder->setParallelJobs(m_parallelSpin->value());

    if (!m_outputDirEdit->text().isEmpty()) {
        m_encoder->setOutputDirectory(m_outputDirEdit->text());
    }

    m_btnEncode->setEnabled(false);
    m_btnCancel->setEnabled(true);
    m_overallProgress->setValue(0);
    m_progressLabel->setText(tr("Starting..."));

    // Reset table status
    for (int i = 0; i < m_table->rowCount(); ++i) {
        auto* bar = qobject_cast<QProgressBar*>(
            m_table->cellWidget(i, ColProgress));
        if (bar) bar->setValue(0);
        m_table->item(i, ColStatus)->setText(tr("Pending"));
        m_table->item(i, ColStatus)->setIcon(QIcon());
    }

    m_encoder->startEncoding();
}

void BatchEncoderDialog::cancelEncoding()
{
    m_encoder->cancelAll();
    m_btnEncode->setEnabled(true);
    m_btnCancel->setEnabled(false);
    m_progressLabel->setText(tr("Cancelled"));
}

void BatchEncoderDialog::onFormatChanged(const QString& format)
{
    bool lossless = (format == QStringLiteral("FLAC")
                  || format == QStringLiteral("WAV"));
    m_bitrateSpin->setEnabled(!lossless);
}

// ---------------------------------------------------------------------------
// Context Menu
// ---------------------------------------------------------------------------

void BatchEncoderDialog::showContextMenu(const QPoint& pos)
{
    QTableWidgetItem* item = m_table->itemAt(pos);
    if (!item) return;

    int row = item->row();

    QMenu menu(this);

    auto* actRemove = menu.addAction(tr("Remove"));
    connect(actRemove, &QAction::triggered, this, [this, row]() {
        m_table->removeRow(row);
        m_encoder->removeJob(row);
        updateJobCount();
    });

    auto* actEncodeOne = menu.addAction(tr("Encode This File"));
    connect(actEncodeOne, &QAction::triggered, this, [this, row]() {
        syncJobSettings();
        dawcast::BatchJob& job = m_encoder->jobRef(row);
        job.status = dawcast::BatchJob::Pending;
        m_encoder->setParallelJobs(1);
        // Encode just this one by setting others to Complete temporarily
        // (simpler approach: just call processJob logic directly)
        m_btnEncode->setEnabled(false);
        m_btnCancel->setEnabled(true);
        m_encoder->startEncoding();
    });

    menu.addSeparator();

    auto* actShowFinder = menu.addAction(tr("Show in Finder"));
    connect(actShowFinder, &QAction::triggered, this, [this, row]() {
        if (row < m_encoder->jobCount()) {
            QString path = m_encoder->job(row).inputPath;
            QFileInfo fi(path);
            QDesktopServices::openUrl(
                QUrl::fromLocalFile(fi.absolutePath()));
        }
    });

    menu.exec(m_table->viewport()->mapToGlobal(pos));
}

// ---------------------------------------------------------------------------
// Internal Helpers
// ---------------------------------------------------------------------------

void BatchEncoderDialog::addFilePaths(const QStringList& paths)
{
    for (const QString& path : paths) {
        QFileInfo fi(path);
        if (!fi.exists() || !fi.isFile()) continue;

        dawcast::BatchJob job;
        job.inputPath  = path;
        job.outputPath = buildOutputPath(path);
        job.outputCodec = m_formatCombo->currentText().toLower();
        job.bitrate     = m_bitrateSpin->value();
        job.srcFormat   = fi.suffix().toUpper();

        // Estimate duration from file size (rough — updated on decode)
        job.durationSec = 0.0;

        m_encoder->addJob(job);

        // Add table row
        int row = m_table->rowCount();
        m_table->insertRow(row);

        // # (index)
        auto* idxItem = new QTableWidgetItem(QString::number(row + 1));
        idxItem->setTextAlignment(Qt::AlignCenter);
        idxItem->setFlags(idxItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, ColIndex, idxItem);

        // Status
        auto* statusItem = new QTableWidgetItem(tr("Pending"));
        statusItem->setFlags(statusItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, ColStatus, statusItem);

        // Filename
        auto* nameItem = new QTableWidgetItem(fi.fileName());
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        nameItem->setToolTip(path);
        m_table->setItem(row, ColFilename, nameItem);

        // Input Format
        auto* fmtItem = new QTableWidgetItem(fi.suffix().toUpper());
        fmtItem->setTextAlignment(Qt::AlignCenter);
        fmtItem->setFlags(fmtItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, ColInputFormat, fmtItem);

        // Duration (unknown until decode)
        auto* durItem = new QTableWidgetItem(QStringLiteral("--:--"));
        durItem->setTextAlignment(Qt::AlignCenter);
        durItem->setFlags(durItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, ColDuration, durItem);

        // Output Format
        auto* outFmtItem = new QTableWidgetItem(
            m_formatCombo->currentText());
        outFmtItem->setTextAlignment(Qt::AlignCenter);
        outFmtItem->setFlags(outFmtItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, ColOutputFormat, outFmtItem);

        // Bitrate
        QString bitrateText;
        QString fmt = m_formatCombo->currentText();
        if (fmt == QStringLiteral("FLAC") || fmt == QStringLiteral("WAV")) {
            bitrateText = tr("Lossless");
        } else {
            bitrateText = QStringLiteral("%1k").arg(m_bitrateSpin->value());
        }
        auto* brItem = new QTableWidgetItem(bitrateText);
        brItem->setTextAlignment(Qt::AlignCenter);
        brItem->setFlags(brItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, ColBitrate, brItem);

        // DSP
        QString dspText = (m_applyDspCheck && m_applyDspCheck->isChecked())
                          ? tr("Yes") : tr("No");
        auto* dspItem = new QTableWidgetItem(dspText);
        dspItem->setTextAlignment(Qt::AlignCenter);
        dspItem->setFlags(dspItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, ColDsp, dspItem);

        // LUFS target
        QString lufsText;
        if (m_normalizeCheck && m_normalizeCheck->isChecked()) {
            double target = m_lufsTargetCombo->currentData().toDouble();
            lufsText = QStringLiteral("%1").arg(target, 0, 'f', 0);
        } else {
            lufsText = QStringLiteral("--");
        }
        auto* lufsItem = new QTableWidgetItem(lufsText);
        lufsItem->setTextAlignment(Qt::AlignCenter);
        lufsItem->setFlags(lufsItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, ColLufs, lufsItem);

        // Progress bar
        auto* progressBar = new QProgressBar(m_table);
        progressBar->setRange(0, 100);
        progressBar->setValue(0);
        progressBar->setTextVisible(true);
        m_table->setCellWidget(row, ColProgress, progressBar);
    }

    updateJobCount();
}

void BatchEncoderDialog::updateJobCount()
{
    int count = m_encoder->jobCount();
    if (count == 0) {
        m_progressLabel->setText(tr("No files queued"));
    } else {
        m_progressLabel->setText(tr("%1 file(s) queued").arg(count));
    }
    m_overallProgress->setMaximum(qMax(count, 1));
}

void BatchEncoderDialog::syncJobSettings()
{
    QString codec = m_formatCombo->currentText().toLower();
    int bitrate   = m_bitrateSpin->value();
    int sampleRate = 0;
    int channels   = 0;

    // Sample rate
    QString srText = m_sampleRateCombo->currentText();
    if (srText != tr("Keep Original")) {
        sampleRate = srText.toInt();
    }

    // Channels
    QString chText = m_channelsCombo->currentText();
    if (chText == tr("Mono"))        channels = 1;
    else if (chText == tr("Stereo")) channels = 2;

    // DSP
    bool applyDsp = m_dspGroup->isChecked() && m_applyDspCheck->isChecked();
    QStringList dspPresets;
    if (applyDsp && m_presetCombo->currentIndex() > 0) {
        QString presetPath = m_presetCombo->currentData().toString();
        if (!presetPath.isEmpty()) {
            dspPresets.append(presetPath);
        }
    }

    // LUFS
    float targetLufs = 0.0f;
    if (m_dspGroup->isChecked() && m_normalizeCheck->isChecked()) {
        targetLufs = static_cast<float>(
            m_lufsTargetCombo->currentData().toDouble());
    }

    // Apply to all pending jobs
    for (int i = 0; i < m_encoder->jobCount(); ++i) {
        dawcast::BatchJob& job = m_encoder->jobRef(i);
        if (job.status != dawcast::BatchJob::Pending)
            continue;

        job.outputCodec   = codec;
        job.bitrate       = bitrate;
        job.sampleRate    = sampleRate;
        job.channels      = channels;
        job.applyDspChain = applyDsp;
        job.dspPresets    = dspPresets;
        job.targetLUFS    = targetLufs;
        job.outputPath    = buildOutputPath(job.inputPath);
    }
}

QString BatchEncoderDialog::buildOutputPath(const QString& inputPath) const
{
    QFileInfo fi(inputPath);
    QString baseName = fi.completeBaseName();

    // Determine output extension from format
    QString format = m_formatCombo->currentText().toLower();
    QString ext = format;
    if (ext == QStringLiteral("vorbis")) ext = QStringLiteral("ogg");

    // Determine output directory
    QString outDir = m_outputDirEdit->text();
    if (outDir.isEmpty()) {
        outDir = fi.absolutePath();
    }

    // Apply filename pattern
    QString pattern = m_filenamePatternCombo->currentText();
    QString filename = pattern;
    filename.replace(QStringLiteral("{name}"), baseName);
    filename.replace(QStringLiteral("{ext}"), ext);
    filename.replace(QStringLiteral("{format}"), format);
    filename.replace(QStringLiteral("{bitrate}"),
                     QString::number(m_bitrateSpin->value()));

    return QDir(outDir).filePath(filename);
}

} // namespace dawcast::widgets
