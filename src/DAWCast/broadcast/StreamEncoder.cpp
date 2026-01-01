// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "StreamEncoder.h"

// TODO: #include <shout/shout.h>  // for Icecast
// TODO: Or custom DNAS/RTMP encoder integration

namespace dawcast {

StreamEncoder::StreamEncoder(QObject *parent)
    : QObject(parent)
{
}

StreamEncoder::~StreamEncoder()
{
    if (m_streaming) {
        stopStreaming();
    }
}

void StreamEncoder::setServer(const QString &url, int port)
{
    m_serverUrl = url;
    m_serverPort = port;
}

void StreamEncoder::setMountPoint(const QString &mount)
{
    m_mountPoint = mount;
}

void StreamEncoder::setPassword(const QString &password)
{
    m_password = password;
}

void StreamEncoder::setCodec(const QString &codec)
{
    m_codec = codec;
}

void StreamEncoder::setBitrate(int bitrate)
{
    m_bitrate = bitrate;
}

void StreamEncoder::startStreaming()
{
    // TODO: Initialize encoder based on m_codec (LAME for MP3, fdk-aac for AAC, libopus for Opus)
    // TODO: Connect to server via libshout (Icecast) or custom DNAS/RTMP protocol
    // TODO: Start encoding thread that reads from audio engine output bus
    m_streaming = true;
    m_totalBytesSent = 0;
    emit connected();
}

void StreamEncoder::stopStreaming()
{
    // TODO: Stop encoder thread, disconnect from server, flush buffers
    m_streaming = false;
    emit disconnected();
}

bool StreamEncoder::isStreaming() const
{
    return m_streaming;
}

} // namespace dawcast
