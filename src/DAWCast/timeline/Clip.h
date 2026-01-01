// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <cstdint>

namespace dawcast {

class Clip : public QObject
{
    Q_OBJECT

public:
    explicit Clip(QObject* parent = nullptr);
    ~Clip() override;

    [[nodiscard]] QString sourcePath()       const { return m_sourcePath; }
    [[nodiscard]] int64_t sourceIn()         const { return m_sourceIn; }
    [[nodiscard]] int64_t sourceOut()        const { return m_sourceOut; }
    [[nodiscard]] int64_t timelinePosition() const { return m_timelinePosition; }
    [[nodiscard]] int64_t duration()         const { return m_sourceOut - m_sourceIn; }
    [[nodiscard]] int64_t endPosition()     const { return m_timelinePosition + duration(); }
    [[nodiscard]] float   gain()             const { return m_gain; }
    [[nodiscard]] int64_t fadeIn()           const { return m_fadeIn; }
    [[nodiscard]] int64_t fadeOut()          const { return m_fadeOut; }

    void setSourcePath(const QString& path);
    void setSourceIn(int64_t samples);
    void setSourceOut(int64_t samples);
    void setTimelinePosition(int64_t samples);
    void setGain(float gain);
    void setFadeIn(int64_t samples);
    void setFadeOut(int64_t samples);

private:
    QString m_sourcePath;
    int64_t m_sourceIn         = 0;
    int64_t m_sourceOut        = 0;
    int64_t m_timelinePosition = 0;
    float   m_gain             = 1.0f;
    int64_t m_fadeIn           = 0;
    int64_t m_fadeOut          = 0;
};

} // namespace dawcast
