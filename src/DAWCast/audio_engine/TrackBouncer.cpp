// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "TrackBouncer.h"
#include "AudioClipReader.h"
#include "../timeline/AudioTrack.h"
#include "../timeline/Timeline.h"
#include "../timeline/Clip.h"
#include "../dsp/DspChain.h"

#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QDataStream>
#include <QDebug>

#include <cmath>
#include <cstring>
#include <vector>

namespace dawcast {

// ---------------------------------------------------------------------------
// WAV helper (same IEEE float format as MultitrackRecorder)
// ---------------------------------------------------------------------------

static void writeWavHeader(QFile* file, int channels, int sampleRate)
{
    QDataStream ds(file);
    ds.setByteOrder(QDataStream::LittleEndian);

    file->write("RIFF", 4);
    ds << quint32(0);  // placeholder
    file->write("WAVE", 4);

    file->write("fmt ", 4);
    ds << quint32(16);
    ds << quint16(3);  // IEEE float
    ds << quint16(static_cast<quint16>(channels));
    ds << quint32(static_cast<quint32>(sampleRate));
    quint32 byteRate = static_cast<quint32>(sampleRate * channels) * 4u;
    ds << byteRate;
    ds << quint16(static_cast<quint16>(channels) * 4u);
    ds << quint16(32);

    file->write("data", 4);
    ds << quint32(0);  // placeholder
}

static void finalizeWavHeader(QFile* file)
{
    if (!file || !file->isOpen()) return;
    qint64 fileSize = file->size();
    if (fileSize < 44) return;

    qint64 dataSize = fileSize - 44;

    file->seek(4);
    QDataStream ds(file);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds << quint32(static_cast<quint32>(fileSize - 8));

    file->seek(40);
    ds << quint32(static_cast<quint32>(dataSize));
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TrackBouncer::TrackBouncer(QObject* parent)
    : QObject(parent)
{
}

TrackBouncer::~TrackBouncer() = default;

// ---------------------------------------------------------------------------
// bounceTrack
// ---------------------------------------------------------------------------

void TrackBouncer::bounceTrack(AudioTrack* track, Timeline* timeline,
                               const QString& outputDir)
{
    if (!track || !timeline) {
        emit error(QStringLiteral("Invalid track or timeline"));
        return;
    }

    int clipCount = track->clipCount();
    if (clipCount == 0) {
        emit error(QStringLiteral("Track has no clips to bounce"));
        return;
    }

    // Determine the track's total extent on the timeline
    int64_t trackStart = INT64_MAX;
    int64_t trackEnd   = 0;
    for (int c = 0; c < clipCount; ++c) {
        Clip* clip = track->clip(c);
        if (!clip) continue;
        trackStart = std::min(trackStart, clip->timelinePosition());
        trackEnd   = std::max(trackEnd, clip->endPosition());
    }

    if (trackEnd <= trackStart) {
        emit error(QStringLiteral("Track has zero duration"));
        return;
    }

    int sampleRate = timeline->sampleRate();
    if (sampleRate <= 0) sampleRate = 48000;
    static constexpr int kChannels = 2;
    static constexpr int kBlockSize = 1024;

    // Open clip readers
    std::vector<AudioClipReader*> readers;
    for (int c = 0; c < clipCount; ++c) {
        Clip* clip = track->clip(c);
        if (!clip || clip->sourcePath().isEmpty()) continue;
        auto* reader = new AudioClipReader(clip);
        if (reader->open()) {
            readers.push_back(reader);
        } else {
            delete reader;
        }
    }

    if (readers.empty()) {
        emit error(QStringLiteral("No clips could be opened for reading"));
        return;
    }

    // Create output file
    QDir().mkpath(outputDir);
    QString outPath = outputDir + QStringLiteral("/bounce_%1_%2.wav")
                          .arg(track->name().isEmpty() ? QStringLiteral("track") : track->name())
                          .arg(QDateTime::currentMSecsSinceEpoch());

    QFile outFile(outPath);
    if (!outFile.open(QIODevice::WriteOnly)) {
        emit error(QStringLiteral("Cannot open output file: %1").arg(outPath));
        for (auto* r : readers) delete r;
        return;
    }

    writeWavHeader(&outFile, kChannels, sampleRate);

    // Process block by block
    int64_t totalFrames = trackEnd - trackStart;
    int64_t processed = 0;
    int lastPercent = 0;

    std::vector<float> buffer(static_cast<size_t>(kBlockSize * kChannels), 0.0f);

    DspChain* chain = track->effectChain();

    for (int64_t pos = trackStart; pos < trackEnd; pos += kBlockSize) {
        int frames = static_cast<int>(std::min(static_cast<int64_t>(kBlockSize),
                                                trackEnd - pos));
        int totalSamples = frames * kChannels;

        // Zero the buffer
        std::memset(buffer.data(), 0, static_cast<size_t>(totalSamples) * sizeof(float));

        // Accumulate all clips at this position
        for (auto* reader : readers) {
            reader->readSamples(buffer.data(), pos, frames, kChannels);
        }

        // Apply DSP chain (if present and not bypassed)
        if (chain && chain->effectCount() > 0) {
            chain->process(buffer.data(), frames, kChannels);
        }

        // Write to file
        outFile.write(reinterpret_cast<const char*>(buffer.data()),
                      static_cast<qint64>(totalSamples) * sizeof(float));

        processed += frames;
        int percent = static_cast<int>(processed * 100 / totalFrames);
        if (percent != lastPercent) {
            lastPercent = percent;
            emit progress(percent);
        }
    }

    // Finalize WAV
    finalizeWavHeader(&outFile);
    outFile.close();

    // Clean up readers
    for (auto* r : readers) delete r;

    // Replace the track's clips with a single bounced clip.
    // Remove old clips (from last to first to keep indices valid).
    while (track->clipCount() > 0) {
        track->removeClip(track->clipCount() - 1);
    }

    // Add the new bounced clip
    auto* bouncedClip = new Clip(track);
    bouncedClip->setSourcePath(outPath);
    bouncedClip->setSourceIn(0);
    bouncedClip->setSourceOut(totalFrames);
    bouncedClip->setTimelinePosition(trackStart);
    track->addClip(bouncedClip);

    qDebug() << "TrackBouncer: bounced" << totalFrames << "frames to" << outPath;
    emit finished(outPath);
}

// ---------------------------------------------------------------------------
// freezeTrack
// ---------------------------------------------------------------------------

void TrackBouncer::freezeTrack(AudioTrack* track, Timeline* timeline,
                               const QString& outputDir)
{
    // Bounce first
    bounceTrack(track, timeline, outputDir);

    // After bounce, bypass the DspChain (but keep it for later unfreeze)
    DspChain* chain = track->effectChain();
    if (chain) {
        chain->bypass(true);
    }
}

// ---------------------------------------------------------------------------
// unfreezeTrack (static)
// ---------------------------------------------------------------------------

void TrackBouncer::unfreezeTrack(AudioTrack* track)
{
    if (!track) return;

    DspChain* chain = track->effectChain();
    if (chain) {
        chain->bypass(false);
    }
}

} // namespace dawcast
