// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "LiveRecorder.h"

// TODO: #include <portaudio.h>

namespace dawcast {

LiveRecorder::LiveRecorder(QObject *parent)
    : QObject(parent)
{
}

LiveRecorder::~LiveRecorder()
{
    if (m_recording) {
        stopRecording();
    }
}

void LiveRecorder::setInputDevice(int deviceIndex)
{
    m_deviceIndex = deviceIndex;
}

void LiveRecorder::setInputChannels(int channels)
{
    m_inputChannels = channels;
}

void LiveRecorder::startRecording(const QString &outputPath)
{
    // TODO: Open PortAudio input stream on m_deviceIndex
    // TODO: Open output file at outputPath (WAV via libsndfile)
    // TODO: Start PortAudio callback that writes samples + computes peak levels
    m_outputPath = outputPath;
    m_recording = true;
    emit recordingStarted();
}

void LiveRecorder::stopRecording()
{
    // TODO: Stop PortAudio stream, flush and close output file
    m_recording = false;
    emit recordingStopped();
}

bool LiveRecorder::isRecording() const
{
    return m_recording;
}

void LiveRecorder::setPunchIn(int64_t samplePosition)
{
    m_punchIn = samplePosition;
}

void LiveRecorder::setPunchOut(int64_t samplePosition)
{
    m_punchOut = samplePosition;
}

} // namespace dawcast
