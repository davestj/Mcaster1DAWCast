// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QString>
#include <cstdint>

namespace dawcast {

class Marker
{
public:
    enum class Type {
        Chapter,
        Cue,
        Loop
    };

    Marker() = default;
    Marker(const QString& name, int64_t position, Type type = Type::Cue);
    ~Marker() = default;

    [[nodiscard]] QString name()     const { return m_name; }
    [[nodiscard]] int64_t position() const { return m_position; }
    [[nodiscard]] Type    type()     const { return m_type; }

    void setName(const QString& name);
    void setPosition(int64_t position);
    void setType(Type type);

private:
    QString m_name;
    int64_t m_position = 0;
    Type    m_type     = Type::Cue;
};

} // namespace dawcast
