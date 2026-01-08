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
class QCheckBox;
class QLabel;
class QTableWidget;
class QFrame;
class QPushButton;
class QMainWindow;

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

    void loadSettings();
    void saveSettings();

    // Apply the current shortcut mappings to QActions in the given window
    void applyShortcuts(QMainWindow* window);

    // Default shortcut definitions
    static QList<ShortcutEntry> defaultShortcuts();

private:
    void updateLatencyLabel();
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
    QComboBox* m_audioDeviceCombo  = nullptr;
    QComboBox* m_sampleRateCombo   = nullptr;
    QComboBox* m_bufferSizeCombo   = nullptr;
    QLabel*    m_latencyLabel      = nullptr;

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
};

} // namespace dawcast::widgets
