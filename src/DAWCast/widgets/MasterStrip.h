// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QWidget>

class QSlider;
class QLabel;

namespace dawcast::widgets {

/// Compact horizontal master fader strip matching the DAWCast web UI.
///
/// Layout: "MASTER" label | horizontal fader | percentage display |
///         compact LUFS meter | "LIM" indicator
class MasterStrip : public QWidget {
    Q_OBJECT

public:
    explicit MasterStrip(QWidget* parent = nullptr);
    ~MasterStrip() override = default;

    /// Set the master fader level in dB (range: -60 to +6).
    void setLevel(float db);

    /// Set the current LUFS reading for the compact meter.
    void setLUFS(float lufs);

    /// Illuminate the "LIM" indicator when the limiter is active.
    void setLimiterActive(bool active);

    /// Returns current fader value in dB.
    [[nodiscard]] float levelDb() const;

signals:
    /// Emitted when the user moves the master fader.
    void levelChanged(float db);

private:
    void updatePercentLabel();
    void updateLufsDisplay();

    QLabel*  m_masterLabel   = nullptr;
    QSlider* m_fader         = nullptr;
    QLabel*  m_percentLabel  = nullptr;
    QLabel*  m_lufsLabel     = nullptr;
    QWidget* m_lufsMeter     = nullptr;  // compact bar
    QLabel*  m_limIndicator  = nullptr;

    float m_currentDb   =  0.0f;
    float m_currentLufs = -60.0f;
    bool  m_limiterOn   = false;

    // Fader range mapped to slider integer range
    static constexpr float kMinDb       = -60.0f;
    static constexpr float kMaxDb       =   6.0f;
    static constexpr int   kSliderMin   =    0;
    static constexpr int   kSliderMax   = 1000;
};

} // namespace dawcast::widgets
