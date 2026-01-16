// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ExportEngine.h"
#include "AudioClipReader.h"
#include "../core/AudioBuffer.h"
#include "../codec/TagTransfer.h"
#include "../timeline/Timeline.h"
#include "../timeline/AudioTrack.h"
#include "../timeline/VideoTrack.h"
#include "../timeline/Clip.h"
#include "../dsp/DspChain.h"
#include "../video_engine/VideoMixer.h"
#include "../video_engine/VideoDecoder.h"
#include "../video_engine/VideoEncoder.h"
#include "../video_engine/MuxerDemuxer.h"
#include "../core/VideoFrame.h"

#include <QApplication>
#include <QDebug>
#include <QFileInfo>

#include <algorithm>
#include <cstring>
#include <cmath>

namespace dawcast {

// Block size used for offline rendering — large for throughput,
// but small enough to give reasonable progress granularity.
static constexpr int kExportBlockSize = 4096;

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

ExportEngine::ExportEngine(QObject* parent)
    : QObject(parent)
{
}

ExportEngine::~ExportEngine()
{
    m_cancelled.store(true, std::memory_order_release);
}

bool ExportEngine::isExporting() const
{
    return m_exporting.load(std::memory_order_acquire);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void ExportEngine::startExport(Timeline* timeline, const ExportConfig& config)
{
    if (!timeline) {
        emit error(tr("No timeline to export"));
        return;
    }
    if (config.outputPath.isEmpty()) {
        emit error(tr("No output path specified"));
        return;
    }

    m_cancelled.store(false, std::memory_order_release);
    m_exporting.store(true, std::memory_order_release);

    if (config.videoCodec.isEmpty()) {
        exportAudioOnly(timeline, config);
    } else {
        exportAudioVideo(timeline, config);
    }

    m_exporting.store(false, std::memory_order_release);
}

void ExportEngine::cancelExport()
{
    m_cancelled.store(true, std::memory_order_release);
}

// ---------------------------------------------------------------------------
// Audio-only export
// ---------------------------------------------------------------------------

void ExportEngine::exportAudioOnly(Timeline* timeline, const ExportConfig& config)
{
    // 1. Calculate total duration from the timeline
    int64_t totalSamples = timeline->duration();
    if (totalSamples <= 0) {
        emit error(tr("Timeline is empty -- nothing to export"));
        return;
    }

    int sampleRate = config.sampleRate > 0 ? config.sampleRate
                                           : timeline->sampleRate();
    int channels   = config.channels > 0 ? config.channels : 2;

    // 2. Build offline readers for all audio tracks
    auto trackStates = buildTrackStates(timeline);
    if (trackStates.empty()) {
        emit error(tr("No audio tracks with clips to export"));
        return;
    }

    // 3. Open the muxer / output file
    //    Determine the FFmpeg format string from the container or codec name
    QString ffmpegFormat;
    QString ffmpegAudioCodec;

    QString codec = config.audioCodec.toLower();
    if (codec == QStringLiteral("mp3")) {
        ffmpegFormat     = QStringLiteral("mp3");
        ffmpegAudioCodec = QStringLiteral("libmp3lame");
    } else if (codec == QStringLiteral("aac")) {
        ffmpegFormat     = QStringLiteral("adts");
        ffmpegAudioCodec = QStringLiteral("aac");
    } else if (codec == QStringLiteral("wav")) {
        ffmpegFormat     = QStringLiteral("wav");
        ffmpegAudioCodec = QStringLiteral("pcm_f32le");
    } else if (codec == QStringLiteral("flac")) {
        ffmpegFormat     = QStringLiteral("flac");
        ffmpegAudioCodec = QStringLiteral("flac");
    } else if (codec == QStringLiteral("opus")) {
        ffmpegFormat     = QStringLiteral("ogg");
        ffmpegAudioCodec = QStringLiteral("libopus");
    } else if (codec == QStringLiteral("vorbis")) {
        ffmpegFormat     = QStringLiteral("ogg");
        ffmpegAudioCodec = QStringLiteral("libvorbis");
    } else {
        // Fallback: let FFmpeg guess from the file extension
        ffmpegFormat     = QString();
        ffmpegAudioCodec = codec;
    }

    // Override format from container if explicitly set
    if (!config.container.isEmpty()) {
        QString c = config.container.toLower();
        if (c == QStringLiteral("mp4"))       ffmpegFormat = QStringLiteral("mp4");
        else if (c == QStringLiteral("mkv"))  ffmpegFormat = QStringLiteral("matroska");
        else if (c == QStringLiteral("webm")) ffmpegFormat = QStringLiteral("webm");
        else if (c == QStringLiteral("wav"))  ffmpegFormat = QStringLiteral("wav");
        else if (c == QStringLiteral("mp3"))  ffmpegFormat = QStringLiteral("mp3");
    }

    MuxerDemuxer muxer;
    if (!muxer.openOutput(config.outputPath, ffmpegFormat)) {
        freeTrackStates(trackStates);
        emit error(tr("Failed to open output file: %1").arg(config.outputPath));
        return;
    }
    if (!muxer.addAudioStream(sampleRate, channels, ffmpegAudioCodec)) {
        muxer.close();
        freeTrackStates(trackStates);
        emit error(tr("Failed to configure audio encoder: %1").arg(ffmpegAudioCodec));
        return;
    }

    // 4. Render and write blocks
    std::vector<float> mixBuf;
    int64_t position = 0;
    int lastPercent = -1;

    while (position < totalSamples) {
        if (m_cancelled.load(std::memory_order_acquire)) {
            muxer.close();
            freeTrackStates(trackStates);
            emit error(tr("Export cancelled"));
            return;
        }

        int framesToRender = static_cast<int>(
            std::min(static_cast<int64_t>(kExportBlockSize),
                     totalSamples - position));

        int samplesWritten = renderBlock(trackStates, position,
                                         framesToRender, channels,
                                         sampleRate, mixBuf);

        if (samplesWritten > 0) {
            // Write the interleaved float data as a raw audio packet.
            // The muxer + FFmpeg codec pipeline will encode it.
            int byteSize = samplesWritten * static_cast<int>(sizeof(float));
            muxer.writeAudioPacket(
                reinterpret_cast<const uint8_t*>(mixBuf.data()),
                byteSize, position);
        }

        position += framesToRender;

        // Emit progress
        int percent = static_cast<int>(
            (position * 100) / totalSamples);
        if (percent != lastPercent) {
            lastPercent = percent;
            emit progress(percent);
            QApplication::processEvents();  // Keep GUI responsive
        }
    }

    // 5. Finalize
    muxer.close();
    freeTrackStates(trackStates);

    // 6. Write metadata tags to the exported file
    if (!config.metadata.isEmpty() && TagTransfer::isSupported(config.outputPath)) {
        if (!TagTransfer::writeTags(config.outputPath, config.metadata)) {
            qWarning() << "ExportEngine: failed to write metadata tags to"
                       << config.outputPath;
        }
    }

    qDebug() << "ExportEngine: audio-only export complete ->"
             << config.outputPath;

    emit progress(100);
    emit finished(config.outputPath);
}

// ---------------------------------------------------------------------------
// Audio + video export
// ---------------------------------------------------------------------------

void ExportEngine::exportAudioVideo(Timeline* timeline, const ExportConfig& config)
{
    // Calculate total duration
    int64_t totalSamples = timeline->duration();
    if (totalSamples <= 0) {
        emit error(tr("Timeline is empty -- nothing to export"));
        return;
    }

    int sampleRate = config.sampleRate > 0 ? config.sampleRate
                                           : timeline->sampleRate();
    int channels   = config.channels > 0 ? config.channels : 2;
    double fps     = config.videoFps > 0.0 ? config.videoFps : 30.0;
    int videoW     = config.videoWidth  > 0 ? config.videoWidth  : 1920;
    int videoH     = config.videoHeight > 0 ? config.videoHeight : 1080;

    // Build offline readers
    auto trackStates = buildTrackStates(timeline);

    // Determine FFmpeg codec names
    QString ffmpegVideoCodec;
    QString vc = config.videoCodec.toLower();
    if (vc == QStringLiteral("h264") || vc == QStringLiteral("h.264"))
        ffmpegVideoCodec = QStringLiteral("libx264");
    else if (vc == QStringLiteral("vp9"))
        ffmpegVideoCodec = QStringLiteral("libvpx-vp9");
    else
        ffmpegVideoCodec = vc;

    QString ffmpegAudioCodec;
    QString ac = config.audioCodec.toLower();
    if (ac == QStringLiteral("mp3"))       ffmpegAudioCodec = QStringLiteral("libmp3lame");
    else if (ac == QStringLiteral("aac"))  ffmpegAudioCodec = QStringLiteral("aac");
    else if (ac == QStringLiteral("opus")) ffmpegAudioCodec = QStringLiteral("libopus");
    else if (ac == QStringLiteral("vorbis")) ffmpegAudioCodec = QStringLiteral("libvorbis");
    else if (ac == QStringLiteral("flac")) ffmpegAudioCodec = QStringLiteral("flac");
    else if (ac == QStringLiteral("wav"))  ffmpegAudioCodec = QStringLiteral("pcm_f32le");
    else ffmpegAudioCodec = ac;

    QString containerFormat;
    QString ct = config.container.toLower();
    if (ct == QStringLiteral("mp4"))       containerFormat = QStringLiteral("mp4");
    else if (ct == QStringLiteral("mkv"))  containerFormat = QStringLiteral("matroska");
    else if (ct == QStringLiteral("webm")) containerFormat = QStringLiteral("webm");
    else if (ct == QStringLiteral("avi"))  containerFormat = QStringLiteral("avi");
    else                                   containerFormat = QStringLiteral("mp4");

    // Open muxer with both audio and video streams
    MuxerDemuxer muxer;
    if (!muxer.openOutput(config.outputPath, containerFormat)) {
        freeTrackStates(trackStates);
        emit error(tr("Failed to open output file: %1").arg(config.outputPath));
        return;
    }
    if (!muxer.addAudioStream(sampleRate, channels, ffmpegAudioCodec)) {
        muxer.close();
        freeTrackStates(trackStates);
        emit error(tr("Failed to configure audio encoder: %1").arg(ffmpegAudioCodec));
        return;
    }
    if (!muxer.addVideoStream(videoW, videoH, fps, ffmpegVideoCodec)) {
        muxer.close();
        freeTrackStates(trackStates);
        emit error(tr("Failed to configure video encoder: %1").arg(ffmpegVideoCodec));
        return;
    }

    // Set up video mixer for compositing
    VideoMixer videoMixer;
    videoMixer.setOutputSize(videoW, videoH);

    // Calculate timing
    double totalSeconds = static_cast<double>(totalSamples) / sampleRate;
    int64_t totalVideoFrames = static_cast<int64_t>(std::ceil(totalSeconds * fps));

    // Render interleaved audio + video
    std::vector<float> mixBuf;
    int64_t audioPosition = 0;
    int64_t videoFrameIdx = 0;
    int lastPercent = -1;

    while (audioPosition < totalSamples) {
        if (m_cancelled.load(std::memory_order_acquire)) {
            muxer.close();
            freeTrackStates(trackStates);
            emit error(tr("Export cancelled"));
            return;
        }

        // Determine how many audio frames to render this iteration
        // (align with video frame boundaries for clean interleaving)
        int framesToRender = static_cast<int>(
            std::min(static_cast<int64_t>(kExportBlockSize),
                     totalSamples - audioPosition));

        // Render audio block
        int samplesWritten = renderBlock(trackStates, audioPosition,
                                         framesToRender, channels,
                                         sampleRate, mixBuf);
        if (samplesWritten > 0) {
            int byteSize = samplesWritten * static_cast<int>(sizeof(float));
            muxer.writeAudioPacket(
                reinterpret_cast<const uint8_t*>(mixBuf.data()),
                byteSize, audioPosition);
        }

        // Check if we need to emit video frames for this audio chunk
        int64_t audioEnd = audioPosition + framesToRender;
        while (videoFrameIdx < totalVideoFrames) {
            int64_t videoSamplePos = static_cast<int64_t>(
                videoFrameIdx * sampleRate / fps);
            if (videoSamplePos >= audioEnd) break;

            // Composite video frame at this time position.
            // Gather video layers from all video tracks.
            QList<VideoFrame> layers;
            int trackCount = timeline->trackCount();
            for (int t = 0; t < trackCount; ++t) {
                QObject* trackObj = timeline->track(t);
                auto* vTrack = qobject_cast<VideoTrack*>(trackObj);
                if (!vTrack || vTrack->isMuted() || !vTrack->isVisible())
                    continue;

                // For each clip on the video track, check if it overlaps
                for (int c = 0; c < vTrack->clipCount(); ++c) {
                    Clip* clip = vTrack->clip(c);
                    if (!clip) continue;

                    int64_t clipStart = clip->timelinePosition();
                    int64_t clipEnd   = clip->endPosition();
                    if (videoSamplePos < clipStart || videoSamplePos >= clipEnd)
                        continue;

                    // Create a placeholder video frame
                    // (In a full implementation, a VideoClipReader would
                    //  decode the actual frame from the source file.)
                    VideoFrame frame;
                    frame.width       = videoW;
                    frame.height      = videoH;
                    frame.opacity     = vTrack->opacity();
                    frame.timeSeconds = static_cast<double>(videoSamplePos) / sampleRate;
                    frame.pts         = videoFrameIdx;
                    // frame.image would be filled by VideoDecoder in production
                    layers.append(frame);
                }
            }

            if (!layers.isEmpty()) {
                QImage composited = videoMixer.composite(layers);
                // Convert QImage to raw bytes for the muxer packet
                const uchar* bits = composited.constBits();
                int imgSize = static_cast<int>(composited.sizeInBytes());
                muxer.writeVideoPacket(bits, imgSize, videoFrameIdx);
            }

            ++videoFrameIdx;
        }

        audioPosition += framesToRender;

        // Progress
        int percent = static_cast<int>(
            (audioPosition * 100) / totalSamples);
        if (percent != lastPercent) {
            lastPercent = percent;
            emit progress(percent);
            QApplication::processEvents();
        }
    }

    // Finalize
    muxer.close();
    freeTrackStates(trackStates);

    // Write metadata tags (for containers that support it)
    if (!config.metadata.isEmpty() && TagTransfer::isSupported(config.outputPath)) {
        if (!TagTransfer::writeTags(config.outputPath, config.metadata)) {
            qWarning() << "ExportEngine: failed to write metadata tags to"
                       << config.outputPath;
        }
    }

    qDebug() << "ExportEngine: audio+video export complete ->"
             << config.outputPath;

    emit progress(100);
    emit finished(config.outputPath);
}

// ---------------------------------------------------------------------------
// Track state management
// ---------------------------------------------------------------------------

std::vector<ExportEngine::TrackState>
ExportEngine::buildTrackStates(Timeline* timeline)
{
    std::vector<TrackState> states;

    int trackCount = timeline->trackCount();
    for (int t = 0; t < trackCount; ++t) {
        QObject* trackObj = timeline->track(t);
        auto* audioTrack = qobject_cast<AudioTrack*>(trackObj);
        if (!audioTrack) continue;
        if (audioTrack->isMuted()) continue;

        TrackState ts;
        ts.audioTrack = audioTrack;

        for (int c = 0; c < audioTrack->clipCount(); ++c) {
            Clip* clip = audioTrack->clip(c);
            if (!clip || clip->sourcePath().isEmpty()) continue;

            auto* reader = new AudioClipReader(clip);
            if (reader->open()) {
                ts.readers.push_back(reader);
            } else {
                qWarning() << "ExportEngine: failed to open reader for"
                           << clip->sourcePath();
                delete reader;
            }
        }

        if (!ts.readers.empty()) {
            states.push_back(std::move(ts));
        }
    }

    qDebug() << "ExportEngine: prepared" << states.size()
             << "audio tracks for export";
    return states;
}

void ExportEngine::freeTrackStates(std::vector<TrackState>& states)
{
    for (auto& ts : states) {
        for (auto* reader : ts.readers) {
            delete reader;
        }
        ts.readers.clear();
    }
    states.clear();
}

// ---------------------------------------------------------------------------
// Offline render one block
// ---------------------------------------------------------------------------

int ExportEngine::renderBlock(std::vector<TrackState>& tracks,
                              int64_t position, int frames, int channels,
                              int sampleRate, std::vector<float>& outBuf)
{
    const auto totalSamples = static_cast<size_t>(frames * channels);

    // Ensure the output buffer is large enough and zeroed
    outBuf.resize(totalSamples);
    std::memset(outBuf.data(), 0, totalSamples * sizeof(float));

    // Temporary per-track buffer
    std::vector<float> trackBuf(totalSamples, 0.0f);

    for (auto& ts : tracks) {
        // Zero the track scratch buffer
        std::memset(trackBuf.data(), 0, totalSamples * sizeof(float));

        bool hasAudio = false;

        // Read all overlapping clips into the track buffer
        for (auto* reader : ts.readers) {
            if (!reader || !reader->isOpen()) continue;

            Clip* clip = reader->clip();
            if (!clip) continue;

            int64_t clipStart = clip->timelinePosition();
            int64_t clipEnd   = clip->endPosition();
            if (position + frames <= clipStart || position >= clipEnd)
                continue;

            // readSamples adds (accumulates) into the buffer
            reader->readSamples(trackBuf.data(), position, frames, channels);
            hasAudio = true;
        }

        if (!hasAudio) continue;

        // Apply the track's DSP chain if present
        if (ts.audioTrack) {
            const DspChain* chain = ts.audioTrack->effectChain();
            if (chain && chain->effectCount() > 0) {
                const_cast<DspChain*>(chain)->process(
                    trackBuf.data(), frames, channels);
            }
        }

        // Apply track volume (convert dB to linear gain)
        float volumeDb = ts.audioTrack ? ts.audioTrack->volumeDb() : 0.0f;
        float gain = std::pow(10.0f, volumeDb / 20.0f);

        // Apply track pan (constant-power panning for stereo)
        float pan = ts.audioTrack ? ts.audioTrack->pan() : 0.0f;

        if (channels == 2 && std::fabs(pan) > 0.001f) {
            // Constant-power pan law
            float angle = (pan + 1.0f) * 0.25f * static_cast<float>(M_PI);
            float gainL = gain * std::cos(angle);
            float gainR = gain * std::sin(angle);

            for (int i = 0; i < frames; ++i) {
                outBuf[static_cast<size_t>(i * 2)]     += trackBuf[static_cast<size_t>(i * 2)]     * gainL;
                outBuf[static_cast<size_t>(i * 2 + 1)] += trackBuf[static_cast<size_t>(i * 2 + 1)] * gainR;
            }
        } else {
            // Apply uniform gain
            for (size_t i = 0; i < totalSamples; ++i) {
                outBuf[i] += trackBuf[i] * gain;
            }
        }
    }

    // Clamp output to [-1.0, 1.0] to prevent clipping artifacts
    for (size_t i = 0; i < totalSamples; ++i) {
        outBuf[i] = std::clamp(outBuf[i], -1.0f, 1.0f);
    }

    Q_UNUSED(sampleRate)
    return static_cast<int>(totalSamples);
}

} // namespace dawcast
