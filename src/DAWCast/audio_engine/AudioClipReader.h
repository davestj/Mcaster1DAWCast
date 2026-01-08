// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <vector>
#include <cstdint>

namespace dawcast {

class Clip;

/// Decodes and caches the audio for a single timeline Clip.
/// All decoded PCM lives in a contiguous float buffer so the
/// audio thread can read it without allocation or I/O.
class AudioClipReader
{
public:
    explicit AudioClipReader(Clip* clip);
    ~AudioClipReader();

    /// Decode the source file into m_decodedAudio.
    /// Must be called on a worker / GUI thread (not RT-safe).
    bool open();

    /// Copy samples for the given timeline position into output.
    /// RT-safe: no allocation, no I/O — pure index arithmetic + memcpy.
    /// output is interleaved float [frames * channels].
    /// Samples are *added* to output (not overwritten) so callers
    /// can mix multiple readers into the same buffer.
    void readSamples(float* output, int64_t timelinePosition,
                     int frames, int channels);

    /// Release decoded audio memory.
    void close();

    /// Whether the reader has valid decoded audio.
    bool isOpen() const { return !m_decodedAudio.empty(); }

    /// The clip this reader is bound to (never null after construction).
    Clip* clip() const { return m_clip; }

    int channels()   const { return m_channels; }
    int sampleRate() const { return m_sampleRate; }

private:
    Clip*              m_clip = nullptr;
    std::vector<float> m_decodedAudio;   // interleaved float32
    int                m_channels   = 0;
    int                m_sampleRate = 0;
    int                m_totalFrames = 0;
};

} // namespace dawcast
