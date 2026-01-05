// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "CodecRegistry.h"

#include <QDebug>

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
    // ---- WAV: always available (built-in, no external library) ----
    registerCodec({"wav", "WAV (PCM)", true, true, {"wav", "wave"}});

    // ---- FLAC ----
#ifdef HAVE_FLAC
    registerCodec({"flac", "FLAC (Free Lossless)", true, true, {"flac"}});
#elif defined(HAVE_AVFORMAT)
    // FFmpeg fallback for FLAC (encode + decode)
    registerCodec({"flac", "FLAC (FFmpeg)", true, true, {"flac"}});
#endif

    // ---- MP3 ----
    {
        bool canEncMp3 = false;
        bool canDecMp3 = false;
        QString mp3Name;

#ifdef HAVE_LAME
        canEncMp3 = true;
        mp3Name = QStringLiteral("MP3 (LAME");
#endif
#ifdef HAVE_MPG123
        canDecMp3 = true;
        if (mp3Name.isEmpty())
            mp3Name = QStringLiteral("MP3 (mpg123");
        else
            mp3Name += QStringLiteral("/mpg123");
#endif
#ifdef HAVE_AVFORMAT
        // FFmpeg can fill in either gap
        if (!canEncMp3) {
            canEncMp3 = true;
            if (mp3Name.isEmpty())
                mp3Name = QStringLiteral("MP3 (FFmpeg");
            else
                mp3Name += QStringLiteral("/FFmpeg");
        }
        if (!canDecMp3) {
            canDecMp3 = true;
            if (mp3Name.isEmpty())
                mp3Name = QStringLiteral("MP3 (FFmpeg");
            else if (!mp3Name.contains(QStringLiteral("FFmpeg")))
                mp3Name += QStringLiteral("/FFmpeg");
        }
#endif
        if (canEncMp3 || canDecMp3) {
            mp3Name += QStringLiteral(")");
            registerCodec({"mp3", mp3Name, canEncMp3, canDecMp3, {"mp3"}});
        }
    }

    // ---- AAC ----
    {
        bool canEncAac = false;
        bool canDecAac = false;
        QString aacName;

#ifdef HAVE_FDKAAC
        canEncAac = true;
        aacName = QStringLiteral("AAC (FDK-AAC");
#endif
#ifdef HAVE_AVFORMAT
        // Decode always via FFmpeg; encode as fallback
        canDecAac = true;
        if (!canEncAac) {
            canEncAac = true;
            aacName = QStringLiteral("AAC (FFmpeg");
        } else {
            aacName += QStringLiteral("/FFmpeg");
        }
#endif
        if (canEncAac || canDecAac) {
            aacName += QStringLiteral(")");
            registerCodec({"aac", aacName, canEncAac, canDecAac, {"aac", "m4a"}});
        }
    }

    // ---- Opus ----
    {
        bool canEncOpus = false;
        bool canDecOpus = false;
        QString opusName;

#ifdef HAVE_OPUS
        canEncOpus = true;
        opusName = QStringLiteral("Opus (libopusenc");
#endif
#ifdef HAVE_OPUSFILE
        canDecOpus = true;
        if (opusName.isEmpty())
            opusName = QStringLiteral("Opus (opusfile");
        else
            opusName += QStringLiteral("/opusfile");
#endif
#ifdef HAVE_AVFORMAT
        if (!canEncOpus) {
            canEncOpus = true;
            if (opusName.isEmpty())
                opusName = QStringLiteral("Opus (FFmpeg");
            else
                opusName += QStringLiteral("/FFmpeg");
        }
        if (!canDecOpus) {
            canDecOpus = true;
            if (opusName.isEmpty())
                opusName = QStringLiteral("Opus (FFmpeg");
            else if (!opusName.contains(QStringLiteral("FFmpeg")))
                opusName += QStringLiteral("/FFmpeg");
        }
#endif
        if (canEncOpus || canDecOpus) {
            opusName += QStringLiteral(")");
            registerCodec({"opus", opusName, canEncOpus, canDecOpus, {"opus"}});
        }
    }

    // ---- Vorbis ----
    {
        bool canEncVorbis = false;
        bool canDecVorbis = false;
        QString vorbisName;

#ifdef HAVE_VORBIS
        canEncVorbis = true;
        canDecVorbis = true;
        vorbisName = QStringLiteral("Ogg Vorbis (libvorbis");
#endif
#ifdef HAVE_AVFORMAT
        if (!canEncVorbis) {
            canEncVorbis = true;
            vorbisName = QStringLiteral("Ogg Vorbis (FFmpeg");
        }
        if (!canDecVorbis) {
            canDecVorbis = true;
            if (vorbisName.isEmpty())
                vorbisName = QStringLiteral("Ogg Vorbis (FFmpeg");
            else if (!vorbisName.contains(QStringLiteral("FFmpeg")))
                vorbisName += QStringLiteral("/FFmpeg");
        }
#endif
        if (canEncVorbis || canDecVorbis) {
            vorbisName += QStringLiteral(")");
            registerCodec({"vorbis", vorbisName, canEncVorbis, canDecVorbis,
                           {"ogg", "oga"}});
        }
    }

    // ---- FFmpeg generic (catch-all for any format FFmpeg supports) ----
#ifdef HAVE_AVFORMAT
    registerCodec({"ffmpeg", "FFmpeg (generic)", true, true, {"*"}});
#endif

    qDebug() << "CodecRegistry: registered" << m_codecs.size() << "codecs";
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
