// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QString>

class QWidget;

namespace dawcast {

class IModule
{
public:
    virtual ~IModule() = default;

    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual QString name() const = 0;
    virtual QWidget* getWidget() = 0;
};

} // namespace dawcast
