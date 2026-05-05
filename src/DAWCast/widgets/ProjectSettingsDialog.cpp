// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ProjectSettingsDialog.h"
#include "ProjectManager.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QGroupBox>

namespace dawcast::widgets {

ProjectSettingsDialog::ProjectSettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Project Settings"));
    setMinimumWidth(300);

    auto* mainLayout = new QVBoxLayout(this);

    // --- Project Info ---
    auto* infoGroup = new QGroupBox(tr("Project Information"), this);
    auto* infoLayout = new QFormLayout(infoGroup);

    m_projectNameEdit = new QLineEdit(this);
    m_projectNameEdit->setPlaceholderText(tr("Untitled Project"));
    infoLayout->addRow(tr("Project Name:"), m_projectNameEdit);

    m_authorEdit = new QLineEdit(this);
    m_authorEdit->setPlaceholderText(tr("Author name"));
    infoLayout->addRow(tr("Author:"), m_authorEdit);

    mainLayout->addWidget(infoGroup);

    // --- Audio Settings ---
    auto* audioGroup = new QGroupBox(tr("Audio Settings"), this);
    auto* audioLayout = new QFormLayout(audioGroup);

    m_sampleRateCombo = new QComboBox(this);
    m_sampleRateCombo->addItems({
        QStringLiteral("44100"), QStringLiteral("48000"), QStringLiteral("96000")
    });
    m_sampleRateCombo->setCurrentIndex(1);
    audioLayout->addRow(tr("Sample Rate:"), m_sampleRateCombo);

    m_bitDepthCombo = new QComboBox(this);
    m_bitDepthCombo->addItems({
        QStringLiteral("16-bit"), QStringLiteral("24-bit"), QStringLiteral("32-bit float")
    });
    m_bitDepthCombo->setCurrentIndex(2);
    audioLayout->addRow(tr("Bit Depth:"), m_bitDepthCombo);

    mainLayout->addWidget(audioGroup);

    // --- Video Settings ---
    auto* videoGroup = new QGroupBox(tr("Video Settings"), this);
    auto* videoLayout = new QFormLayout(videoGroup);

    m_videoResolutionCombo = new QComboBox(this);
    m_videoResolutionCombo->addItem(tr("720p (1280x720)"), QStringLiteral("1280x720"));
    m_videoResolutionCombo->addItem(tr("1080p (1920x1080)"), QStringLiteral("1920x1080"));
    m_videoResolutionCombo->addItem(tr("4K (3840x2160)"), QStringLiteral("3840x2160"));
    m_videoResolutionCombo->setCurrentIndex(1);
    videoLayout->addRow(tr("Resolution:"), m_videoResolutionCombo);

    m_videoFpsCombo = new QComboBox(this);
    m_videoFpsCombo->addItems({
        QStringLiteral("23.976"), QStringLiteral("24"), QStringLiteral("25"),
        QStringLiteral("29.97"), QStringLiteral("30"), QStringLiteral("60")
    });
    m_videoFpsCombo->setCurrentIndex(4); // 30 fps
    videoLayout->addRow(tr("Framerate:"), m_videoFpsCombo);

    mainLayout->addWidget(videoGroup);

    mainLayout->addStretch();

    // --- OK / Cancel ---
    auto* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, [this] {
        applyToProject();
        accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

ProjectSettingsDialog::~ProjectSettingsDialog() = default;

void ProjectSettingsDialog::setProjectManager(ProjectManager* pm)
{
    m_projectManager = pm;

    if (!pm) return;

    // Populate fields from project manager
    m_projectNameEdit->setText(pm->projectName());
    m_authorEdit->setText(pm->author());

    int sr = pm->sampleRate();
    for (int i = 0; i < m_sampleRateCombo->count(); ++i) {
        if (m_sampleRateCombo->itemText(i).toInt() == sr) {
            m_sampleRateCombo->setCurrentIndex(i);
            break;
        }
    }
}

void ProjectSettingsDialog::applyToProject()
{
    if (!m_projectManager) return;

    m_projectManager->setProjectName(m_projectNameEdit->text());
    m_projectManager->setAuthor(m_authorEdit->text());
    m_projectManager->setSampleRate(m_sampleRateCombo->currentText().toInt());
}

} // namespace dawcast::widgets
