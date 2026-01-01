// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AudioWorker.h"

namespace dawcast {

AudioWorker::AudioWorker(QObject* parent)
    : QObject(parent)
{
}

AudioWorker::~AudioWorker()
{
    stop();
}

void AudioWorker::setSource(const QString& path)
{
    m_sourcePath = path;
}

void AudioWorker::start()
{
    if (m_running) return;
    if (m_sourcePath.isEmpty()) {
        emit error("No source path set");
        return;
    }

    m_running = true;

    // TODO: Open audio file with FFmpeg/libav
    // TODO: Decode frames in a loop, emit bufferReady() for each decoded buffer
    // TODO: Emit finished() when done

    m_running = false;
    emit finished();
}

void AudioWorker::stop()
{
    m_running = false;
    // TODO: Signal the decode loop to break
}

} // namespace dawcast
