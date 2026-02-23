// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QDialog>
#include <QTabWidget>
#include <QComboBox>
#include <QKeySequenceEdit>
#include <QJsonArray>
#include <QMap>

class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QLabel;
class QTableWidget;
class QFrame;
class QPushButton;
class QRadioButton;
class QMainWindow;

namespace dawcast { class AudioEngine; }
namespace dawcast { class AudioMixer; }

namespace dawcast::widgets {

// ── ShortcutEntry ──────────────────────────────────────────────────────────
struct ShortcutEntry {
    QString action;
    QKeySequence shortcut;
};

// ── PreferencesDialog ──────────────────────────────────────────────────────
class PreferencesDialog : public QDialog {
    Q_OBJECT

public:
    explicit PreferencesDialog(QWidget* parent = nullptr);
    ~PreferencesDialog() override;

    /// Set the audio engine so device changes can be applied immediately.
    void setAudioEngine(dawcast::AudioEngine* engine);

    /// Set the audio mixer so solo mode changes can be applied.
    void setAudioMixer(dawcast::AudioMixer* mixer);

    void loadSettings();
    void saveSettings();

    // Apply the current shortcut mappings to QActions in the given window
    void applyShortcuts(QMainWindow* window);

    // Default shortcut definitions
    static QList<ShortcutEntry> defaultShortcuts();

private:
    void updateLatencyLabel();
    void populateAudioDevices();
    void populateShortcutsTable(const QList<ShortcutEntry>& entries);
    void resetShortcutsToDefaults();
    bool checkConflict(int editingRow, const QKeySequence& seq);
    QList<ShortcutEntry> currentShortcuts() const;
    void saveShortcuts();
    void loadShortcuts();

    QTabWidget* m_tabs = nullptr;

    // General tab
    QWidget*   m_generalTab            = nullptr;
    QComboBox* m_defaultSampleRateCombo = nullptr;
    QComboBox* m_defaultBitDepthCombo  = nullptr;
    QSpinBox*  m_autoSaveIntervalSpin  = nullptr;

    // Audio tab
    QComboBox* m_outputDeviceCombo = nullptr;   ///< Output device selector
    QComboBox* m_inputDeviceCombo  = nullptr;   ///< Input device selector
    QComboBox* m_audioDeviceCombo  = nullptr;   ///< Legacy alias (points to output)
    QComboBox* m_sampleRateCombo   = nullptr;
    QComboBox* m_bufferSizeCombo   = nullptr;
    QLabel*    m_latencyLabel      = nullptr;

    // Audio tab — solo mode
    QRadioButton*   m_soloInPlaceRadio  = nullptr;
    QRadioButton*   m_soloInFrontRadio  = nullptr;
    QSpinBox*       m_soloDimSpin       = nullptr;
    QCheckBox*      m_showImportDialogCheck = nullptr;

    // Video tab
    QWidget*   m_videoTab              = nullptr;
    QComboBox* m_videoResolutionCombo  = nullptr;
    QComboBox* m_videoFramerateCombo   = nullptr;
    QCheckBox* m_hwAccelCheck          = nullptr;

    // Theme tab
    QComboBox* m_themeCombo   = nullptr;
    QFrame*    m_themePreview = nullptr;

    // Shortcuts tab
    QWidget*       m_shortcutsTab     = nullptr;
    QTableWidget*  m_shortcutsTable   = nullptr;
    QPushButton*   m_resetShortcutsBtn = nullptr;

    dawcast::AudioEngine* m_audioEngine = nullptr;
    dawcast::AudioMixer*  m_audioMixer  = nullptr;
};

} // namespace dawcast::widgets
