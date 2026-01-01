// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstdint>

namespace dawcast {

/// Non-destructive edit boundary region.
class Region
{
public:
    Region() = default;
    Region(int64_t start, int64_t end);
    ~Region() = default;

    [[nodiscard]] int64_t start()  const { return m_start; }
    [[nodiscard]] int64_t end()    const { return m_end; }
    [[nodiscard]] int64_t length() const { return m_end - m_start; }

    void setStart(int64_t start);
    void setEnd(int64_t end);

    [[nodiscard]] bool contains(int64_t position) const;
    [[nodiscard]] bool overlaps(const Region& other) const;

private:
    int64_t m_start = 0;
    int64_t m_end   = 0;
};

} // namespace dawcast
