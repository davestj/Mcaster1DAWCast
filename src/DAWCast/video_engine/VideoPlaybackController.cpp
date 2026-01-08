// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoPlaybackController.h"
#include "VideoDecoder.h"
#include "VideoMixer.h"
#include "VideoPreview.h"
#include "Timeline.h"
#include "AudioTrack.h"
#include "VideoTrack.h"
#include "Clip.h"

#include <QTimer>
#include <QList>

namespace dawcast {

VideoPlaybackController::VideoPlaybackController(QObject* parent)
    : QObject(parent)
{
    m_mixer = new VideoMixer(this);

    m_frameTimer = new QTimer(this);
    m_frameTimer->setTimerType(Qt::PreciseTimer);
    connect(m_frameTimer, &QTimer::timeout, this, &VideoPlaybackController::onTimerTick);
}

VideoPlaybackController::~VideoPlaybackController()
{
    stop();
}

void VideoPlaybackController::setVideoPreview(widgets::VideoPreview* preview)
{
    m_preview = preview;
}

void VideoPlaybackController::setTimeline(Timeline* timeline)
{
    m_timeline = timeline;
}

void VideoPlaybackController::play()
{
    if (!m_timeline) return;

    m_playing = true;

    // Start timer at configured fps (default 30fps = ~33ms)
    int intervalMs = static_cast<int>(1000.0 / m_fps);
    m_frameTimer->start(intervalMs);
}

void VideoPlaybackController::stop()
{
    m_playing = false;
    m_frameTimer->stop();
    closeAllDecoders();
}

void VideoPlaybackController::seekTo(double timeSeconds)
{
    m_currentTime = timeSeconds;

    if (!m_timeline) return;

    int sampleRate = m_timeline->sampleRate();
    if (sampleRate <= 0) sampleRate = 48000;

    // Seek all active decoders
    int64_t pts = static_cast<int64_t>(timeSeconds * 1000000.0); // microseconds

    for (auto it = m_decoders.begin(); it != m_decoders.end(); ++it) {
        if (it.value() && it.value()->isOpen()) {
            it.value()->seekTo(pts);
        }
    }

    // Decode and display a single frame at the seek position
    if (m_preview) {
        onTimerTick();
    }
}

void VideoPlaybackController::setPlaying(bool playing)
{
    if (playing)
        play();
    else
        stop();
}

void VideoPlaybackController::onTimerTick()
{
    if (!m_timeline || !m_preview) return;

    int sampleRate = m_timeline->sampleRate();
    if (sampleRate <= 0) sampleRate = 48000;

    // Current position in samples
    int64_t currentSample = static_cast<int64_t>(m_currentTime * sampleRate);

    QList<VideoFrame> layers;

    int trackCount = m_timeline->trackCount();
    for (int t = 0; t < trackCount; ++t) {
        QObject* trackObj = m_timeline->track(t);
        auto* videoTrack = qobject_cast<VideoTrack*>(trackObj);
        if (!videoTrack) continue;
        if (videoTrack->isMuted() || !videoTrack->isVisible()) continue;

        int clipCount = videoTrack->clipCount();
        for (int c = 0; c < clipCount; ++c) {
            Clip* clip = videoTrack->clip(c);
            if (!clip) continue;

            // Check if clip overlaps the current time position
            if (currentSample < clip->timelinePosition() || currentSample >= clip->endPosition())
                continue;

            // Get or create a decoder for this clip
            VideoDecoder* decoder = decoderForClip(t, c, clip->sourcePath());
            if (!decoder || !decoder->isOpen()) continue;

            // Compute the source-relative position in the clip
            int64_t clipOffset = currentSample - clip->timelinePosition();
            int64_t sourcePos  = clip->sourceIn() + clipOffset;
            double sourceTimeSec = static_cast<double>(sourcePos) / sampleRate;

            // Seek decoder to the correct position and decode a frame
            int64_t pts = static_cast<int64_t>(sourceTimeSec * 1000000.0);
            decoder->seekTo(pts);
            decoder->decodeNextFrame();

            // Build a VideoFrame layer with track compositing properties
            VideoFrame frame;
            frame.opacity = videoTrack->opacity();
            frame.width   = decoder->width();
            frame.height  = decoder->height();
            frame.timeSeconds = m_currentTime;

            layers.append(frame);
        }
    }

    // Composite all layers through the VideoMixer
    QImage composited;
    if (!layers.isEmpty()) {
        composited = m_mixer->composite(layers);
    }

    // Send to VideoPreview
    if (!composited.isNull()) {
        m_preview->setFrame(composited);
        emit frameReady(composited);
    } else if (!m_playing) {
        // If stopped and no video, clear preview is optional
    }

    // Advance current time if playing
    if (m_playing) {
        m_currentTime += 1.0 / m_fps;

        // Update the timeline playhead to stay in sync
        if (m_timeline) {
            int64_t newPlayhead = static_cast<int64_t>(m_currentTime * sampleRate);
            m_timeline->setPlayhead(newPlayhead);
        }
    }
}

VideoDecoder* VideoPlaybackController::decoderForClip(int trackIndex, int clipIndex,
                                                       const QString& sourcePath)
{
    QString key = QStringLiteral("%1:%2").arg(trackIndex).arg(clipIndex);

    auto it = m_decoders.find(key);
    if (it != m_decoders.end()) {
        return it.value();
    }

    // Create a new decoder for this clip
    auto* decoder = new VideoDecoder(this);
    if (!decoder->open(sourcePath)) {
        delete decoder;
        return nullptr;
    }

    m_decoders.insert(key, decoder);
    return decoder;
}

void VideoPlaybackController::closeAllDecoders()
{
    for (auto it = m_decoders.begin(); it != m_decoders.end(); ++it) {
        if (it.value()) {
            it.value()->close();
            it.value()->deleteLater();
        }
    }
    m_decoders.clear();
}

} // namespace dawcast
