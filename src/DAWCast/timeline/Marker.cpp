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

void Marker::setColor(const QColor& color)
{
    m_color = color;
}

void Marker::setComment(const QString& comment)
{
    m_comment = comment;
}

void Marker::setEndPosition(int64_t endPosition)
{
    m_endPosition = endPosition;
}

QString Marker::typeName(Type type)
{
    switch (type) {
    case Type::Chapter: return QStringLiteral("Chapter");
    case Type::Cue:     return QStringLiteral("Cue");
    case Type::Loop:    return QStringLiteral("Loop");
    case Type::Region:  return QStringLiteral("Region");
    }
    return QStringLiteral("Unknown");
}

} // namespace dawcast
