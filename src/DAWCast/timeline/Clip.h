// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QList>
#include <QString>
#include <cstdint>

namespace dawcast {

/// A single breakpoint in the clip gain envelope (rubber-band editing).
struct GainPoint {
    int64_t offsetSamples = 0;  ///< Offset from clip start (not timeline position)
    float   gainDb        = 0.0f; ///< Gain in dB (-inf to +12 dB)
};

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

    // ── Clip Gain Envelope (rubber-band editing) ─────────────────────────
    void addGainPoint(int64_t offsetSamples, float gainDb);
    void removeGainPoint(int index);
    void moveGainPoint(int index, int64_t offsetSamples, float gainDb);
    [[nodiscard]] QList<GainPoint> gainEnvelope() const { return m_gainEnvelope; }
    void setGainEnvelope(const QList<GainPoint>& envelope);

    /// Interpolated gain (linear) at the given sample offset from clip start.
    /// Returns the base gain (m_gain) converted to linear if the envelope is empty.
    /// When the envelope has points, they define the gain curve and the base
    /// gain is ignored.
    [[nodiscard]] float gainAt(int64_t offsetSamples) const;

private:
    QString m_sourcePath;
    int64_t m_sourceIn         = 0;
    int64_t m_sourceOut        = 0;
    int64_t m_timelinePosition = 0;
    float   m_gain             = 1.0f;
    int64_t m_fadeIn           = 0;
    int64_t m_fadeOut          = 0;

    QList<GainPoint> m_gainEnvelope;  ///< Sorted by offsetSamples
};

} // namespace dawcast
