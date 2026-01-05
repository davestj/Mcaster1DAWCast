// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "RSSGenerator.h"

#include <QXmlStreamWriter>
#include <QFile>

namespace dawcast {

RSSGenerator::RSSGenerator(QObject *parent)
    : QObject(parent)
{
}

RSSGenerator::~RSSGenerator() = default;

void RSSGenerator::setFeedTitle(const QString &title)
{
    m_feedTitle = title;
}

void RSSGenerator::setFeedDescription(const QString &description)
{
    m_feedDescription = description;
}

void RSSGenerator::setFeedURL(const QString &url)
{
    m_feedURL = url;
}

void RSSGenerator::setAuthor(const QString &author)
{
    m_author = author;
}

void RSSGenerator::setFeedImageUrl(const QString &url)
{
    m_feedImageUrl = url;
}

void RSSGenerator::setCategory(const QString &category)
{
    m_category = category;
}

void RSSGenerator::setLanguage(const QString &language)
{
    m_language = language;
}

void RSSGenerator::setExplicit(bool isExplicit)
{
    m_explicit = isExplicit;
}

void RSSGenerator::addEpisode(const PodcastEpisode &episode)
{
    m_episodes.append(episode);
}

void RSSGenerator::clearEpisodes()
{
    m_episodes.clear();
}

QString RSSGenerator::generateXML() const
{
    QString output;
    QXmlStreamWriter xml(&output);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();

    // RSS 2.0 root
    xml.writeStartElement("rss");
    xml.writeAttribute("version", "2.0");
    xml.writeAttribute("xmlns:itunes", "http://www.itunes.com/dtds/podcast-1.0.dtd");
    xml.writeAttribute("xmlns:content", "http://purl.org/rss/1.0/modules/content/");

    xml.writeStartElement("channel");
    xml.writeTextElement("title", m_feedTitle);
    xml.writeTextElement("description", m_feedDescription);
    xml.writeTextElement("link", m_feedURL);
    xml.writeTextElement("language", m_language.isEmpty() ? QStringLiteral("en-us") : m_language);
    xml.writeTextElement("generator", QStringLiteral("Mcaster1DAWCast RSS Generator"));

    // iTunes namespace elements
    if (!m_author.isEmpty()) {
        xml.writeTextElement("itunes:author", m_author);
    }
    if (!m_feedImageUrl.isEmpty()) {
        xml.writeStartElement("itunes:image");
        xml.writeAttribute("href", m_feedImageUrl);
        xml.writeEndElement();
    }
    if (!m_category.isEmpty()) {
        xml.writeStartElement("itunes:category");
        xml.writeAttribute("text", m_category);
        xml.writeEndElement();
    }
    xml.writeTextElement("itunes:explicit", m_explicit ? QStringLiteral("true") : QStringLiteral("false"));

    for (const auto &ep : m_episodes) {
        xml.writeStartElement("item");
        xml.writeTextElement("title", ep.title);
        xml.writeTextElement("description", ep.description);

        // Unique GUID (use audioUrl as fallback)
        xml.writeStartElement("guid");
        xml.writeAttribute("isPermaLink", QStringLiteral("false"));
        xml.writeCharacters(ep.guid.isEmpty() ? ep.audioUrl : ep.guid);
        xml.writeEndElement(); // guid

        xml.writeTextElement("pubDate", ep.pubDate.toString(Qt::RFC2822Date));

        xml.writeStartElement("enclosure");
        xml.writeAttribute("url", ep.audioUrl);
        xml.writeAttribute("length", QString::number(ep.audioLength));
        xml.writeAttribute("type", ep.audioMimeType.isEmpty() ? QStringLiteral("audio/mpeg") : ep.audioMimeType);
        xml.writeEndElement(); // enclosure

        // iTunes episode metadata
        if (ep.durationSeconds > 0) {
            // Format as HH:MM:SS
            int h = ep.durationSeconds / 3600;
            int m = (ep.durationSeconds % 3600) / 60;
            int s = ep.durationSeconds % 60;
            xml.writeTextElement("itunes:duration",
                QStringLiteral("%1:%2:%3")
                    .arg(h, 2, 10, QLatin1Char('0'))
                    .arg(m, 2, 10, QLatin1Char('0'))
                    .arg(s, 2, 10, QLatin1Char('0')));
        }
        if (!ep.description.isEmpty()) {
            xml.writeTextElement("itunes:summary", ep.description);
        }
        if (ep.episodeNumber > 0) {
            xml.writeTextElement("itunes:episode", QString::number(ep.episodeNumber));
        }
        if (ep.seasonNumber > 0) {
            xml.writeTextElement("itunes:season", QString::number(ep.seasonNumber));
        }
        if (!ep.episodeType.isEmpty()) {
            xml.writeTextElement("itunes:episodeType", ep.episodeType);
        }

        xml.writeEndElement(); // item
    }

    xml.writeEndElement(); // channel
    xml.writeEndElement(); // rss
    xml.writeEndDocument();

    return output;
}

bool RSSGenerator::saveToFile(const QString &path) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    file.write(generateXML().toUtf8());
    return true;
}

} // namespace dawcast
