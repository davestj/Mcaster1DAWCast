// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QAbstractTableModel>
#include <QList>
#include <QMimeData>
#include <QStringList>

#include "../core/MediaLibrary.h"

namespace dawcast::widgets {

// ---------------------------------------------------------------------------
// LibraryTableModel — QAbstractTableModel backed by MediaLibrary
//
// Provides an 8-column view of the media library with:
//   Title, Artist, Duration, BPM, Format, Category, Date Added, Path
//
// Supports:
//   - Sortable columns (via QSortFilterProxyModel in the view)
//   - Drag to timeline (produces file:// URL MIME data)
//   - Real-time filtering by search query and category
// ---------------------------------------------------------------------------

class LibraryTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        ColTitle = 0,
        ColArtist,
        ColDuration,
        ColBPM,
        ColFormat,
        ColCategory,
        ColDateAdded,
        ColPath,
        ColCount  // sentinel
    };

    explicit LibraryTableModel(QObject* parent = nullptr);
    ~LibraryTableModel() override;

    // ── QAbstractTableModel interface ─────────────────────────────────────
    int           rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int           columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant      data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant      headerData(int section, Qt::Orientation orientation,
                             int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    // ── Drag support ──────────────────────────────────────────────────────
    QStringList   mimeTypes() const override;
    QMimeData*    mimeData(const QModelIndexList& indexes) const override;
    Qt::DropActions supportedDragActions() const override;

    // ── Filtering ─────────────────────────────────────────────────────────
    /// Rebuild the visible item list from MediaLibrary with current filters.
    void refresh();

    /// Set the search text (filters title, artist, album, path).
    void setSearchQuery(const QString& query);

    /// Set the category filter ("" = all).
    void setCategoryFilter(const QString& category);

    // ── Access ────────────────────────────────────────────────────────────
    /// Return the LibraryItem id for a given model row, or -1.
    int itemIdAtRow(int row) const;

    /// Return the file path for a given model row.
    QString pathAtRow(int row) const;

    /// Return the full LibraryItem at a given row (by const ref to internal list).
    const LibraryItem& itemAtRow(int row) const;

private:
    static QString formatDuration(int ms);

    QList<LibraryItem> m_displayItems;   // filtered subset
    QString            m_searchQuery;
    QString            m_categoryFilter;
};

} // namespace dawcast::widgets
