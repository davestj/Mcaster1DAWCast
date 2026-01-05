// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ChapterEditor.h"

#include <QDebug>
#include <algorithm>

#ifdef HAVE_TAGLIB
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/id3v2frame.h>
#include <taglib/chapterframe.h>
#include <taglib/textidentificationframe.h>
#include <taglib/tstring.h>
#endif

namespace dawcast {

ChapterEditor::ChapterEditor(QObject *parent)
    : QObject(parent)
{
}

ChapterEditor::~ChapterEditor() = default;

void ChapterEditor::addChapter(int64_t position, const QString &title)
{
    Chapter ch;
    ch.position = position;
    ch.title = title;

    // Insert sorted by position using binary search
    auto it = std::lower_bound(
        m_chapters.begin(), m_chapters.end(), ch,
        [](const Chapter &a, const Chapter &b) {
            return a.position < b.position;
        });
    m_chapters.insert(it, ch);

    emit chaptersChanged();
}

void ChapterEditor::removeChapter(int index)
{
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
#ifdef HAVE_TAGLIB
    if (m_chapters.isEmpty()) {
        return;
    }

    TagLib::MPEG::File file(audioFile.toUtf8().constData());
    if (!file.isValid()) {
        qWarning() << "ChapterEditor: Cannot open file for ID3 writing:" << audioFile;
        return;
    }

    TagLib::ID3v2::Tag *tag = file.ID3v2Tag(true);
    if (!tag) {
        qWarning() << "ChapterEditor: Cannot create ID3v2 tag";
        return;
    }

    // Remove any existing chapter frames
    const auto &frameList = tag->frameList("CHAP");
    for (auto *frame : frameList) {
        tag->removeFrame(frame);
    }

    // Write a CHAP frame for each chapter
    for (int i = 0; i < m_chapters.size(); ++i) {
        const Chapter &ch = m_chapters[i];

        // Calculate end time: next chapter's start, or 0xFFFFFFFF for the last chapter
        uint32_t startTimeMs = static_cast<uint32_t>(ch.position);
        uint32_t endTimeMs = (i + 1 < m_chapters.size())
            ? static_cast<uint32_t>(m_chapters[i + 1].position)
            : 0xFFFFFFFF;

        // Create element ID (e.g., "chp0", "chp1", ...)
        QByteArray elementId = QStringLiteral("chp%1").arg(i).toLatin1();

        auto *chapterFrame = new TagLib::ID3v2::ChapterFrame(
            TagLib::ByteVector(elementId.constData(), elementId.size()),
            startTimeMs,
            endTimeMs,
            0xFFFFFFFF,    // startOffset (not used)
            0xFFFFFFFF);   // endOffset (not used)

        // Add a TIT2 sub-frame with the chapter title
        auto *titleFrame = new TagLib::ID3v2::TextIdentificationFrame("TIT2");
        titleFrame->setText(TagLib::String(ch.title.toUtf8().constData(), TagLib::String::UTF8));
        chapterFrame->addEmbeddedFrame(titleFrame);

        tag->addFrame(chapterFrame);
    }

    if (!file.save()) {
        qWarning() << "ChapterEditor: Failed to save ID3v2 chapter frames to:" << audioFile;
    }
#else
    Q_UNUSED(audioFile)
    qWarning() << "ChapterEditor: TagLib not available, cannot export ID3 chapters";
#endif
}

} // namespace dawcast
