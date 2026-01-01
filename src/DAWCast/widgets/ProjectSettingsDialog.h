// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QDialog>
#include <QComboBox>
#include <QSpinBox>

namespace dawcast::core { class ProjectManager; }

namespace dawcast::widgets {

class ProjectSettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit ProjectSettingsDialog(QWidget* parent = nullptr);
    ~ProjectSettingsDialog() override;

    void setProjectManager(core::ProjectManager* pm);

private:
    core::ProjectManager* m_projectManager = nullptr;

    QComboBox* m_sampleRateCombo  = nullptr;
    QComboBox* m_bitDepthCombo    = nullptr;
    QSpinBox*  m_videoWidthSpin   = nullptr;
    QSpinBox*  m_videoHeightSpin  = nullptr;
    QComboBox* m_videoFpsCombo    = nullptr;
};

} // namespace dawcast::widgets
