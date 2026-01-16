// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QAbstractTableModel>
#include <QList>
#include <QString>
#include <QStringList>
#include <QSet>
#include <QMimeData>

#include "../codec/TagTransfer.h"

namespace dawcast::widgets {

// ---------------------------------------------------------------------------
// TagTableModel — high-performance table model for batch tag editing
//
// Designed to handle 10,000+ files without lag. Each row represents one audio
// file with its full metadata (read via TagTransfer) plus audio properties.
// Supports in-place editing, undo via originalTags snapshots, and clipboard
// copy/paste of tag blocks.
// ---------------------------------------------------------------------------

class TagTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    // Column indices — user-reorderable in the view via header drag
    enum Column {
        ColRow = 0,
        ColFilename,
        ColTitle,
        ColArtist,
        ColAlbum,
        ColAlbumArtist,
        ColTrack,
        ColYear,
        ColGenre,
        ColComposer,
        ColDuration,
        ColBitrate,
        ColSampleRate,
        ColFormat,
        ColSize,
        ColPath,
        ColCount  // sentinel — total number of columns
    };

    struct FileEntry {
        QString    path;
        AudioTags  tags;
        AudioTags  originalTags;   // snapshot at load time (for undo / change detection)
        bool       modified = false;

        // Audio file properties (read-only, filled at load time)
        int        durationMs  = 0;
        int        bitrate     = 0;
        int        sampleRate  = 0;
        int        channels    = 0;
        QString    format;         // "MP3", "FLAC", "AAC", "OGG", "WAV", "OPUS"
        qint64     fileSize    = 0;
    };

    explicit TagTableModel(QObject* parent = nullptr);
    ~TagTableModel() override;

    // ── QAbstractTableModel interface ──────────────────────────────────
    int           rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int           columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant      data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool          setData(const QModelIndex& index, const QVariant& value,
                          int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant      headerData(int section, Qt::Orientation orientation,
                             int role = Qt::DisplayRole) const override;

    // ── Bulk operations ────────────────────────────────────────────────
    /// Add files (reads tags via TagTransfer in the calling thread).
    void addFiles(const QStringList& paths);

    /// Add all supported audio files from a directory.
    void addFolder(const QString& dirPath, bool recursive);

    /// Remove rows by index.
    void removeRows(const QList<int>& rows);

    /// Remove all entries.
    void clear();

    // ── Entry access ───────────────────────────────────────────────────
    const FileEntry& entryAt(int row) const;
    FileEntry&       entryAt(int row);
    int              entryCount() const;

    /// Apply tags to a specific row (marks modified).
    void setTagsAt(int row, const AudioTags& tags);

    /// Write all modified tags to disk via TagTransfer.
    /// Returns the number of files successfully written.
    int saveAll();

    /// Revert a row to its original tags.
    void revertRow(int row);

    /// Revert all rows to original tags.
    void revertAll();

    /// Returns indices of all modified rows.
    QList<int> modifiedRows() const;
    int        modifiedCount() const;

    // ── Tag clipboard (copy/paste between rows) ────────────────────────
    void copyTagsFrom(int row);
    void pasteTags(const QList<int>& rows);

signals:
    void modifiedCountChanged(int count);
    void saveProgress(int current, int total);
    void fileLoadProgress(int current, int total);

private:
    void loadFileEntry(const QString& path);
    static QString formatDuration(int ms);
    static QString formatFileSize(qint64 bytes);
    static QString detectFormat(const QString& path);

    QList<FileEntry> m_entries;
    AudioTags        m_clipboard;   // for copy/paste
    bool             m_hasClipboard = false;
};

} // namespace dawcast::widgets
