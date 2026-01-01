// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "CodecRegistry.h"

namespace dawcast {

CodecRegistry &CodecRegistry::instance()
{
    static CodecRegistry registry;
    return registry;
}

CodecRegistry::CodecRegistry(QObject *parent)
    : QObject(parent)
{
    populateDefaults();
}

CodecRegistry::~CodecRegistry() = default;

void CodecRegistry::populateDefaults()
{
    // WAV is always available (built-in)
    registerCodec({"wav", "WAV (PCM)", true, true, {"wav", "wave"}});

#ifdef HAVE_FLAC
    registerCodec({"flac", "FLAC (Free Lossless)", true, true, {"flac"}});
#endif

#ifdef HAVE_LAME
    registerCodec({"mp3", "MP3 (LAME)", true,
#ifdef HAVE_MPG123
                    true,
#else
                    false,
#endif
                    {"mp3"}});
#elif defined(HAVE_MPG123)
    registerCodec({"mp3", "MP3 (mpg123 decode-only)", false, true, {"mp3"}});
#endif

#ifdef HAVE_FDKAAC
    registerCodec({"aac", "AAC (FDK-AAC)", true, true, {"aac", "m4a"}});
#endif

#ifdef HAVE_OPUS
    registerCodec({"opus", "Opus", true, true, {"opus", "ogg"}});
#endif

#ifdef HAVE_VORBIS
    registerCodec({"vorbis", "Ogg Vorbis", true, true, {"ogg", "oga"}});
#endif

#ifdef HAVE_AVFORMAT
    registerCodec({"ffmpeg", "FFmpeg (generic)", true, true, {"*"}});
#endif
}

void CodecRegistry::registerCodec(const CodecInfo &info)
{
    m_codecs.insert(info.id, info);
}

QList<CodecInfo> CodecRegistry::availableCodecs() const
{
    return m_codecs.values();
}

CodecInfo CodecRegistry::codecById(const QString &id) const
{
    return m_codecs.value(id);
}

bool CodecRegistry::canEncode(const QString &id) const
{
    return m_codecs.contains(id) && m_codecs.value(id).canEncode;
}

bool CodecRegistry::canDecode(const QString &id) const
{
    return m_codecs.contains(id) && m_codecs.value(id).canDecode;
}

} // namespace dawcast
