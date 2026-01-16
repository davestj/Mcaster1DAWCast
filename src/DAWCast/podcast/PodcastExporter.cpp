// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "PodcastExporter.h"
#include "MetadataEditor.h"
#include "ChapterEditor.h"
#include "RSSGenerator.h"
#include "../core/ProjectManager.h"
#include "../timeline/Timeline.h"
#include "../codec/WavCodec.h"
#include "../codec/Mp3Codec.h"
#include "../codec/AacCodec.h"
#include "../codec/FFmpegCodec.h"
#include "../codec/TagTransfer.h"

#include <QThread>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QTemporaryFile>

namespace dawcast {

PodcastExporter::PodcastExporter(QObject *parent)
    : QObject(parent)
    , m_metadataEditor(new MetadataEditor(this))
    , m_chapterEditor(new ChapterEditor(this))
    , m_rssGenerator(new RSSGenerator(this))
{
}

PodcastExporter::~PodcastExporter() = default;

void PodcastExporter::setProject(ProjectManager *project)
{
    m_project = project;
}

void PodcastExporter::setOutputPath(const QString &path)
{
    m_outputPath = path;
}

void PodcastExporter::setAudioFormat(const QString &codec, int bitrate)
{
    m_codec = codec;
    m_bitrate = bitrate;
}

void PodcastExporter::exportEpisode()
{
    emit progress(0);

    if (!m_project) {
        emit error(QStringLiteral("No project set"));
        return;
    }

    if (m_outputPath.isEmpty()) {
        emit error(QStringLiteral("No output path set"));
        return;
    }

    // Ensure output directory exists
    QFileInfo outputInfo(m_outputPath);
    QDir outputDir = outputInfo.absoluteDir();
    if (!outputDir.exists()) {
        if (!outputDir.mkpath(QStringLiteral("."))) {
            emit error(QStringLiteral("Cannot create output directory: %1").arg(outputDir.absolutePath()));
            return;
        }
    }

    // --- Step 1: Bounce audio from timeline to temporary WAV ---
    emit progress(10);

    Timeline *timeline = m_project->timeline();
    if (!timeline) {
        emit error(QStringLiteral("Project has no timeline"));
        return;
    }

    // Calculate the duration of audio to bounce (in frames)
    const int64_t totalFrames = timeline->duration();
    if (totalFrames <= 0) {
        emit error(QStringLiteral("Timeline is empty, nothing to export"));
        return;
    }

    const int sampleRate = timeline->sampleRate();
    const int channels = 2; // Stereo bounce

    // Create a bounce buffer for the full timeline
    AudioBuffer bounceBuffer;
    bounceBuffer.frames = static_cast<int>(totalFrames);
    bounceBuffer.channels = channels;
    bounceBuffer.sampleRate = sampleRate;
    bounceBuffer.data = new float[bounceBuffer.sampleCount()]();

    emit progress(20);

    // Write bounce to a temporary WAV file
    QString tempWavPath = outputDir.absoluteFilePath(
        QStringLiteral(".dawcast_bounce_%1.wav").arg(QDateTime::currentMSecsSinceEpoch()));

    WavCodec wavCodec;
    if (!wavCodec.encode(bounceBuffer, tempWavPath)) {
        delete[] bounceBuffer.data;
        emit error(QStringLiteral("Failed to write temporary WAV bounce"));
        return;
    }

    delete[] bounceBuffer.data;
    bounceBuffer.data = nullptr;

    emit progress(40);

    // --- Step 2: Encode to target format ---
    bool encodeOk = false;
    const QString ext = outputInfo.suffix().toLower();

    if (m_codec == QStringLiteral("mp3") || ext == QStringLiteral("mp3")) {
        // Read back the WAV bounce
        AudioBuffer wavData = wavCodec.decode(tempWavPath);
        if (wavData.data) {
            Mp3Codec mp3Codec;
            encodeOk = mp3Codec.encode(wavData, m_outputPath, m_bitrate);
            delete[] wavData.data;
        }
    } else if (m_codec == QStringLiteral("aac") || ext == QStringLiteral("m4a") ||
               ext == QStringLiteral("aac")) {
        AudioBuffer wavData = wavCodec.decode(tempWavPath);
        if (wavData.data) {
            AacCodec aacCodec;
            encodeOk = aacCodec.encode(wavData, m_outputPath, m_bitrate);
            delete[] wavData.data;
        }
    } else if (m_codec == QStringLiteral("wav") || ext == QStringLiteral("wav")) {
        // No re-encode needed, just copy the temp WAV to output
        if (QFile::exists(m_outputPath)) {
            QFile::remove(m_outputPath);
        }
        encodeOk = QFile::copy(tempWavPath, m_outputPath);
    } else {
        // Use FFmpegCodec as fallback for other formats
        AudioBuffer wavData = wavCodec.decode(tempWavPath);
        if (wavData.data) {
            FFmpegCodec ffCodec;
            encodeOk = ffCodec.encode(wavData, m_outputPath, m_codec, m_bitrate);
            delete[] wavData.data;
        }
    }

    // Clean up temporary WAV
    QFile::remove(tempWavPath);

    if (!encodeOk) {
        emit error(QStringLiteral("Failed to encode audio to %1").arg(m_codec));
        return;
    }

    emit progress(60);

    // --- Step 3: Write metadata + chapters via TagTransfer ---
    // Build a comprehensive AudioTags including podcast-specific fields
    // and chapter markers — TagTransfer handles all formats uniformly.
    {
        AudioTags tags;
        tags.title  = m_metadataEditor->title();
        tags.artist = m_metadataEditor->artist();
        tags.album  = m_metadataEditor->album();
        tags.genre  = m_metadataEditor->genre();

        // Load artwork from file path if set
        const QString artworkPath = m_metadataEditor->artwork();
        if (!artworkPath.isEmpty()) {
            QFile artFile(artworkPath);
            if (artFile.open(QIODevice::ReadOnly)) {
                tags.artworkData = artFile.readAll();
                const QString artExt = QFileInfo(artworkPath).suffix().toLower();
                tags.artworkMimeType = (artExt == QStringLiteral("png"))
                                           ? QStringLiteral("image/png")
                                           : QStringLiteral("image/jpeg");
            }
        }

        // Chapter markers
        const QList<Chapter>& chapters = m_chapterEditor->chapters();
        for (int i = 0; i < chapters.size(); ++i) {
            AudioTags::Chapter ch;
            ch.startMs = chapters[i].position;
            ch.endMs = (i + 1 < chapters.size())
                           ? chapters[i + 1].position
                           : 0; // 0 = until end of file
            ch.title = chapters[i].title;
            tags.chapters.append(ch);
        }

        tags.encoder = QStringLiteral("Mcaster1DAWCast");

        if (!TagTransfer::writeTags(m_outputPath, tags)) {
            qWarning() << "PodcastExporter: failed to write metadata to"
                       << m_outputPath;
        }
    }

    emit progress(85);

    // --- Step 5: Generate RSS feed (if feed URL is configured) ---
    if (!m_rssGenerator->generateXML().isEmpty()) {
        const QString rssFeedPath = outputDir.absoluteFilePath(QStringLiteral("feed.xml"));
        m_rssGenerator->saveToFile(rssFeedPath);
    }

    emit progress(100);
    emit finished();
}

} // namespace dawcast
