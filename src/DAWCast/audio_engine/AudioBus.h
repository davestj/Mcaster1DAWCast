// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <vector>

namespace dawcast {

class DspChain;

/// An audio bus that can mix multiple inputs and apply insert effects.
/// Used for sub-groups, aux sends, and the master bus.
///
/// Thread safety:
///   - addInput() / process() / output() / clearBuffer() are called on the
///     audio thread (RT-safe).
///   - set* methods are called from the GUI thread; they write atomically-
///     sized values and are safe to race with the audio thread reads.
class AudioBus : public QObject
{
    Q_OBJECT

public:
    enum BusType { Master, SubGroup, Aux, Send };
    Q_ENUM(BusType)

    explicit AudioBus(const QString& name, BusType type, QObject* parent = nullptr);
    ~AudioBus() override;

    [[nodiscard]] QString  name() const { return m_name; }
    [[nodiscard]] BusType  busType() const { return m_type; }

    void  setVolume(float db);
    [[nodiscard]] float volume() const { return m_volumeDb; }

    void  setPan(float pan);    // -1.0 to 1.0
    [[nodiscard]] float pan() const { return m_pan; }

    void  setMuted(bool muted);
    [[nodiscard]] bool isMuted() const { return m_muted; }

    void  setSolo(bool solo);
    [[nodiscard]] bool isSolo() const { return m_solo; }

    /// Returns the insert effect chain for this bus.
    /// Created lazily on first call (GUI thread only).
    DspChain* effectChain();

    /// Const accessor -- returns nullptr if chain was never created.
    [[nodiscard]] const DspChain* effectChain() const { return m_effectChain; }

    // ── Audio-thread mixing (RT-safe) ─────────────────────────────────

    /// Add interleaved audio into this bus's internal buffer, scaled by
    /// sendLevel.  May be called multiple times per block (one per input).
    void addInput(const float* buffer, int frames, int channels, float sendLevel = 1.0f);

    /// Apply the insert effect chain, volume, and pan to the accumulated
    /// buffer.  Call once per block after all addInput() calls.
    void process(int frames, int channels);

    /// Pointer to the processed output buffer (interleaved, frames*channels).
    [[nodiscard]] const float* output() const;

    /// Zero the internal buffer in preparation for the next block.
    void clearBuffer(int frames, int channels);

    /// Peak level of the most recently processed block (for metering).
    [[nodiscard]] float peakLevel() const { return m_peakLevel; }

private:
    QString  m_name;
    BusType  m_type;
    float    m_volumeDb = 0.0f;
    float    m_pan      = 0.0f;
    bool     m_muted    = false;
    bool     m_solo     = false;
    float    m_peakLevel = 0.0f;

    std::vector<float> m_buffer;
    DspChain*          m_effectChain = nullptr;
};

} // namespace dawcast
