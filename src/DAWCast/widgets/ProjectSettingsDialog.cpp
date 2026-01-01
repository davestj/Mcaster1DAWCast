// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ProjectSettingsDialog.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QSpinBox>
#include <QDialogButtonBox>

namespace dawcast::widgets {

ProjectSettingsDialog::ProjectSettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Project Settings"));

    auto* mainLayout = new QVBoxLayout(this);
    auto* formLayout = new QFormLayout;

    m_sampleRateCombo = new QComboBox(this);
    m_sampleRateCombo->addItems({QStringLiteral("44100"), QStringLiteral("48000"),
                                  QStringLiteral("96000"), QStringLiteral("192000")});
    m_sampleRateCombo->setCurrentIndex(1);
    formLayout->addRow(tr("Sample Rate:"), m_sampleRateCombo);

    m_bitDepthCombo = new QComboBox(this);
    m_bitDepthCombo->addItems({QStringLiteral("16-bit"), QStringLiteral("24-bit"),
                                QStringLiteral("32-bit float")});
    m_bitDepthCombo->setCurrentIndex(2);
    formLayout->addRow(tr("Bit Depth:"), m_bitDepthCombo);

    m_videoWidthSpin = new QSpinBox(this);
    m_videoWidthSpin->setRange(320, 7680);
    m_videoWidthSpin->setValue(1920);
    formLayout->addRow(tr("Video Width:"), m_videoWidthSpin);

    m_videoHeightSpin = new QSpinBox(this);
    m_videoHeightSpin->setRange(240, 4320);
    m_videoHeightSpin->setValue(1080);
    formLayout->addRow(tr("Video Height:"), m_videoHeightSpin);

    m_videoFpsCombo = new QComboBox(this);
    m_videoFpsCombo->addItems({QStringLiteral("23.976"), QStringLiteral("24"),
                                QStringLiteral("25"), QStringLiteral("29.97"),
                                QStringLiteral("30"), QStringLiteral("60")});
    m_videoFpsCombo->setCurrentIndex(4);
    formLayout->addRow(tr("Video FPS:"), m_videoFpsCombo);

    mainLayout->addLayout(formLayout);

    auto* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

ProjectSettingsDialog::~ProjectSettingsDialog() = default;

void ProjectSettingsDialog::setProjectManager(core::ProjectManager* pm)
{
    m_projectManager = pm;
    // TODO: populate fields from project manager
}

} // namespace dawcast::widgets
