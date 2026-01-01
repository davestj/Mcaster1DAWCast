// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QImage>
#include <QString>
#include <QVector>

namespace dawcast {

class SubtitleRenderer : public QObject
{
    Q_OBJECT

public:
    explicit SubtitleRenderer(QObject* parent = nullptr);
    ~SubtitleRenderer() override;

    bool loadSRT(const QString& path);
    bool loadASS(const QString& path);
    bool loadVTT(const QString& path);

    void renderAt(QImage& frame, double timeSeconds);
    void clear();

    [[nodiscard]] bool hasSubtitles() const { return !m_entries.isEmpty(); }

private:
    struct SubtitleEntry {
        double startTime  = 0.0;
        double endTime    = 0.0;
        QString text;
    };

    QVector<SubtitleEntry> m_entries;
};

} // namespace dawcast
