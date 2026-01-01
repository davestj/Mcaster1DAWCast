// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "MetadataEditor.h"

// TODO: #include <taglib/fileref.h>
// TODO: #include <taglib/tag.h>

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
    // TODO: Use TagLib to write ID3/Vorbis/MP4 tags to the audio file
    Q_UNUSED(path)
}

void MetadataEditor::readFromFile(const QString &path)
{
    // TODO: Use TagLib to read metadata from audio file and populate members
    Q_UNUSED(path)
}

} // namespace dawcast
