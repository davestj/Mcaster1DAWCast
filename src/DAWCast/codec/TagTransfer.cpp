// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "TagTransfer.h"

#include <QDebug>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#ifdef HAVE_TAGLIB
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/tpropertymap.h>

// MP3 / ID3v2
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/chapterframe.h>
#include <taglib/commentsframe.h>
#include <taglib/textidentificationframe.h>
#include <taglib/urllinkframe.h>
#include <taglib/unsynchronizedlyricsframe.h>

// FLAC
#include <taglib/flacfile.h>
#include <taglib/flacpicture.h>
#include <taglib/xiphcomment.h>

// Ogg Vorbis
#include <taglib/vorbisfile.h>

// Opus
#include <taglib/opusfile.h>

// MP4 / M4A / AAC
#include <taglib/mp4file.h>
#include <taglib/mp4tag.h>
#include <taglib/mp4coverart.h>
#include <taglib/mp4item.h>

// WAV (RIFF)
#include <taglib/wavfile.h>

// WavPack
#include <taglib/wavpackfile.h>
#include <taglib/apetag.h>
#include <taglib/apeitem.h>
#endif // HAVE_TAGLIB

namespace dawcast {

// ---------------------------------------------------------------------------
// AudioTags::isEmpty
// ---------------------------------------------------------------------------

bool AudioTags::isEmpty() const
{
    return title.isEmpty()
        && artist.isEmpty()
        && album.isEmpty()
        && albumArtist.isEmpty()
        && genre.isEmpty()
        && comment.isEmpty()
        && composer.isEmpty()
        && year == 0
        && trackNumber == 0
        && trackTotal == 0
        && discNumber == 0
        && discTotal == 0
        && lyrics.isEmpty()
        && copyright.isEmpty()
        && encoder.isEmpty()
        && url.isEmpty()
        && podcastTitle.isEmpty()
        && podcastEpisode.isEmpty()
        && podcastSeason.isEmpty()
        && podcastCategory.isEmpty()
        && podcastDescription.isEmpty()
        && artworkData.isEmpty()
        && chapters.isEmpty()
        && replayGainTrackGain == 0.0f
        && replayGainTrackPeak == 0.0f;
}

// ---------------------------------------------------------------------------
// Helper: file extension (lower-cased, without the dot)
// ---------------------------------------------------------------------------

static QString fileExt(const QString& path)
{
    return QFileInfo(path).suffix().toLower();
}

#ifdef HAVE_TAGLIB

// ---------------------------------------------------------------------------
// Helper: TagLib::String <-> QString conversion
// ---------------------------------------------------------------------------

static inline TagLib::String toTString(const QString& s)
{
    return TagLib::String(s.toUtf8().constData(), TagLib::String::UTF8);
}

static inline QString fromTString(const TagLib::String& s)
{
    return QString::fromUtf8(s.toCString(true));
}

// ---------------------------------------------------------------------------
// Helper: read a TXXX frame value by description from ID3v2 tag
// ---------------------------------------------------------------------------

static QString readTXXX(const TagLib::ID3v2::Tag* tag, const char* desc)
{
    if (!tag) return {};
    const auto& frames = tag->frameList("TXXX");
    for (const auto* frame : frames) {
        auto* txxx = dynamic_cast<const TagLib::ID3v2::UserTextIdentificationFrame*>(frame);
        if (txxx && txxx->description().upper() == TagLib::String(desc).upper()) {
            TagLib::StringList fields = txxx->fieldList();
            // fieldList()[0] is the description, [1] is the value
            if (fields.size() >= 2) {
                return fromTString(fields[1]);
            }
        }
    }
    return {};
}

// ---------------------------------------------------------------------------
// Helper: write a TXXX frame (remove existing with same description first)
// ---------------------------------------------------------------------------

static void writeTXXX(TagLib::ID3v2::Tag* tag, const char* desc, const QString& value)
{
    if (!tag) return;

    // Remove existing frames with this description
    const auto& frames = tag->frameList("TXXX");
    for (auto* frame : frames) {
        auto* txxx = dynamic_cast<TagLib::ID3v2::UserTextIdentificationFrame*>(frame);
        if (txxx && txxx->description().upper() == TagLib::String(desc).upper()) {
            tag->removeFrame(frame);
            break; // iterator invalidated
        }
    }

    if (value.isEmpty()) return;

    auto* txxx = new TagLib::ID3v2::UserTextIdentificationFrame(TagLib::String::UTF8);
    txxx->setDescription(TagLib::String(desc, TagLib::String::UTF8));
    txxx->setText(toTString(value));
    tag->addFrame(txxx);
}

// ---------------------------------------------------------------------------
// Helper: read Vorbis comment field
// ---------------------------------------------------------------------------

static QString readVorbisField(const TagLib::Ogg::XiphComment* vc, const char* key)
{
    if (!vc) return {};
    const auto& fields = vc->fieldListMap();
    TagLib::String tkey(key, TagLib::String::UTF8);
    if (fields.contains(tkey) && !fields[tkey].isEmpty()) {
        return fromTString(fields[tkey].front());
    }
    return {};
}

// ---------------------------------------------------------------------------
// Helper: write Vorbis comment field
// ---------------------------------------------------------------------------

static void writeVorbisField(TagLib::Ogg::XiphComment* vc,
                             const char* key, const QString& value)
{
    if (!vc) return;
    if (value.isEmpty()) {
        vc->removeFields(TagLib::String(key, TagLib::String::UTF8));
    } else {
        vc->addField(TagLib::String(key, TagLib::String::UTF8), toTString(value), true);
    }
}

// ---------------------------------------------------------------------------
// Helper: serialize / deserialize chapters as JSON for non-MP3 formats
// ---------------------------------------------------------------------------

static QString chaptersToJson(const QList<AudioTags::Chapter>& chapters)
{
    if (chapters.isEmpty()) return {};
    QJsonArray arr;
    for (const auto& ch : chapters) {
        QJsonObject obj;
        obj[QStringLiteral("start")] = static_cast<qint64>(ch.startMs);
        obj[QStringLiteral("end")]   = static_cast<qint64>(ch.endMs);
        obj[QStringLiteral("title")] = ch.title;
        if (!ch.url.isEmpty())
            obj[QStringLiteral("url")] = ch.url;
        arr.append(obj);
    }
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

static QList<AudioTags::Chapter> chaptersFromJson(const QString& json)
{
    QList<AudioTags::Chapter> result;
    if (json.isEmpty()) return result;
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isArray()) return result;
    const QJsonArray arr = doc.array();
    for (const QJsonValue& val : arr) {
        QJsonObject obj = val.toObject();
        AudioTags::Chapter ch;
        ch.startMs = obj.value(QStringLiteral("start")).toInteger();
        ch.endMs   = obj.value(QStringLiteral("end")).toInteger();
        ch.title   = obj.value(QStringLiteral("title")).toString();
        ch.url     = obj.value(QStringLiteral("url")).toString();
        result.append(ch);
    }
    return result;
}

// =========================================================================
// readTags — Format-specific readers
// =========================================================================

// ---------------------------------------------------------------------------
// readGenericTags — baseline via TagLib::FileRef (works for all formats)
// ---------------------------------------------------------------------------

static void readGenericTags(const QString& path, AudioTags& tags)
{
    TagLib::FileRef fileRef(path.toUtf8().constData());
    if (fileRef.isNull() || !fileRef.tag()) return;

    const TagLib::Tag* tag = fileRef.tag();
    tags.title   = fromTString(tag->title());
    tags.artist  = fromTString(tag->artist());
    tags.album   = fromTString(tag->album());
    tags.genre   = fromTString(tag->genre());
    tags.comment = fromTString(tag->comment());
    tags.year    = static_cast<int>(tag->year());
    tags.trackNumber = static_cast<int>(tag->track());

    // PropertyMap for extended tags
    TagLib::PropertyMap props = fileRef.file()->properties();
    auto readProp = [&](const char* key) -> QString {
        TagLib::String tkey(key, TagLib::String::UTF8);
        if (props.contains(tkey) && !props[tkey].isEmpty())
            return fromTString(props[tkey].front());
        return {};
    };

    if (tags.albumArtist.isEmpty()) tags.albumArtist = readProp("ALBUMARTIST");
    if (tags.composer.isEmpty())    tags.composer    = readProp("COMPOSER");
    if (tags.copyright.isEmpty())   tags.copyright   = readProp("COPYRIGHT");
    if (tags.lyrics.isEmpty())      tags.lyrics      = readProp("LYRICS");
    if (tags.encoder.isEmpty())     tags.encoder     = readProp("ENCODEDBY");
    if (tags.url.isEmpty())         tags.url         = readProp("URL");

    // Disc number (DISCNUMBER might be "1/2")
    QString disc = readProp("DISCNUMBER");
    if (!disc.isEmpty()) {
        QStringList parts = disc.split(QLatin1Char('/'));
        tags.discNumber = parts.value(0).toInt();
        if (parts.size() > 1) tags.discTotal = parts.value(1).toInt();
    }

    // Track total (TRACKTOTAL or TRACKNUMBER = "3/12")
    QString trackTotal = readProp("TRACKTOTAL");
    if (!trackTotal.isEmpty()) {
        tags.trackTotal = trackTotal.toInt();
    }
}

// ---------------------------------------------------------------------------
// readMp3Tags — ID3v2-specific: APIC, CHAP, TXXX, USLT, COMM
// ---------------------------------------------------------------------------

static void readMp3Tags(const QString& path, AudioTags& tags)
{
    TagLib::MPEG::File file(path.toUtf8().constData());
    if (!file.isValid()) return;

    TagLib::ID3v2::Tag* id3 = file.ID3v2Tag(false);
    if (!id3) return;

    // Artwork (APIC)
    {
        const auto& frames = id3->frameList("APIC");
        for (const auto* frame : frames) {
            auto* pic = dynamic_cast<const TagLib::ID3v2::AttachedPictureFrame*>(frame);
            if (pic && pic->type() == TagLib::ID3v2::AttachedPictureFrame::FrontCover) {
                const TagLib::ByteVector& bv = pic->picture();
                tags.artworkData = QByteArray(bv.data(), static_cast<int>(bv.size()));
                tags.artworkMimeType = fromTString(pic->mimeType());
                break;
            }
        }
        // Fallback: take the first APIC regardless of type
        if (tags.artworkData.isEmpty() && !frames.isEmpty()) {
            auto* pic = dynamic_cast<const TagLib::ID3v2::AttachedPictureFrame*>(frames.front());
            if (pic) {
                const TagLib::ByteVector& bv = pic->picture();
                tags.artworkData = QByteArray(bv.data(), static_cast<int>(bv.size()));
                tags.artworkMimeType = fromTString(pic->mimeType());
            }
        }
    }

    // Chapters (CHAP)
    {
        const auto& frames = id3->frameList("CHAP");
        for (const auto* frame : frames) {
            auto* chap = dynamic_cast<const TagLib::ID3v2::ChapterFrame*>(frame);
            if (!chap) continue;
            AudioTags::Chapter ch;
            ch.startMs = chap->startTime();
            ch.endMs   = chap->endTime();

            // Extract TIT2 sub-frame for chapter title
            const auto& embedded = chap->embeddedFrameList("TIT2");
            if (!embedded.isEmpty()) {
                auto* tit2 = dynamic_cast<const TagLib::ID3v2::TextIdentificationFrame*>(
                    embedded.front());
                if (tit2) ch.title = fromTString(tit2->toString());
            }

            // Extract WXXX sub-frame for chapter URL
            const auto& urlFrames = chap->embeddedFrameList("WXXX");
            if (!urlFrames.isEmpty()) {
                auto* wxxx = dynamic_cast<const TagLib::ID3v2::UserUrlLinkFrame*>(
                    urlFrames.front());
                if (wxxx) ch.url = fromTString(wxxx->url());
            }

            tags.chapters.append(ch);
        }
    }

    // Unsynchronized lyrics (USLT)
    if (tags.lyrics.isEmpty()) {
        const auto& frames = id3->frameList("USLT");
        if (!frames.isEmpty()) {
            auto* uslt = dynamic_cast<const TagLib::ID3v2::UnsynchronizedLyricsFrame*>(
                frames.front());
            if (uslt) tags.lyrics = fromTString(uslt->text());
        }
    }

    // TXXX frames for extended metadata
    tags.podcastTitle       = readTXXX(id3, "PODCAST_TITLE");
    tags.podcastEpisode     = readTXXX(id3, "PODCAST_EPISODE");
    tags.podcastSeason      = readTXXX(id3, "PODCAST_SEASON");
    tags.podcastCategory    = readTXXX(id3, "PODCAST_CATEGORY");
    tags.podcastDescription = readTXXX(id3, "PODCAST_DESCRIPTION");

    QString rgGain = readTXXX(id3, "REPLAYGAIN_TRACK_GAIN");
    if (!rgGain.isEmpty()) {
        // Format: "-6.5 dB" — strip the " dB" suffix
        tags.replayGainTrackGain = rgGain.remove(QStringLiteral(" dB"),
                                                  Qt::CaseInsensitive).toFloat();
    }
    QString rgPeak = readTXXX(id3, "REPLAYGAIN_TRACK_PEAK");
    if (!rgPeak.isEmpty()) {
        tags.replayGainTrackPeak = rgPeak.toFloat();
    }

    // Composer from TCOM if not already set
    if (tags.composer.isEmpty()) {
        const auto& frames = id3->frameList("TCOM");
        if (!frames.isEmpty()) {
            tags.composer = fromTString(frames.front()->toString());
        }
    }

    // Copyright from TCOP
    if (tags.copyright.isEmpty()) {
        const auto& frames = id3->frameList("TCOP");
        if (!frames.isEmpty()) {
            tags.copyright = fromTString(frames.front()->toString());
        }
    }

    // Album artist from TPE2
    if (tags.albumArtist.isEmpty()) {
        const auto& frames = id3->frameList("TPE2");
        if (!frames.isEmpty()) {
            tags.albumArtist = fromTString(frames.front()->toString());
        }
    }

    // Encoder from TENC
    if (tags.encoder.isEmpty()) {
        const auto& frames = id3->frameList("TENC");
        if (!frames.isEmpty()) {
            tags.encoder = fromTString(frames.front()->toString());
        }
    }

    // URL from WXXX
    if (tags.url.isEmpty()) {
        const auto& frames = id3->frameList("WXXX");
        if (!frames.isEmpty()) {
            auto* wxxx = dynamic_cast<const TagLib::ID3v2::UserUrlLinkFrame*>(
                frames.front());
            if (wxxx) tags.url = fromTString(wxxx->url());
        }
    }
}

// ---------------------------------------------------------------------------
// readFlacTags — Vorbis comments + FLAC Picture blocks
// ---------------------------------------------------------------------------

static void readFlacTags(const QString& path, AudioTags& tags)
{
    TagLib::FLAC::File file(path.toUtf8().constData());
    if (!file.isValid()) return;

    // Artwork from FLAC Picture blocks
    const auto& pictures = file.pictureList();
    for (const auto* pic : pictures) {
        if (pic->type() == TagLib::FLAC::Picture::FrontCover || tags.artworkData.isEmpty()) {
            const TagLib::ByteVector& bv = pic->data();
            tags.artworkData = QByteArray(bv.data(), static_cast<int>(bv.size()));
            tags.artworkMimeType = fromTString(pic->mimeType());
            if (pic->type() == TagLib::FLAC::Picture::FrontCover) break;
        }
    }

    // Extended tags from Vorbis comments
    TagLib::Ogg::XiphComment* vc = file.xiphComment(false);
    if (!vc) return;

    if (tags.albumArtist.isEmpty()) tags.albumArtist = readVorbisField(vc, "ALBUMARTIST");
    if (tags.composer.isEmpty())    tags.composer    = readVorbisField(vc, "COMPOSER");
    if (tags.copyright.isEmpty())   tags.copyright   = readVorbisField(vc, "COPYRIGHT");
    if (tags.lyrics.isEmpty())      tags.lyrics      = readVorbisField(vc, "LYRICS");
    if (tags.encoder.isEmpty())     tags.encoder     = readVorbisField(vc, "ENCODER");
    if (tags.url.isEmpty())         tags.url         = readVorbisField(vc, "URL");

    tags.podcastTitle       = readVorbisField(vc, "PODCAST_TITLE");
    tags.podcastEpisode     = readVorbisField(vc, "PODCAST_EPISODE");
    tags.podcastSeason      = readVorbisField(vc, "PODCAST_SEASON");
    tags.podcastCategory    = readVorbisField(vc, "PODCAST_CATEGORY");
    tags.podcastDescription = readVorbisField(vc, "PODCAST_DESCRIPTION");

    QString rgGain = readVorbisField(vc, "REPLAYGAIN_TRACK_GAIN");
    if (!rgGain.isEmpty())
        tags.replayGainTrackGain = rgGain.remove(QStringLiteral(" dB"),
                                                  Qt::CaseInsensitive).toFloat();
    QString rgPeak = readVorbisField(vc, "REPLAYGAIN_TRACK_PEAK");
    if (!rgPeak.isEmpty())
        tags.replayGainTrackPeak = rgPeak.toFloat();

    // Chapters stored as JSON in DAWCAST_CHAPTERS
    QString chapJson = readVorbisField(vc, "DAWCAST_CHAPTERS");
    if (!chapJson.isEmpty() && tags.chapters.isEmpty()) {
        tags.chapters = chaptersFromJson(chapJson);
    }

    // Disc number
    if (tags.discNumber == 0) {
        QString disc = readVorbisField(vc, "DISCNUMBER");
        if (!disc.isEmpty()) {
            QStringList parts = disc.split(QLatin1Char('/'));
            tags.discNumber = parts.value(0).toInt();
            if (parts.size() > 1) tags.discTotal = parts.value(1).toInt();
        }
    }
    if (tags.discTotal == 0) {
        QString dt = readVorbisField(vc, "DISCTOTAL");
        if (!dt.isEmpty()) tags.discTotal = dt.toInt();
    }
    if (tags.trackTotal == 0) {
        QString tt = readVorbisField(vc, "TRACKTOTAL");
        if (!tt.isEmpty()) tags.trackTotal = tt.toInt();
    }
}

// ---------------------------------------------------------------------------
// readOggVorbisTags — Vorbis comments (artwork via METADATA_BLOCK_PICTURE)
// ---------------------------------------------------------------------------

static void readOggVorbisTags(const QString& path, AudioTags& tags)
{
    TagLib::Ogg::Vorbis::File file(path.toUtf8().constData());
    if (!file.isValid()) return;

    TagLib::Ogg::XiphComment* vc = file.tag();
    if (!vc) return;

    // Extended tags
    if (tags.albumArtist.isEmpty()) tags.albumArtist = readVorbisField(vc, "ALBUMARTIST");
    if (tags.composer.isEmpty())    tags.composer    = readVorbisField(vc, "COMPOSER");
    if (tags.copyright.isEmpty())   tags.copyright   = readVorbisField(vc, "COPYRIGHT");
    if (tags.lyrics.isEmpty())      tags.lyrics      = readVorbisField(vc, "LYRICS");
    if (tags.encoder.isEmpty())     tags.encoder     = readVorbisField(vc, "ENCODER");
    if (tags.url.isEmpty())         tags.url         = readVorbisField(vc, "URL");

    tags.podcastTitle       = readVorbisField(vc, "PODCAST_TITLE");
    tags.podcastEpisode     = readVorbisField(vc, "PODCAST_EPISODE");
    tags.podcastSeason      = readVorbisField(vc, "PODCAST_SEASON");
    tags.podcastCategory    = readVorbisField(vc, "PODCAST_CATEGORY");
    tags.podcastDescription = readVorbisField(vc, "PODCAST_DESCRIPTION");

    QString rgGain = readVorbisField(vc, "REPLAYGAIN_TRACK_GAIN");
    if (!rgGain.isEmpty())
        tags.replayGainTrackGain = rgGain.remove(QStringLiteral(" dB"),
                                                  Qt::CaseInsensitive).toFloat();
    QString rgPeak = readVorbisField(vc, "REPLAYGAIN_TRACK_PEAK");
    if (!rgPeak.isEmpty())
        tags.replayGainTrackPeak = rgPeak.toFloat();

    // Chapters
    QString chapJson = readVorbisField(vc, "DAWCAST_CHAPTERS");
    if (!chapJson.isEmpty() && tags.chapters.isEmpty())
        tags.chapters = chaptersFromJson(chapJson);

    // Artwork via METADATA_BLOCK_PICTURE (base64-encoded FLAC Picture)
    if (tags.artworkData.isEmpty()) {
        const auto& picList = vc->pictureList();
        for (const auto* pic : picList) {
            if (!pic) continue;
            const TagLib::ByteVector& bv = pic->data();
            tags.artworkData = QByteArray(bv.data(), static_cast<int>(bv.size()));
            tags.artworkMimeType = fromTString(pic->mimeType());
            if (pic->type() == TagLib::FLAC::Picture::FrontCover) break;
        }
    }

    // Disc / track totals
    if (tags.discNumber == 0) {
        QString disc = readVorbisField(vc, "DISCNUMBER");
        if (!disc.isEmpty()) tags.discNumber = disc.toInt();
    }
    if (tags.discTotal == 0) {
        QString dt = readVorbisField(vc, "DISCTOTAL");
        if (!dt.isEmpty()) tags.discTotal = dt.toInt();
    }
    if (tags.trackTotal == 0) {
        QString tt = readVorbisField(vc, "TRACKTOTAL");
        if (!tt.isEmpty()) tags.trackTotal = tt.toInt();
    }
}

// ---------------------------------------------------------------------------
// readOpusTags — same Vorbis comment structure as Ogg Vorbis
// ---------------------------------------------------------------------------

static void readOpusTags(const QString& path, AudioTags& tags)
{
    TagLib::Ogg::Opus::File file(path.toUtf8().constData());
    if (!file.isValid()) return;

    TagLib::Ogg::XiphComment* vc = file.tag();
    if (!vc) return;

    // Reuse the same Vorbis comment reading logic
    if (tags.albumArtist.isEmpty()) tags.albumArtist = readVorbisField(vc, "ALBUMARTIST");
    if (tags.composer.isEmpty())    tags.composer    = readVorbisField(vc, "COMPOSER");
    if (tags.copyright.isEmpty())   tags.copyright   = readVorbisField(vc, "COPYRIGHT");
    if (tags.lyrics.isEmpty())      tags.lyrics      = readVorbisField(vc, "LYRICS");
    if (tags.encoder.isEmpty())     tags.encoder     = readVorbisField(vc, "ENCODER");
    if (tags.url.isEmpty())         tags.url         = readVorbisField(vc, "URL");

    tags.podcastTitle       = readVorbisField(vc, "PODCAST_TITLE");
    tags.podcastEpisode     = readVorbisField(vc, "PODCAST_EPISODE");
    tags.podcastSeason      = readVorbisField(vc, "PODCAST_SEASON");
    tags.podcastCategory    = readVorbisField(vc, "PODCAST_CATEGORY");
    tags.podcastDescription = readVorbisField(vc, "PODCAST_DESCRIPTION");

    QString rgGain = readVorbisField(vc, "REPLAYGAIN_TRACK_GAIN");
    if (!rgGain.isEmpty())
        tags.replayGainTrackGain = rgGain.remove(QStringLiteral(" dB"),
                                                  Qt::CaseInsensitive).toFloat();
    QString rgPeak = readVorbisField(vc, "REPLAYGAIN_TRACK_PEAK");
    if (!rgPeak.isEmpty())
        tags.replayGainTrackPeak = rgPeak.toFloat();

    QString chapJson = readVorbisField(vc, "DAWCAST_CHAPTERS");
    if (!chapJson.isEmpty() && tags.chapters.isEmpty())
        tags.chapters = chaptersFromJson(chapJson);

    // Artwork
    if (tags.artworkData.isEmpty()) {
        const auto& picList = vc->pictureList();
        for (const auto* pic : picList) {
            if (!pic) continue;
            const TagLib::ByteVector& bv = pic->data();
            tags.artworkData = QByteArray(bv.data(), static_cast<int>(bv.size()));
            tags.artworkMimeType = fromTString(pic->mimeType());
            if (pic->type() == TagLib::FLAC::Picture::FrontCover) break;
        }
    }

    if (tags.discNumber == 0) {
        QString disc = readVorbisField(vc, "DISCNUMBER");
        if (!disc.isEmpty()) tags.discNumber = disc.toInt();
    }
    if (tags.discTotal == 0) {
        QString dt = readVorbisField(vc, "DISCTOTAL");
        if (!dt.isEmpty()) tags.discTotal = dt.toInt();
    }
    if (tags.trackTotal == 0) {
        QString tt = readVorbisField(vc, "TRACKTOTAL");
        if (!tt.isEmpty()) tags.trackTotal = tt.toInt();
    }
}

// ---------------------------------------------------------------------------
// readMp4Tags — MP4/M4A/AAC iTunes-style atoms
// ---------------------------------------------------------------------------

static void readMp4Tags(const QString& path, AudioTags& tags)
{
    TagLib::MP4::File file(path.toUtf8().constData());
    if (!file.isValid()) return;

    TagLib::MP4::Tag* mp4tag = file.tag();
    if (!mp4tag) return;

    const auto& items = mp4tag->itemMap();

    auto readItem = [&](const char* key) -> QString {
        if (items.contains(key)) {
            return fromTString(items[key].toStringList().toString(", "));
        }
        return {};
    };

    auto readIntItem = [&](const char* key) -> int {
        if (items.contains(key)) {
            return items[key].toInt();
        }
        return 0;
    };

    // Standard atoms
    if (tags.albumArtist.isEmpty()) tags.albumArtist = readItem("aART");
    if (tags.composer.isEmpty())    tags.composer    = readItem("\251wrt");
    if (tags.copyright.isEmpty())   tags.copyright   = readItem("cprt");
    if (tags.lyrics.isEmpty())      tags.lyrics      = readItem("\251lyr");
    if (tags.encoder.isEmpty())     tags.encoder     = readItem("\251too");

    // Disc number from "disk" atom (pair: disc/total)
    if (tags.discNumber == 0 && items.contains("disk")) {
        auto pair = items["disk"].toIntPair();
        tags.discNumber = pair.first;
        tags.discTotal  = pair.second;
    }

    // Track total from "trkn" atom (pair: track/total)
    if (items.contains("trkn")) {
        auto pair = items["trkn"].toIntPair();
        if (tags.trackNumber == 0) tags.trackNumber = pair.first;
        if (tags.trackTotal == 0)  tags.trackTotal  = pair.second;
    }

    // Podcast-specific atoms
    tags.podcastTitle       = readItem("----:com.apple.iTunes:PODCAST_TITLE");
    tags.podcastEpisode     = readItem("----:com.apple.iTunes:PODCAST_EPISODE");
    tags.podcastSeason      = readItem("----:com.apple.iTunes:PODCAST_SEASON");
    tags.podcastCategory    = readItem("----:com.apple.iTunes:PODCAST_CATEGORY");
    tags.podcastDescription = readItem("----:com.apple.iTunes:PODCAST_DESCRIPTION");

    // ReplayGain
    QString rgGain = readItem("----:com.apple.iTunes:REPLAYGAIN_TRACK_GAIN");
    if (!rgGain.isEmpty())
        tags.replayGainTrackGain = rgGain.remove(QStringLiteral(" dB"),
                                                  Qt::CaseInsensitive).toFloat();
    QString rgPeak = readItem("----:com.apple.iTunes:REPLAYGAIN_TRACK_PEAK");
    if (!rgPeak.isEmpty())
        tags.replayGainTrackPeak = rgPeak.toFloat();

    // Chapters
    QString chapJson = readItem("----:com.apple.iTunes:DAWCAST_CHAPTERS");
    if (!chapJson.isEmpty() && tags.chapters.isEmpty())
        tags.chapters = chaptersFromJson(chapJson);

    // Artwork (covr atom)
    if (tags.artworkData.isEmpty() && items.contains("covr")) {
        const TagLib::MP4::CoverArtList coverList = items["covr"].toCoverArtList();
        if (!coverList.isEmpty()) {
            const TagLib::MP4::CoverArt& art = coverList.front();
            const TagLib::ByteVector& bv = art.data();
            tags.artworkData = QByteArray(bv.data(), static_cast<int>(bv.size()));
            tags.artworkMimeType = (art.format() == TagLib::MP4::CoverArt::PNG)
                                       ? QStringLiteral("image/png")
                                       : QStringLiteral("image/jpeg");
        }
    }
}

// ---------------------------------------------------------------------------
// readWavTags — RIFF INFO chunks or ID3v2 embedded tag
// ---------------------------------------------------------------------------

static void readWavTags(const QString& path, AudioTags& tags)
{
    TagLib::RIFF::WAV::File file(path.toUtf8().constData());
    if (!file.isValid()) return;

    // WAV files can have an embedded ID3v2 tag
    TagLib::ID3v2::Tag* id3 = file.ID3v2Tag();
    if (id3) {
        // Read artwork from APIC
        const auto& frames = id3->frameList("APIC");
        for (const auto* frame : frames) {
            auto* pic = dynamic_cast<const TagLib::ID3v2::AttachedPictureFrame*>(frame);
            if (pic) {
                const TagLib::ByteVector& bv = pic->picture();
                tags.artworkData = QByteArray(bv.data(), static_cast<int>(bv.size()));
                tags.artworkMimeType = fromTString(pic->mimeType());
                if (pic->type() == TagLib::ID3v2::AttachedPictureFrame::FrontCover) break;
            }
        }

        // Podcast / extended tags
        tags.podcastTitle       = readTXXX(id3, "PODCAST_TITLE");
        tags.podcastEpisode     = readTXXX(id3, "PODCAST_EPISODE");
        tags.podcastSeason      = readTXXX(id3, "PODCAST_SEASON");
        tags.podcastCategory    = readTXXX(id3, "PODCAST_CATEGORY");
        tags.podcastDescription = readTXXX(id3, "PODCAST_DESCRIPTION");

        QString rgGain = readTXXX(id3, "REPLAYGAIN_TRACK_GAIN");
        if (!rgGain.isEmpty())
            tags.replayGainTrackGain = rgGain.remove(QStringLiteral(" dB"),
                                                      Qt::CaseInsensitive).toFloat();
        QString rgPeak = readTXXX(id3, "REPLAYGAIN_TRACK_PEAK");
        if (!rgPeak.isEmpty())
            tags.replayGainTrackPeak = rgPeak.toFloat();

        // Composer, copyright, album artist, encoder
        if (tags.composer.isEmpty()) {
            const auto& f = id3->frameList("TCOM");
            if (!f.isEmpty()) tags.composer = fromTString(f.front()->toString());
        }
        if (tags.copyright.isEmpty()) {
            const auto& f = id3->frameList("TCOP");
            if (!f.isEmpty()) tags.copyright = fromTString(f.front()->toString());
        }
        if (tags.albumArtist.isEmpty()) {
            const auto& f = id3->frameList("TPE2");
            if (!f.isEmpty()) tags.albumArtist = fromTString(f.front()->toString());
        }
        if (tags.encoder.isEmpty()) {
            const auto& f = id3->frameList("TENC");
            if (!f.isEmpty()) tags.encoder = fromTString(f.front()->toString());
        }
    }
}

// ---------------------------------------------------------------------------
// readWavPackTags — APE tags
// ---------------------------------------------------------------------------

static void readWavPackTags(const QString& path, AudioTags& tags)
{
    TagLib::WavPack::File file(path.toUtf8().constData());
    if (!file.isValid()) return;

    TagLib::APE::Tag* ape = file.APETag(false);
    if (!ape) return;

    auto readApe = [&](const char* key) -> QString {
        const auto& items = ape->itemListMap();
        if (items.contains(key)) {
            return fromTString(items[key].toString());
        }
        return {};
    };

    if (tags.albumArtist.isEmpty()) tags.albumArtist = readApe("ALBUMARTIST");
    if (tags.composer.isEmpty())    tags.composer    = readApe("COMPOSER");
    if (tags.copyright.isEmpty())   tags.copyright   = readApe("COPYRIGHT");
    if (tags.lyrics.isEmpty())      tags.lyrics      = readApe("LYRICS");
    if (tags.encoder.isEmpty())     tags.encoder     = readApe("ENCODEDBY");
    if (tags.url.isEmpty())         tags.url         = readApe("URL");

    tags.podcastTitle       = readApe("PODCAST_TITLE");
    tags.podcastEpisode     = readApe("PODCAST_EPISODE");
    tags.podcastSeason      = readApe("PODCAST_SEASON");
    tags.podcastCategory    = readApe("PODCAST_CATEGORY");
    tags.podcastDescription = readApe("PODCAST_DESCRIPTION");

    QString rgGain = readApe("REPLAYGAIN_TRACK_GAIN");
    if (!rgGain.isEmpty())
        tags.replayGainTrackGain = rgGain.remove(QStringLiteral(" dB"),
                                                  Qt::CaseInsensitive).toFloat();
    QString rgPeak = readApe("REPLAYGAIN_TRACK_PEAK");
    if (!rgPeak.isEmpty())
        tags.replayGainTrackPeak = rgPeak.toFloat();

    QString chapJson = readApe("DAWCAST_CHAPTERS");
    if (!chapJson.isEmpty() && tags.chapters.isEmpty())
        tags.chapters = chaptersFromJson(chapJson);

    // APE binary items can hold artwork
    if (tags.artworkData.isEmpty()) {
        const auto& items = ape->itemListMap();
        if (items.contains("COVER ART (FRONT)")) {
            const TagLib::APE::Item& item = items["COVER ART (FRONT)"];
            if (item.type() == TagLib::APE::Item::Binary) {
                TagLib::ByteVector bv = item.binaryData();
                // APE cover art: null-terminated filename followed by image data
                int nullPos = bv.find('\0');
                if (nullPos >= 0 && nullPos < static_cast<int>(bv.size()) - 1) {
                    QByteArray data(bv.data() + nullPos + 1,
                                    static_cast<int>(bv.size()) - nullPos - 1);
                    tags.artworkData = data;
                    // Detect MIME from magic bytes
                    if (data.startsWith("\x89PNG"))
                        tags.artworkMimeType = QStringLiteral("image/png");
                    else
                        tags.artworkMimeType = QStringLiteral("image/jpeg");
                }
            }
        }
    }
}

// =========================================================================
// writeTags — Format-specific writers
// =========================================================================

// ---------------------------------------------------------------------------
// writeMp3Tags — ID3v2.4 with APIC, CHAP, TXXX, USLT
// ---------------------------------------------------------------------------

static bool writeMp3Tags(const QString& path, const AudioTags& tags)
{
    TagLib::MPEG::File file(path.toUtf8().constData());
    if (!file.isValid()) return false;

    TagLib::ID3v2::Tag* id3 = file.ID3v2Tag(true);
    if (!id3) return false;

    // Standard tags
    id3->setTitle(toTString(tags.title));
    id3->setArtist(toTString(tags.artist));
    id3->setAlbum(toTString(tags.album));
    id3->setGenre(toTString(tags.genre));
    id3->setComment(toTString(tags.comment));
    if (tags.year > 0) id3->setYear(static_cast<unsigned int>(tags.year));
    if (tags.trackNumber > 0) id3->setTrack(static_cast<unsigned int>(tags.trackNumber));

    // TPE2 — Album Artist
    if (!tags.albumArtist.isEmpty()) {
        // Remove existing
        const auto& f = id3->frameList("TPE2");
        for (auto* frame : f) id3->removeFrame(frame);
        auto* tpe2 = new TagLib::ID3v2::TextIdentificationFrame("TPE2", TagLib::String::UTF8);
        tpe2->setText(toTString(tags.albumArtist));
        id3->addFrame(tpe2);
    }

    // TCOM — Composer
    if (!tags.composer.isEmpty()) {
        const auto& f = id3->frameList("TCOM");
        for (auto* frame : f) id3->removeFrame(frame);
        auto* tcom = new TagLib::ID3v2::TextIdentificationFrame("TCOM", TagLib::String::UTF8);
        tcom->setText(toTString(tags.composer));
        id3->addFrame(tcom);
    }

    // TCOP — Copyright
    if (!tags.copyright.isEmpty()) {
        const auto& f = id3->frameList("TCOP");
        for (auto* frame : f) id3->removeFrame(frame);
        auto* tcop = new TagLib::ID3v2::TextIdentificationFrame("TCOP", TagLib::String::UTF8);
        tcop->setText(toTString(tags.copyright));
        id3->addFrame(tcop);
    }

    // TENC — Encoder
    if (!tags.encoder.isEmpty()) {
        const auto& f = id3->frameList("TENC");
        for (auto* frame : f) id3->removeFrame(frame);
        auto* tenc = new TagLib::ID3v2::TextIdentificationFrame("TENC", TagLib::String::UTF8);
        tenc->setText(toTString(tags.encoder));
        id3->addFrame(tenc);
    }

    // TRCK — Track number/total (e.g., "3/12")
    if (tags.trackNumber > 0 || tags.trackTotal > 0) {
        const auto& f = id3->frameList("TRCK");
        for (auto* frame : f) id3->removeFrame(frame);
        QString trck;
        if (tags.trackTotal > 0)
            trck = QStringLiteral("%1/%2").arg(tags.trackNumber).arg(tags.trackTotal);
        else
            trck = QString::number(tags.trackNumber);
        auto* frame = new TagLib::ID3v2::TextIdentificationFrame("TRCK", TagLib::String::UTF8);
        frame->setText(toTString(trck));
        id3->addFrame(frame);
    }

    // TPOS — Disc number/total
    if (tags.discNumber > 0 || tags.discTotal > 0) {
        const auto& f = id3->frameList("TPOS");
        for (auto* frame : f) id3->removeFrame(frame);
        QString tpos;
        if (tags.discTotal > 0)
            tpos = QStringLiteral("%1/%2").arg(tags.discNumber).arg(tags.discTotal);
        else
            tpos = QString::number(tags.discNumber);
        auto* frame = new TagLib::ID3v2::TextIdentificationFrame("TPOS", TagLib::String::UTF8);
        frame->setText(toTString(tpos));
        id3->addFrame(frame);
    }

    // USLT — Lyrics
    if (!tags.lyrics.isEmpty()) {
        const auto& f = id3->frameList("USLT");
        for (auto* frame : f) id3->removeFrame(frame);
        auto* uslt = new TagLib::ID3v2::UnsynchronizedLyricsFrame(TagLib::String::UTF8);
        uslt->setText(toTString(tags.lyrics));
        id3->addFrame(uslt);
    }

    // WXXX — URL
    if (!tags.url.isEmpty()) {
        const auto& f = id3->frameList("WXXX");
        for (auto* frame : f) id3->removeFrame(frame);
        auto* wxxx = new TagLib::ID3v2::UserUrlLinkFrame(TagLib::String::UTF8);
        wxxx->setUrl(toTString(tags.url));
        id3->addFrame(wxxx);
    }

    // TXXX — Podcast metadata
    writeTXXX(id3, "PODCAST_TITLE",       tags.podcastTitle);
    writeTXXX(id3, "PODCAST_EPISODE",     tags.podcastEpisode);
    writeTXXX(id3, "PODCAST_SEASON",      tags.podcastSeason);
    writeTXXX(id3, "PODCAST_CATEGORY",    tags.podcastCategory);
    writeTXXX(id3, "PODCAST_DESCRIPTION", tags.podcastDescription);

    // TXXX — ReplayGain
    if (tags.replayGainTrackGain != 0.0f) {
        writeTXXX(id3, "REPLAYGAIN_TRACK_GAIN",
                  QStringLiteral("%1 dB").arg(
                      static_cast<double>(tags.replayGainTrackGain), 0, 'f', 2));
    }
    if (tags.replayGainTrackPeak != 0.0f) {
        writeTXXX(id3, "REPLAYGAIN_TRACK_PEAK",
                  QStringLiteral("%1").arg(
                      static_cast<double>(tags.replayGainTrackPeak), 0, 'f', 6));
    }

    // APIC — Artwork
    if (!tags.artworkData.isEmpty()) {
        // Remove existing APIC frames
        const auto& apicFrames = id3->frameList("APIC");
        for (auto* frame : apicFrames) id3->removeFrame(frame);

        auto* pic = new TagLib::ID3v2::AttachedPictureFrame();
        pic->setMimeType(toTString(tags.artworkMimeType));
        pic->setPicture(TagLib::ByteVector(tags.artworkData.constData(),
                                           static_cast<unsigned int>(tags.artworkData.size())));
        pic->setType(TagLib::ID3v2::AttachedPictureFrame::FrontCover);
        id3->addFrame(pic);
    }

    // CHAP — Chapter markers
    if (!tags.chapters.isEmpty()) {
        // Remove existing CHAP frames
        const auto& chapFrames = id3->frameList("CHAP");
        for (auto* frame : chapFrames) id3->removeFrame(frame);

        for (int i = 0; i < tags.chapters.size(); ++i) {
            const auto& ch = tags.chapters[i];
            QByteArray elementId = QStringLiteral("chp%1").arg(i).toLatin1();

            auto* chap = new TagLib::ID3v2::ChapterFrame(
                TagLib::ByteVector(elementId.constData(),
                                   static_cast<unsigned int>(elementId.size())),
                static_cast<unsigned int>(ch.startMs),
                static_cast<unsigned int>(ch.endMs),
                0xFFFFFFFF,   // startOffset (unused)
                0xFFFFFFFF);  // endOffset (unused)

            // TIT2 sub-frame for chapter title
            auto* titleFrame = new TagLib::ID3v2::TextIdentificationFrame("TIT2",
                                                                          TagLib::String::UTF8);
            titleFrame->setText(toTString(ch.title));
            chap->addEmbeddedFrame(titleFrame);

            // WXXX sub-frame for chapter URL
            if (!ch.url.isEmpty()) {
                auto* urlFrame = new TagLib::ID3v2::UserUrlLinkFrame(TagLib::String::UTF8);
                urlFrame->setUrl(toTString(ch.url));
                chap->addEmbeddedFrame(urlFrame);
            }

            id3->addFrame(chap);
        }
    }

    return file.save(TagLib::MPEG::File::ID3v2, TagLib::File::StripOthers,
                     TagLib::ID3v2::v4);
}

// ---------------------------------------------------------------------------
// writeFlacTags — Vorbis comments + FLAC Picture blocks
// ---------------------------------------------------------------------------

static bool writeFlacTags(const QString& path, const AudioTags& tags)
{
    TagLib::FLAC::File file(path.toUtf8().constData());
    if (!file.isValid()) return false;

    TagLib::Tag* tag = file.tag();
    if (!tag) return false;

    // Standard tags
    tag->setTitle(toTString(tags.title));
    tag->setArtist(toTString(tags.artist));
    tag->setAlbum(toTString(tags.album));
    tag->setGenre(toTString(tags.genre));
    tag->setComment(toTString(tags.comment));
    if (tags.year > 0) tag->setYear(static_cast<unsigned int>(tags.year));
    if (tags.trackNumber > 0) tag->setTrack(static_cast<unsigned int>(tags.trackNumber));

    // Extended tags via Vorbis comments
    TagLib::Ogg::XiphComment* vc = file.xiphComment(true);
    if (vc) {
        writeVorbisField(vc, "ALBUMARTIST",  tags.albumArtist);
        writeVorbisField(vc, "COMPOSER",     tags.composer);
        writeVorbisField(vc, "COPYRIGHT",    tags.copyright);
        writeVorbisField(vc, "LYRICS",       tags.lyrics);
        writeVorbisField(vc, "ENCODER",      tags.encoder);
        writeVorbisField(vc, "URL",          tags.url);

        // Track / disc totals
        if (tags.trackTotal > 0)
            writeVorbisField(vc, "TRACKTOTAL", QString::number(tags.trackTotal));
        if (tags.discNumber > 0)
            writeVorbisField(vc, "DISCNUMBER", QString::number(tags.discNumber));
        if (tags.discTotal > 0)
            writeVorbisField(vc, "DISCTOTAL", QString::number(tags.discTotal));

        // Podcast
        writeVorbisField(vc, "PODCAST_TITLE",       tags.podcastTitle);
        writeVorbisField(vc, "PODCAST_EPISODE",     tags.podcastEpisode);
        writeVorbisField(vc, "PODCAST_SEASON",      tags.podcastSeason);
        writeVorbisField(vc, "PODCAST_CATEGORY",    tags.podcastCategory);
        writeVorbisField(vc, "PODCAST_DESCRIPTION", tags.podcastDescription);

        // ReplayGain
        if (tags.replayGainTrackGain != 0.0f) {
            writeVorbisField(vc, "REPLAYGAIN_TRACK_GAIN",
                             QStringLiteral("%1 dB").arg(
                                 static_cast<double>(tags.replayGainTrackGain), 0, 'f', 2));
        }
        if (tags.replayGainTrackPeak != 0.0f) {
            writeVorbisField(vc, "REPLAYGAIN_TRACK_PEAK",
                             QStringLiteral("%1").arg(
                                 static_cast<double>(tags.replayGainTrackPeak), 0, 'f', 6));
        }

        // Chapters as JSON
        if (!tags.chapters.isEmpty()) {
            writeVorbisField(vc, "DAWCAST_CHAPTERS", chaptersToJson(tags.chapters));
        }
    }

    // Artwork
    if (!tags.artworkData.isEmpty()) {
        file.removePictures();
        auto* pic = new TagLib::FLAC::Picture();
        pic->setMimeType(toTString(tags.artworkMimeType));
        pic->setData(TagLib::ByteVector(tags.artworkData.constData(),
                                        static_cast<unsigned int>(tags.artworkData.size())));
        pic->setType(TagLib::FLAC::Picture::FrontCover);
        file.addPicture(pic);
    }

    return file.save();
}

// ---------------------------------------------------------------------------
// Helper: write Vorbis comment tags (shared by OGG, Opus)
// ---------------------------------------------------------------------------

static void writeVorbisCommonTags(TagLib::Ogg::XiphComment* vc, const AudioTags& tags)
{
    if (!vc) return;

    // Standard tags (set via the comment interface)
    vc->setTitle(toTString(tags.title));
    vc->setArtist(toTString(tags.artist));
    vc->setAlbum(toTString(tags.album));
    vc->setGenre(toTString(tags.genre));
    vc->setComment(toTString(tags.comment));
    if (tags.year > 0) vc->setYear(static_cast<unsigned int>(tags.year));
    if (tags.trackNumber > 0) vc->setTrack(static_cast<unsigned int>(tags.trackNumber));

    // Extended
    writeVorbisField(vc, "ALBUMARTIST",  tags.albumArtist);
    writeVorbisField(vc, "COMPOSER",     tags.composer);
    writeVorbisField(vc, "COPYRIGHT",    tags.copyright);
    writeVorbisField(vc, "LYRICS",       tags.lyrics);
    writeVorbisField(vc, "ENCODER",      tags.encoder);
    writeVorbisField(vc, "URL",          tags.url);

    if (tags.trackTotal > 0)
        writeVorbisField(vc, "TRACKTOTAL", QString::number(tags.trackTotal));
    if (tags.discNumber > 0)
        writeVorbisField(vc, "DISCNUMBER", QString::number(tags.discNumber));
    if (tags.discTotal > 0)
        writeVorbisField(vc, "DISCTOTAL", QString::number(tags.discTotal));

    // Podcast
    writeVorbisField(vc, "PODCAST_TITLE",       tags.podcastTitle);
    writeVorbisField(vc, "PODCAST_EPISODE",     tags.podcastEpisode);
    writeVorbisField(vc, "PODCAST_SEASON",      tags.podcastSeason);
    writeVorbisField(vc, "PODCAST_CATEGORY",    tags.podcastCategory);
    writeVorbisField(vc, "PODCAST_DESCRIPTION", tags.podcastDescription);

    // ReplayGain
    if (tags.replayGainTrackGain != 0.0f) {
        writeVorbisField(vc, "REPLAYGAIN_TRACK_GAIN",
                         QStringLiteral("%1 dB").arg(
                             static_cast<double>(tags.replayGainTrackGain), 0, 'f', 2));
    }
    if (tags.replayGainTrackPeak != 0.0f) {
        writeVorbisField(vc, "REPLAYGAIN_TRACK_PEAK",
                         QStringLiteral("%1").arg(
                             static_cast<double>(tags.replayGainTrackPeak), 0, 'f', 6));
    }

    // Chapters
    if (!tags.chapters.isEmpty()) {
        writeVorbisField(vc, "DAWCAST_CHAPTERS", chaptersToJson(tags.chapters));
    }

    // Artwork via METADATA_BLOCK_PICTURE
    if (!tags.artworkData.isEmpty()) {
        // Remove existing pictures
        vc->removeAllPictures();

        auto* pic = new TagLib::FLAC::Picture();
        pic->setMimeType(toTString(tags.artworkMimeType));
        pic->setData(TagLib::ByteVector(tags.artworkData.constData(),
                                        static_cast<unsigned int>(tags.artworkData.size())));
        pic->setType(TagLib::FLAC::Picture::FrontCover);
        vc->addPicture(pic);
    }
}

// ---------------------------------------------------------------------------
// writeOggVorbisTags
// ---------------------------------------------------------------------------

static bool writeOggVorbisTags(const QString& path, const AudioTags& tags)
{
    TagLib::Ogg::Vorbis::File file(path.toUtf8().constData());
    if (!file.isValid()) return false;

    writeVorbisCommonTags(file.tag(), tags);
    return file.save();
}

// ---------------------------------------------------------------------------
// writeOpusTags
// ---------------------------------------------------------------------------

static bool writeOpusTags(const QString& path, const AudioTags& tags)
{
    TagLib::Ogg::Opus::File file(path.toUtf8().constData());
    if (!file.isValid()) return false;

    writeVorbisCommonTags(file.tag(), tags);
    return file.save();
}

// ---------------------------------------------------------------------------
// writeMp4Tags — iTunes-style atoms
// ---------------------------------------------------------------------------

static bool writeMp4Tags(const QString& path, const AudioTags& tags)
{
    TagLib::MP4::File file(path.toUtf8().constData());
    if (!file.isValid()) return false;

    TagLib::MP4::Tag* mp4tag = file.tag();
    if (!mp4tag) return false;

    // Standard tags
    mp4tag->setTitle(toTString(tags.title));
    mp4tag->setArtist(toTString(tags.artist));
    mp4tag->setAlbum(toTString(tags.album));
    mp4tag->setGenre(toTString(tags.genre));
    mp4tag->setComment(toTString(tags.comment));
    if (tags.year > 0) mp4tag->setYear(static_cast<unsigned int>(tags.year));
    if (tags.trackNumber > 0) mp4tag->setTrack(static_cast<unsigned int>(tags.trackNumber));

    // Atoms
    auto setStringItem = [&](const char* key, const QString& val) {
        if (!val.isEmpty())
            mp4tag->setItem(key, TagLib::MP4::Item(toTString(val)));
    };

    setStringItem("aART",     tags.albumArtist);
    setStringItem("\251wrt",   tags.composer);
    setStringItem("cprt",     tags.copyright);
    setStringItem("\251lyr",   tags.lyrics);
    setStringItem("\251too",   tags.encoder);

    // Track number/total
    if (tags.trackNumber > 0 || tags.trackTotal > 0) {
        mp4tag->setItem("trkn", TagLib::MP4::Item(tags.trackNumber, tags.trackTotal));
    }

    // Disc number/total
    if (tags.discNumber > 0 || tags.discTotal > 0) {
        mp4tag->setItem("disk", TagLib::MP4::Item(tags.discNumber, tags.discTotal));
    }

    // Freeform atoms for podcast metadata
    auto setFreeformItem = [&](const char* key, const QString& val) {
        if (!val.isEmpty()) {
            TagLib::StringList sl;
            sl.append(toTString(val));
            mp4tag->setItem(key, TagLib::MP4::Item(sl));
        }
    };

    setFreeformItem("----:com.apple.iTunes:PODCAST_TITLE",       tags.podcastTitle);
    setFreeformItem("----:com.apple.iTunes:PODCAST_EPISODE",     tags.podcastEpisode);
    setFreeformItem("----:com.apple.iTunes:PODCAST_SEASON",      tags.podcastSeason);
    setFreeformItem("----:com.apple.iTunes:PODCAST_CATEGORY",    tags.podcastCategory);
    setFreeformItem("----:com.apple.iTunes:PODCAST_DESCRIPTION", tags.podcastDescription);

    // ReplayGain
    if (tags.replayGainTrackGain != 0.0f) {
        setFreeformItem("----:com.apple.iTunes:REPLAYGAIN_TRACK_GAIN",
                        QStringLiteral("%1 dB").arg(
                            static_cast<double>(tags.replayGainTrackGain), 0, 'f', 2));
    }
    if (tags.replayGainTrackPeak != 0.0f) {
        setFreeformItem("----:com.apple.iTunes:REPLAYGAIN_TRACK_PEAK",
                        QStringLiteral("%1").arg(
                            static_cast<double>(tags.replayGainTrackPeak), 0, 'f', 6));
    }

    // Chapters
    if (!tags.chapters.isEmpty()) {
        setFreeformItem("----:com.apple.iTunes:DAWCAST_CHAPTERS",
                        chaptersToJson(tags.chapters));
    }

    // Artwork
    if (!tags.artworkData.isEmpty()) {
        TagLib::MP4::CoverArt::Format fmt =
            tags.artworkMimeType.contains(QStringLiteral("png"))
                ? TagLib::MP4::CoverArt::PNG
                : TagLib::MP4::CoverArt::JPEG;
        TagLib::MP4::CoverArt coverArt(
            fmt, TagLib::ByteVector(tags.artworkData.constData(),
                                    static_cast<unsigned int>(tags.artworkData.size())));
        TagLib::MP4::CoverArtList coverList;
        coverList.append(coverArt);
        mp4tag->setItem("covr", TagLib::MP4::Item(coverList));
    }

    return file.save();
}

// ---------------------------------------------------------------------------
// writeWavTags — ID3v2 tag embedded in RIFF WAV
// ---------------------------------------------------------------------------

static bool writeWavTags(const QString& path, const AudioTags& tags)
{
    TagLib::RIFF::WAV::File file(path.toUtf8().constData());
    if (!file.isValid()) return false;

    TagLib::ID3v2::Tag* id3 = file.ID3v2Tag();
    if (!id3) return false;

    // Standard tags
    id3->setTitle(toTString(tags.title));
    id3->setArtist(toTString(tags.artist));
    id3->setAlbum(toTString(tags.album));
    id3->setGenre(toTString(tags.genre));
    id3->setComment(toTString(tags.comment));
    if (tags.year > 0) id3->setYear(static_cast<unsigned int>(tags.year));
    if (tags.trackNumber > 0) id3->setTrack(static_cast<unsigned int>(tags.trackNumber));

    // Extended frames (same as MP3 ID3v2)
    if (!tags.albumArtist.isEmpty()) {
        const auto& f = id3->frameList("TPE2");
        for (auto* frame : f) id3->removeFrame(frame);
        auto* tpe2 = new TagLib::ID3v2::TextIdentificationFrame("TPE2", TagLib::String::UTF8);
        tpe2->setText(toTString(tags.albumArtist));
        id3->addFrame(tpe2);
    }
    if (!tags.composer.isEmpty()) {
        const auto& f = id3->frameList("TCOM");
        for (auto* frame : f) id3->removeFrame(frame);
        auto* tcom = new TagLib::ID3v2::TextIdentificationFrame("TCOM", TagLib::String::UTF8);
        tcom->setText(toTString(tags.composer));
        id3->addFrame(tcom);
    }
    if (!tags.copyright.isEmpty()) {
        const auto& f = id3->frameList("TCOP");
        for (auto* frame : f) id3->removeFrame(frame);
        auto* tcop = new TagLib::ID3v2::TextIdentificationFrame("TCOP", TagLib::String::UTF8);
        tcop->setText(toTString(tags.copyright));
        id3->addFrame(tcop);
    }
    if (!tags.encoder.isEmpty()) {
        const auto& f = id3->frameList("TENC");
        for (auto* frame : f) id3->removeFrame(frame);
        auto* tenc = new TagLib::ID3v2::TextIdentificationFrame("TENC", TagLib::String::UTF8);
        tenc->setText(toTString(tags.encoder));
        id3->addFrame(tenc);
    }

    // TXXX — Podcast metadata
    writeTXXX(id3, "PODCAST_TITLE",       tags.podcastTitle);
    writeTXXX(id3, "PODCAST_EPISODE",     tags.podcastEpisode);
    writeTXXX(id3, "PODCAST_SEASON",      tags.podcastSeason);
    writeTXXX(id3, "PODCAST_CATEGORY",    tags.podcastCategory);
    writeTXXX(id3, "PODCAST_DESCRIPTION", tags.podcastDescription);

    // ReplayGain
    if (tags.replayGainTrackGain != 0.0f) {
        writeTXXX(id3, "REPLAYGAIN_TRACK_GAIN",
                  QStringLiteral("%1 dB").arg(
                      static_cast<double>(tags.replayGainTrackGain), 0, 'f', 2));
    }
    if (tags.replayGainTrackPeak != 0.0f) {
        writeTXXX(id3, "REPLAYGAIN_TRACK_PEAK",
                  QStringLiteral("%1").arg(
                      static_cast<double>(tags.replayGainTrackPeak), 0, 'f', 6));
    }

    // Artwork
    if (!tags.artworkData.isEmpty()) {
        const auto& apicFrames = id3->frameList("APIC");
        for (auto* frame : apicFrames) id3->removeFrame(frame);

        auto* pic = new TagLib::ID3v2::AttachedPictureFrame();
        pic->setMimeType(toTString(tags.artworkMimeType));
        pic->setPicture(TagLib::ByteVector(tags.artworkData.constData(),
                                           static_cast<unsigned int>(tags.artworkData.size())));
        pic->setType(TagLib::ID3v2::AttachedPictureFrame::FrontCover);
        id3->addFrame(pic);
    }

    return file.save();
}

// ---------------------------------------------------------------------------
// writeWavPackTags — APE tags
// ---------------------------------------------------------------------------

static bool writeWavPackTags(const QString& path, const AudioTags& tags)
{
    TagLib::WavPack::File file(path.toUtf8().constData());
    if (!file.isValid()) return false;

    TagLib::APE::Tag* ape = file.APETag(true);
    if (!ape) return false;

    // Standard tags
    ape->setTitle(toTString(tags.title));
    ape->setArtist(toTString(tags.artist));
    ape->setAlbum(toTString(tags.album));
    ape->setGenre(toTString(tags.genre));
    ape->setComment(toTString(tags.comment));
    if (tags.year > 0) ape->setYear(static_cast<unsigned int>(tags.year));
    if (tags.trackNumber > 0) ape->setTrack(static_cast<unsigned int>(tags.trackNumber));

    // Extended tags
    auto setApe = [&](const char* key, const QString& val) {
        if (!val.isEmpty())
            ape->addValue(TagLib::String(key, TagLib::String::UTF8), toTString(val), true);
    };

    setApe("ALBUMARTIST",  tags.albumArtist);
    setApe("COMPOSER",     tags.composer);
    setApe("COPYRIGHT",    tags.copyright);
    setApe("LYRICS",       tags.lyrics);
    setApe("ENCODEDBY",   tags.encoder);
    setApe("URL",          tags.url);

    if (tags.trackTotal > 0)
        setApe("TRACKTOTAL", QString::number(tags.trackTotal));
    if (tags.discNumber > 0)
        setApe("DISCNUMBER", QString::number(tags.discNumber));
    if (tags.discTotal > 0)
        setApe("DISCTOTAL", QString::number(tags.discTotal));

    // Podcast
    setApe("PODCAST_TITLE",       tags.podcastTitle);
    setApe("PODCAST_EPISODE",     tags.podcastEpisode);
    setApe("PODCAST_SEASON",      tags.podcastSeason);
    setApe("PODCAST_CATEGORY",    tags.podcastCategory);
    setApe("PODCAST_DESCRIPTION", tags.podcastDescription);

    // ReplayGain
    if (tags.replayGainTrackGain != 0.0f) {
        setApe("REPLAYGAIN_TRACK_GAIN",
               QStringLiteral("%1 dB").arg(
                   static_cast<double>(tags.replayGainTrackGain), 0, 'f', 2));
    }
    if (tags.replayGainTrackPeak != 0.0f) {
        setApe("REPLAYGAIN_TRACK_PEAK",
               QStringLiteral("%1").arg(
                   static_cast<double>(tags.replayGainTrackPeak), 0, 'f', 6));
    }

    // Chapters
    if (!tags.chapters.isEmpty()) {
        setApe("DAWCAST_CHAPTERS", chaptersToJson(tags.chapters));
    }

    // Artwork as binary APE item
    if (!tags.artworkData.isEmpty()) {
        // APE cover art: null-terminated filename + image data
        QByteArray binData;
        binData.append("cover.jpg");
        binData.append('\0');
        binData.append(tags.artworkData);
        ape->setItem("COVER ART (FRONT)",
                     TagLib::APE::Item("COVER ART (FRONT)",
                                       TagLib::ByteVector(binData.constData(),
                                                          static_cast<unsigned int>(binData.size())),
                                       true));
    }

    return file.save();
}

// ---------------------------------------------------------------------------
// writeGenericTags — fallback via TagLib::FileRef
// ---------------------------------------------------------------------------

static bool writeGenericTags(const QString& path, const AudioTags& tags)
{
    TagLib::FileRef fileRef(path.toUtf8().constData());
    if (fileRef.isNull() || !fileRef.tag()) return false;

    TagLib::Tag* tag = fileRef.tag();
    tag->setTitle(toTString(tags.title));
    tag->setArtist(toTString(tags.artist));
    tag->setAlbum(toTString(tags.album));
    tag->setGenre(toTString(tags.genre));
    tag->setComment(toTString(tags.comment));
    if (tags.year > 0) tag->setYear(static_cast<unsigned int>(tags.year));
    if (tags.trackNumber > 0) tag->setTrack(static_cast<unsigned int>(tags.trackNumber));

    return fileRef.save();
}

#endif // HAVE_TAGLIB

// =========================================================================
// Public API — TagTransfer
// =========================================================================

AudioTags TagTransfer::readTags(const QString& filePath)
{
    AudioTags tags;

#ifdef HAVE_TAGLIB
    // 1. Read generic tags as baseline
    readGenericTags(filePath, tags);

    // 2. Read format-specific extended tags
    const QString ext = fileExt(filePath);

    if (ext == QStringLiteral("mp3")) {
        readMp3Tags(filePath, tags);
    } else if (ext == QStringLiteral("flac")) {
        readFlacTags(filePath, tags);
    } else if (ext == QStringLiteral("ogg")) {
        readOggVorbisTags(filePath, tags);
    } else if (ext == QStringLiteral("opus")) {
        readOpusTags(filePath, tags);
    } else if (ext == QStringLiteral("m4a") || ext == QStringLiteral("mp4") ||
               ext == QStringLiteral("aac")) {
        readMp4Tags(filePath, tags);
    } else if (ext == QStringLiteral("wav")) {
        readWavTags(filePath, tags);
    } else if (ext == QStringLiteral("wv")) {
        readWavPackTags(filePath, tags);
    }
#else
    Q_UNUSED(filePath)
    qWarning() << "TagTransfer: TagLib not available, cannot read tags";
#endif

    return tags;
}

bool TagTransfer::writeTags(const QString& filePath, const AudioTags& tags)
{
#ifdef HAVE_TAGLIB
    const QString ext = fileExt(filePath);

    if (ext == QStringLiteral("mp3")) {
        return writeMp3Tags(filePath, tags);
    } else if (ext == QStringLiteral("flac")) {
        return writeFlacTags(filePath, tags);
    } else if (ext == QStringLiteral("ogg")) {
        return writeOggVorbisTags(filePath, tags);
    } else if (ext == QStringLiteral("opus")) {
        return writeOpusTags(filePath, tags);
    } else if (ext == QStringLiteral("m4a") || ext == QStringLiteral("mp4") ||
               ext == QStringLiteral("aac")) {
        return writeMp4Tags(filePath, tags);
    } else if (ext == QStringLiteral("wav")) {
        return writeWavTags(filePath, tags);
    } else if (ext == QStringLiteral("wv")) {
        return writeWavPackTags(filePath, tags);
    }

    // Fallback for other formats
    return writeGenericTags(filePath, tags);
#else
    Q_UNUSED(filePath)
    Q_UNUSED(tags)
    qWarning() << "TagTransfer: TagLib not available, cannot write tags";
    return false;
#endif
}

bool TagTransfer::copyTags(const QString& sourcePath, const QString& destPath)
{
    AudioTags tags = readTags(sourcePath);
    if (tags.isEmpty()) {
        qDebug() << "TagTransfer::copyTags: no tags found in" << sourcePath;
        return true; // Not an error — source simply had no tags
    }

    return writeTags(destPath, tags);
}

QByteArray TagTransfer::readArtwork(const QString& filePath)
{
    AudioTags tags = readTags(filePath);
    return tags.artworkData;
}

bool TagTransfer::writeArtwork(const QString& filePath,
                               const QByteArray& data,
                               const QString& mimeType)
{
    // Read existing tags, set artwork, write back
    AudioTags tags = readTags(filePath);
    tags.artworkData     = data;
    tags.artworkMimeType = mimeType;
    return writeTags(filePath, tags);
}

bool TagTransfer::isSupported(const QString& filePath)
{
    static const QStringList supported = {
        QStringLiteral("mp3"),
        QStringLiteral("flac"),
        QStringLiteral("ogg"),
        QStringLiteral("opus"),
        QStringLiteral("m4a"),
        QStringLiteral("mp4"),
        QStringLiteral("aac"),
        QStringLiteral("wav"),
        QStringLiteral("wv"),
    };
    return supported.contains(fileExt(filePath));
}

} // namespace dawcast
