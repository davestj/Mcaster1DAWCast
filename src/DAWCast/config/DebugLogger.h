// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QString>
#include <QMutex>
#include <QFile>

namespace dawcast::config {

class DebugLogger {
public:
    static DebugLogger* instance();
    static void init(const QString& logPath);

    void debug(const QString& msg);
    void info(const QString& msg);
    void warn(const QString& msg);
    void error(const QString& msg);

private:
    DebugLogger();
    ~DebugLogger();

    void write(const QString& level, const QString& msg);
    QString captureStackTrace() const;

    QMutex  m_mutex;
    QFile   m_logFile;
    bool    m_initialized = false;

    static DebugLogger* s_instance;
};

} // namespace dawcast::config
