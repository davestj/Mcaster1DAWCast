// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ChapterWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QListWidget>

namespace dawcast::widgets {

ChapterWidget::ChapterWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);

    m_chapterList = new QListWidget(this);
    layout->addWidget(m_chapterList);

    auto* btnLayout = new QHBoxLayout;
    auto* addBtn    = new QPushButton(tr("Add"), this);
    auto* editBtn   = new QPushButton(tr("Edit"), this);
    auto* deleteBtn = new QPushButton(tr("Delete"), this);
    btnLayout->addWidget(addBtn);
    btnLayout->addWidget(editBtn);
    btnLayout->addWidget(deleteBtn);
    layout->addLayout(btnLayout);

    connect(m_chapterList, &QListWidget::currentRowChanged, this, [this](int /*row*/) {
        // TODO: look up chapter position from timeline and emit
        emit chapterSelected(0);
    });

    // TODO: connect add/edit/delete buttons
}

ChapterWidget::~ChapterWidget() = default;

void ChapterWidget::setTimeline(core::Timeline* timeline)
{
    m_timeline = timeline;
    // TODO: populate chapter list from timeline markers
}

} // namespace dawcast::widgets
