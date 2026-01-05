// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "MetadataEditor.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QBuffer>

#ifdef HAVE_TAGLIB
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/tpropertymap.h>
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/flacfile.h>
#include <taglib/flacpicture.h>
#include <taglib/vorbisfile.h>
#include <taglib/mp4file.h>
#include <taglib/mp4tag.h>
#include <taglib/mp4coverart.h>
#endif

namespace dawcast {

MetadataEditor::MetadataEditor(QObject *parent)
    : QObject(parent)
{
}

MetadataEditor::~MetadataEditor() = default;

void MetadataEditor::setTitle(const QString &title)
{
    if (m_title != title) {
        m_title = title;
        emit metadataChanged();
    }
}

void MetadataEditor::setArtist(const QString &artist)
{
    if (m_artist != artist) {
        m_artist = artist;
        emit metadataChanged();
    }
}

void MetadataEditor::setAlbum(const QString &album)
{
    if (m_album != album) {
        m_album = album;
        emit metadataChanged();
    }
}

void MetadataEditor::setArtwork(const QString &path)
{
    if (m_artwork != path) {
        m_artwork = path;
        emit metadataChanged();
    }
}

void MetadataEditor::setGenre(const QString &genre)
{
    if (m_genre != genre) {
        m_genre = genre;
        emit metadataChanged();
    }
}

QString MetadataEditor::title() const { return m_title; }
QString MetadataEditor::artist() const { return m_artist; }
QString MetadataEditor::album() const { return m_album; }
QString MetadataEditor::artwork() const { return m_artwork; }
QString MetadataEditor::genre() const { return m_genre; }

void MetadataEditor::writeToFile(const QString &path)
{
#ifdef HAVE_TAGLIB
    const QFileInfo fi(path);
    const QString ext = fi.suffix().toLower();

    // Load artwork image data if a path is set
    QByteArray artworkData;
    QString artworkMime;
    if (!m_artwork.isEmpty()) {
        QFile artFile(m_artwork);
        if (artFile.open(QIODevice::ReadOnly)) {
            artworkData = artFile.readAll();
            const QString artExt = QFileInfo(m_artwork).suffix().toLower();
            if (artExt == QStringLiteral("png")) {
                artworkMime = QStringLiteral("image/png");
            } else {
                artworkMime = QStringLiteral("image/jpeg");
            }
        }
    }

    if (ext == QStringLiteral("mp3")) {
        // --- MP3: ID3v2 tags ---
        TagLib::MPEG::File file(path.toUtf8().constData());
        if (!file.isValid()) {
            qWarning() << "MetadataEditor: Cannot open MP3 file:" << path;
            return;
        }
        TagLib::ID3v2::Tag *tag = file.ID3v2Tag(true);
        tag->setTitle(TagLib::String(m_title.toUtf8().constData(), TagLib::String::UTF8));
        tag->setArtist(TagLib::String(m_artist.toUtf8().constData(), TagLib::String::UTF8));
        tag->setAlbum(TagLib::String(m_album.toUtf8().constData(), TagLib::String::UTF8));
        tag->setGenre(TagLib::String(m_genre.toUtf8().constData(), TagLib::String::UTF8));

        // Embed artwork as APIC frame
        if (!artworkData.isEmpty()) {
            // Remove existing APIC frames
            const auto &frames = tag->frameList("APIC");
            for (auto *frame : frames) {
                tag->removeFrame(frame);
            }
            auto *picFrame = new TagLib::ID3v2::AttachedPictureFrame();
            picFrame->setMimeType(TagLib::String(artworkMime.toUtf8().constData(), TagLib::String::UTF8));
            picFrame->setPicture(TagLib::ByteVector(artworkData.constData(), artworkData.size()));
            picFrame->setType(TagLib::ID3v2::AttachedPictureFrame::FrontCover);
            tag->addFrame(picFrame);
        }

        file.save();

    } else if (ext == QStringLiteral("flac")) {
        // --- FLAC: Vorbis comments + FLAC picture ---
        TagLib::FLAC::File file(path.toUtf8().constData());
        if (!file.isValid()) {
            qWarning() << "MetadataEditor: Cannot open FLAC file:" << path;
            return;
        }
        TagLib::Tag *tag = file.tag();
        tag->setTitle(TagLib::String(m_title.toUtf8().constData(), TagLib::String::UTF8));
        tag->setArtist(TagLib::String(m_artist.toUtf8().constData(), TagLib::String::UTF8));
        tag->setAlbum(TagLib::String(m_album.toUtf8().constData(), TagLib::String::UTF8));
        tag->setGenre(TagLib::String(m_genre.toUtf8().constData(), TagLib::String::UTF8));

        if (!artworkData.isEmpty()) {
            file.removePictures();
            auto *pic = new TagLib::FLAC::Picture();
            pic->setMimeType(TagLib::String(artworkMime.toUtf8().constData(), TagLib::String::UTF8));
            pic->setData(TagLib::ByteVector(artworkData.constData(), artworkData.size()));
            pic->setType(TagLib::FLAC::Picture::FrontCover);
            file.addPicture(pic);
        }

        file.save();

    } else if (ext == QStringLiteral("ogg")) {
        // --- Ogg Vorbis ---
        TagLib::Ogg::Vorbis::File file(path.toUtf8().constData());
        if (!file.isValid()) {
            qWarning() << "MetadataEditor: Cannot open Ogg Vorbis file:" << path;
            return;
        }
        TagLib::Tag *tag = file.tag();
        tag->setTitle(TagLib::String(m_title.toUtf8().constData(), TagLib::String::UTF8));
        tag->setArtist(TagLib::String(m_artist.toUtf8().constData(), TagLib::String::UTF8));
        tag->setAlbum(TagLib::String(m_album.toUtf8().constData(), TagLib::String::UTF8));
        tag->setGenre(TagLib::String(m_genre.toUtf8().constData(), TagLib::String::UTF8));
        file.save();

    } else if (ext == QStringLiteral("m4a") || ext == QStringLiteral("mp4") ||
               ext == QStringLiteral("aac")) {
        // --- MP4/M4A: iTunes-style atoms ---
        TagLib::MP4::File file(path.toUtf8().constData());
        if (!file.isValid()) {
            qWarning() << "MetadataEditor: Cannot open MP4 file:" << path;
            return;
        }
        TagLib::MP4::Tag *tag = file.tag();
        tag->setTitle(TagLib::String(m_title.toUtf8().constData(), TagLib::String::UTF8));
        tag->setArtist(TagLib::String(m_artist.toUtf8().constData(), TagLib::String::UTF8));
        tag->setAlbum(TagLib::String(m_album.toUtf8().constData(), TagLib::String::UTF8));
        tag->setGenre(TagLib::String(m_genre.toUtf8().constData(), TagLib::String::UTF8));

        // Embed artwork as covr atom
        if (!artworkData.isEmpty()) {
            TagLib::MP4::CoverArt::Format fmt =
                artworkMime.contains(QStringLiteral("png"))
                    ? TagLib::MP4::CoverArt::PNG
                    : TagLib::MP4::CoverArt::JPEG;
            TagLib::MP4::CoverArt coverArt(fmt,
                TagLib::ByteVector(artworkData.constData(), artworkData.size()));
            TagLib::MP4::CoverArtList coverList;
            coverList.append(coverArt);
            tag->setItem("covr", TagLib::MP4::Item(coverList));
        }

        file.save();

    } else {
        // --- Generic fallback via FileRef ---
        TagLib::FileRef fileRef(path.toUtf8().constData());
        if (fileRef.isNull() || !fileRef.tag()) {
            qWarning() << "MetadataEditor: Unsupported file format:" << path;
            return;
        }
        TagLib::Tag *tag = fileRef.tag();
        tag->setTitle(TagLib::String(m_title.toUtf8().constData(), TagLib::String::UTF8));
        tag->setArtist(TagLib::String(m_artist.toUtf8().constData(), TagLib::String::UTF8));
        tag->setAlbum(TagLib::String(m_album.toUtf8().constData(), TagLib::String::UTF8));
        tag->setGenre(TagLib::String(m_genre.toUtf8().constData(), TagLib::String::UTF8));
        fileRef.save();
    }
#else
    Q_UNUSED(path)
    qWarning() << "MetadataEditor: TagLib not available, cannot write metadata";
#endif
}

void MetadataEditor::readFromFile(const QString &path)
{
#ifdef HAVE_TAGLIB
    const QFileInfo fi(path);
    const QString ext = fi.suffix().toLower();

    // Read basic tags via FileRef (works for all supported formats)
    TagLib::FileRef fileRef(path.toUtf8().constData());
    if (fileRef.isNull() || !fileRef.tag()) {
        qWarning() << "MetadataEditor: Cannot read metadata from:" << path;
        return;
    }

    TagLib::Tag *tag = fileRef.tag();
    m_title  = QString::fromUtf8(tag->title().toCString(true));
    m_artist = QString::fromUtf8(tag->artist().toCString(true));
    m_album  = QString::fromUtf8(tag->album().toCString(true));
    m_genre  = QString::fromUtf8(tag->genre().toCString(true));

    // Extract embedded artwork for specific formats
    if (ext == QStringLiteral("mp3")) {
        TagLib::MPEG::File file(path.toUtf8().constData());
        if (file.isValid() && file.ID3v2Tag()) {
            const auto &frames = file.ID3v2Tag()->frameList("APIC");
            if (!frames.isEmpty()) {
                auto *picFrame = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame*>(frames.front());
                if (picFrame) {
                    const TagLib::ByteVector &picData = picFrame->picture();
                    // Store as a temporary file or data URI — for now clear the path
                    // since we read embedded art but have no file path for it
                    m_artwork.clear();
                }
            }
        }
    } else if (ext == QStringLiteral("flac")) {
        TagLib::FLAC::File file(path.toUtf8().constData());
        if (file.isValid()) {
            const auto &pictures = file.pictureList();
            if (!pictures.isEmpty()) {
                m_artwork.clear(); // Embedded art available but no file path
            }
        }
    } else if (ext == QStringLiteral("m4a") || ext == QStringLiteral("mp4")) {
        TagLib::MP4::File file(path.toUtf8().constData());
        if (file.isValid() && file.tag()) {
            const auto &items = file.tag()->itemMap();
            if (items.contains("covr")) {
                m_artwork.clear(); // Embedded art available
            }
        }
    }

    emit metadataChanged();
#else
    Q_UNUSED(path)
    qWarning() << "MetadataEditor: TagLib not available, cannot read metadata";
#endif
}

} // namespace dawcast
