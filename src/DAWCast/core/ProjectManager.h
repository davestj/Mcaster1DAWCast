// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QString>

class QJsonDocument;
class QJsonObject;

namespace dawcast {

class Timeline;
class AudioTrack;
class VideoTrack;

class ProjectManager : public QObject
{
    Q_OBJECT

public:
    explicit ProjectManager(QObject* parent = nullptr);
    ~ProjectManager() override;

    void newProject();
    bool openProject(const QString& path);
    bool saveProject();
    bool saveProjectAs(const QString& path);

    void setTimeline(Timeline* timeline) { m_timeline = timeline; }
    [[nodiscard]] Timeline* timeline() const { return m_timeline; }

    void setProjectName(const QString& name) { m_projectName = name; markModified(); }
    [[nodiscard]] QString projectName() const { return m_projectName; }

    void setSampleRate(int rate) { m_sampleRate = rate; markModified(); }
    [[nodiscard]] int sampleRate() const { return m_sampleRate; }

    void setAuthor(const QString& author) { m_author = author; markModified(); }
    [[nodiscard]] QString author() const { return m_author; }

    [[nodiscard]] QString projectPath() const;
    [[nodiscard]] bool isModified() const;

    void markModified();

signals:
    void projectChanged();
    void projectSaved();

private:
    QJsonObject serializeTimeline() const;
    bool deserializeTimeline(const QJsonObject& obj);

    Timeline* m_timeline    = nullptr;
    QString   m_projectPath;
    QString   m_projectName = QStringLiteral("Untitled Project");
    QString   m_author;
    int       m_sampleRate  = 48000;
    bool      m_modified    = false;
};

} // namespace dawcast
