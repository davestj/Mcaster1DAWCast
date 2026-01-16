// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

class QDoubleSpinBox;
class QSlider;
class QCheckBox;
class QPushButton;
class QLabel;

namespace dawcast::widgets {

/// Dialog for applying time stretch and/or pitch shift to an audio clip.
///
/// Stretch ratio: 0.25 (25%) to 4.0 (400%)
///   - 1.0 = no change
///   - < 1.0 = slower / longer
///   - > 1.0 = faster / shorter
///
/// Pitch shift: -12 to +12 semitones (with 0.01 cent resolution)
///   - Negative = lower pitch
///   - Positive = higher pitch
///
/// "Preserve pitch" checkbox: when unchecked, pitch changes with speed
/// (tape-machine behavior).

class TimeStretchDialog : public QDialog {
    Q_OBJECT

public:
    explicit TimeStretchDialog(QWidget* parent = nullptr);
    ~TimeStretchDialog() override;

    float stretchRatio() const;     // 0.25 to 4.0
    float pitchSemitones() const;   // -12.0 to +12.0

    bool preservePitch() const;
    bool applyToSelection() const;  // true = selection, false = whole clip

signals:
    void previewRequested(float ratio, float semitones, bool preservePitch);

private slots:
    void onStretchSliderChanged(int value);
    void onStretchSpinChanged(double value);
    void onPitchSliderChanged(int value);
    void onPitchSpinChanged(double value);
    void onPreservePitchToggled(bool checked);
    void onPreviewClicked();

private:
    void buildUI();
    void updateDurationLabel();

    // Stretch ratio
    QSlider*        m_stretchSlider  = nullptr;
    QDoubleSpinBox* m_stretchSpin    = nullptr;
    QLabel*         m_durationLabel  = nullptr;

    // Pitch shift
    QSlider*        m_pitchSlider    = nullptr;
    QDoubleSpinBox* m_pitchSpin      = nullptr;
    QLabel*         m_pitchLabel     = nullptr;

    // Options
    QCheckBox*      m_preservePitch  = nullptr;
    QCheckBox*      m_applySelection = nullptr;

    // Action buttons
    QPushButton*    m_previewBtn     = nullptr;
    QPushButton*    m_applyBtn       = nullptr;
    QPushButton*    m_cancelBtn      = nullptr;

    // Internal state
    bool m_updatingControls = false;
};

} // namespace dawcast::widgets
