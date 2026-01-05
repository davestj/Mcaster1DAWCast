// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QDialog>
#include <QComboBox>
#include <QSpinBox>

class QLineEdit;

namespace dawcast { class ProjectManager; }

namespace dawcast::widgets {

class ProjectSettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit ProjectSettingsDialog(QWidget* parent = nullptr);
    ~ProjectSettingsDialog() override;

    void setProjectManager(ProjectManager* pm);

private:
    void applyToProject();

    ProjectManager* m_projectManager = nullptr;

    QLineEdit* m_projectNameEdit     = nullptr;
    QLineEdit* m_authorEdit          = nullptr;
    QComboBox* m_sampleRateCombo     = nullptr;
    QComboBox* m_bitDepthCombo       = nullptr;
    QComboBox* m_videoResolutionCombo = nullptr;
    QComboBox* m_videoFpsCombo       = nullptr;
};

} // namespace dawcast::widgets
