// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QList>

namespace dawcast {

class IEffectUnit;

class DspChain : public QObject
{
    Q_OBJECT

public:
    explicit DspChain(QObject* parent = nullptr);
    ~DspChain() override;

    void addEffect(IEffectUnit* effect);
    void removeEffect(int index);
    void process(float* buffer, int frames, int channels);
    int  effectCount() const;
    IEffectUnit* effect(int index) const;
    void bypass(bool bypassed);
    [[nodiscard]] bool isBypassed() const { return m_bypassed; }

signals:
    void chainChanged();

private:
    QList<IEffectUnit*> m_effects;
    bool m_bypassed = false;
};

} // namespace dawcast
