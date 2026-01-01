// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <cstdint>

namespace dawcast {

class MuxerDemuxer : public QObject
{
    Q_OBJECT

public:
    explicit MuxerDemuxer(QObject* parent = nullptr);
    ~MuxerDemuxer() override;

    // Demuxer (input)
    bool openInput(const QString& path);

    // Muxer (output)
    bool openOutput(const QString& path, const QString& format = "mp4");
    bool addAudioStream(int sampleRate, int channels, const QString& codec = "aac");
    bool addVideoStream(int width, int height, double fps, const QString& codec = "libx264");

    // Write packets
    bool writeAudioPacket(const uint8_t* data, int size, int64_t pts);
    bool writeVideoPacket(const uint8_t* data, int size, int64_t pts);

    void close();

    [[nodiscard]] bool isInputOpen()  const { return m_inputOpen; }
    [[nodiscard]] bool isOutputOpen() const { return m_outputOpen; }

private:
    bool m_inputOpen  = false;
    bool m_outputOpen = false;

    // FFmpeg opaque pointers
    void* m_inputFmtCtx  = nullptr;  // AVFormatContext* for demux
    void* m_outputFmtCtx = nullptr;  // AVFormatContext* for mux
    int   m_audioStreamIdx = -1;
    int   m_videoStreamIdx = -1;
};

} // namespace dawcast
