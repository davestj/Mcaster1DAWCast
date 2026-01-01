// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QDialog>
#include <QString>
#include <QComboBox>
#include <QSpinBox>

namespace dawcast::core { class ProjectManager; }

namespace dawcast::widgets {

struct ExportSettings {
    bool   audioOnly    = true;
    QString audioCodec  = QStringLiteral("aac");
    int    audioBitrate = 256;
    int    sampleRate   = 48000;
    QString videoCodec  = QStringLiteral("h264");
    int    videoWidth   = 1920;
    int    videoHeight  = 1080;
    int    videoFps     = 30;
};

class ExportDialog : public QDialog {
    Q_OBJECT

public:
    explicit ExportDialog(QWidget* parent = nullptr);
    ~ExportDialog() override;

    void setProject(core::ProjectManager* project);
    ExportSettings exportSettings() const;

private:
    core::ProjectManager* m_project = nullptr;

    QComboBox* m_formatCombo    = nullptr;
    QComboBox* m_audioCodecCombo = nullptr;
    QSpinBox*  m_bitrateSpin    = nullptr;
    QComboBox* m_sampleRateCombo = nullptr;
    QComboBox* m_videoCodecCombo = nullptr;
    QSpinBox*  m_widthSpin      = nullptr;
    QSpinBox*  m_heightSpin     = nullptr;
    QSpinBox*  m_fpsSpin        = nullptr;
};

} // namespace dawcast::widgets
