// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "MediaLibrary.h"
#include "../codec/TagTransfer.h"
#include "../config/AppConfig.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDebug>

#ifdef HAVE_AVFORMAT
extern "C" {
#include <libavformat/avformat.h>
}
#endif

namespace dawcast {

// ---------------------------------------------------------------------------
// LibraryItem JSON serialization
// ---------------------------------------------------------------------------

QJsonObject LibraryItem::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("id")]           = id;
    obj[QStringLiteral("path")]         = path;
    obj[QStringLiteral("title")]        = title;
    obj[QStringLiteral("artist")]       = artist;
    obj[QStringLiteral("album")]        = album;
    obj[QStringLiteral("durationMs")]   = durationMs;
    obj[QStringLiteral("sampleRate")]   = sampleRate;
    obj[QStringLiteral("channels")]     = channels;
    obj[QStringLiteral("format")]       = format;
    obj[QStringLiteral("category")]     = category;
    obj[QStringLiteral("fileSize")]     = static_cast<qint64>(fileSize);
    obj[QStringLiteral("dateAdded")]    = dateAdded.toString(Qt::ISODate);
    obj[QStringLiteral("dateModified")] = dateModified.toString(Qt::ISODate);
    obj[QStringLiteral("bpm")]          = static_cast<double>(bpm);
    obj[QStringLiteral("favorite")]     = favorite;
    return obj;
}

LibraryItem LibraryItem::fromJson(const QJsonObject& obj)
{
    LibraryItem item;
    item.id           = obj[QStringLiteral("id")].toInt();
    item.path         = obj[QStringLiteral("path")].toString();
    item.title        = obj[QStringLiteral("title")].toString();
    item.artist       = obj[QStringLiteral("artist")].toString();
    item.album        = obj[QStringLiteral("album")].toString();
    item.durationMs   = obj[QStringLiteral("durationMs")].toInt();
    item.sampleRate   = obj[QStringLiteral("sampleRate")].toInt();
    item.channels     = obj[QStringLiteral("channels")].toInt();
    item.format       = obj[QStringLiteral("format")].toString();
    item.category     = obj[QStringLiteral("category")].toString();
    item.fileSize     = static_cast<qint64>(obj[QStringLiteral("fileSize")].toDouble());
    item.dateAdded    = QDateTime::fromString(obj[QStringLiteral("dateAdded")].toString(), Qt::ISODate);
    item.dateModified = QDateTime::fromString(obj[QStringLiteral("dateModified")].toString(), Qt::ISODate);
    item.bpm          = static_cast<float>(obj[QStringLiteral("bpm")].toDouble());
    item.favorite     = obj[QStringLiteral("favorite")].toBool();
    return item;
}

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------

MediaLibrary* MediaLibrary::s_instance = nullptr;

MediaLibrary* MediaLibrary::instance()
{
    if (!s_instance) {
        s_instance = new MediaLibrary(nullptr);
        s_instance->loadDatabase();
    }
    return s_instance;
}

MediaLibrary::MediaLibrary(QObject* parent)
    : QObject(parent)
{
}

MediaLibrary::~MediaLibrary()
{
    saveDatabase();
}

// ---------------------------------------------------------------------------
// Database path
// ---------------------------------------------------------------------------

QString MediaLibrary::databasePath()
{
    return dawcast::config::AppConfig::mediaLibraryPath();
}

// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------

void MediaLibrary::loadDatabase()
{
    QString path = databasePath();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "MediaLibrary: no database file at" << path;
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "MediaLibrary: JSON parse error:" << err.errorString();
        return;
    }

    QJsonObject root = doc.object();
    m_nextId = root[QStringLiteral("nextId")].toInt(1);

    QJsonArray arr = root[QStringLiteral("items")].toArray();
    m_items.clear();
    m_items.reserve(arr.size());

    for (const QJsonValue& val : arr) {
        LibraryItem item = LibraryItem::fromJson(val.toObject());
        m_items.append(item);
    }

    qDebug() << "MediaLibrary: loaded" << m_items.size() << "items from" << path;
}

void MediaLibrary::saveDatabase()
{
    QJsonArray arr;
    for (const LibraryItem& item : m_items) {
        arr.append(item.toJson());
    }

    QJsonObject root;
    root[QStringLiteral("nextId")] = m_nextId;
    root[QStringLiteral("items")]  = arr;

    QJsonDocument doc(root);

    QString path = databasePath();
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "MediaLibrary: failed to write database" << path;
        return;
    }

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    qDebug() << "MediaLibrary: saved" << m_items.size() << "items to" << path;
}

// ---------------------------------------------------------------------------
// Supported file extensions
// ---------------------------------------------------------------------------

bool MediaLibrary::isSupportedExtension(const QString& ext)
{
    static const QSet<QString> s_audio = {
        QStringLiteral("wav"), QStringLiteral("mp3"), QStringLiteral("flac"),
        QStringLiteral("aac"), QStringLiteral("ogg"), QStringLiteral("opus"),
        QStringLiteral("m4a"), QStringLiteral("wma"), QStringLiteral("aiff"),
        QStringLiteral("aif")
    };
    static const QSet<QString> s_video = {
        QStringLiteral("mp4"), QStringLiteral("mov"), QStringLiteral("avi"),
        QStringLiteral("mkv"), QStringLiteral("webm"), QStringLiteral("wmv"),
        QStringLiteral("flv"), QStringLiteral("m4v")
    };

    QString lower = ext.toLower();
    return s_audio.contains(lower) || s_video.contains(lower);
}

// ---------------------------------------------------------------------------
// Format detection from extension
// ---------------------------------------------------------------------------

QString MediaLibrary::formatFromExtension(const QString& path)
{
    QString ext = QFileInfo(path).suffix().toLower();

    if (ext == QLatin1String("wav"))                           return QStringLiteral("WAV");
    if (ext == QLatin1String("mp3"))                           return QStringLiteral("MP3");
    if (ext == QLatin1String("flac"))                          return QStringLiteral("FLAC");
    if (ext == QLatin1String("aac") || ext == QLatin1String("m4a"))
        return QStringLiteral("AAC");
    if (ext == QLatin1String("ogg"))                           return QStringLiteral("OGG");
    if (ext == QLatin1String("opus"))                          return QStringLiteral("OPUS");
    if (ext == QLatin1String("aiff") || ext == QLatin1String("aif"))
        return QStringLiteral("AIFF");
    if (ext == QLatin1String("wma"))                           return QStringLiteral("WMA");
    if (ext == QLatin1String("mp4") || ext == QLatin1String("m4v"))
        return QStringLiteral("MP4");
    if (ext == QLatin1String("mov"))                           return QStringLiteral("MOV");
    if (ext == QLatin1String("avi"))                           return QStringLiteral("AVI");
    if (ext == QLatin1String("mkv"))                           return QStringLiteral("MKV");
    if (ext == QLatin1String("webm"))                          return QStringLiteral("WEBM");
    if (ext == QLatin1String("wmv"))                           return QStringLiteral("WMV");
    if (ext == QLatin1String("flv"))                           return QStringLiteral("FLV");

    return ext.toUpper();
}

// ---------------------------------------------------------------------------
// Auto-categorization
// ---------------------------------------------------------------------------

QString MediaLibrary::suggestCategory(const QString& path) const
{
    QString lower = path.toLower();

    // Video extensions
    static const QSet<QString> videoExts = {
        QStringLiteral("mp4"), QStringLiteral("mov"), QStringLiteral("avi"),
        QStringLiteral("mkv"), QStringLiteral("webm"), QStringLiteral("wmv"),
        QStringLiteral("flv"), QStringLiteral("m4v")
    };
    QString ext = QFileInfo(path).suffix().toLower();
    if (videoExts.contains(ext))
        return QStringLiteral("Video");

    // Folder-based and filename-based detection
    if (lower.contains(QLatin1String("podcast")) || lower.contains(QLatin1String("episode")))
        return QStringLiteral("Podcast");

    if (lower.contains(QLatin1String("voiceover")) || lower.contains(QLatin1String("voice_over"))
        || lower.contains(QLatin1String("voice over")) || lower.contains(QLatin1String("narration"))
        || lower.contains(QLatin1String("/vo/")) || lower.contains(QLatin1String("_vo_")))
        return QStringLiteral("Voice Over");

    if (lower.contains(QLatin1String("vocal")) || lower.contains(QLatin1String("vox")))
        return QStringLiteral("Vocal");

    if (lower.contains(QLatin1String("sfx")) || lower.contains(QLatin1String("sound effect"))
        || lower.contains(QLatin1String("sound_effect")) || lower.contains(QLatin1String("foley")))
        return QStringLiteral("SFX");

    if (lower.contains(QLatin1String("recording")) || lower.contains(QLatin1String("/recordings/")))
        return QStringLiteral("Recording");

    // Default for audio files
    return QStringLiteral("Music Bed");
}

// ---------------------------------------------------------------------------
// File probing
// ---------------------------------------------------------------------------

LibraryItem MediaLibrary::probeFile(const QString& path)
{
    LibraryItem item;
    item.path = path;

    QFileInfo fi(path);
    if (!fi.exists() || !fi.isFile() || !fi.isReadable()) {
        qWarning() << "MediaLibrary::probeFile skipping unreadable" << path;
        item.title = fi.fileName();
        return item;
    }

    item.fileSize     = fi.size();
    item.dateModified = fi.lastModified();
    item.format       = formatFromExtension(path);
    item.category     = suggestCategory(path);

    // Read metadata via TagTransfer — TagLib can throw on malformed files;
    // catch all so a bad ID3 frame doesn't take down import.
    try {
        AudioTags tags = TagTransfer::readTags(path);
        item.title  = tags.title;
        item.artist = tags.artist;
        item.album  = tags.album;
    } catch (const std::exception& e) {
        qWarning() << "MediaLibrary::probeFile tag read failed for" << path << e.what();
    } catch (...) {
        qWarning() << "MediaLibrary::probeFile tag read crashed for" << path;
    }

    // Fallback title: use the filename without extension
    if (item.title.isEmpty())
        item.title = fi.completeBaseName();

    // Probe duration and audio properties via FFmpeg (avformat)
#ifdef HAVE_AVFORMAT
    AVFormatContext* ctx = nullptr;
    if (avformat_open_input(&ctx, path.toUtf8().constData(), nullptr, nullptr) == 0
        && ctx != nullptr) {
        if (avformat_find_stream_info(ctx, nullptr) >= 0) {
            // Duration in microseconds
            if (ctx->duration > 0) {
                item.durationMs = static_cast<int>(ctx->duration / 1000);
            }

            // Find the first audio stream for sample rate and channels.
            // Guard every dereference — malformed containers can have null
            // streams or null codecpar, which would crash this loop.
            for (unsigned i = 0; i < ctx->nb_streams; ++i) {
                AVStream* st = ctx->streams[i];
                if (!st) continue;
                AVCodecParameters* cp = st->codecpar;
                if (!cp) continue;
                if (cp->codec_type != AVMEDIA_TYPE_AUDIO) continue;
                item.sampleRate = cp->sample_rate > 0 ? cp->sample_rate : 44100;
                int ch = cp->ch_layout.nb_channels;
                item.channels = ch > 0 ? ch : 2;
                break;
            }
        }
        avformat_close_input(&ctx);
    }
#else
    // Without FFmpeg, duration and sample rate remain at defaults.
    // For WAV files, we can compute duration from file size and format.
    if (item.format == QLatin1String("WAV") && item.fileSize > 44) {
        // Assume 16-bit stereo 44100 Hz as a rough estimate
        qint64 dataSize = item.fileSize - 44;
        int bytesPerSample = 4; // 16-bit stereo = 4 bytes/frame
        item.sampleRate = 44100;
        item.channels   = 2;
        item.durationMs = static_cast<int>((dataSize * 1000) / (item.sampleRate * bytesPerSample));
    }
#endif

    return item;
}

// ---------------------------------------------------------------------------
// Import
// ---------------------------------------------------------------------------

bool MediaLibrary::contains(const QString& path) const
{
    QString canonical = QFileInfo(path).canonicalFilePath();
    for (const LibraryItem& item : m_items) {
        if (QFileInfo(item.path).canonicalFilePath() == canonical)
            return true;
    }
    return false;
}

int MediaLibrary::importFile(const QString& path)
{
    QFileInfo fi(path);
    if (!fi.exists() || !fi.isFile())
        return -1;

    if (!isSupportedExtension(fi.suffix()))
        return -1;

    // Skip duplicates
    if (contains(path))
        return -1;

    LibraryItem item = probeFile(path);
    item.id        = m_nextId++;
    item.dateAdded = QDateTime::currentDateTime();

    m_items.append(item);

    emit itemAdded(item.id);
    emit libraryChanged();

    return item.id;
}

void MediaLibrary::importFolder(const QString& dirPath, bool recursive)
{
    QDirIterator::IteratorFlags flags = recursive
        ? QDirIterator::Subdirectories
        : QDirIterator::NoIteratorFlags;

    QDirIterator it(dirPath, QDir::Files | QDir::NoDotAndDotDot, flags);
    int importCount = 0;

    while (it.hasNext()) {
        it.next();
        QFileInfo fi = it.fileInfo();
        if (isSupportedExtension(fi.suffix())) {
            if (importFile(fi.absoluteFilePath()) > 0)
                ++importCount;
        }
    }

    if (importCount > 0) {
        saveDatabase();
        qDebug() << "MediaLibrary: imported" << importCount << "files from" << dirPath;
    }
}

void MediaLibrary::removeItem(int id)
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].id == id) {
            m_items.removeAt(i);
            emit itemRemoved(id);
            emit libraryChanged();
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// Query
// ---------------------------------------------------------------------------

QList<LibraryItem> MediaLibrary::items() const
{
    return m_items;
}

int MediaLibrary::itemCount() const
{
    return m_items.size();
}

QList<LibraryItem> MediaLibrary::search(const QString& query) const
{
    if (query.isEmpty())
        return m_items;

    QList<LibraryItem> results;
    QString q = query.toLower();

    for (const LibraryItem& item : m_items) {
        if (item.title.toLower().contains(q)
            || item.artist.toLower().contains(q)
            || item.album.toLower().contains(q)
            || item.category.toLower().contains(q)
            || item.path.toLower().contains(q))
        {
            results.append(item);
        }
    }
    return results;
}

QList<LibraryItem> MediaLibrary::filterByCategory(const QString& category) const
{
    if (category.isEmpty())
        return m_items;

    QList<LibraryItem> results;
    for (const LibraryItem& item : m_items) {
        if (item.category.compare(category, Qt::CaseInsensitive) == 0)
            results.append(item);
    }
    return results;
}

QList<LibraryItem> MediaLibrary::filter(const QString& query, const QString& category) const
{
    QList<LibraryItem> base = category.isEmpty() ? m_items : filterByCategory(category);

    if (query.isEmpty())
        return base;

    QList<LibraryItem> results;
    QString q = query.toLower();

    for (const LibraryItem& item : base) {
        if (item.title.toLower().contains(q)
            || item.artist.toLower().contains(q)
            || item.album.toLower().contains(q)
            || item.path.toLower().contains(q))
        {
            results.append(item);
        }
    }
    return results;
}

const LibraryItem* MediaLibrary::itemById(int id) const
{
    for (const LibraryItem& item : m_items) {
        if (item.id == id)
            return &item;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Mutators
// ---------------------------------------------------------------------------

void MediaLibrary::setCategory(int id, const QString& category)
{
    for (LibraryItem& item : m_items) {
        if (item.id == id) {
            item.category = category;
            emit itemChanged(id);
            emit libraryChanged();
            return;
        }
    }
}

void MediaLibrary::setFavorite(int id, bool fav)
{
    for (LibraryItem& item : m_items) {
        if (item.id == id) {
            item.favorite = fav;
            emit itemChanged(id);
            emit libraryChanged();
            return;
        }
    }
}

} // namespace dawcast
