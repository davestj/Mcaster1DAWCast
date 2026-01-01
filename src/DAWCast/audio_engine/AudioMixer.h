// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QVector>
#include "../core/AudioBuffer.h"

namespace dawcast {

class AudioMixer : public QObject
{
    Q_OBJECT

public:
    static constexpr int MaxStrips = 32;

    explicit AudioMixer(QObject* parent = nullptr);
    ~AudioMixer() override;

    int  addStrip();
    void removeStrip(int index);
    [[nodiscard]] int stripCount() const;

    void setStripVolume(int strip, float db);
    void setStripPan(int strip, float pan);
    void setStripMuted(int strip, bool muted);
    void setStripSolo(int strip, bool solo);
    void setStripBuffer(int strip, const AudioBuffer* buffer);

    [[nodiscard]] float stripVolume(int strip) const;
    [[nodiscard]] float stripPan(int strip) const;

    void process(AudioBuffer& output);

private:
    struct Strip {
        float volumeDb = 0.0f;
        float pan      = 0.0f;   // -1.0 left, 0.0 center, 1.0 right
        bool  muted    = false;
        bool  solo     = false;
        const AudioBuffer* inputBuffer = nullptr;
    };

    QVector<Strip> m_strips;
};

} // namespace dawcast
