// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "BroadcastClock.h"

#include <QTimer>

namespace dawcast {

BroadcastClock::BroadcastClock(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &BroadcastClock::tick);
}

BroadcastClock::~BroadcastClock() = default;

void BroadcastClock::start()
{
    m_elapsed.start();
    m_segmentElapsed.start();
    m_running = true;
    m_timer->start();
}

void BroadcastClock::stop()
{
    m_timer->stop();
    m_running = false;
}

QTime BroadcastClock::elapsed() const
{
    if (!m_running) {
        return QTime(0, 0);
    }
    int ms = static_cast<int>(m_elapsed.elapsed());
    return QTime(0, 0).addMSecs(ms);
}

QTime BroadcastClock::segmentElapsed() const
{
    if (!m_running) {
        return QTime(0, 0);
    }
    int ms = static_cast<int>(m_segmentElapsed.elapsed());
    return QTime(0, 0).addMSecs(ms);
}

void BroadcastClock::resetSegment()
{
    m_segmentElapsed.restart();
}

} // namespace dawcast
