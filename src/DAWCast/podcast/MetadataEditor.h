// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QString>

namespace dawcast {

class MetadataEditor : public QObject
{
    Q_OBJECT

public:
    explicit MetadataEditor(QObject *parent = nullptr);
    ~MetadataEditor() override;

    void setTitle(const QString &title);
    void setArtist(const QString &artist);
    void setAlbum(const QString &album);
    void setArtwork(const QString &path);
    void setGenre(const QString &genre);

    QString title() const;
    QString artist() const;
    QString album() const;
    QString artwork() const;
    QString genre() const;

    void writeToFile(const QString &path);
    void readFromFile(const QString &path);

Q_SIGNALS:
    void metadataChanged();

private:
    QString m_title;
    QString m_artist;
    QString m_album;
    QString m_artwork;
    QString m_genre;
};

} // namespace dawcast
