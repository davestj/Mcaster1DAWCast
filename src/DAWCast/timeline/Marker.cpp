// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Marker.h"

namespace dawcast {

Marker::Marker(const QString& name, int64_t position, Type type)
    : m_name(name)
    , m_position(position)
    , m_type(type)
{
}

void Marker::setName(const QString& name)
{
    m_name = name;
}

void Marker::setPosition(int64_t position)
{
    m_position = position;
}

void Marker::setType(Type type)
{
    m_type = type;
}

} // namespace dawcast
