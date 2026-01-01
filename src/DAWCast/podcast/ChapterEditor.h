// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QList>
#include <QString>

namespace dawcast {

struct Chapter {
    int64_t position{0};
    QString title;
};

class ChapterEditor : public QObject
{
    Q_OBJECT

public:
    explicit ChapterEditor(QObject *parent = nullptr);
    ~ChapterEditor() override;

    void addChapter(int64_t position, const QString &title);
    void removeChapter(int index);
    QList<Chapter> chapters() const;

    void exportToID3(const QString &audioFile);

Q_SIGNALS:
    void chaptersChanged();

private:
    QList<Chapter> m_chapters;
};

} // namespace dawcast
