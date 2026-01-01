// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/IVideoEffect.h"

namespace dawcast {

class DipToWhite : public IVideoEffect
{
public:
    DipToWhite();
    ~DipToWhite() override;

    QImage  process(QImage& frameA, QImage& frameB, float progress) override;
    QString name() const override;
    int     parameterCount() const override;
};

} // namespace dawcast
