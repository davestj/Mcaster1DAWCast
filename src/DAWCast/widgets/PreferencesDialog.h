// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QDialog>
#include <QTabWidget>
#include <QComboBox>

namespace dawcast::widgets {

class PreferencesDialog : public QDialog {
    Q_OBJECT

public:
    explicit PreferencesDialog(QWidget* parent = nullptr);
    ~PreferencesDialog() override;

    void loadSettings();
    void saveSettings();

private:
    QTabWidget* m_tabs = nullptr;

    // General tab
    QWidget* m_generalTab = nullptr;

    // Audio tab
    QComboBox* m_audioDeviceCombo  = nullptr;
    QComboBox* m_sampleRateCombo   = nullptr;
    QComboBox* m_bufferSizeCombo   = nullptr;

    // Video tab
    QWidget* m_videoTab = nullptr;

    // Theme tab
    QComboBox* m_themeCombo = nullptr;

    // Shortcuts tab
    QWidget* m_shortcutsTab = nullptr;
};

} // namespace dawcast::widgets
