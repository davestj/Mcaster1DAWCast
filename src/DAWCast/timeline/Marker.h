// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QColor>
#include <QString>
#include <cstdint>

namespace dawcast {

class Marker
{
public:
    enum class Type {
        Chapter,
        Cue,
        Loop,
        Region
    };

    Marker() = default;
    Marker(const QString& name, int64_t position, Type type = Type::Cue);
    ~Marker() = default;

    [[nodiscard]] QString  name()     const { return m_name; }
    [[nodiscard]] int64_t  position() const { return m_position; }
    [[nodiscard]] Type     type()     const { return m_type; }
    [[nodiscard]] QColor   color()    const { return m_color; }
    [[nodiscard]] QString  comment()  const { return m_comment; }

    /// End position for Region-type markers. Ignored for other types.
    [[nodiscard]] int64_t  endPosition() const { return m_endPosition; }

    void setName(const QString& name);
    void setPosition(int64_t position);
    void setType(Type type);
    void setColor(const QColor& color);
    void setComment(const QString& comment);
    void setEndPosition(int64_t endPosition);

    /// Convenience: return a human-readable label for the marker type.
    [[nodiscard]] static QString typeName(Type type);

private:
    QString m_name;
    int64_t m_position    = 0;
    int64_t m_endPosition = 0;
    Type    m_type        = Type::Cue;
    QColor  m_color       = QColor(255, 200, 40);   // default: amber/gold
    QString m_comment;
};

} // namespace dawcast
