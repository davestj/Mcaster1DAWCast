// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QDialog>
#include <QTabWidget>
#include <QComboBox>

class QSpinBox;
class QCheckBox;
class QLabel;
class QTableWidget;
class QFrame;

namespace dawcast::widgets {

class PreferencesDialog : public QDialog {
    Q_OBJECT

public:
    explicit PreferencesDialog(QWidget* parent = nullptr);
    ~PreferencesDialog() override;

    void loadSettings();
    void saveSettings();

private:
    void updateLatencyLabel();

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
    QWidget*       m_shortcutsTab   = nullptr;
    QTableWidget*  m_shortcutsTable = nullptr;
};

} // namespace dawcast::widgets
