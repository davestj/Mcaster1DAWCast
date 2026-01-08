// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QMap>
#include <QImage>

class QTimer;

namespace dawcast {

class Timeline;
class VideoDecoder;
class VideoMixer;

namespace widgets {
class VideoPreview;
}

class VideoPlaybackController : public QObject
{
    Q_OBJECT

public:
    explicit VideoPlaybackController(QObject* parent = nullptr);
    ~VideoPlaybackController() override;

    void setVideoPreview(widgets::VideoPreview* preview);
    void setTimeline(Timeline* timeline);

    void play();
    void stop();
    void seekTo(double timeSeconds);
    void setPlaying(bool playing);

    [[nodiscard]] bool isPlaying() const { return m_playing; }
    [[nodiscard]] double currentTime() const { return m_currentTime; }

signals:
    void frameReady(const QImage& frame);

private slots:
    void onTimerTick();

private:
    VideoDecoder* decoderForClip(int trackIndex, int clipIndex, const QString& sourcePath);
    void closeAllDecoders();

    widgets::VideoPreview* m_preview  = nullptr;
    Timeline*              m_timeline = nullptr;
    VideoMixer*            m_mixer    = nullptr;

    // Key: "trackIdx:clipIdx" -> decoder
    QMap<QString, VideoDecoder*> m_decoders;

    QTimer* m_frameTimer   = nullptr;
    double  m_currentTime  = 0.0;
    bool    m_playing      = false;
    double  m_fps          = 30.0;
};

} // namespace dawcast
