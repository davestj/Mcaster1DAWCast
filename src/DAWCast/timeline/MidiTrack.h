// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QList>
#include <QString>

namespace dawcast {

class MidiClip;

class MidiTrack : public QObject
{
    Q_OBJECT

public:
    explicit MidiTrack(QObject* parent = nullptr);
    ~MidiTrack() override;

    void addClip(MidiClip* clip);
    void removeClip(int index);
    [[nodiscard]] MidiClip* clip(int index) const;
    [[nodiscard]] int clipCount() const;

    void setChannel(int ch);
    [[nodiscard]] int channel() const { return m_channel; }

    void setInstrumentName(const QString& name);
    [[nodiscard]] QString instrumentName() const { return m_instrumentName; }

    void setName(const QString& name) { m_name = name; }
    [[nodiscard]] QString name() const { return m_name; }

    void setMuted(bool muted);
    void setSolo(bool solo);
    void setRecordArmed(bool armed);

    [[nodiscard]] bool isMuted()      const { return m_muted; }
    [[nodiscard]] bool isSolo()       const { return m_solo; }
    [[nodiscard]] bool isRecordArmed() const { return m_recordArmed; }

    void setVolume(float db) { m_volumeDb = db; }
    [[nodiscard]] float volumeDb() const { return m_volumeDb; }

private:
    QList<MidiClip*> m_clips;
    int     m_channel        = 0;
    QString m_instrumentName;
    QString m_name;
    float   m_volumeDb       = 0.0f;
    bool    m_muted          = false;
    bool    m_solo           = false;
    bool    m_recordArmed    = false;
};

} // namespace dawcast
