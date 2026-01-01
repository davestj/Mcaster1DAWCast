// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QString>
#include <QImage>

namespace dawcast {

class IVideoEffect
{
public:
    virtual ~IVideoEffect() = default;

    virtual QImage process(QImage& frameA, QImage& frameB, float progress) = 0;
    virtual QString name() const = 0;
    virtual int parameterCount() const = 0;
};

} // namespace dawcast
