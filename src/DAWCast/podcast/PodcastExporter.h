// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QString>

namespace dawcast {

class ProjectManager; // forward declaration

class PodcastExporter : public QObject
{
    Q_OBJECT

public:
    explicit PodcastExporter(QObject *parent = nullptr);
    ~PodcastExporter() override;

    void setProject(ProjectManager *project);
    void setOutputPath(const QString &path);
    void setAudioFormat(const QString &codec, int bitrate);

    void exportEpisode();

Q_SIGNALS:
    void progress(int percent);
    void finished();
    void error(const QString &message);

private:
    ProjectManager *m_project{nullptr};
    QString m_outputPath;
    QString m_codec;
    int m_bitrate{192};
};

} // namespace dawcast
