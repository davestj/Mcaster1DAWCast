// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "RTMPStreamer.h"
#include "../audio_engine/RingBuffer.h"

#include <QDebug>
#include <QThread>
#include <QMutexLocker>
#include <QElapsedTimer>

#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>

#ifdef HAVE_AVFORMAT
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
}
#endif

namespace dawcast {

// ---------------------------------------------------------------------------
// Ring buffer sizing constants
// ---------------------------------------------------------------------------
static constexpr int kAudioRingBufferSeconds = 2;
static constexpr int kDefaultSampleRate      = 48000;
static constexpr int kDefaultChannels        = 2;

// ---------------------------------------------------------------------------
// ProcessingThread -- internal worker that encodes and writes to RTMP
// ---------------------------------------------------------------------------

class RTMPStreamer::ProcessingThread : public QThread
{
public:
    explicit ProcessingThread(RTMPStreamer* streamer)
        : QThread(streamer)
        , m_streamer(streamer)
    {}

    void requestStop() { m_stopRequested.store(true, std::memory_order_release); }

protected:
    void run() override
    {
#ifdef HAVE_AVFORMAT
        if (!openStream()) {
            emit m_streamer->error(QStringLiteral("Failed to open RTMP stream"));
            return;
        }

        m_streamer->m_streaming.store(true, std::memory_order_release);
        emit m_streamer->connected();

        processLoop();

        closeStream();
#else
        emit m_streamer->error(QStringLiteral("RTMP streaming requires FFmpeg (libavformat). "
                                              "Rebuild with --enable-avformat."));
#endif
        m_streamer->m_streaming.store(false, std::memory_order_release);
        emit m_streamer->disconnected();
    }

private:
#ifdef HAVE_AVFORMAT
    bool openStream()
    {
        const auto& cfg = m_streamer->m_config;
        QString url = m_streamer->buildUrl();
        QByteArray urlUtf8 = url.toUtf8();

        // Allocate output context for FLV (RTMP uses FLV container)
        int ret = avformat_alloc_output_context2(&m_fmtCtx, nullptr, "flv",
                                                  urlUtf8.constData());
        if (ret < 0 || !m_fmtCtx) {
            qWarning() << "RTMPStreamer: avformat_alloc_output_context2 failed:" << ret;
            return false;
        }

        // -- Add audio stream --
        if (!addAudioStream(cfg)) {
            qWarning() << "RTMPStreamer: failed to add audio stream";
            return false;
        }

        // -- Optionally add video stream --
        if (cfg.enableVideo) {
            if (!addVideoStream(cfg)) {
                qWarning() << "RTMPStreamer: failed to add video stream";
                return false;
            }
        }

        // Set RTMP-specific options
        AVDictionary* opts = nullptr;
        av_dict_set(&opts, "flvflags", "no_duration_filesize", 0);

        // Open the RTMP connection
        if (!(m_fmtCtx->oformat->flags & AVFMT_NOFILE)) {
            ret = avio_open2(&m_fmtCtx->pb, urlUtf8.constData(),
                             AVIO_FLAG_WRITE, nullptr, &opts);
            if (ret < 0) {
                char errbuf[AV_ERROR_MAX_STRING_SIZE];
                av_strerror(ret, errbuf, sizeof(errbuf));
                qWarning() << "RTMPStreamer: avio_open2 failed:" << errbuf;
                av_dict_free(&opts);
                return false;
            }
        }

        // Write stream header
        ret = avformat_write_header(m_fmtCtx, &opts);
        av_dict_free(&opts);
        if (ret < 0) {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, sizeof(errbuf));
            qWarning() << "RTMPStreamer: avformat_write_header failed:" << errbuf;
            return false;
        }

        m_headerWritten = true;
        return true;
    }

    bool addAudioStream(const StreamConfig& cfg)
    {
        // Find AAC encoder
        const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
        if (!codec) {
            qWarning() << "RTMPStreamer: AAC encoder not found";
            return false;
        }

        m_audioStream = avformat_new_stream(m_fmtCtx, nullptr);
        if (!m_audioStream) return false;
        m_audioStream->id = m_fmtCtx->nb_streams - 1;

        m_audioCodecCtx = avcodec_alloc_context3(codec);
        if (!m_audioCodecCtx) return false;

        m_audioCodecCtx->codec_id    = AV_CODEC_ID_AAC;
        m_audioCodecCtx->codec_type  = AVMEDIA_TYPE_AUDIO;
        m_audioCodecCtx->bit_rate    = static_cast<int64_t>(cfg.audioBitrate) * 1000;
        m_audioCodecCtx->sample_rate = cfg.audioSampleRate;
        m_audioCodecCtx->sample_fmt  = AV_SAMPLE_FMT_FLTP; // AAC needs planar float

        // Channel layout
        AVChannelLayout layout;
        if (cfg.audioChannels == 1) {
            av_channel_layout_default(&layout, 1);
        } else {
            av_channel_layout_default(&layout, 2);
        }
        av_channel_layout_copy(&m_audioCodecCtx->ch_layout, &layout);
        av_channel_layout_uninit(&layout);

        m_audioCodecCtx->time_base = AVRational{1, cfg.audioSampleRate};

        // Some formats need global headers
        if (m_fmtCtx->oformat->flags & AVFMT_GLOBALHEADER) {
            m_audioCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }

        int ret = avcodec_open2(m_audioCodecCtx, codec, nullptr);
        if (ret < 0) {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, sizeof(errbuf));
            qWarning() << "RTMPStreamer: avcodec_open2 (audio) failed:" << errbuf;
            return false;
        }

        ret = avcodec_parameters_from_context(m_audioStream->codecpar, m_audioCodecCtx);
        if (ret < 0) return false;

        m_audioStream->time_base = m_audioCodecCtx->time_base;

        // Allocate resampler (interleaved float -> planar float)
        ret = swr_alloc_set_opts2(&m_swrCtx,
                                  &m_audioCodecCtx->ch_layout, AV_SAMPLE_FMT_FLTP,
                                  cfg.audioSampleRate,
                                  &m_audioCodecCtx->ch_layout, AV_SAMPLE_FMT_FLT,
                                  cfg.audioSampleRate,
                                  0, nullptr);
        if (ret < 0 || !m_swrCtx) {
            qWarning() << "RTMPStreamer: swr_alloc_set_opts2 failed";
            return false;
        }
        if (swr_init(m_swrCtx) < 0) {
            qWarning() << "RTMPStreamer: swr_init failed";
            return false;
        }

        // Allocate audio frame
        m_audioFrame = av_frame_alloc();
        if (!m_audioFrame) return false;

        m_audioFrame->format      = m_audioCodecCtx->sample_fmt;
        av_channel_layout_copy(&m_audioFrame->ch_layout, &m_audioCodecCtx->ch_layout);
        m_audioFrame->sample_rate = cfg.audioSampleRate;
        m_audioFrame->nb_samples  = m_audioCodecCtx->frame_size;
        if (m_audioFrame->nb_samples <= 0) {
            m_audioFrame->nb_samples = 1024; // Fallback for encoders without fixed frame size
        }

        ret = av_frame_get_buffer(m_audioFrame, 0);
        if (ret < 0) return false;

        m_audioFrameSize = m_audioFrame->nb_samples;
        m_audioSampleRate = cfg.audioSampleRate;
        m_audioChannels   = cfg.audioChannels;

        return true;
    }

    bool addVideoStream(const StreamConfig& cfg)
    {
        const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (!codec) {
            qWarning() << "RTMPStreamer: H.264 encoder not found";
            return false;
        }

        m_videoStream = avformat_new_stream(m_fmtCtx, nullptr);
        if (!m_videoStream) return false;
        m_videoStream->id = m_fmtCtx->nb_streams - 1;

        m_videoCodecCtx = avcodec_alloc_context3(codec);
        if (!m_videoCodecCtx) return false;

        m_videoCodecCtx->codec_id    = AV_CODEC_ID_H264;
        m_videoCodecCtx->codec_type  = AVMEDIA_TYPE_VIDEO;
        m_videoCodecCtx->bit_rate    = static_cast<int64_t>(cfg.videoBitrate) * 1000;
        m_videoCodecCtx->width       = cfg.videoWidth;
        m_videoCodecCtx->height      = cfg.videoHeight;
        m_videoCodecCtx->time_base   = AVRational{1, static_cast<int>(cfg.videoFps * 1000)};
        m_videoCodecCtx->framerate   = AVRational{static_cast<int>(cfg.videoFps * 1000), 1000};
        m_videoCodecCtx->gop_size    = static_cast<int>(cfg.videoFps * 2); // keyframe every 2s
        m_videoCodecCtx->max_b_frames = 0; // No B-frames for low latency
        m_videoCodecCtx->pix_fmt     = AV_PIX_FMT_YUV420P;

        // x264 preset for low-latency streaming
        av_opt_set(m_videoCodecCtx->priv_data, "preset", "veryfast", 0);
        av_opt_set(m_videoCodecCtx->priv_data, "tune", "zerolatency", 0);

        if (m_fmtCtx->oformat->flags & AVFMT_GLOBALHEADER) {
            m_videoCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }

        int ret = avcodec_open2(m_videoCodecCtx, codec, nullptr);
        if (ret < 0) {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, sizeof(errbuf));
            qWarning() << "RTMPStreamer: avcodec_open2 (video) failed:" << errbuf;
            return false;
        }

        ret = avcodec_parameters_from_context(m_videoStream->codecpar, m_videoCodecCtx);
        if (ret < 0) return false;

        m_videoStream->time_base = m_videoCodecCtx->time_base;

        // Allocate scaler for QImage (BGRA) -> YUV420P
        m_swsCtx = sws_getContext(cfg.videoWidth, cfg.videoHeight, AV_PIX_FMT_BGRA,
                                  cfg.videoWidth, cfg.videoHeight, AV_PIX_FMT_YUV420P,
                                  SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
        if (!m_swsCtx) return false;

        // Allocate video frame
        m_videoFrame = av_frame_alloc();
        if (!m_videoFrame) return false;

        m_videoFrame->format = AV_PIX_FMT_YUV420P;
        m_videoFrame->width  = cfg.videoWidth;
        m_videoFrame->height = cfg.videoHeight;
        ret = av_frame_get_buffer(m_videoFrame, 0);
        if (ret < 0) return false;

        return true;
    }

    void processLoop()
    {
        // Intermediate buffer for reading from the ring buffer
        const int readChunkSamples = m_audioFrameSize * m_audioChannels;
        std::vector<float> readBuf(static_cast<size_t>(readChunkSamples));

        QElapsedTimer statsTimer;
        statsTimer.start();

        while (!m_stopRequested.load(std::memory_order_acquire)) {
            bool didWork = false;

            // -- Encode audio --
            if (m_streamer->m_audioRingBuffer) {
                size_t available = m_streamer->m_audioRingBuffer->availableRead();
                size_t needed = static_cast<size_t>(readChunkSamples);

                while (available >= needed) {
                    size_t read = m_streamer->m_audioRingBuffer->read(
                        readBuf.data(), needed);
                    if (read < needed) break;

                    encodeAudioFrame(readBuf.data());
                    available = m_streamer->m_audioRingBuffer->availableRead();
                    didWork = true;
                }
            }

            // -- Encode video --
            if (m_videoCodecCtx) {
                QImage frame;
                {
                    QMutexLocker lock(&m_streamer->m_videoMutex);
                    if (!m_streamer->m_videoQueue.isEmpty()) {
                        frame = m_streamer->m_videoQueue.takeFirst();
                    }
                }
                if (!frame.isNull()) {
                    encodeVideoFrame(frame);
                    didWork = true;
                }
            }

            // Emit stats periodically (every 500ms)
            if (statsTimer.elapsed() >= 500) {
                int64_t totalBytes = m_streamer->m_bytesSent.load(std::memory_order_acquire);
                int64_t prevBytes = m_streamer->m_bytesAtLastSample.load(std::memory_order_acquire);
                double elapsed = statsTimer.elapsed() / 1000.0;
                double bitrate = (elapsed > 0.0)
                    ? static_cast<double>(totalBytes - prevBytes) * 8.0 / 1000.0 / elapsed
                    : 0.0;
                m_streamer->m_currentBitrate.store(bitrate, std::memory_order_release);
                m_streamer->m_bytesAtLastSample.store(totalBytes, std::memory_order_release);

                int dropped = m_streamer->m_droppedFrames.load(std::memory_order_acquire);
                emit m_streamer->statsUpdated(totalBytes, bitrate, dropped);
                statsTimer.restart();
            }

            if (!didWork) {
                // Sleep briefly to avoid busy-waiting when no data is available
                QThread::usleep(1000); // 1ms
            }
        }

        // Flush encoders
        flushAudioEncoder();
        if (m_videoCodecCtx) {
            flushVideoEncoder();
        }
    }

    void encodeAudioFrame(const float* interleavedSamples)
    {
        if (!m_audioCodecCtx || !m_audioFrame || !m_swrCtx) return;

        av_frame_make_writable(m_audioFrame);

        // Resample: interleaved float -> planar float
        const uint8_t* inData[1] = { reinterpret_cast<const uint8_t*>(interleavedSamples) };
        int converted = swr_convert(m_swrCtx,
                                    m_audioFrame->data, m_audioFrameSize,
                                    inData, m_audioFrameSize);
        if (converted < 0) return;

        m_audioFrame->nb_samples = converted;
        m_audioFrame->pts = m_audioPts;
        m_audioPts += converted;

        writeEncodedFrame(m_audioCodecCtx, m_audioStream, m_audioFrame);
    }

    void encodeVideoFrame(const QImage& image)
    {
        if (!m_videoCodecCtx || !m_videoFrame || !m_swsCtx) return;

        av_frame_make_writable(m_videoFrame);

        // Convert QImage to the correct size and format
        QImage scaled = image.scaled(m_videoCodecCtx->width, m_videoCodecCtx->height,
                                     Qt::IgnoreAspectRatio, Qt::FastTransformation);
        QImage bgra = scaled.convertToFormat(QImage::Format_ARGB32);

        // sws_scale: BGRA -> YUV420P
        const uint8_t* srcSlice[1] = { bgra.constBits() };
        int srcStride[1] = { static_cast<int>(bgra.bytesPerLine()) };

        sws_scale(m_swsCtx, srcSlice, srcStride, 0, m_videoCodecCtx->height,
                  m_videoFrame->data, m_videoFrame->linesize);

        m_videoFrame->pts = m_videoPts;
        m_videoPts += av_rescale_q(1,
                                   AVRational{1000, static_cast<int>(m_streamer->m_config.videoFps * 1000)},
                                   m_videoCodecCtx->time_base);

        writeEncodedFrame(m_videoCodecCtx, m_videoStream, m_videoFrame);
    }

    void writeEncodedFrame(AVCodecContext* codecCtx, AVStream* stream, AVFrame* frame)
    {
        int ret = avcodec_send_frame(codecCtx, frame);
        if (ret < 0) return;

        AVPacket* pkt = av_packet_alloc();
        if (!pkt) return;

        while (ret >= 0) {
            ret = avcodec_receive_packet(codecCtx, pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
            if (ret < 0) break;

            // Rescale PTS/DTS to the stream's time base
            av_packet_rescale_ts(pkt, codecCtx->time_base, stream->time_base);
            pkt->stream_index = stream->index;

            ret = av_interleaved_write_frame(m_fmtCtx, pkt);
            if (ret < 0) {
                m_streamer->m_droppedFrames.fetch_add(1, std::memory_order_relaxed);
            } else {
                m_streamer->m_bytesSent.fetch_add(pkt->size, std::memory_order_relaxed);
            }
        }

        av_packet_free(&pkt);
    }

    void flushAudioEncoder()
    {
        if (!m_audioCodecCtx || !m_audioStream) return;
        writeEncodedFrame(m_audioCodecCtx, m_audioStream, nullptr);
    }

    void flushVideoEncoder()
    {
        if (!m_videoCodecCtx || !m_videoStream) return;
        writeEncodedFrame(m_videoCodecCtx, m_videoStream, nullptr);
    }

    void closeStream()
    {
        if (m_fmtCtx) {
            if (m_headerWritten) {
                av_write_trailer(m_fmtCtx);
            }

            if (m_fmtCtx->pb && !(m_fmtCtx->oformat->flags & AVFMT_NOFILE)) {
                avio_closep(&m_fmtCtx->pb);
            }
        }

        if (m_audioFrame)    av_frame_free(&m_audioFrame);
        if (m_videoFrame)    av_frame_free(&m_videoFrame);
        if (m_audioCodecCtx) avcodec_free_context(&m_audioCodecCtx);
        if (m_videoCodecCtx) avcodec_free_context(&m_videoCodecCtx);
        if (m_swrCtx)        swr_free(&m_swrCtx);
        if (m_swsCtx) { sws_freeContext(m_swsCtx); m_swsCtx = nullptr; }
        if (m_fmtCtx)        avformat_free_context(m_fmtCtx);

        m_fmtCtx        = nullptr;
        m_audioCodecCtx = nullptr;
        m_videoCodecCtx = nullptr;
        m_audioStream   = nullptr;
        m_videoStream   = nullptr;
        m_audioFrame    = nullptr;
        m_videoFrame    = nullptr;
        m_swrCtx        = nullptr;
        m_swsCtx        = nullptr;
        m_headerWritten = false;
    }

    // FFmpeg state
    AVFormatContext*  m_fmtCtx        = nullptr;
    AVCodecContext*   m_audioCodecCtx = nullptr;
    AVCodecContext*   m_videoCodecCtx = nullptr;
    AVStream*         m_audioStream   = nullptr;
    AVStream*         m_videoStream   = nullptr;
    AVFrame*          m_audioFrame    = nullptr;
    AVFrame*          m_videoFrame    = nullptr;
    SwrContext*        m_swrCtx       = nullptr;
    SwsContext*        m_swsCtx       = nullptr;
    bool              m_headerWritten = false;

    // Audio encoding state
    int     m_audioFrameSize  = 1024;
    int     m_audioSampleRate = 48000;
    int     m_audioChannels   = 2;
    int64_t m_audioPts        = 0;
    int64_t m_videoPts        = 0;

#endif // HAVE_AVFORMAT

    RTMPStreamer* m_streamer;
    std::atomic<bool> m_stopRequested{false};
};


// ---------------------------------------------------------------------------
// RTMPStreamer implementation
// ---------------------------------------------------------------------------

RTMPStreamer::RTMPStreamer(QObject* parent)
    : QObject(parent)
{
    // Pre-allocate ring buffer for 2 seconds of stereo 48 kHz audio
    const int ringSize = kAudioRingBufferSeconds * kDefaultSampleRate * kDefaultChannels;
    m_audioRingBuffer = std::make_unique<RingBuffer<float>>(
        static_cast<size_t>(ringSize));
}

RTMPStreamer::~RTMPStreamer()
{
    stopStreaming();
}

void RTMPStreamer::setConfig(const StreamConfig& config)
{
    m_config = config;
}

RTMPStreamer::StreamConfig RTMPStreamer::config() const
{
    return m_config;
}

QString RTMPStreamer::buildUrl() const
{
    if (m_config.streamKey.isEmpty()) {
        return m_config.url;
    }

    QString url = m_config.url;
    if (!url.endsWith(QLatin1Char('/'))) {
        url.append(QLatin1Char('/'));
    }
    url.append(m_config.streamKey);
    return url;
}

bool RTMPStreamer::startStreaming()
{
    if (m_streaming.load(std::memory_order_acquire)) {
        qWarning() << "RTMPStreamer: already streaming";
        return false;
    }

    if (m_config.url.isEmpty()) {
        emit error(QStringLiteral("Stream URL is empty"));
        return false;
    }

    // Reallocate ring buffer for the configured sample rate/channels
    const int ringSize = kAudioRingBufferSeconds
                         * m_config.audioSampleRate
                         * m_config.audioChannels;
    m_audioRingBuffer = std::make_unique<RingBuffer<float>>(
        static_cast<size_t>(ringSize));

    // Reset statistics
    m_bytesSent.store(0, std::memory_order_release);
    m_droppedFrames.store(0, std::memory_order_release);
    m_bytesAtLastSample.store(0, std::memory_order_release);
    m_currentBitrate.store(0.0, std::memory_order_release);

    // Clear video queue
    {
        QMutexLocker lock(&m_videoMutex);
        m_videoQueue.clear();
    }

    m_uptimeTimer.start();

    // Launch processing thread
    m_thread = new ProcessingThread(this);
    m_thread->start(QThread::HighPriority);

    return true;
}

void RTMPStreamer::stopStreaming()
{
    if (m_thread) {
        m_thread->requestStop();
        m_thread->wait(5000); // Wait up to 5 seconds
        if (m_thread->isRunning()) {
            qWarning() << "RTMPStreamer: processing thread did not stop in time, terminating";
            m_thread->terminate();
            m_thread->wait(1000);
        }
        delete m_thread;
        m_thread = nullptr;
    }

    m_streaming.store(false, std::memory_order_release);
}

bool RTMPStreamer::isStreaming() const
{
    return m_streaming.load(std::memory_order_acquire);
}

void RTMPStreamer::pushAudioFrame(const float* samples, int frames,
                                  int channels, int /*sampleRate*/)
{
    if (!m_streaming.load(std::memory_order_acquire)) return;
    if (!m_audioRingBuffer || !samples || frames <= 0) return;

    const size_t totalSamples = static_cast<size_t>(frames * channels);
    size_t written = m_audioRingBuffer->write(samples, totalSamples);

    // If the ring buffer is full, count dropped frames
    if (written < totalSamples) {
        m_droppedFrames.fetch_add(1, std::memory_order_relaxed);
    }
}

void RTMPStreamer::pushVideoFrame(const QImage& frame)
{
    if (!m_streaming.load(std::memory_order_acquire)) return;
    if (frame.isNull()) return;

    QMutexLocker lock(&m_videoMutex);
    if (m_videoQueue.size() >= kMaxVideoQueueSize) {
        // Drop oldest frame to keep latency low
        m_videoQueue.removeFirst();
        m_droppedFrames.fetch_add(1, std::memory_order_relaxed);
    }
    m_videoQueue.append(frame);
}

int64_t RTMPStreamer::bytesSent() const
{
    return m_bytesSent.load(std::memory_order_acquire);
}

double RTMPStreamer::uptimeSeconds() const
{
    if (!m_streaming.load(std::memory_order_acquire)) return 0.0;
    return m_uptimeTimer.elapsed() / 1000.0;
}

int RTMPStreamer::droppedFrames() const
{
    return m_droppedFrames.load(std::memory_order_acquire);
}

double RTMPStreamer::currentBitrateKbps() const
{
    return m_currentBitrate.load(std::memory_order_acquire);
}

} // namespace dawcast
