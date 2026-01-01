// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QList>
#include <QImage>

namespace dawcast {

class IVideoEffect;

class VideoEffectChain : public QObject
{
    Q_OBJECT

public:
    explicit VideoEffectChain(QObject* parent = nullptr);
    ~VideoEffectChain() override;

    void addEffect(IVideoEffect* effect);
    void removeEffect(int index);
    QImage process(QImage& frameA, QImage& frameB, float progress);
    int effectCount() const;
    IVideoEffect* effect(int index) const;

signals:
    void chainChanged();

private:
    QList<IVideoEffect*> m_effects;
};

} // namespace dawcast
