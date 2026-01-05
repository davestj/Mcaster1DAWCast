// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QDateTime>

namespace dawcast {

struct PodcastEpisode {
    QString title;
    QString description;
    QString audioUrl;
    QString audioMimeType;
    int64_t audioLength{0};
    int     durationSeconds{0};
    QDateTime pubDate;
    QString guid;
    int     episodeNumber{0};      // itunes:episode
    int     seasonNumber{0};       // itunes:season
    QString episodeType;           // itunes:episodeType (full, trailer, bonus)
};

class RSSGenerator : public QObject
{
    Q_OBJECT

public:
    explicit RSSGenerator(QObject *parent = nullptr);
    ~RSSGenerator() override;

    void setFeedTitle(const QString &title);
    void setFeedDescription(const QString &description);
    void setFeedURL(const QString &url);
    void setAuthor(const QString &author);
    void setFeedImageUrl(const QString &url);
    void setCategory(const QString &category);
    void setLanguage(const QString &language);
    void setExplicit(bool isExplicit);

    void addEpisode(const PodcastEpisode &episode);
    void clearEpisodes();

    QString generateXML() const;
    bool saveToFile(const QString &path) const;

private:
    QString m_feedTitle;
    QString m_feedDescription;
    QString m_feedURL;
    QString m_author;
    QString m_feedImageUrl;
    QString m_category;
    QString m_language;
    bool    m_explicit{false};
    QList<PodcastEpisode> m_episodes;
};

} // namespace dawcast
