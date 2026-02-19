// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AudioClipReader.h"
#include "../timeline/Clip.h"
#include "../codec/FFmpegCodec.h"
#include "../codec/WavCodec.h"

#include <QFileInfo>
#include <QDebug>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace dawcast {

AudioClipReader::AudioClipReader(Clip* clip)
    : m_clip(clip)
{
}

AudioClipReader::~AudioClipReader()
{
    close();
}

bool AudioClipReader::open()
{
    if (!m_clip) return false;

    const QString path = m_clip->sourcePath();
    if (path.isEmpty()) {
        qWarning() << "AudioClipReader::open: no source path on clip";
        return false;
    }

    const QFileInfo fi(path);
    if (!fi.exists() || !fi.isReadable()) {
        qWarning() << "AudioClipReader::open: cannot read" << path;
        return false;
    }

    // Decode using the appropriate codec
    AudioBuffer buf{};
    const QString ext = fi.suffix().toLower();

    if (ext == QStringLiteral("wav")) {
        WavCodec codec;
        buf = codec.decode(path);
    } else {
        FFmpegCodec codec;
        buf = codec.decode(path);
    }

    if (!buf.data || buf.frames <= 0 || buf.channels <= 0) {
        qWarning() << "AudioClipReader::open: decode failed for" << path;
        return false;
    }

    // Copy into our owned vector
    int totalSamples = buf.frames * buf.channels;
    m_decodedAudio.resize(static_cast<size_t>(totalSamples));
    std::memcpy(m_decodedAudio.data(), buf.data,
                static_cast<size_t>(totalSamples) * sizeof(float));

    m_channels    = buf.channels;
    m_sampleRate  = buf.sampleRate;
    m_totalFrames = buf.frames;

    // Free the codec-allocated buffer
    delete[] buf.data;
    buf.data = nullptr;

    qDebug() << "AudioClipReader::open:" << path
             << "decoded" << m_totalFrames << "frames,"
             << m_channels << "ch @" << m_sampleRate << "Hz";

    return true;
}

void AudioClipReader::readSamples(float* output, int64_t timelinePosition,
                                  int frames, int channels)
{
    if (!output || m_decodedAudio.empty() || !m_clip) return;

    const int64_t clipStart    = m_clip->timelinePosition();
    const int64_t clipEnd      = m_clip->endPosition();
    const int64_t sourceIn     = m_clip->sourceIn();
    const int64_t fadeInLen    = m_clip->fadeIn();
    const int64_t fadeOutLen   = m_clip->fadeOut();
    const float   clipGain     = m_clip->gain();
    const bool    hasGainEnvelope = !m_clip->gainEnvelope().isEmpty();

    for (int f = 0; f < frames; ++f) {
        int64_t tlPos = timelinePosition + f;

        // Skip frames outside this clip's range
        if (tlPos < clipStart || tlPos >= clipEnd) continue;

        // Offset into the decoded source audio
        int64_t sourceFrame = sourceIn + (tlPos - clipStart);
        if (sourceFrame < 0 || sourceFrame >= m_totalFrames) continue;

        // --- Fade gain calculation ---
        float fadeGain = 1.0f;

        // Fade-in: linear ramp over fadeInLen samples from clip start
        if (fadeInLen > 0) {
            int64_t posInClip = tlPos - clipStart;
            if (posInClip < fadeInLen) {
                fadeGain = static_cast<float>(posInClip) /
                           static_cast<float>(fadeInLen);
            }
        }

        // Fade-out: linear ramp over fadeOutLen samples to clip end
        if (fadeOutLen > 0) {
            int64_t distToEnd = clipEnd - tlPos;
            if (distToEnd <= fadeOutLen) {
                fadeGain *= static_cast<float>(distToEnd) /
                            static_cast<float>(fadeOutLen);
            }
        }

        // --- Clip gain envelope ---
        // When the envelope has points, use the interpolated envelope gain
        // (which overrides the base clip gain).  Otherwise use clipGain.
        float baseGain = clipGain;
        if (hasGainEnvelope) {
            int64_t posInClipForEnv = tlPos - clipStart;
            baseGain = m_clip->gainAt(posInClipForEnv);
        }

        float totalGain = baseGain * fadeGain;

        // Read from decoded buffer and add to output
        int srcCh = m_channels;
        const float* src = m_decodedAudio.data() +
                           static_cast<size_t>(sourceFrame) * static_cast<size_t>(srcCh);

        if (channels >= 2 && srcCh >= 2) {
            // Stereo source -> stereo output
            output[f * channels]     += src[0] * totalGain;
            output[f * channels + 1] += src[1] * totalGain;
        } else if (channels >= 2 && srcCh == 1) {
            // Mono source -> stereo output (dual-mono)
            output[f * channels]     += src[0] * totalGain;
            output[f * channels + 1] += src[0] * totalGain;
        } else if (channels == 1 && srcCh >= 1) {
            // Downmix to mono: average all source channels
            float sum = 0.0f;
            for (int c = 0; c < srcCh; ++c) sum += src[c];
            output[f] += (sum / static_cast<float>(srcCh)) * totalGain;
        }
    }
}

void AudioClipReader::close()
{
    m_decodedAudio.clear();
    m_decodedAudio.shrink_to_fit();
    m_channels    = 0;
    m_sampleRate  = 0;
    m_totalFrames = 0;
}

} // namespace dawcast
