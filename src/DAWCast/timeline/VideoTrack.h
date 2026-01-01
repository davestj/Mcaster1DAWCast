// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QList>
#include <QString>

namespace dawcast {

class Clip;
class VideoEffectChain;

class VideoTrack : public QObject
{
    Q_OBJECT

public:
    explicit VideoTrack(QObject* parent = nullptr);
    ~VideoTrack() override;

    void addClip(Clip* clip);
    void removeClip(int index);
    [[nodiscard]] int clipCount() const;
    [[nodiscard]] Clip* clip(int index) const;

    void setVisible(bool visible);
    void setOpacity(float opacity);
    void setMuted(bool muted);
    void setSolo(bool solo);

    [[nodiscard]] bool  isVisible() const { return m_visible; }
    [[nodiscard]] float opacity()   const { return m_opacity; }
    [[nodiscard]] bool  isMuted()   const { return m_muted; }
    [[nodiscard]] bool  isSolo()    const { return m_solo; }

    void setName(const QString& name) { m_name = name; }
    [[nodiscard]] QString name() const { return m_name; }

    VideoEffectChain* videoEffectChain();
    [[nodiscard]] const VideoEffectChain* videoEffectChain() const { return m_videoEffectChain; }

private:
    QList<Clip*>      m_clips;
    VideoEffectChain* m_videoEffectChain = nullptr;
    QString           m_name;
    float             m_opacity = 1.0f;
    bool              m_visible = true;
    bool              m_muted   = false;
    bool              m_solo    = false;
};

} // namespace dawcast
