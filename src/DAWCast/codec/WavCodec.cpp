// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "WavCodec.h"

// TODO: #include <sndfile.h>  // libsndfile for robust WAV I/O

namespace dawcast {

bool WavCodec::encode(const AudioBuffer &buffer, const QString &path, int bitDepth)
{
    // TODO: Use libsndfile to write WAV file
    // SF_INFO info;
    // info.samplerate = buffer.sampleRate;
    // info.channels = buffer.channels;
    // info.format = SF_FORMAT_WAV | (bitDepth == 24 ? SF_FORMAT_PCM_24 : SF_FORMAT_PCM_16);
    // SNDFILE *file = sf_open(path.toUtf8().constData(), SFM_WRITE, &info);
    // sf_writef_float(file, buffer.data, buffer.frames);
    // sf_close(file);
    Q_UNUSED(buffer)
    Q_UNUSED(path)
    Q_UNUSED(bitDepth)
    return false;
}

AudioBuffer WavCodec::decode(const QString &path)
{
    // TODO: Use libsndfile to read WAV file into AudioBuffer
    // SF_INFO info;
    // SNDFILE *file = sf_open(path.toUtf8().constData(), SFM_READ, &info);
    // Allocate buffer.data, set frames/channels/sampleRate from info
    // sf_readf_float(file, buffer.data, info.frames);
    // sf_close(file);
    Q_UNUSED(path)
    return AudioBuffer{};
}

} // namespace dawcast
