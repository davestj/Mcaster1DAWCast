// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QWidget>
#include <QListWidget>
#include <cstdint>

namespace dawcast::core { class Timeline; }

namespace dawcast::widgets {

class ChapterWidget : public QWidget {
    Q_OBJECT

public:
    explicit ChapterWidget(QWidget* parent = nullptr);
    ~ChapterWidget() override;

    void setTimeline(core::Timeline* timeline);

signals:
    void chapterSelected(int64_t position);

private:
    core::Timeline* m_timeline = nullptr;

    QListWidget* m_chapterList = nullptr;
};

} // namespace dawcast::widgets
