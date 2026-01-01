// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Region.h"
#include <algorithm>

namespace dawcast {

Region::Region(int64_t start, int64_t end)
    : m_start(start)
    , m_end(end)
{
}

void Region::setStart(int64_t start)
{
    m_start = start;
}

void Region::setEnd(int64_t end)
{
    m_end = end;
}

bool Region::contains(int64_t position) const
{
    return position >= m_start && position < m_end;
}

bool Region::overlaps(const Region& other) const
{
    return m_start < other.m_end && other.m_start < m_end;
}

} // namespace dawcast
