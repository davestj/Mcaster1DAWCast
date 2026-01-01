// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "PreferencesDialog.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>

namespace dawcast::widgets {

PreferencesDialog::PreferencesDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Preferences"));
    resize(600, 400);

    auto* mainLayout = new QVBoxLayout(this);

    m_tabs = new QTabWidget(this);

    // --- General tab ---
    m_generalTab = new QWidget(m_tabs);
    auto* generalLayout = new QFormLayout(m_generalTab);
    generalLayout->addRow(tr("Language:"), new QComboBox(m_generalTab));
    m_tabs->addTab(m_generalTab, tr("General"));

    // --- Audio tab ---
    auto* audioTab = new QWidget(m_tabs);
    auto* audioLayout = new QFormLayout(audioTab);

    m_audioDeviceCombo = new QComboBox(audioTab);
    audioLayout->addRow(tr("Audio Device:"), m_audioDeviceCombo);

    m_sampleRateCombo = new QComboBox(audioTab);
    m_sampleRateCombo->addItems({QStringLiteral("44100"), QStringLiteral("48000"),
                                  QStringLiteral("96000")});
    audioLayout->addRow(tr("Sample Rate:"), m_sampleRateCombo);

    m_bufferSizeCombo = new QComboBox(audioTab);
    m_bufferSizeCombo->addItems({QStringLiteral("64"), QStringLiteral("128"),
                                  QStringLiteral("256"), QStringLiteral("512"),
                                  QStringLiteral("1024"), QStringLiteral("2048")});
    m_bufferSizeCombo->setCurrentIndex(3);
    audioLayout->addRow(tr("Buffer Size:"), m_bufferSizeCombo);

    m_tabs->addTab(audioTab, tr("Audio"));

    // --- Video tab ---
    m_videoTab = new QWidget(m_tabs);
    auto* videoLayout = new QFormLayout(m_videoTab);
    videoLayout->addRow(tr("Video settings placeholder"), new QLabel(m_videoTab));
    m_tabs->addTab(m_videoTab, tr("Video"));

    // --- Theme tab ---
    auto* themeTab = new QWidget(m_tabs);
    auto* themeLayout = new QFormLayout(themeTab);
    m_themeCombo = new QComboBox(themeTab);
    themeLayout->addRow(tr("Theme:"), m_themeCombo);
    m_tabs->addTab(themeTab, tr("Theme"));

    // --- Shortcuts tab ---
    m_shortcutsTab = new QWidget(m_tabs);
    auto* shortcutsLayout = new QVBoxLayout(m_shortcutsTab);
    shortcutsLayout->addWidget(new QLabel(tr("Keyboard shortcuts placeholder"), m_shortcutsTab));
    m_tabs->addTab(m_shortcutsTab, tr("Keyboard Shortcuts"));

    mainLayout->addWidget(m_tabs);

    // OK / Cancel
    auto* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        saveSettings();
        accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

PreferencesDialog::~PreferencesDialog() = default;

void PreferencesDialog::loadSettings()
{
    // TODO: load from AppConfig
}

void PreferencesDialog::saveSettings()
{
    // TODO: save to AppConfig
}

} // namespace dawcast::widgets
