// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "TagTableModel.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QColor>
#include <QFont>

namespace dawcast::widgets {

// Supported audio extensions for folder scanning
static const QStringList kAudioExtensions = {
    QStringLiteral("mp3"),  QStringLiteral("flac"), QStringLiteral("ogg"),
    QStringLiteral("opus"), QStringLiteral("m4a"),  QStringLiteral("aac"),
    QStringLiteral("wav"),  QStringLiteral("wma"),  QStringLiteral("aiff"),
    QStringLiteral("aif"),  QStringLiteral("ape"),  QStringLiteral("wv"),
    QStringLiteral("mpc"),  QStringLiteral("mp4"),  QStringLiteral("oga")
};

TagTableModel::TagTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

TagTableModel::~TagTableModel() = default;

// ── QAbstractTableModel interface ──────────────────────────────────────────

int TagTableModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return static_cast<int>(m_entries.size());
}

int TagTableModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return ColCount;
}

QVariant TagTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) return {};
    const int row = index.row();
    const int col = index.column();
    if (row < 0 || row >= m_entries.size()) return {};

    const FileEntry& e = m_entries.at(row);

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (col) {
        case ColRow:         return row + 1;
        case ColFilename:    return QFileInfo(e.path).fileName();
        case ColTitle:       return e.tags.title;
        case ColArtist:      return e.tags.artist;
        case ColAlbum:       return e.tags.album;
        case ColAlbumArtist: return e.tags.albumArtist;
        case ColTrack:
            if (e.tags.trackNumber > 0) {
                if (e.tags.trackTotal > 0)
                    return QStringLiteral("%1/%2").arg(e.tags.trackNumber).arg(e.tags.trackTotal);
                return QString::number(e.tags.trackNumber);
            }
            return {};
        case ColYear:        return e.tags.year > 0 ? QVariant(e.tags.year) : QVariant();
        case ColGenre:       return e.tags.genre;
        case ColComposer:    return e.tags.composer;
        case ColDuration:    return formatDuration(e.durationMs);
        case ColBitrate:     return e.bitrate > 0 ? QStringLiteral("%1 kbps").arg(e.bitrate) : QString();
        case ColSampleRate:  return e.sampleRate > 0 ? QStringLiteral("%1 Hz").arg(e.sampleRate) : QString();
        case ColFormat:      return e.format;
        case ColSize:        return formatFileSize(e.fileSize);
        case ColPath:        return e.path;
        default: break;
        }
    }

    // Modified rows get a subtle background tint
    if (role == Qt::BackgroundRole && e.modified) {
        return QColor(255, 255, 200, 60);  // pale yellow
    }

    // Bold font for modified cells
    if (role == Qt::FontRole && e.modified) {
        QFont f;
        f.setBold(true);
        return f;
    }

    // Tooltip: show full path
    if (role == Qt::ToolTipRole) {
        return e.path;
    }

    return {};
}

bool TagTableModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || role != Qt::EditRole) return false;
    const int row = index.row();
    const int col = index.column();
    if (row < 0 || row >= m_entries.size()) return false;

    FileEntry& e = m_entries[row];
    const QString str = value.toString().trimmed();

    switch (col) {
    case ColTitle:       e.tags.title = str; break;
    case ColArtist:      e.tags.artist = str; break;
    case ColAlbum:       e.tags.album = str; break;
    case ColAlbumArtist: e.tags.albumArtist = str; break;
    case ColTrack: {
        // Parse "N" or "N/Total"
        if (str.contains(QLatin1Char('/'))) {
            auto parts = str.split(QLatin1Char('/'));
            e.tags.trackNumber = parts.value(0).trimmed().toInt();
            e.tags.trackTotal  = parts.value(1).trimmed().toInt();
        } else {
            e.tags.trackNumber = str.toInt();
        }
        break;
    }
    case ColYear:     e.tags.year = str.toInt(); break;
    case ColGenre:    e.tags.genre = str; break;
    case ColComposer: e.tags.composer = str; break;
    default:
        return false;  // read-only columns
    }

    e.modified = true;
    emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole, Qt::BackgroundRole, Qt::FontRole});
    emit modifiedCountChanged(modifiedCount());
    return true;
}

Qt::ItemFlags TagTableModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;

    Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;

    // Editable columns
    switch (index.column()) {
    case ColTitle:
    case ColArtist:
    case ColAlbum:
    case ColAlbumArtist:
    case ColTrack:
    case ColYear:
    case ColGenre:
    case ColComposer:
        f |= Qt::ItemIsEditable;
        break;
    default:
        break;
    }
    return f;
}

QVariant TagTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};

    switch (section) {
    case ColRow:         return QStringLiteral("#");
    case ColFilename:    return tr("Filename");
    case ColTitle:       return tr("Title");
    case ColArtist:      return tr("Artist");
    case ColAlbum:       return tr("Album");
    case ColAlbumArtist: return tr("Album Artist");
    case ColTrack:       return tr("Track");
    case ColYear:        return tr("Year");
    case ColGenre:       return tr("Genre");
    case ColComposer:    return tr("Composer");
    case ColDuration:    return tr("Duration");
    case ColBitrate:     return tr("Bitrate");
    case ColSampleRate:  return tr("Sample Rate");
    case ColFormat:      return tr("Format");
    case ColSize:        return tr("Size");
    case ColPath:        return tr("Path");
    default: break;
    }
    return {};
}

// ── Bulk operations ────────────────────────────────────────────────────────

void TagTableModel::addFiles(const QStringList& paths)
{
    // De-duplicate against existing entries
    QSet<QString> existing;
    existing.reserve(m_entries.size());
    for (const auto& e : m_entries)
        existing.insert(e.path);

    QStringList newPaths;
    for (const QString& p : paths) {
        if (!existing.contains(p) && TagTransfer::isSupported(p))
            newPaths.append(p);
    }

    if (newPaths.isEmpty()) return;

    const int firstRow = m_entries.size();
    beginInsertRows(QModelIndex(), firstRow, firstRow + newPaths.size() - 1);
    for (int i = 0; i < newPaths.size(); ++i) {
        loadFileEntry(newPaths.at(i));
        emit fileLoadProgress(i + 1, newPaths.size());
    }
    endInsertRows();
}

void TagTableModel::addFolder(const QString& dirPath, bool recursive)
{
    QStringList files;
    QDirIterator::IteratorFlags flags = recursive
        ? QDirIterator::Subdirectories
        : QDirIterator::NoIteratorFlags;

    QDirIterator it(dirPath, QDir::Files | QDir::Readable, flags);
    while (it.hasNext()) {
        it.next();
        const QString suffix = it.fileInfo().suffix().toLower();
        if (kAudioExtensions.contains(suffix))
            files.append(it.filePath());
    }

    addFiles(files);
}

void TagTableModel::removeRows(const QList<int>& rows)
{
    if (rows.isEmpty()) return;

    // Sort descending so removal doesn't shift subsequent indices
    QList<int> sorted = rows;
    std::sort(sorted.begin(), sorted.end(), std::greater<int>());

    for (int r : sorted) {
        if (r >= 0 && r < m_entries.size()) {
            beginRemoveRows(QModelIndex(), r, r);
            m_entries.removeAt(r);
            endRemoveRows();
        }
    }
    emit modifiedCountChanged(modifiedCount());
}

void TagTableModel::clear()
{
    if (m_entries.isEmpty()) return;
    beginResetModel();
    m_entries.clear();
    endResetModel();
    emit modifiedCountChanged(0);
}

// ── Entry access ───────────────────────────────────────────────────────────

const TagTableModel::FileEntry& TagTableModel::entryAt(int row) const
{
    return m_entries.at(row);
}

TagTableModel::FileEntry& TagTableModel::entryAt(int row)
{
    return m_entries[row];
}

int TagTableModel::entryCount() const
{
    return static_cast<int>(m_entries.size());
}

void TagTableModel::setTagsAt(int row, const AudioTags& tags)
{
    if (row < 0 || row >= m_entries.size()) return;
    m_entries[row].tags = tags;
    m_entries[row].modified = true;
    emit dataChanged(index(row, 0), index(row, ColCount - 1));
    emit modifiedCountChanged(modifiedCount());
}

int TagTableModel::saveAll()
{
    int saved = 0;
    const int total = m_entries.size();

    for (int i = 0; i < total; ++i) {
        FileEntry& e = m_entries[i];
        if (!e.modified) continue;

        if (TagTransfer::writeTags(e.path, e.tags)) {
            e.originalTags = e.tags;
            e.modified = false;
            ++saved;
        }
        emit saveProgress(i + 1, total);
    }

    // Refresh the entire view
    if (saved > 0) {
        emit dataChanged(index(0, 0), index(total - 1, ColCount - 1));
        emit modifiedCountChanged(modifiedCount());
    }
    return saved;
}

void TagTableModel::revertRow(int row)
{
    if (row < 0 || row >= m_entries.size()) return;
    FileEntry& e = m_entries[row];
    if (!e.modified) return;

    e.tags = e.originalTags;
    e.modified = false;
    emit dataChanged(index(row, 0), index(row, ColCount - 1));
    emit modifiedCountChanged(modifiedCount());
}

void TagTableModel::revertAll()
{
    bool any = false;
    for (int i = 0; i < m_entries.size(); ++i) {
        FileEntry& e = m_entries[i];
        if (e.modified) {
            e.tags = e.originalTags;
            e.modified = false;
            any = true;
        }
    }
    if (any) {
        emit dataChanged(index(0, 0), index(m_entries.size() - 1, ColCount - 1));
        emit modifiedCountChanged(0);
    }
}

QList<int> TagTableModel::modifiedRows() const
{
    QList<int> rows;
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).modified)
            rows.append(i);
    }
    return rows;
}

int TagTableModel::modifiedCount() const
{
    int count = 0;
    for (const auto& e : m_entries) {
        if (e.modified) ++count;
    }
    return count;
}

// ── Tag clipboard ──────────────────────────────────────────────────────────

void TagTableModel::copyTagsFrom(int row)
{
    if (row < 0 || row >= m_entries.size()) return;
    m_clipboard = m_entries.at(row).tags;
    m_hasClipboard = true;
}

void TagTableModel::pasteTags(const QList<int>& rows)
{
    if (!m_hasClipboard) return;
    for (int r : rows) {
        if (r >= 0 && r < m_entries.size()) {
            FileEntry& e = m_entries[r];
            // Paste only text metadata, preserve artwork
            e.tags.title       = m_clipboard.title;
            e.tags.artist      = m_clipboard.artist;
            e.tags.album       = m_clipboard.album;
            e.tags.albumArtist = m_clipboard.albumArtist;
            e.tags.genre       = m_clipboard.genre;
            e.tags.comment     = m_clipboard.comment;
            e.tags.composer    = m_clipboard.composer;
            e.tags.year        = m_clipboard.year;
            e.tags.trackNumber = m_clipboard.trackNumber;
            e.tags.trackTotal  = m_clipboard.trackTotal;
            e.tags.discNumber  = m_clipboard.discNumber;
            e.tags.discTotal   = m_clipboard.discTotal;
            e.tags.copyright   = m_clipboard.copyright;
            e.tags.encoder     = m_clipboard.encoder;
            e.tags.url         = m_clipboard.url;
            e.modified = true;
        }
    }
    if (!rows.isEmpty()) {
        emit dataChanged(index(0, 0), index(m_entries.size() - 1, ColCount - 1));
        emit modifiedCountChanged(modifiedCount());
    }
}

// ── Private helpers ────────────────────────────────────────────────────────

void TagTableModel::loadFileEntry(const QString& path)
{
    FileEntry e;
    e.path = path;
    e.tags = TagTransfer::readTags(path);
    e.originalTags = e.tags;
    e.modified = false;

    QFileInfo fi(path);
    e.fileSize = fi.size();
    e.format = detectFormat(path);

    // Duration, bitrate, and sample rate would ideally come from TagLib's
    // AudioProperties. TagTransfer::readTags populates the tag fields;
    // for audio properties we'd need to extend TagTransfer or read them
    // here. For now, we leave them at 0 and the display shows "--".
    // A production build would add:
    //   TagLib::FileRef f(path.toUtf8().constData());
    //   if (f.audioProperties()) { ... }
    // But we keep the TagLib dependency centralized in TagTransfer.

    m_entries.append(std::move(e));
}

QString TagTableModel::formatDuration(int ms)
{
    if (ms <= 0) return QStringLiteral("--:--");
    int totalSec = ms / 1000;
    int hours = totalSec / 3600;
    int mins  = (totalSec % 3600) / 60;
    int secs  = totalSec % 60;
    if (hours > 0)
        return QStringLiteral("%1:%2:%3")
            .arg(hours)
            .arg(mins, 2, 10, QLatin1Char('0'))
            .arg(secs, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2")
        .arg(mins)
        .arg(secs, 2, 10, QLatin1Char('0'));
}

QString TagTableModel::formatFileSize(qint64 bytes)
{
    if (bytes <= 0) return {};
    if (bytes < 1024)
        return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QStringLiteral("%1 KB").arg(bytes / 1024);
    if (bytes < 1024LL * 1024 * 1024)
        return QStringLiteral("%1.%2 MB")
            .arg(bytes / (1024 * 1024))
            .arg((bytes % (1024 * 1024)) * 10 / (1024 * 1024));
    return QStringLiteral("%1.%2 GB")
        .arg(bytes / (1024LL * 1024 * 1024))
        .arg((bytes % (1024LL * 1024 * 1024)) * 10 / (1024LL * 1024 * 1024));
}

QString TagTableModel::detectFormat(const QString& path)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == QLatin1String("mp3"))  return QStringLiteral("MP3");
    if (ext == QLatin1String("flac")) return QStringLiteral("FLAC");
    if (ext == QLatin1String("ogg") || ext == QLatin1String("oga"))
        return QStringLiteral("OGG");
    if (ext == QLatin1String("opus")) return QStringLiteral("OPUS");
    if (ext == QLatin1String("m4a") || ext == QLatin1String("aac"))
        return QStringLiteral("AAC");
    if (ext == QLatin1String("wav"))  return QStringLiteral("WAV");
    if (ext == QLatin1String("aiff") || ext == QLatin1String("aif"))
        return QStringLiteral("AIFF");
    if (ext == QLatin1String("wma"))  return QStringLiteral("WMA");
    if (ext == QLatin1String("ape"))  return QStringLiteral("APE");
    if (ext == QLatin1String("wv"))   return QStringLiteral("WavPack");
    if (ext == QLatin1String("mpc"))  return QStringLiteral("Musepack");
    if (ext == QLatin1String("mp4"))  return QStringLiteral("MP4");
    return ext.toUpper();
}

} // namespace dawcast::widgets
