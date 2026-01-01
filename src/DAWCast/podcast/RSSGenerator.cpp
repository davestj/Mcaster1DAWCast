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

    // TODO: Add itunes:author, itunes:image, itunes:category, language, etc.

    for (const auto &ep : m_episodes) {
        xml.writeStartElement("item");
        xml.writeTextElement("title", ep.title);
        xml.writeTextElement("description", ep.description);
        xml.writeTextElement("guid", ep.guid.isEmpty() ? ep.audioUrl : ep.guid);
        xml.writeTextElement("pubDate", ep.pubDate.toString(Qt::RFC2822Date));

        xml.writeStartElement("enclosure");
        xml.writeAttribute("url", ep.audioUrl);
        xml.writeAttribute("length", QString::number(ep.audioLength));
        xml.writeAttribute("type", ep.audioMimeType.isEmpty() ? "audio/mpeg" : ep.audioMimeType);
        xml.writeEndElement(); // enclosure

        // TODO: itunes:duration, itunes:summary, itunes:episode
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
