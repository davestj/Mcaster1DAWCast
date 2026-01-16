// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QString>
#include <QVariantMap>
#include <QByteArray>
#include <QList>

#include <cstdint>

namespace dawcast {

// ---------------------------------------------------------------------------
// AudioTags — unified metadata container for all supported audio formats
// ---------------------------------------------------------------------------

struct AudioTags
{
    // Standard tags
    QString title;
    QString artist;
    QString album;
    QString albumArtist;
    QString genre;
    QString comment;
    QString composer;
    int     year        = 0;
    int     trackNumber = 0;
    int     trackTotal  = 0;
    int     discNumber  = 0;
    int     discTotal   = 0;
    QString lyrics;
    QString copyright;
    QString encoder;
    QString url;

    // Podcast-specific
    QString podcastTitle;
    QString podcastEpisode;
    QString podcastSeason;
    QString podcastCategory;
    QString podcastDescription;

    // Artwork
    QByteArray artworkData;
    QString    artworkMimeType;   // "image/jpeg", "image/png"

    // Chapter markers (ID3v2 CHAP frames natively in MP3;
    // stored as Vorbis comment JSON for other formats)
    struct Chapter {
        int64_t startMs  = 0;
        int64_t endMs    = 0;
        QString title;
        QString url;
    };
    QList<Chapter> chapters;

    // ReplayGain / loudness
    float replayGainTrackGain = 0.0f;   // dB
    float replayGainTrackPeak = 0.0f;

    /// Returns true when no meaningful metadata has been populated.
    bool isEmpty() const;
};

// ---------------------------------------------------------------------------
// TagTransfer — read / write / copy metadata between any supported format
// ---------------------------------------------------------------------------

class TagTransfer
{
public:
    /// Read all tags from any supported format.
    static AudioTags readTags(const QString& filePath);

    /// Write tags to any supported format.
    /// Existing tags not represented in AudioTags are preserved.
    static bool writeTags(const QString& filePath, const AudioTags& tags);

    /// Copy tags from source to destination (different formats OK).
    static bool copyTags(const QString& sourcePath, const QString& destPath);

    /// Read just the embedded artwork.
    static QByteArray readArtwork(const QString& filePath);

    /// Write just the embedded artwork.
    static bool writeArtwork(const QString& filePath,
                             const QByteArray& data,
                             const QString& mimeType);

    /// Returns true if the file format is one we can tag.
    static bool isSupported(const QString& filePath);
};

} // namespace dawcast
