// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "PreferencesDialog.h"
#include "AppConfig.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QGroupBox>
#include <QPushButton>
#include <QFrame>

namespace dawcast::widgets {

PreferencesDialog::PreferencesDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Preferences"));
    resize(640, 480);

    auto* mainLayout = new QVBoxLayout(this);

    m_tabs = new QTabWidget(this);

    // =========================================================================
    // Tab 1: General
    // =========================================================================
    m_generalTab = new QWidget(m_tabs);
    auto* generalLayout = new QFormLayout(m_generalTab);
    generalLayout->setContentsMargins(12, 12, 12, 12);
    generalLayout->setSpacing(8);

    m_defaultSampleRateCombo = new QComboBox(m_generalTab);
    m_defaultSampleRateCombo->addItems({
        QStringLiteral("44100"), QStringLiteral("48000"), QStringLiteral("96000")
    });
    m_defaultSampleRateCombo->setCurrentIndex(1);
    generalLayout->addRow(tr("Default Sample Rate:"), m_defaultSampleRateCombo);

    m_defaultBitDepthCombo = new QComboBox(m_generalTab);
    m_defaultBitDepthCombo->addItems({
        QStringLiteral("16-bit"), QStringLiteral("24-bit"), QStringLiteral("32-bit float")
    });
    m_defaultBitDepthCombo->setCurrentIndex(2);
    generalLayout->addRow(tr("Default Bit Depth:"), m_defaultBitDepthCombo);

    m_autoSaveIntervalSpin = new QSpinBox(m_generalTab);
    m_autoSaveIntervalSpin->setRange(0, 60);
    m_autoSaveIntervalSpin->setValue(5);
    m_autoSaveIntervalSpin->setSuffix(tr(" min"));
    m_autoSaveIntervalSpin->setSpecialValueText(tr("Disabled"));
    generalLayout->addRow(tr("Auto-save Interval:"), m_autoSaveIntervalSpin);

    m_tabs->addTab(m_generalTab, tr("General"));

    // =========================================================================
    // Tab 2: Audio
    // =========================================================================
    auto* audioTab = new QWidget(m_tabs);
    auto* audioLayout = new QFormLayout(audioTab);
    audioLayout->setContentsMargins(12, 12, 12, 12);
    audioLayout->setSpacing(8);

    m_audioDeviceCombo = new QComboBox(audioTab);
    m_audioDeviceCombo->addItem(tr("System Default"));
    // In a real implementation, this would enumerate PortAudio devices
    // For now, add placeholder devices
    m_audioDeviceCombo->addItem(tr("Built-in Output"));
    m_audioDeviceCombo->addItem(tr("Built-in Input"));
    audioLayout->addRow(tr("Audio Device:"), m_audioDeviceCombo);

    m_bufferSizeCombo = new QComboBox(audioTab);
    m_bufferSizeCombo->addItems({
        QStringLiteral("64"), QStringLiteral("128"), QStringLiteral("256"),
        QStringLiteral("512"), QStringLiteral("1024"), QStringLiteral("2048"),
        QStringLiteral("4096")
    });
    m_bufferSizeCombo->setCurrentIndex(3); // 512
    audioLayout->addRow(tr("Buffer Size:"), m_bufferSizeCombo);

    m_sampleRateCombo = new QComboBox(audioTab);
    m_sampleRateCombo->addItems({
        QStringLiteral("44100"), QStringLiteral("48000"), QStringLiteral("96000")
    });
    m_sampleRateCombo->setCurrentIndex(1);
    audioLayout->addRow(tr("Sample Rate:"), m_sampleRateCombo);

    // Latency info label
    m_latencyLabel = new QLabel(audioTab);
    m_latencyLabel->setStyleSheet(QStringLiteral("QLabel { color: #888; font-style: italic; }"));
    audioLayout->addRow(tr("Estimated Latency:"), m_latencyLabel);
    updateLatencyLabel();

    // Update latency when buffer size or sample rate changes
    connect(m_bufferSizeCombo, &QComboBox::currentIndexChanged, this, &PreferencesDialog::updateLatencyLabel);
    connect(m_sampleRateCombo, &QComboBox::currentIndexChanged, this, &PreferencesDialog::updateLatencyLabel);

    m_tabs->addTab(audioTab, tr("Audio"));

    // =========================================================================
    // Tab 3: Video
    // =========================================================================
    m_videoTab = new QWidget(m_tabs);
    auto* videoLayout = new QFormLayout(m_videoTab);
    videoLayout->setContentsMargins(12, 12, 12, 12);
    videoLayout->setSpacing(8);

    m_videoResolutionCombo = new QComboBox(m_videoTab);
    m_videoResolutionCombo->addItems({
        tr("720p (1280x720)"), tr("1080p (1920x1080)"), tr("4K (3840x2160)")
    });
    m_videoResolutionCombo->setCurrentIndex(1);
    videoLayout->addRow(tr("Default Resolution:"), m_videoResolutionCombo);

    m_videoFramerateCombo = new QComboBox(m_videoTab);
    m_videoFramerateCombo->addItems({
        QStringLiteral("24"), QStringLiteral("25"),
        QStringLiteral("30"), QStringLiteral("60")
    });
    m_videoFramerateCombo->setCurrentIndex(2);
    videoLayout->addRow(tr("Default Framerate:"), m_videoFramerateCombo);

    m_hwAccelCheck = new QCheckBox(tr("Enable hardware acceleration"), m_videoTab);
    m_hwAccelCheck->setChecked(true);
    videoLayout->addRow(QString(), m_hwAccelCheck);

    m_tabs->addTab(m_videoTab, tr("Video"));

    // =========================================================================
    // Tab 4: Theme
    // =========================================================================
    auto* themeTab = new QWidget(m_tabs);
    auto* themeLayout = new QVBoxLayout(themeTab);
    themeLayout->setContentsMargins(12, 12, 12, 12);
    themeLayout->setSpacing(8);

    auto* themeFormLayout = new QFormLayout;
    m_themeCombo = new QComboBox(themeTab);
    m_themeCombo->addItems({
        tr("Dark (Default)"), tr("Midnight Blue"), tr("Charcoal"),
        tr("Studio Gray"), tr("High Contrast")
    });
    themeFormLayout->addRow(tr("Theme:"), m_themeCombo);
    themeLayout->addLayout(themeFormLayout);

    // Theme preview area
    m_themePreview = new QFrame(themeTab);
    m_themePreview->setFrameShape(QFrame::Box);
    m_themePreview->setMinimumHeight(120);
    m_themePreview->setStyleSheet(QStringLiteral(
        "QFrame { background: #2a2a30; border: 1px solid #555; border-radius: 4px; }"));

    auto* previewLayout = new QVBoxLayout(m_themePreview);
    auto* previewLabel = new QLabel(tr("Theme Preview"), m_themePreview);
    previewLabel->setAlignment(Qt::AlignCenter);
    previewLabel->setStyleSheet(QStringLiteral("QLabel { color: #aaa; font-size: 12px; }"));
    previewLayout->addWidget(previewLabel);

    // Sample UI elements in preview
    auto* previewBtnRow = new QHBoxLayout;
    auto* previewBtn1 = new QPushButton(tr("Button"), m_themePreview);
    auto* previewBtn2 = new QPushButton(tr("Active"), m_themePreview);
    previewBtn2->setCheckable(true);
    previewBtn2->setChecked(true);
    previewBtnRow->addWidget(previewBtn1);
    previewBtnRow->addWidget(previewBtn2);
    previewBtnRow->addStretch();
    previewLayout->addLayout(previewBtnRow);

    themeLayout->addWidget(m_themePreview, 1);

    // Update preview when theme changes
    connect(m_themeCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        static const char* bgColors[] = { "#2a2a30", "#1a2040", "#303030", "#484848", "#000000" };
        if (index >= 0 && index < 5) {
            m_themePreview->setStyleSheet(
                QString("QFrame { background: %1; border: 1px solid #555; border-radius: 4px; }")
                    .arg(bgColors[index]));
        }
    });

    m_tabs->addTab(themeTab, tr("Theme"));

    // =========================================================================
    // Tab 5: Keyboard Shortcuts
    // =========================================================================
    m_shortcutsTab = new QWidget(m_tabs);
    auto* shortcutsLayout = new QVBoxLayout(m_shortcutsTab);
    shortcutsLayout->setContentsMargins(12, 12, 12, 12);

    m_shortcutsTable = new QTableWidget(m_shortcutsTab);
    m_shortcutsTable->setColumnCount(2);
    m_shortcutsTable->setHorizontalHeaderLabels({tr("Action"), tr("Shortcut")});
    m_shortcutsTable->horizontalHeader()->setStretchLastSection(true);
    m_shortcutsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_shortcutsTable->verticalHeader()->setVisible(false);
    m_shortcutsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_shortcutsTable->setAlternatingRowColors(true);

    // Populate with default shortcuts
    struct ShortcutEntry { const char* action; const char* key; };
    static const ShortcutEntry defaultShortcuts[] = {
        {"New Project",     "Ctrl+N"},
        {"Open Project",    "Ctrl+O"},
        {"Save Project",    "Ctrl+S"},
        {"Save As...",      "Ctrl+Shift+S"},
        {"Export...",        "Ctrl+E"},
        {"Undo",            "Ctrl+Z"},
        {"Redo",            "Ctrl+Shift+Z"},
        {"Cut",             "Ctrl+X"},
        {"Copy",            "Ctrl+C"},
        {"Paste",           "Ctrl+V"},
        {"Delete",          "Delete"},
        {"Play/Pause",      "Space"},
        {"Stop",            "Escape"},
        {"Record",          "R"},
        {"Zoom In",         "Ctrl+="},
        {"Zoom Out",        "Ctrl+-"},
        {"Zoom to Fit",     "Ctrl+0"},
    };
    constexpr int shortcutCount = sizeof(defaultShortcuts) / sizeof(defaultShortcuts[0]);

    m_shortcutsTable->setRowCount(shortcutCount);
    for (int i = 0; i < shortcutCount; ++i) {
        auto* actionItem = new QTableWidgetItem(tr(defaultShortcuts[i].action));
        actionItem->setFlags(actionItem->flags() & ~Qt::ItemIsEditable);
        m_shortcutsTable->setItem(i, 0, actionItem);
        m_shortcutsTable->setItem(i, 1, new QTableWidgetItem(
            QString::fromLatin1(defaultShortcuts[i].key)));
    }

    shortcutsLayout->addWidget(m_shortcutsTable);

    auto* shortcutBtnRow = new QHBoxLayout;
    shortcutBtnRow->addStretch();
    auto* resetShortcutsBtn = new QPushButton(tr("Reset to Defaults"), m_shortcutsTab);
    shortcutBtnRow->addWidget(resetShortcutsBtn);
    shortcutsLayout->addLayout(shortcutBtnRow);

    m_tabs->addTab(m_shortcutsTab, tr("Keyboard Shortcuts"));

    // =========================================================================
    mainLayout->addWidget(m_tabs);

    // OK / Cancel / Apply
    auto* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply, this);
    mainLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        saveSettings();
        accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, [this]() {
        saveSettings();
    });
}

PreferencesDialog::~PreferencesDialog() = default;

void PreferencesDialog::loadSettings()
{
    auto* cfg = config::AppConfig::instance();
    if (!cfg) return;

    // General
    QString sr = cfg->value(QStringLiteral("general/defaultSampleRate"), QStringLiteral("48000")).toString();
    int idx = m_defaultSampleRateCombo->findText(sr);
    if (idx >= 0) m_defaultSampleRateCombo->setCurrentIndex(idx);

    QString bd = cfg->value(QStringLiteral("general/defaultBitDepth"), QStringLiteral("32-bit float")).toString();
    idx = m_defaultBitDepthCombo->findText(bd);
    if (idx >= 0) m_defaultBitDepthCombo->setCurrentIndex(idx);

    int autoSave = cfg->value(QStringLiteral("general/autoSaveInterval"), 5).toInt();
    m_autoSaveIntervalSpin->setValue(autoSave);

    // Audio
    QString device = cfg->value(QStringLiteral("audio/device"), tr("System Default")).toString();
    idx = m_audioDeviceCombo->findText(device);
    if (idx >= 0) m_audioDeviceCombo->setCurrentIndex(idx);

    QString bufSize = cfg->value(QStringLiteral("audio/bufferSize"), QStringLiteral("512")).toString();
    idx = m_bufferSizeCombo->findText(bufSize);
    if (idx >= 0) m_bufferSizeCombo->setCurrentIndex(idx);

    QString audioSr = cfg->value(QStringLiteral("audio/sampleRate"), QStringLiteral("48000")).toString();
    idx = m_sampleRateCombo->findText(audioSr);
    if (idx >= 0) m_sampleRateCombo->setCurrentIndex(idx);

    // Video
    int resIdx = cfg->value(QStringLiteral("video/defaultResolution"), 1).toInt();
    m_videoResolutionCombo->setCurrentIndex(qBound(0, resIdx, m_videoResolutionCombo->count() - 1));

    int fpsIdx = cfg->value(QStringLiteral("video/defaultFramerate"), 2).toInt();
    m_videoFramerateCombo->setCurrentIndex(qBound(0, fpsIdx, m_videoFramerateCombo->count() - 1));

    bool hwAccel = cfg->value(QStringLiteral("video/hwAcceleration"), true).toBool();
    m_hwAccelCheck->setChecked(hwAccel);

    // Theme
    int themeIdx = cfg->value(QStringLiteral("theme/index"), 0).toInt();
    m_themeCombo->setCurrentIndex(qBound(0, themeIdx, m_themeCombo->count() - 1));
}

void PreferencesDialog::saveSettings()
{
    auto* cfg = config::AppConfig::instance();
    if (!cfg) return;

    // General
    cfg->setValue(QStringLiteral("general/defaultSampleRate"), m_defaultSampleRateCombo->currentText());
    cfg->setValue(QStringLiteral("general/defaultBitDepth"), m_defaultBitDepthCombo->currentText());
    cfg->setValue(QStringLiteral("general/autoSaveInterval"), m_autoSaveIntervalSpin->value());

    // Audio
    cfg->setValue(QStringLiteral("audio/device"), m_audioDeviceCombo->currentText());
    cfg->setValue(QStringLiteral("audio/bufferSize"), m_bufferSizeCombo->currentText());
    cfg->setValue(QStringLiteral("audio/sampleRate"), m_sampleRateCombo->currentText());

    // Video
    cfg->setValue(QStringLiteral("video/defaultResolution"), m_videoResolutionCombo->currentIndex());
    cfg->setValue(QStringLiteral("video/defaultFramerate"), m_videoFramerateCombo->currentIndex());
    cfg->setValue(QStringLiteral("video/hwAcceleration"), m_hwAccelCheck->isChecked());

    // Theme
    cfg->setValue(QStringLiteral("theme/index"), m_themeCombo->currentIndex());

    cfg->save();
}

void PreferencesDialog::updateLatencyLabel()
{
    int bufferSize = m_bufferSizeCombo->currentText().toInt();
    int sampleRate = m_sampleRateCombo->currentText().toInt();
    if (sampleRate > 0 && bufferSize > 0) {
        double latencyMs = static_cast<double>(bufferSize) / sampleRate * 1000.0;
        m_latencyLabel->setText(QString::number(latencyMs, 'f', 1) + tr(" ms"));
    }
}

} // namespace dawcast::widgets
