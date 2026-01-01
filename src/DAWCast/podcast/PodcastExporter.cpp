// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "PodcastExporter.h"

namespace dawcast {

PodcastExporter::PodcastExporter(QObject *parent)
    : QObject(parent)
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
    // TODO: One-pass export pipeline:
    // 1. Bounce audio from project timeline using m_codec / m_bitrate
    // 2. Write metadata (title, artist, artwork) via MetadataEditor
    // 3. Write chapter markers via ChapterEditor::exportToID3
    // 4. Generate/update RSS feed via RSSGenerator
    // 5. Emit progress() throughout, finished() on success, error() on failure

    emit progress(0);

    if (!m_project) {
        emit error("No project set");
        return;
    }

    if (m_outputPath.isEmpty()) {
        emit error("No output path set");
        return;
    }

    // TODO: Implement bounce + metadata + RSS pipeline
    emit progress(100);
    emit finished();
}

} // namespace dawcast
