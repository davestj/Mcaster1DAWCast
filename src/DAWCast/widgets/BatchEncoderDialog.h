// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QTableWidget>
#include <QGroupBox>
#include <QLabel>

namespace dawcast { class BatchEncoder; }

namespace dawcast::widgets {

/// Dialog for batch-encoding multiple audio files with configurable
/// output format, DSP processing, and loudness normalization.
class BatchEncoderDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BatchEncoderDialog(QWidget* parent = nullptr);
    ~BatchEncoderDialog() override;

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void addFiles();
    void addFolder();
    void removeSelected();
    void clearAll();
    void startEncoding();
    void cancelEncoding();
    void onFormatChanged(const QString& format);
    void showContextMenu(const QPoint& pos);

private:
    void setupUi();
    void setupToolbar();
    void setupFileTable();
    void setupOutputSettings();
    void setupDspSettings();
    void setupBottomBar();
    void addFilePaths(const QStringList& paths);
    void updateJobCount();
    void syncJobSettings();
    QString buildOutputPath(const QString& inputPath) const;

    // ── Encoder engine ───────────────────────────────────────────────────
    BatchEncoder* m_encoder = nullptr;

    // ── Toolbar ──────────────────────────────────────────────────────────
    QPushButton* m_btnAddFiles   = nullptr;
    QPushButton* m_btnAddFolder  = nullptr;
    QPushButton* m_btnRemove     = nullptr;
    QPushButton* m_btnClearAll   = nullptr;

    // ── File queue table ─────────────────────────────────────────────────
    QTableWidget* m_table = nullptr;

    // ── Output settings ──────────────────────────────────────────────────
    QGroupBox*  m_outputGroup       = nullptr;
    QComboBox*  m_formatCombo       = nullptr;
    QSpinBox*   m_bitrateSpin       = nullptr;
    QComboBox*  m_sampleRateCombo   = nullptr;
    QComboBox*  m_channelsCombo     = nullptr;
    QLineEdit*  m_outputDirEdit     = nullptr;
    QPushButton* m_browseDirBtn     = nullptr;
    QComboBox*  m_filenamePatternCombo = nullptr;

    // ── DSP settings ─────────────────────────────────────────────────────
    QGroupBox*  m_dspGroup          = nullptr;
    QCheckBox*  m_applyDspCheck     = nullptr;
    QComboBox*  m_presetCombo       = nullptr;
    QCheckBox*  m_normalizeCheck    = nullptr;
    QComboBox*  m_lufsTargetCombo   = nullptr;

    // ── Bottom bar ───────────────────────────────────────────────────────
    QProgressBar* m_overallProgress = nullptr;
    QLabel*       m_progressLabel   = nullptr;
    QPushButton*  m_btnEncode       = nullptr;
    QPushButton*  m_btnCancel       = nullptr;
    QPushButton*  m_btnClose        = nullptr;
    QSpinBox*     m_parallelSpin    = nullptr;
};

} // namespace dawcast::widgets
