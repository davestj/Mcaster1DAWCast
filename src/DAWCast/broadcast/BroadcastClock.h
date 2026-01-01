// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QTime>
#include <QElapsedTimer>

class QTimer;

namespace dawcast {

class BroadcastClock : public QObject
{
    Q_OBJECT

public:
    explicit BroadcastClock(QObject *parent = nullptr);
    ~BroadcastClock() override;

    void start();
    void stop();

    QTime elapsed() const;
    QTime segmentElapsed() const;
    void resetSegment();

Q_SIGNALS:
    void tick();

private:
    QTimer *m_timer{nullptr};
    QElapsedTimer m_elapsed;
    QElapsedTimer m_segmentElapsed;
    bool m_running{false};
};

} // namespace dawcast
