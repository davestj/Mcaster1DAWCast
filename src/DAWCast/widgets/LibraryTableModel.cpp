// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "LibraryTableModel.h"

#include <QUrl>
#include <QMimeData>
#include <QDateTime>
#include <QFileInfo>
#include <QSet>

namespace dawcast::widgets {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

LibraryTableModel::LibraryTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

LibraryTableModel::~LibraryTableModel() = default;

// ---------------------------------------------------------------------------
// QAbstractTableModel interface
// ---------------------------------------------------------------------------

int LibraryTableModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return m_displayItems.size();
}

int LibraryTableModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return ColCount;
}

QVariant LibraryTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) return {};
    if (index.row() < 0 || index.row() >= m_displayItems.size()) return {};

    const LibraryItem& item = m_displayItems[index.row()];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case ColTitle:     return item.title;
        case ColArtist:    return item.artist;
        case ColDuration:  return formatDuration(item.durationMs);
        case ColBPM:       return item.bpm > 0.0f
                                  ? QString::number(static_cast<double>(item.bpm), 'f', 1)
                                  : QString();
        case ColFormat:    return item.format;
        case ColCategory:  return item.category;
        case ColDateAdded: return item.dateAdded.toString(QStringLiteral("yyyy-MM-dd"));
        case ColPath:      return item.path;
        default:           return {};
        }
    }

    if (role == Qt::ToolTipRole) {
        return QStringLiteral("%1\n%2 — %3\n%4 Hz, %5 ch\n%6")
            .arg(item.title, item.artist, item.album)
            .arg(item.sampleRate)
            .arg(item.channels)
            .arg(item.path);
    }

    // UserRole: store the item id for lookup
    if (role == Qt::UserRole) {
        return item.id;
    }

    // UserRole+1: store the file path for drag/context-menu convenience
    if (role == Qt::UserRole + 1) {
        return item.path;
    }

    return {};
}

QVariant LibraryTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};

    switch (section) {
    case ColTitle:     return tr("Title");
    case ColArtist:    return tr("Artist");
    case ColDuration:  return tr("Duration");
    case ColBPM:       return tr("BPM");
    case ColFormat:    return tr("Format");
    case ColCategory:  return tr("Category");
    case ColDateAdded: return tr("Date Added");
    case ColPath:      return tr("Path");
    default:           return {};
    }
}

Qt::ItemFlags LibraryTableModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags f = QAbstractTableModel::flags(index);
    if (index.isValid())
        f |= Qt::ItemIsDragEnabled;
    return f;
}

// ---------------------------------------------------------------------------
// Drag support
// ---------------------------------------------------------------------------

QStringList LibraryTableModel::mimeTypes() const
{
    return { QStringLiteral("text/uri-list") };
}

QMimeData* LibraryTableModel::mimeData(const QModelIndexList& indexes) const
{
    QList<QUrl> urls;
    QSet<int> seenRows;

    for (const QModelIndex& idx : indexes) {
        if (!idx.isValid()) continue;
        if (seenRows.contains(idx.row())) continue;
        seenRows.insert(idx.row());

        if (idx.row() >= 0 && idx.row() < m_displayItems.size()) {
            urls.append(QUrl::fromLocalFile(m_displayItems[idx.row()].path));
        }
    }

    if (urls.isEmpty()) return nullptr;

    auto* mimeData = new QMimeData;
    mimeData->setUrls(urls);
    return mimeData;
}

Qt::DropActions LibraryTableModel::supportedDragActions() const
{
    return Qt::CopyAction;
}

// ---------------------------------------------------------------------------
// Filtering
// ---------------------------------------------------------------------------

void LibraryTableModel::refresh()
{
    beginResetModel();
    m_displayItems = MediaLibrary::instance()->filter(m_searchQuery, m_categoryFilter);
    endResetModel();
}

void LibraryTableModel::setSearchQuery(const QString& query)
{
    if (m_searchQuery == query) return;
    m_searchQuery = query;
    refresh();
}

void LibraryTableModel::setCategoryFilter(const QString& category)
{
    if (m_categoryFilter == category) return;
    m_categoryFilter = category;
    refresh();
}

// ---------------------------------------------------------------------------
// Access helpers
// ---------------------------------------------------------------------------

int LibraryTableModel::itemIdAtRow(int row) const
{
    if (row < 0 || row >= m_displayItems.size()) return -1;
    return m_displayItems[row].id;
}

QString LibraryTableModel::pathAtRow(int row) const
{
    if (row < 0 || row >= m_displayItems.size()) return {};
    return m_displayItems[row].path;
}

const LibraryItem& LibraryTableModel::itemAtRow(int row) const
{
    return m_displayItems[row];
}

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------

QString LibraryTableModel::formatDuration(int ms)
{
    if (ms <= 0) return QStringLiteral("--:--");

    int totalSec = ms / 1000;
    int hours = totalSec / 3600;
    int mins  = (totalSec % 3600) / 60;
    int secs  = totalSec % 60;

    if (hours > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(hours)
            .arg(mins, 2, 10, QLatin1Char('0'))
            .arg(secs, 2, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1:%2")
        .arg(mins)
        .arg(secs, 2, 10, QLatin1Char('0'));
}

} // namespace dawcast::widgets
