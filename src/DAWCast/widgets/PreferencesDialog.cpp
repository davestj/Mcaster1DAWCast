// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "PreferencesDialog.h"
#include "ThemeEngine.h"
#include "AppConfig.h"
#include "../audio_engine/AudioEngine.h"
#include "../audio_engine/AudioMixer.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QGroupBox>
#include <QPushButton>
#include <QFrame>
#include <QKeySequenceEdit>
#include <QMessageBox>
#include <QSettings>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QAction>
#include <QMainWindow>

namespace dawcast::widgets {

PreferencesDialog::PreferencesDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Preferences"));
    resize(480, 360);

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

    // Output device selector
    m_outputDeviceCombo = new QComboBox(audioTab);
    m_outputDeviceCombo->addItem(tr("System Default"), -1);
    audioLayout->addRow(tr("Output Device:"), m_outputDeviceCombo);

    // Input device selector
    m_inputDeviceCombo = new QComboBox(audioTab);
    m_inputDeviceCombo->addItem(tr("System Default"), -1);
    audioLayout->addRow(tr("Input Device:"), m_inputDeviceCombo);

    // Keep m_audioDeviceCombo as an alias for backward compatibility
    m_audioDeviceCombo = m_outputDeviceCombo;

    // Populate the device combos with real PortAudio devices
    populateAudioDevices();

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

    // ── Solo Mode section ──────────────────────────────────────────
    auto* soloGroup = new QGroupBox(tr("Solo Mode"), audioTab);
    auto* soloLayout = new QVBoxLayout(soloGroup);

    m_soloInPlaceRadio = new QRadioButton(tr("Solo in Place (SIP) — mute non-soloed tracks"), soloGroup);
    m_soloInPlaceRadio->setChecked(true);
    soloLayout->addWidget(m_soloInPlaceRadio);

    auto* sifRow = new QHBoxLayout;
    m_soloInFrontRadio = new QRadioButton(tr("Solo in Front (SIF) — dim non-soloed tracks by:"), soloGroup);
    sifRow->addWidget(m_soloInFrontRadio);

    m_soloDimSpin = new QSpinBox(soloGroup);
    m_soloDimSpin->setRange(-40, -6);
    m_soloDimSpin->setValue(-20);
    m_soloDimSpin->setSuffix(tr(" dB"));
    m_soloDimSpin->setEnabled(false);
    m_soloDimSpin->setFixedWidth(60);
    sifRow->addWidget(m_soloDimSpin);
    sifRow->addStretch();
    soloLayout->addLayout(sifRow);

    auto* soloBtnGroup = new QButtonGroup(this);
    soloBtnGroup->addButton(m_soloInPlaceRadio);
    soloBtnGroup->addButton(m_soloInFrontRadio);

    connect(m_soloInFrontRadio, &QRadioButton::toggled,
            m_soloDimSpin, &QSpinBox::setEnabled);

    audioLayout->addRow(soloGroup);

    // ── Import options ─────────────────────────────────────────────
    m_showImportDialogCheck = new QCheckBox(tr("Always show import options dialog"), audioTab);
    m_showImportDialogCheck->setChecked(true);
    audioLayout->addRow(QString(), m_showImportDialogCheck);

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
    // DAWCast is a light-only application — only the Default theme ships.
    QStringList themes = ThemeEngine::instance()->availableThemes();
    if (themes.isEmpty()) {
        themes << QStringLiteral("Default");
    }
    m_themeCombo->addItems(themes);
    QString current = ThemeEngine::instance()->currentTheme();
    int idx = m_themeCombo->findText(current);
    if (idx >= 0) m_themeCombo->setCurrentIndex(idx);
    themeFormLayout->addRow(tr("Theme:"), m_themeCombo);
    themeLayout->addLayout(themeFormLayout);

    // Theme preview area
    m_themePreview = new QFrame(themeTab);
    m_themePreview->setFrameShape(QFrame::Box);
    m_themePreview->setMinimumHeight(90);
    m_themePreview->setStyleSheet(QStringLiteral(
        "QFrame { background: #f0f0f4; border: 1px solid #c8c8d0; border-radius: 4px; }"));

    auto* previewLayout = new QVBoxLayout(m_themePreview);
    auto* previewLabel = new QLabel(tr("Theme Preview"), m_themePreview);
    previewLabel->setAlignment(Qt::AlignCenter);
    previewLabel->setStyleSheet(QStringLiteral("QLabel { color: #1a1a1a; font-size: 12px; }"));
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

    auto* shortcutsHint = new QLabel(tr("Click a shortcut cell to edit. Press the new key combination to assign it."), m_shortcutsTab);
    shortcutsHint->setStyleSheet(QStringLiteral("QLabel { color: #888; font-style: italic; font-size: 11px; }"));
    shortcutsLayout->addWidget(shortcutsHint);

    m_shortcutsTable = new QTableWidget(m_shortcutsTab);
    m_shortcutsTable->setColumnCount(2);
    m_shortcutsTable->setHorizontalHeaderLabels({tr("Action"), tr("Shortcut")});
    m_shortcutsTable->horizontalHeader()->setStretchLastSection(true);
    m_shortcutsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_shortcutsTable->verticalHeader()->setVisible(false);
    m_shortcutsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_shortcutsTable->setAlternatingRowColors(true);

    // Load saved shortcuts, or use defaults
    loadShortcuts();

    shortcutsLayout->addWidget(m_shortcutsTable);

    auto* shortcutBtnRow = new QHBoxLayout;
    shortcutBtnRow->addStretch();
    m_resetShortcutsBtn = new QPushButton(tr("Reset to Defaults"), m_shortcutsTab);
    connect(m_resetShortcutsBtn, &QPushButton::clicked, this, &PreferencesDialog::resetShortcutsToDefaults);
    shortcutBtnRow->addWidget(m_resetShortcutsBtn);
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

    // Audio — output device
    int outIdx = cfg->value(QStringLiteral("audio/outputDeviceIndex"), -1).toInt();
    for (int i = 0; i < m_outputDeviceCombo->count(); ++i) {
        if (m_outputDeviceCombo->itemData(i).toInt() == outIdx) {
            m_outputDeviceCombo->setCurrentIndex(i);
            break;
        }
    }

    // Audio — input device
    int inIdx = cfg->value(QStringLiteral("audio/inputDeviceIndex"), -1).toInt();
    for (int i = 0; i < m_inputDeviceCombo->count(); ++i) {
        if (m_inputDeviceCombo->itemData(i).toInt() == inIdx) {
            m_inputDeviceCombo->setCurrentIndex(i);
            break;
        }
    }

    QString bufSize = cfg->value(QStringLiteral("audio/bufferSize"), QStringLiteral("512")).toString();
    idx = m_bufferSizeCombo->findText(bufSize);
    if (idx >= 0) m_bufferSizeCombo->setCurrentIndex(idx);

    QString audioSr = cfg->value(QStringLiteral("audio/sampleRate"), QStringLiteral("48000")).toString();
    idx = m_sampleRateCombo->findText(audioSr);
    if (idx >= 0) m_sampleRateCombo->setCurrentIndex(idx);

    // Solo mode
    int soloMode = cfg->value(QStringLiteral("audio/soloMode"), 0).toInt();
    if (soloMode == 1 && m_soloInFrontRadio)
        m_soloInFrontRadio->setChecked(true);
    else if (m_soloInPlaceRadio)
        m_soloInPlaceRadio->setChecked(true);

    int soloDim = cfg->value(QStringLiteral("audio/soloDimDb"), -20).toInt();
    if (m_soloDimSpin)
        m_soloDimSpin->setValue(qBound(-40, soloDim, -6));

    // Import dialog
    bool showImport = cfg->value(QStringLiteral("audio/showImportDialog"), true).toBool();
    if (m_showImportDialogCheck)
        m_showImportDialogCheck->setChecked(showImport);

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

    // Audio — device indices
    int outDevIdx = m_outputDeviceCombo->currentData().toInt();
    int inDevIdx  = m_inputDeviceCombo->currentData().toInt();
    cfg->setValue(QStringLiteral("audio/outputDeviceIndex"), outDevIdx);
    cfg->setValue(QStringLiteral("audio/inputDeviceIndex"),  inDevIdx);
    cfg->setValue(QStringLiteral("audio/bufferSize"), m_bufferSizeCombo->currentText());
    cfg->setValue(QStringLiteral("audio/sampleRate"), m_sampleRateCombo->currentText());

    // Apply device selection to the AudioEngine if available
    if (m_audioEngine) {
        m_audioEngine->setOutputDevice(outDevIdx);
        m_audioEngine->setInputDevice(inDevIdx);

        int sr = m_sampleRateCombo->currentText().toInt();
        if (sr > 0) m_audioEngine->setSampleRate(sr);

        int bs = m_bufferSizeCombo->currentText().toInt();
        if (bs > 0) m_audioEngine->setBufferSize(bs);
    }

    // Solo mode
    int soloModeVal = (m_soloInFrontRadio && m_soloInFrontRadio->isChecked()) ? 1 : 0;
    cfg->setValue(QStringLiteral("audio/soloMode"), soloModeVal);
    if (m_soloDimSpin)
        cfg->setValue(QStringLiteral("audio/soloDimDb"), m_soloDimSpin->value());

    // Apply solo mode to mixer
    if (m_audioMixer) {
        m_audioMixer->setSoloMode(soloModeVal == 1
            ? dawcast::AudioMixer::SoloInFront
            : dawcast::AudioMixer::SoloInPlace);
        if (m_soloDimSpin)
            m_audioMixer->setSoloDimDb(static_cast<float>(m_soloDimSpin->value()));
    }

    // Import dialog preference
    if (m_showImportDialogCheck)
        cfg->setValue(QStringLiteral("audio/showImportDialog"), m_showImportDialogCheck->isChecked());

    // Video
    cfg->setValue(QStringLiteral("video/defaultResolution"), m_videoResolutionCombo->currentIndex());
    cfg->setValue(QStringLiteral("video/defaultFramerate"), m_videoFramerateCombo->currentIndex());
    cfg->setValue(QStringLiteral("video/hwAcceleration"), m_hwAccelCheck->isChecked());

    // Theme — apply immediately
    QString themeName = m_themeCombo->currentText();
    cfg->setValue(QStringLiteral("theme/name"), themeName);
    cfg->setValue(QStringLiteral("theme/index"), m_themeCombo->currentIndex());
    ThemeEngine::instance()->loadTheme(themeName);

    // Persist the user's theme choice to QSettings so main.cpp picks it
    // up on next launch (main.cpp reads QSettings before AppConfig exists).
    {
        QSettings qs;
        qs.setValue(QStringLiteral("appearance/theme"), themeName);
        qs.setValue(QStringLiteral("appearance/theme_user_set"), true);
    }

    // Shortcuts
    saveShortcuts();

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

// ── Audio Device Enumeration ───────────────────────────────────────────────

void PreferencesDialog::setAudioEngine(dawcast::AudioEngine* engine)
{
    m_audioEngine = engine;
}

void PreferencesDialog::setAudioMixer(dawcast::AudioMixer* mixer)
{
    m_audioMixer = mixer;
}

void PreferencesDialog::populateAudioDevices()
{
    QList<dawcast::AudioDeviceInfo> devices = dawcast::AudioEngine::enumerateDevices();

    for (const auto& dev : devices) {
        // Add devices with output channels to the output combo
        if (dev.maxOutputChannels > 0) {
            QString label = dev.name;
            if (dev.isDefaultOutput) label += tr(" (default)");
            m_outputDeviceCombo->addItem(label, dev.index);
        }

        // Add devices with input channels to the input combo
        if (dev.maxInputChannels > 0) {
            QString label = dev.name;
            if (dev.isDefaultInput) label += tr(" (default)");
            m_inputDeviceCombo->addItem(label, dev.index);
        }
    }
}

// ── Shortcut Defaults ──────────────────────────────────────────────────────

QList<ShortcutEntry> PreferencesDialog::defaultShortcuts()
{
    return {
        { QStringLiteral("New Project"),       QKeySequence(QStringLiteral("Ctrl+N")) },
        { QStringLiteral("Open Project"),      QKeySequence(QStringLiteral("Ctrl+O")) },
        { QStringLiteral("Save Project"),      QKeySequence(QStringLiteral("Ctrl+S")) },
        { QStringLiteral("Save As..."),        QKeySequence(QStringLiteral("Ctrl+Shift+S")) },
        { QStringLiteral("Export..."),         QKeySequence(QStringLiteral("Ctrl+E")) },
        { QStringLiteral("Undo"),              QKeySequence(QStringLiteral("Ctrl+Z")) },
        { QStringLiteral("Redo"),              QKeySequence(QStringLiteral("Ctrl+Shift+Z")) },
        { QStringLiteral("Cut"),               QKeySequence(QStringLiteral("Ctrl+X")) },
        { QStringLiteral("Copy"),              QKeySequence(QStringLiteral("Ctrl+C")) },
        { QStringLiteral("Paste"),             QKeySequence(QStringLiteral("Ctrl+V")) },
        { QStringLiteral("Delete"),            QKeySequence(QStringLiteral("Delete")) },
        { QStringLiteral("Play/Pause"),        QKeySequence(QStringLiteral("Space")) },
        { QStringLiteral("Stop"),              QKeySequence(QStringLiteral("Escape")) },
        { QStringLiteral("Record"),            QKeySequence(QStringLiteral("R")) },
        { QStringLiteral("Zoom In"),           QKeySequence(QStringLiteral("Ctrl+=")) },
        { QStringLiteral("Zoom Out"),          QKeySequence(QStringLiteral("Ctrl+-")) },
        { QStringLiteral("Zoom to Fit"),       QKeySequence(QStringLiteral("Ctrl+0")) },
        { QStringLiteral("Add Audio Track"),   QKeySequence(QStringLiteral("Ctrl+Shift+A")) },
        { QStringLiteral("Add Video Track"),   QKeySequence(QStringLiteral("Ctrl+Shift+V")) },
        { QStringLiteral("Add MIDI Track"),    QKeySequence(QStringLiteral("Ctrl+Shift+M")) },
    };
}

// ── Shortcut Table Population ──────────────────────────────────────────────

void PreferencesDialog::populateShortcutsTable(const QList<ShortcutEntry>& entries)
{
    m_shortcutsTable->setRowCount(entries.size());

    for (int i = 0; i < entries.size(); ++i) {
        // Column 0: Action name (read-only)
        auto* actionItem = new QTableWidgetItem(entries[i].action);
        actionItem->setFlags(actionItem->flags() & ~Qt::ItemIsEditable);
        m_shortcutsTable->setItem(i, 0, actionItem);

        // Column 1: Editable shortcut via QKeySequenceEdit widget
        auto* keySeqEdit = new QKeySequenceEdit(entries[i].shortcut, m_shortcutsTable);
        keySeqEdit->setMaximumSequenceLength(1);

        // When the user finishes editing, check for conflicts
        int row = i;
        connect(keySeqEdit, &QKeySequenceEdit::editingFinished, this, [this, keySeqEdit, row]() {
            QKeySequence seq = keySeqEdit->keySequence();
            if (!seq.isEmpty() && checkConflict(row, seq)) {
                // Conflict found -- revert
                QMessageBox::warning(this, tr("Shortcut Conflict"),
                    tr("The shortcut \"%1\" is already assigned to another action.\n"
                       "Please choose a different shortcut.").arg(seq.toString()));
                // Restore previous value from the table item data
                auto* item = m_shortcutsTable->item(row, 0);
                if (item) {
                    QKeySequence prev = QKeySequence(item->data(Qt::UserRole).toString());
                    keySeqEdit->setKeySequence(prev);
                }
                return;
            }
            // Store current sequence as previous for future conflict checks
            auto* item = m_shortcutsTable->item(row, 0);
            if (item) {
                item->setData(Qt::UserRole, seq.toString());
            }
        });

        // Store the initial sequence as "previous" in UserRole
        actionItem->setData(Qt::UserRole, entries[i].shortcut.toString());

        m_shortcutsTable->setCellWidget(i, 1, keySeqEdit);
    }
}

// ── Conflict Detection ─────────────────────────────────────────────────────

bool PreferencesDialog::checkConflict(int editingRow, const QKeySequence& seq)
{
    for (int i = 0; i < m_shortcutsTable->rowCount(); ++i) {
        if (i == editingRow) continue;

        auto* widget = qobject_cast<QKeySequenceEdit*>(m_shortcutsTable->cellWidget(i, 1));
        if (widget && widget->keySequence() == seq) {
            return true; // conflict
        }
    }
    return false;
}

// ── Reset to Defaults ──────────────────────────────────────────────────────

void PreferencesDialog::resetShortcutsToDefaults()
{
    populateShortcutsTable(defaultShortcuts());
}

// ── Current Shortcuts from Table ───────────────────────────────────────────

QList<ShortcutEntry> PreferencesDialog::currentShortcuts() const
{
    QList<ShortcutEntry> entries;
    for (int i = 0; i < m_shortcutsTable->rowCount(); ++i) {
        ShortcutEntry entry;
        auto* actionItem = m_shortcutsTable->item(i, 0);
        if (actionItem) {
            entry.action = actionItem->text();
        }
        auto* widget = qobject_cast<QKeySequenceEdit*>(m_shortcutsTable->cellWidget(i, 1));
        if (widget) {
            entry.shortcut = widget->keySequence();
        }
        entries.append(entry);
    }
    return entries;
}

// ── Save/Load Shortcuts to AppConfig ───────────────────────────────────────

void PreferencesDialog::saveShortcuts()
{
    auto* cfg = config::AppConfig::instance();
    if (!cfg) return;

    QJsonArray arr;
    QList<ShortcutEntry> entries = currentShortcuts();
    for (const auto& entry : entries) {
        QJsonObject obj;
        obj[QStringLiteral("action")]   = entry.action;
        obj[QStringLiteral("shortcut")] = entry.shortcut.toString();
        arr.append(obj);
    }

    cfg->setValue(QStringLiteral("shortcuts"), QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

void PreferencesDialog::loadShortcuts()
{
    auto* cfg = config::AppConfig::instance();
    QList<ShortcutEntry> entries;

    if (cfg) {
        QString jsonStr = cfg->value(QStringLiteral("shortcuts")).toString();
        if (!jsonStr.isEmpty()) {
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(), &err);
            if (err.error == QJsonParseError::NoError && doc.isArray()) {
                QJsonArray arr = doc.array();
                for (const auto& val : arr) {
                    QJsonObject obj = val.toObject();
                    ShortcutEntry entry;
                    entry.action   = obj[QStringLiteral("action")].toString();
                    entry.shortcut = QKeySequence(obj[QStringLiteral("shortcut")].toString());
                    entries.append(entry);
                }
            }
        }
    }

    // If no saved shortcuts or parse failed, use defaults
    if (entries.isEmpty()) {
        entries = defaultShortcuts();
    }

    populateShortcutsTable(entries);
}

// ── Apply Shortcuts to QActions ────────────────────────────────────────────

void PreferencesDialog::applyShortcuts(QMainWindow* window)
{
    if (!window) return;

    QList<ShortcutEntry> entries = currentShortcuts();

    // Build a map of action text -> shortcut
    QMap<QString, QKeySequence> shortcutMap;
    for (const auto& entry : entries) {
        shortcutMap[entry.action] = entry.shortcut;
    }

    // Walk all QActions in the window and match by text
    const QList<QAction*> actions = window->findChildren<QAction*>();
    for (QAction* action : actions) {
        // Strip '&' accelerator markers for comparison
        QString actionText = action->text().remove(QLatin1Char('&'));

        // Try exact match first
        if (shortcutMap.contains(actionText)) {
            action->setShortcut(shortcutMap[actionText]);
            continue;
        }

        // Try matching without "..." suffix
        QString stripped = actionText;
        stripped.remove(QStringLiteral("..."));
        stripped = stripped.trimmed();
        if (shortcutMap.contains(stripped)) {
            action->setShortcut(shortcutMap[stripped]);
        }
    }
}

} // namespace dawcast::widgets
