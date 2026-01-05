// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ChapterWidget.h"
#include "Timeline.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QListWidget>
#include <QInputDialog>
#include <QMessageBox>

namespace dawcast::widgets {

namespace {
// Format sample position as MM:SS.mmm
QString formatTime(int64_t samples, int sampleRate)
{
    if (sampleRate <= 0) sampleRate = 48000;
    double seconds = static_cast<double>(samples) / sampleRate;
    int minutes = static_cast<int>(seconds) / 60;
    double secs = seconds - minutes * 60;
    return QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(secs, 6, 'f', 3, QChar('0'));
}

// Parse time string back to samples
int64_t parseTime(const QString& text, int sampleRate)
{
    if (sampleRate <= 0) sampleRate = 48000;
    QStringList parts = text.split(QChar(':'));
    if (parts.size() != 2) return 0;
    int minutes = parts[0].toInt();
    double secs = parts[1].toDouble();
    return static_cast<int64_t>((minutes * 60.0 + secs) * sampleRate);
}
} // anonymous namespace

ChapterWidget::ChapterWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    // Chapter list
    m_chapterList = new QListWidget(this);
    m_chapterList->setAlternatingRowColors(true);
    m_chapterList->setStyleSheet(QStringLiteral(
        "QListWidget { background: #1e1e24; color: #ccc; border: 1px solid #444; }"
        "QListWidget::item { padding: 4px; }"
        "QListWidget::item:selected { background: #3a5080; }"));
    layout->addWidget(m_chapterList, 1);

    // Buttons
    auto* btnLayout = new QHBoxLayout;
    btnLayout->setSpacing(4);

    auto* addBtn    = new QPushButton(tr("Add"), this);
    auto* editBtn   = new QPushButton(tr("Edit"), this);
    auto* deleteBtn = new QPushButton(tr("Delete"), this);

    addBtn->setToolTip(tr("Add chapter at current playhead position"));
    editBtn->setToolTip(tr("Edit selected chapter title"));
    deleteBtn->setToolTip(tr("Delete selected chapter"));

    btnLayout->addWidget(addBtn);
    btnLayout->addWidget(editBtn);
    btnLayout->addWidget(deleteBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    // --- Connections ---

    // Double-click to jump to chapter position
    connect(m_chapterList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        if (!item || !m_timeline) return;
        int64_t position = item->data(Qt::UserRole).toLongLong();
        m_timeline->setPlayhead(position);
        emit chapterSelected(position);
    });

    // Selection changed
    connect(m_chapterList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0 || row >= m_chapterList->count()) return;
        auto* item = m_chapterList->item(row);
        if (item) {
            int64_t position = item->data(Qt::UserRole).toLongLong();
            emit chapterSelected(position);
        }
    });

    // Add button: create chapter at current playhead
    connect(addBtn, &QPushButton::clicked, this, [this] {
        if (!m_timeline) return;

        bool ok = false;
        QString title = QInputDialog::getText(
            this, tr("Add Chapter"),
            tr("Chapter title:"),
            QLineEdit::Normal,
            tr("Chapter %1").arg(m_chapterList->count() + 1),
            &ok);

        if (!ok || title.isEmpty()) return;

        int64_t position = m_timeline->playhead();
        addChapter(position, title);
    });

    // Edit button: rename selected chapter
    connect(editBtn, &QPushButton::clicked, this, [this] {
        auto* item = m_chapterList->currentItem();
        if (!item) return;

        // Extract current title (after the time prefix)
        QString currentText = item->text();
        int dashIdx = currentText.indexOf(QStringLiteral(" - "));
        QString currentTitle = (dashIdx >= 0) ? currentText.mid(dashIdx + 3) : currentText;

        bool ok = false;
        QString newTitle = QInputDialog::getText(
            this, tr("Edit Chapter"),
            tr("Chapter title:"),
            QLineEdit::Normal,
            currentTitle,
            &ok);

        if (!ok || newTitle.isEmpty()) return;

        int64_t position = item->data(Qt::UserRole).toLongLong();
        int sampleRate = m_timeline ? m_timeline->sampleRate() : 48000;
        item->setText(formatTime(position, sampleRate) + QStringLiteral(" - ") + newTitle);
    });

    // Delete button
    connect(deleteBtn, &QPushButton::clicked, this, [this] {
        int row = m_chapterList->currentRow();
        if (row < 0) return;

        auto* item = m_chapterList->takeItem(row);
        delete item;
    });
}

ChapterWidget::~ChapterWidget() = default;

void ChapterWidget::setTimeline(Timeline* timeline)
{
    m_timeline = timeline;
    refreshFromTimeline();
}

void ChapterWidget::addChapter(int64_t position, const QString& title)
{
    int sampleRate = m_timeline ? m_timeline->sampleRate() : 48000;
    QString displayText = formatTime(position, sampleRate) + QStringLiteral(" - ") + title;

    auto* item = new QListWidgetItem(displayText, m_chapterList);
    item->setData(Qt::UserRole, QVariant::fromValue(position));

    // Insert in sorted order by position
    int insertRow = 0;
    for (int i = 0; i < m_chapterList->count() - 1; ++i) {
        int64_t existingPos = m_chapterList->item(i)->data(Qt::UserRole).toLongLong();
        if (position > existingPos) {
            insertRow = i + 1;
        }
    }

    // Move the newly added item to the correct sorted position
    if (insertRow < m_chapterList->count() - 1) {
        m_chapterList->takeItem(m_chapterList->count() - 1);
        m_chapterList->insertItem(insertRow, item);
    }

    m_chapterList->setCurrentItem(item);
}

void ChapterWidget::refreshFromTimeline()
{
    m_chapterList->clear();

    // In a full implementation, the Timeline would provide a list of markers/chapters.
    // For now the chapter list is managed locally in this widget.
}

} // namespace dawcast::widgets
