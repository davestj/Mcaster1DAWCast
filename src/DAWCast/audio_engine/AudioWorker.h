// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <atomic>
#include "../core/AudioBuffer.h"

namespace dawcast {

class AudioWorker : public QObject
{
    Q_OBJECT

public:
    explicit AudioWorker(QObject* parent = nullptr);
    ~AudioWorker() override;

    void setSource(const QString& path);

public slots:
    void start();
    void stop();

signals:
    void bufferReady(dawcast::AudioBuffer buffer);
    void finished();
    void error(const QString& message);

private:
    QString           m_sourcePath;
    std::atomic<bool> m_running{false};
};

} // namespace dawcast
