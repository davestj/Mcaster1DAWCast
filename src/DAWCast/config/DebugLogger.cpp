// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DebugLogger.h"

#include <QDateTime>
#include <QTextStream>
#include <QMutexLocker>

#if defined(__APPLE__) || defined(__linux__)
#include <execinfo.h>
#include <cstdlib>
#endif

#include <iostream>

namespace dawcast::config {

DebugLogger* DebugLogger::s_instance = nullptr;

DebugLogger::DebugLogger() = default;

DebugLogger::~DebugLogger()
{
    if (m_logFile.isOpen()) {
        m_logFile.close();
    }
}

DebugLogger* DebugLogger::instance()
{
    if (!s_instance) {
        s_instance = new DebugLogger();
    }
    return s_instance;
}

void DebugLogger::init(const QString& logPath)
{
    auto* logger = instance();
    QMutexLocker lock(&logger->m_mutex);

    if (logger->m_logFile.isOpen()) {
        logger->m_logFile.close();
    }

    logger->m_logFile.setFileName(logPath);
    logger->m_initialized = logger->m_logFile.open(
        QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
}

void DebugLogger::debug(const QString& msg) { write(QStringLiteral("DEBUG"), msg); }
void DebugLogger::info(const QString& msg)  { write(QStringLiteral("INFO"),  msg); }
void DebugLogger::warn(const QString& msg)  { write(QStringLiteral("WARN"),  msg); }

void DebugLogger::error(const QString& msg)
{
    QString fullMsg = msg + QStringLiteral("\n  Stack trace:\n") + captureStackTrace();
    write(QStringLiteral("ERROR"), fullMsg);
}

void DebugLogger::write(const QString& level, const QString& msg)
{
    QMutexLocker lock(&m_mutex);

    QString timestamp = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    QString line = QStringLiteral("[%1] [%2] %3\n").arg(timestamp, level, msg);

    // Write to stderr
    std::cerr << line.toStdString();

    // Write to file
    if (m_initialized && m_logFile.isOpen()) {
        QTextStream stream(&m_logFile);
        stream << line;
        stream.flush();
    }
}

QString DebugLogger::captureStackTrace() const
{
    QString trace;

#if defined(__APPLE__) || defined(__linux__)
    constexpr int maxFrames = 32;
    void* callstack[maxFrames];
    int frames = backtrace(callstack, maxFrames);
    char** symbols = backtrace_symbols(callstack, frames);

    if (symbols) {
        for (int i = 1; i < frames; ++i) { // skip frame 0 (this function)
            trace += QStringLiteral("    %1\n").arg(QString::fromUtf8(symbols[i]));
        }
        free(symbols);
    }
#else
    trace = QStringLiteral("    (stack trace not available on this platform)\n");
#endif

    return trace;
}

} // namespace dawcast::config
