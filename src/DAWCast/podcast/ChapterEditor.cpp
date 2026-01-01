// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ChapterEditor.h"

namespace dawcast {

ChapterEditor::ChapterEditor(QObject *parent)
    : QObject(parent)
{
}

ChapterEditor::~ChapterEditor() = default;

void ChapterEditor::addChapter(int64_t position, const QString &title)
{
    // TODO: Insert chapter sorted by position
    Chapter ch;
    ch.position = position;
    ch.title = title;
    m_chapters.append(ch);
    emit chaptersChanged();
}

void ChapterEditor::removeChapter(int index)
{
    // TODO: Validate index bounds
    if (index >= 0 && index < m_chapters.size()) {
        m_chapters.removeAt(index);
        emit chaptersChanged();
    }
}

QList<Chapter> ChapterEditor::chapters() const
{
    return m_chapters;
}

void ChapterEditor::exportToID3(const QString &audioFile)
{
    // TODO: Write chapter markers to ID3v2 CHAP frames using TagLib
    Q_UNUSED(audioFile)
}

} // namespace dawcast
