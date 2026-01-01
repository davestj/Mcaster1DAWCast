// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QList>
#include <QString>

namespace dawcast {

class Clip;
class DspChain;

class AudioTrack : public QObject
{
    Q_OBJECT

public:
    explicit AudioTrack(QObject* parent = nullptr);
    ~AudioTrack() override;

    void addClip(Clip* clip);
    void removeClip(int index);
    [[nodiscard]] int clipCount() const;
    [[nodiscard]] Clip* clip(int index) const;

    void setVolume(float db);
    void setPan(float pan);
    void setMuted(bool muted);
    void setSolo(bool solo);
    void setRecordArmed(bool armed);

    [[nodiscard]] float volumeDb()    const { return m_volumeDb; }
    [[nodiscard]] float pan()         const { return m_pan; }
    [[nodiscard]] bool  isMuted()     const { return m_muted; }
    [[nodiscard]] bool  isSolo()      const { return m_solo; }
    [[nodiscard]] bool  isRecordArmed() const { return m_recordArmed; }

    void setName(const QString& name) { m_name = name; }
    [[nodiscard]] QString name() const { return m_name; }

    DspChain* effectChain();
    [[nodiscard]] const DspChain* effectChain() const { return m_effectChain; }

private:
    QList<Clip*> m_clips;
    DspChain*    m_effectChain = nullptr;
    QString      m_name;
    float        m_volumeDb    = 0.0f;
    float        m_pan         = 0.0f;
    bool         m_muted       = false;
    bool         m_solo        = false;
    bool         m_recordArmed = false;
};

} // namespace dawcast
