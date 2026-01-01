// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ExportDialog.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QGroupBox>

namespace dawcast::widgets {

ExportDialog::ExportDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Export"));
    auto* mainLayout = new QVBoxLayout(this);

    // Format selection
    auto* formatGroup = new QGroupBox(tr("Format"), this);
    auto* formatLayout = new QFormLayout(formatGroup);

    m_formatCombo = new QComboBox(this);
    m_formatCombo->addItem(tr("Audio Only"));
    m_formatCombo->addItem(tr("Audio + Video"));
    formatLayout->addRow(tr("Output:"), m_formatCombo);
    mainLayout->addWidget(formatGroup);

    // Audio settings
    auto* audioGroup = new QGroupBox(tr("Audio"), this);
    auto* audioLayout = new QFormLayout(audioGroup);

    m_audioCodecCombo = new QComboBox(this);
    m_audioCodecCombo->addItems({QStringLiteral("aac"), QStringLiteral("mp3"),
                                  QStringLiteral("flac"), QStringLiteral("opus")});
    audioLayout->addRow(tr("Codec:"), m_audioCodecCombo);

    m_bitrateSpin = new QSpinBox(this);
    m_bitrateSpin->setRange(64, 512);
    m_bitrateSpin->setValue(256);
    m_bitrateSpin->setSuffix(QStringLiteral(" kbps"));
    audioLayout->addRow(tr("Bitrate:"), m_bitrateSpin);

    m_sampleRateCombo = new QComboBox(this);
    m_sampleRateCombo->addItems({QStringLiteral("44100"), QStringLiteral("48000"),
                                  QStringLiteral("96000")});
    m_sampleRateCombo->setCurrentIndex(1);
    audioLayout->addRow(tr("Sample Rate:"), m_sampleRateCombo);

    mainLayout->addWidget(audioGroup);

    // Video settings
    auto* videoGroup = new QGroupBox(tr("Video"), this);
    auto* videoLayout = new QFormLayout(videoGroup);

    m_videoCodecCombo = new QComboBox(this);
    m_videoCodecCombo->addItems({QStringLiteral("h264"), QStringLiteral("h265"),
                                  QStringLiteral("prores")});
    videoLayout->addRow(tr("Codec:"), m_videoCodecCombo);

    m_widthSpin = new QSpinBox(this);
    m_widthSpin->setRange(320, 7680);
    m_widthSpin->setValue(1920);
    videoLayout->addRow(tr("Width:"), m_widthSpin);

    m_heightSpin = new QSpinBox(this);
    m_heightSpin->setRange(240, 4320);
    m_heightSpin->setValue(1080);
    videoLayout->addRow(tr("Height:"), m_heightSpin);

    m_fpsSpin = new QSpinBox(this);
    m_fpsSpin->setRange(15, 120);
    m_fpsSpin->setValue(30);
    videoLayout->addRow(tr("FPS:"), m_fpsSpin);

    mainLayout->addWidget(videoGroup);

    // OK / Cancel
    auto* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

ExportDialog::~ExportDialog() = default;

void ExportDialog::setProject(core::ProjectManager* project)
{
    m_project = project;
    // TODO: populate defaults from project settings
}

ExportSettings ExportDialog::exportSettings() const
{
    ExportSettings s;
    s.audioOnly    = (m_formatCombo->currentIndex() == 0);
    s.audioCodec   = m_audioCodecCombo->currentText();
    s.audioBitrate = m_bitrateSpin->value();
    s.sampleRate   = m_sampleRateCombo->currentText().toInt();
    s.videoCodec   = m_videoCodecCombo->currentText();
    s.videoWidth   = m_widthSpin->value();
    s.videoHeight  = m_heightSpin->value();
    s.videoFps     = m_fpsSpin->value();
    return s;
}

} // namespace dawcast::widgets
