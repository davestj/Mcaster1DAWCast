// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QWidget>
#include <QString>
#include <QColor>

class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QMenu;

namespace dawcast::widgets {

/// Professional DAW-style track header with inline controls matching the
/// web UI reference.  Layout (top to bottom):
///
///   Row 1: Color indicator + track name + dropdown menu
///   Row 2: M | S | Rec | EQ | Split | Auto | Settings  (7 toggle buttons)
///   Row 3: Vol label + horizontal volume slider
///   Row 4: L label + horizontal pan slider + R label
///
class TrackHeaderWidget : public QWidget {
    Q_OBJECT

public:
    explicit TrackHeaderWidget(QWidget* parent = nullptr);
    ~TrackHeaderWidget() override;

    /// Fixed width for this header style (matches timeline header column)
    static constexpr int kHeaderWidth = 380;

    void setTrackName(const QString& name);
    void setTrackColor(const QColor& color);
    void setTrackIndex(int index);
    void setRecordArmed(bool armed);
    void setMuted(bool muted);
    void setSolo(bool solo);
    void setVerticalZoom(float zoom);
    void setFrozen(bool frozen);

    QString trackName() const;
    QColor  trackColor() const;
    int     trackIndex() const;
    bool    isRecordArmed() const;
    bool    isMuted() const;
    bool    isSolo() const;
    float   volume() const;
    float   pan() const;
    float   verticalZoom() const;
    bool    isFrozen() const;

signals:
    void recordArmToggled(bool armed);
    void muteToggled(bool muted);
    void soloToggled(bool solo);
    void volumeChanged(float volume);
    void panChanged(float pan);
    void eqRequested(int trackIndex);
    void splitToolToggled(bool active);
    void automationToggled(bool visible);
    void settingsRequested(int trackIndex);
    void trackNameEdited(const QString& name);
    void inputMonitorToggled(bool enabled);
    void colorChangeRequested(int trackIndex);
    void bounceRequested(int trackIndex);
    void freezeRequested(int trackIndex);
    void createGroupRequested(int trackIndex);
    void moveToGroupRequested(int trackIndex, const QString& groupName);
    void duplicateRequested(int trackIndex);
    void deleteRequested(int trackIndex);
    void saveTrackPresetRequested(int trackIndex);
    void loadTrackPresetRequested(int trackIndex, const QString& presetName);

private:
    void buildUI();
    void updateButtonStyle(QPushButton* btn, bool active,
                           const QColor& activeColor) const;
    QString sliderStyleSheet(const QColor& accentColor) const;
    void showTrackMenu();

    int     m_trackIndex  = 0;
    QString m_trackName;
    QColor  m_trackColor  = QColor(0, 180, 180);  // default teal
    bool    m_recordArmed = false;
    bool    m_muted       = false;
    bool    m_solo        = false;
    bool    m_frozen      = false;
    float   m_volume      = 0.0f;   // dB: -60 to +6
    float   m_pan         = 0.0f;   // -100 to +100
    float   m_verticalZoom = 1.0f;  // 0.25 to 4.0

    // Widgets we need to access after construction
    QLineEdit*   m_nameEdit   = nullptr;
    QPushButton* m_muteBtn    = nullptr;
    QPushButton* m_soloBtn    = nullptr;
    QPushButton* m_recBtn     = nullptr;
    QPushButton* m_monitorBtn = nullptr;
    QPushButton* m_eqBtn      = nullptr;
    QPushButton* m_splitBtn   = nullptr;
    QPushButton* m_autoBtn    = nullptr;
    QPushButton* m_settingsBtn = nullptr;
    QPushButton* m_menuBtn    = nullptr;
    QSlider*     m_volumeSlider = nullptr;
    QSlider*     m_panSlider    = nullptr;
    QWidget*     m_colorDot     = nullptr;
    QLabel*      m_freezeLabel  = nullptr;   // snowflake indicator
    QLabel*      m_zoomLabel    = nullptr;   // vertical zoom factor label
};

} // namespace dawcast::widgets
