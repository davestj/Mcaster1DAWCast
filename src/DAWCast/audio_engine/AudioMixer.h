// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QVector>
#include <atomic>
#include <memory>
#include <vector>
#include "../core/AudioBuffer.h"

namespace dawcast {

class AudioMixer : public QObject
{
    Q_OBJECT

public:
    static constexpr int MaxStrips = 32;

    /// Solo routing modes — determines how non-soloed tracks behave
    /// when at least one track is soloed.
    enum SoloMode {
        SoloInPlace,   ///< Non-soloed tracks are fully muted (default)
        SoloInFront    ///< Non-soloed tracks are dimmed by soloDimDb()
    };
    Q_ENUM(SoloMode)

    explicit AudioMixer(QObject* parent = nullptr);
    ~AudioMixer() override;

    int  addStrip();
    void removeStrip(int index);
    [[nodiscard]] int stripCount() const;

    void setStripVolume(int strip, float db);
    void setStripPan(int strip, float pan);
    void setStripMuted(int strip, bool muted);
    void setStripSolo(int strip, bool solo);
    void setStripBuffer(int strip, const AudioBuffer* buffer);

    [[nodiscard]] float stripVolume(int strip) const;
    [[nodiscard]] float stripPan(int strip) const;

    /// Read the most recent peak level captured for a strip (linear amplitude).
    /// Safe to call from the GUI thread — backed by atomics written from the
    /// audio callback.
    [[nodiscard]] float stripPeakL(int strip) const;
    [[nodiscard]] float stripPeakR(int strip) const;

    /// Read the most recent master output peak (linear amplitude).
    [[nodiscard]] float masterPeakL() const;
    [[nodiscard]] float masterPeakR() const;

    /// Solo routing mode
    void setSoloMode(SoloMode mode);
    [[nodiscard]] SoloMode soloMode() const;

    /// Dim level for Solo-in-Front mode (negative dB, e.g. -20.0f)
    void setSoloDimDb(float db);
    [[nodiscard]] float soloDimDb() const;

    void process(AudioBuffer& output);

signals:
    void soloModeChanged(SoloMode mode);
    void soloDimDbChanged(float db);

private:
    struct Strip {
        float volumeDb = 0.0f;
        float pan      = 0.0f;   // -1.0 left, 0.0 center, 1.0 right
        bool  muted    = false;
        bool  solo     = false;
        const AudioBuffer* inputBuffer = nullptr;
    };

    QVector<Strip> m_strips;
    SoloMode m_soloMode  = SoloInPlace;
    float    m_soloDimDb  = -20.0f;  ///< Default dim: -20 dB for Solo-in-Front

    // Per-strip peak meters (written from the audio thread in process(),
    // read from the GUI thread via a polling QTimer in MixerWidget).
    //
    // std::atomic<float> is neither copyable nor movable so we can't store
    // StripMeters directly in QList/QVector. Use std::vector with
    // unique_ptr — std::vector tolerates non-copyable element types when
    // we only ever push_back/emplace_back.
    struct StripMeters {
        std::atomic<float> peakL{0.0f};
        std::atomic<float> peakR{0.0f};
    };
    mutable std::vector<std::unique_ptr<StripMeters>> m_meters;

    // Master output peak meters (populated inside process()).
    std::atomic<float> m_masterPeakL{0.0f};
    std::atomic<float> m_masterPeakR{0.0f};

    void ensureMeterCapacity(int count) const;
};

} // namespace dawcast
