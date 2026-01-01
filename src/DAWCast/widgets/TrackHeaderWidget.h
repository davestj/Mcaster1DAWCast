// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QWidget>
#include <QString>

namespace dawcast::widgets {

class TrackHeaderWidget : public QWidget {
    Q_OBJECT

public:
    explicit TrackHeaderWidget(QWidget* parent = nullptr);
    ~TrackHeaderWidget() override;

    void setTrackName(const QString& name);
    void setRecordArmed(bool armed);
    void setMuted(bool muted);
    void setSolo(bool solo);

    QString trackName() const;
    bool isRecordArmed() const;
    bool isMuted() const;
    bool isSolo() const;

signals:
    void recordArmToggled(bool armed);
    void muteToggled(bool muted);
    void soloToggled(bool solo);
    void volumeChanged(float volume);
    void panChanged(float pan);

private:
    QString m_trackName;
    bool    m_recordArmed = false;
    bool    m_muted       = false;
    bool    m_solo        = false;
    float   m_volume      = 1.0f;
    float   m_pan         = 0.0f;
};

} // namespace dawcast::widgets
