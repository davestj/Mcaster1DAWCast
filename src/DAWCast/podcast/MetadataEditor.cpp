// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "MetadataEditor.h"
#include "../codec/TagTransfer.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>

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
    // Build an AudioTags from the editor's current state and delegate
    // to TagTransfer for format-aware writing.
    AudioTags tags;
    tags.title  = m_title;
    tags.artist = m_artist;
    tags.album  = m_album;
    tags.genre  = m_genre;

    // Load artwork image data if a file path is set
    if (!m_artwork.isEmpty()) {
        QFile artFile(m_artwork);
        if (artFile.open(QIODevice::ReadOnly)) {
            tags.artworkData = artFile.readAll();
            const QString artExt = QFileInfo(m_artwork).suffix().toLower();
            if (artExt == QStringLiteral("png")) {
                tags.artworkMimeType = QStringLiteral("image/png");
            } else {
                tags.artworkMimeType = QStringLiteral("image/jpeg");
            }
        }
    }

    if (!TagTransfer::writeTags(path, tags)) {
        qWarning() << "MetadataEditor: failed to write metadata to" << path;
    }
}

void MetadataEditor::readFromFile(const QString &path)
{
    // Delegate to TagTransfer for format-aware reading
    AudioTags tags = TagTransfer::readTags(path);

    m_title  = tags.title;
    m_artist = tags.artist;
    m_album  = tags.album;
    m_genre  = tags.genre;

    // If the file has embedded artwork, clear the file path since the
    // art lives inside the file rather than as a separate file on disk.
    if (!tags.artworkData.isEmpty()) {
        m_artwork.clear();
    }

    emit metadataChanged();
}

} // namespace dawcast
