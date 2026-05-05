// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ImportAudioDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QCheckBox>
#include <QComboBox>
#include <QRadioButton>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QButtonGroup>

namespace dawcast::widgets {

ImportAudioDialog::ImportAudioDialog(const QStringList& filePaths,
                                     QWidget* parent)
    : QDialog(parent)
    , m_filePaths(filePaths)
{
    setWindowTitle(tr("Import Audio"));
    setMinimumSize(510, 390);
    resize(555, 435);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);

    // Title label
    auto* titleLabel = new QLabel(
        tr("Import %n audio file(s)", "", filePaths.size()), this);
    titleLabel->setStyleSheet(QStringLiteral(
        "QLabel { font-size: 13px; font-weight: bold; color: #dde; }"));
    mainLayout->addWidget(titleLabel);

    buildFileInfoTable();
    mainLayout->addWidget(m_fileTable, 1);

    buildOptionsSection();

    // OK / Cancel
    auto* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Import"));
    mainLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

ImportAudioDialog::~ImportAudioDialog() = default;

// ── File Info Table ────────────────────────────────────────────────────────

void ImportAudioDialog::buildFileInfoTable()
{
    m_fileTable = new QTableWidget(m_filePaths.size(), 6, this);
    m_fileTable->setHorizontalHeaderLabels({
        tr("File Name"), tr("Format"), tr("Sample Rate"),
        tr("Bit Depth"), tr("Channels"), tr("Size")
    });
    m_fileTable->horizontalHeader()->setStretchLastSection(true);
    m_fileTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_fileTable->verticalHeader()->setVisible(false);
    m_fileTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_fileTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_fileTable->setAlternatingRowColors(true);
    m_fileTable->setStyleSheet(QStringLiteral(
        "QTableWidget { background: #1e2235; color: #ccd; gridline-color: #2a2e3e; }"
        "QTableWidget::item { padding: 2px 4px; }"
        "QTableWidget::item:selected { background: #3a3e58; }"
        "QHeaderView::section { background: #252840; color: #99a; "
        "  border: 1px solid #2a2e3e; font-size: 10px; padding: 3px; }"));

    for (int i = 0; i < m_filePaths.size(); ++i) {
        QFileInfo fi(m_filePaths[i]);

        // Column 0: File name
        auto* nameItem = new QTableWidgetItem(fi.fileName());
        nameItem->setToolTip(fi.absoluteFilePath());
        m_fileTable->setItem(i, 0, nameItem);

        // Column 1: Format (extension-based)
        QString ext = fi.suffix().toUpper();
        m_fileTable->setItem(i, 1, new QTableWidgetItem(ext));

        // Column 2: Sample Rate (placeholder — real detection needs codec)
        m_fileTable->setItem(i, 2, new QTableWidgetItem(QStringLiteral("--")));

        // Column 3: Bit Depth (placeholder)
        m_fileTable->setItem(i, 3, new QTableWidgetItem(QStringLiteral("--")));

        // Column 4: Channels (placeholder)
        m_fileTable->setItem(i, 4, new QTableWidgetItem(QStringLiteral("--")));

        // Column 5: File size
        m_fileTable->setItem(i, 5, new QTableWidgetItem(formatFileSize(fi.size())));
    }
}

// ── Options Section ────────────────────────────────────────────────────────

void ImportAudioDialog::buildOptionsSection()
{
    auto* optionsLayout = qobject_cast<QVBoxLayout*>(layout());
    if (!optionsLayout) return;

    // ── File handling ──────────────────────────────────────────────
    auto* fileGroup = new QGroupBox(tr("File Handling"), this);
    auto* fileLayout = new QVBoxLayout(fileGroup);

    m_copyToProjectCheck = new QCheckBox(tr("Copy files to project folder"), fileGroup);
    m_copyToProjectCheck->setChecked(true);
    fileLayout->addWidget(m_copyToProjectCheck);

    // Sample rate conversion row
    auto* srRow = new QHBoxLayout;
    m_convertSampleRateCheck = new QCheckBox(tr("Convert sample rate to:"), fileGroup);
    m_convertSampleRateCheck->setChecked(false);
    srRow->addWidget(m_convertSampleRateCheck);

    m_targetSampleRateCombo = new QComboBox(fileGroup);
    m_targetSampleRateCombo->addItems({
        QStringLiteral("44100"), QStringLiteral("48000"), QStringLiteral("96000")
    });
    m_targetSampleRateCombo->setCurrentIndex(1);  // 48000 default
    m_targetSampleRateCombo->setEnabled(false);
    m_targetSampleRateCombo->setFixedWidth(60);
    srRow->addWidget(m_targetSampleRateCombo);
    srRow->addStretch();
    fileLayout->addLayout(srRow);

    connect(m_convertSampleRateCheck, &QCheckBox::toggled,
            m_targetSampleRateCombo, &QComboBox::setEnabled);

    // Bit depth conversion row
    auto* bdRow = new QHBoxLayout;
    m_convertBitDepthCheck = new QCheckBox(tr("Convert bit depth to:"), fileGroup);
    m_convertBitDepthCheck->setChecked(false);
    bdRow->addWidget(m_convertBitDepthCheck);

    m_targetBitDepthCombo = new QComboBox(fileGroup);
    m_targetBitDepthCombo->addItems({
        QStringLiteral("16-bit"), QStringLiteral("24-bit"), QStringLiteral("32-bit float")
    });
    m_targetBitDepthCombo->setCurrentIndex(2);  // 32-bit float default
    m_targetBitDepthCombo->setEnabled(false);
    m_targetBitDepthCombo->setFixedWidth(75);
    bdRow->addWidget(m_targetBitDepthCombo);
    bdRow->addStretch();
    fileLayout->addLayout(bdRow);

    connect(m_convertBitDepthCheck, &QCheckBox::toggled,
            m_targetBitDepthCombo, &QComboBox::setEnabled);

    optionsLayout->addWidget(fileGroup);

    // ── Track assignment + placement ──────────────────────────────
    auto* trackPlacementRow = new QHBoxLayout;

    // Track assignment group
    auto* trackGroup = new QGroupBox(tr("Track Assignment"), this);
    auto* trackLayout = new QVBoxLayout(trackGroup);

    m_newTrackRadio = new QRadioButton(tr("Create new track for each file"), trackGroup);
    m_newTrackRadio->setChecked(true);
    trackLayout->addWidget(m_newTrackRadio);

    m_selectedTrackRadio = new QRadioButton(tr("Add to selected track"), trackGroup);
    trackLayout->addWidget(m_selectedTrackRadio);

    auto* trackBtnGroup = new QButtonGroup(this);
    trackBtnGroup->addButton(m_newTrackRadio);
    trackBtnGroup->addButton(m_selectedTrackRadio);

    trackPlacementRow->addWidget(trackGroup);

    // Placement group
    auto* placementGroup = new QGroupBox(tr("Placement"), this);
    auto* placementLayout = new QVBoxLayout(placementGroup);

    m_placeAtPlayhead = new QRadioButton(tr("At playhead"), placementGroup);
    m_placeAtPlayhead->setChecked(true);
    placementLayout->addWidget(m_placeAtPlayhead);

    m_placeAtStart = new QRadioButton(tr("At timeline start"), placementGroup);
    placementLayout->addWidget(m_placeAtStart);

    m_placeAtEnd = new QRadioButton(tr("At timeline end"), placementGroup);
    placementLayout->addWidget(m_placeAtEnd);

    auto* placeBtnGroup = new QButtonGroup(this);
    placeBtnGroup->addButton(m_placeAtPlayhead);
    placeBtnGroup->addButton(m_placeAtStart);
    placeBtnGroup->addButton(m_placeAtEnd);

    trackPlacementRow->addWidget(placementGroup);

    optionsLayout->addLayout(trackPlacementRow);
}

// ── Public Accessors ───────────────────────────────────────────────────────

ImportAudioDialog::ImportOptions ImportAudioDialog::options() const
{
    ImportOptions opts;
    opts.copyToProjectFolder = m_copyToProjectCheck && m_copyToProjectCheck->isChecked();
    opts.convertSampleRate   = m_convertSampleRateCheck && m_convertSampleRateCheck->isChecked();
    opts.convertBitDepth     = m_convertBitDepthCheck && m_convertBitDepthCheck->isChecked();
    opts.createNewTrack      = !m_selectedTrackRadio || !m_selectedTrackRadio->isChecked();

    if (m_targetSampleRateCombo)
        opts.targetSampleRate = m_targetSampleRateCombo->currentText().toInt();
    if (m_targetBitDepthCombo) {
        QString bd = m_targetBitDepthCombo->currentText();
        if (bd.startsWith(QStringLiteral("16")))      opts.targetBitDepth = 16;
        else if (bd.startsWith(QStringLiteral("24"))) opts.targetBitDepth = 24;
        else                                           opts.targetBitDepth = 32;
    }

    if (m_placeAtStart && m_placeAtStart->isChecked())
        opts.placement = AtStart;
    else if (m_placeAtEnd && m_placeAtEnd->isChecked())
        opts.placement = AtEnd;
    else
        opts.placement = AtPlayhead;

    return opts;
}

QStringList ImportAudioDialog::filePaths() const
{
    return m_filePaths;
}

// ── Helpers ────────────────────────────────────────────────────────────────

QString ImportAudioDialog::formatDuration(double seconds) const
{
    int mins = static_cast<int>(seconds) / 60;
    double secs = seconds - mins * 60;
    return QStringLiteral("%1:%2").arg(mins, 2, 10, QLatin1Char('0'))
                                  .arg(secs, 5, 'f', 2, QLatin1Char('0'));
}

QString ImportAudioDialog::formatFileSize(qint64 bytes) const
{
    if (bytes < 1024)
        return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QStringLiteral("%1 KB").arg(bytes / 1024);
    if (bytes < 1024LL * 1024 * 1024)
        return QStringLiteral("%1 MB").arg(static_cast<double>(bytes) / (1024.0 * 1024.0), 0, 'f', 1);
    return QStringLiteral("%1 GB").arg(static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}

} // namespace dawcast::widgets
