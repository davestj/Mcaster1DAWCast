// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QObject>
#include <QList>
#include <QString>
#include <QDateTime>
#include <QJsonObject>

namespace dawcast {

// ---------------------------------------------------------------------------
// LibraryItem — metadata + audio properties for one imported media file
// ---------------------------------------------------------------------------

struct LibraryItem
{
    int       id            = 0;
    QString   path;
    QString   title;
    QString   artist;
    QString   album;
    int       durationMs    = 0;
    int       sampleRate    = 0;
    int       channels      = 0;
    QString   format;            // "MP3", "WAV", "FLAC", "AAC", "OGG", etc.
    QString   category;          // "Recording", "Podcast", "Vocal", "Voice Over",
                                 // "Music Bed", "SFX", "Video"
    qint64    fileSize      = 0;
    QDateTime dateAdded;
    QDateTime dateModified;
    float     bpm           = 0.0f;   // 0 if unknown
    bool      favorite      = false;

    /// Serialize to JSON for persistence.
    QJsonObject toJson() const;

    /// Deserialize from JSON.
    static LibraryItem fromJson(const QJsonObject& obj);
};

// ---------------------------------------------------------------------------
// MediaLibrary — singleton library manager with import, search, and
// category management.  Persists to ~/.mcaster1/media_library.json.
// ---------------------------------------------------------------------------

class MediaLibrary : public QObject
{
    Q_OBJECT

public:
    static MediaLibrary* instance();

    // ── Import ────────────────────────────────────────────────────────────
    /// Import a single file into the library.  Returns its assigned id.
    int importFile(const QString& path);

    /// Import all supported media files from a directory.
    void importFolder(const QString& dirPath, bool recursive = true);

    /// Remove an item by id.
    void removeItem(int id);

    // ── Query ─────────────────────────────────────────────────────────────
    QList<LibraryItem> items() const;
    int itemCount() const;

    /// Full-text search across title, artist, album, category, and path.
    QList<LibraryItem> search(const QString& query) const;

    /// Filter items whose category matches exactly (case-insensitive).
    QList<LibraryItem> filterByCategory(const QString& category) const;

    /// Combined search + category filter.
    QList<LibraryItem> filter(const QString& query, const QString& category) const;

    /// Lookup by id.  Returns nullptr if not found.
    const LibraryItem* itemById(int id) const;

    // ── Mutators ──────────────────────────────────────────────────────────
    void setCategory(int id, const QString& category);
    void setFavorite(int id, bool fav);

    // ── Auto-categorization ───────────────────────────────────────────────
    /// Suggest a category based on folder name, filename, and extension.
    QString suggestCategory(const QString& path) const;

    // ── Persistence ───────────────────────────────────────────────────────
    void loadDatabase();
    void saveDatabase();

    /// Returns true if the file at @p path is already in the library.
    bool contains(const QString& path) const;

signals:
    void itemAdded(int id);
    void itemRemoved(int id);
    void itemChanged(int id);
    void libraryChanged();

private:
    explicit MediaLibrary(QObject* parent = nullptr);
    ~MediaLibrary() override;

    /// Read metadata + audio properties from a file on disk.
    LibraryItem probeFile(const QString& path);

    /// Determine format string from file extension.
    static QString formatFromExtension(const QString& path);

    /// Return the JSON database file path (~/.mcaster1/media_library.json).
    static QString databasePath();

    /// Check whether a file extension is a recognized media type.
    static bool isSupportedExtension(const QString& ext);

    QList<LibraryItem> m_items;
    int m_nextId = 1;

    static MediaLibrary* s_instance;
};

} // namespace dawcast
