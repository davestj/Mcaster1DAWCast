// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "BusRouter.h"

#include <QDebug>
#include <cstring>

namespace dawcast {

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

BusRouter::BusRouter(QObject* parent)
    : QObject(parent)
{
    // The master bus always exists at index 0
    auto* master = new AudioBus(QStringLiteral("Master"), AudioBus::Master, this);
    m_buses.append(master);
}

BusRouter::~BusRouter() = default;

// ---------------------------------------------------------------------------
// Bus management (GUI thread)
// ---------------------------------------------------------------------------

AudioBus* BusRouter::masterBus() const
{
    return m_buses.isEmpty() ? nullptr : m_buses.first();
}

AudioBus* BusRouter::addBus(const QString& name, AudioBus::BusType type)
{
    auto* bus = new AudioBus(name, type, this);
    m_buses.append(bus);
    int idx = m_buses.size() - 1;
    emit busAdded(idx);
    return bus;
}

void BusRouter::removeBus(int index)
{
    // Protect the master bus
    if (index <= 0 || index >= m_buses.size()) return;

    AudioBus* bus = m_buses.at(index);

    // Clean up any track outputs or sends referencing this bus
    for (auto it = m_trackOutputs.begin(); it != m_trackOutputs.end(); ++it) {
        if (it.value() == bus) {
            it.value() = masterBus();  // Revert to master
        }
    }
    for (auto it = m_trackSends.begin(); it != m_trackSends.end(); ++it) {
        auto& sends = it.value();
        sends.erase(std::remove_if(sends.begin(), sends.end(),
            [bus](const QPair<AudioBus*, float>& p) { return p.first == bus; }),
            sends.end());
    }

    m_buses.removeAt(index);
    bus->deleteLater();

    emit busRemoved(index);
    emit routingChanged();
}

AudioBus* BusRouter::bus(int index) const
{
    if (index < 0 || index >= m_buses.size()) return nullptr;
    return m_buses.at(index);
}

AudioBus* BusRouter::busByName(const QString& name) const
{
    for (auto* b : m_buses) {
        if (b->name() == name) return b;
    }
    return nullptr;
}

int BusRouter::busCount() const
{
    return m_buses.size();
}

// ---------------------------------------------------------------------------
// Per-track routing (GUI thread)
// ---------------------------------------------------------------------------

void BusRouter::setTrackOutput(int trackIndex, AudioBus* bus)
{
    if (!bus) bus = masterBus();
    m_trackOutputs[trackIndex] = bus;
    emit routingChanged();
}

AudioBus* BusRouter::trackOutput(int trackIndex) const
{
    auto it = m_trackOutputs.constFind(trackIndex);
    if (it != m_trackOutputs.constEnd()) {
        return it.value();
    }
    // Default: route to master
    return const_cast<BusRouter*>(this)->masterBus();
}

void BusRouter::setTrackSend(int trackIndex, AudioBus* sendBus, float level)
{
    if (!sendBus) return;

    auto& sends = m_trackSends[trackIndex];

    // Update existing send if present
    for (auto& pair : sends) {
        if (pair.first == sendBus) {
            pair.second = level;
            emit routingChanged();
            return;
        }
    }

    // Add new send
    sends.append(qMakePair(sendBus, level));
    emit routingChanged();
}

void BusRouter::removeTrackSend(int trackIndex, AudioBus* sendBus)
{
    if (!sendBus) return;

    auto it = m_trackSends.find(trackIndex);
    if (it == m_trackSends.end()) return;

    auto& sends = it.value();
    sends.erase(std::remove_if(sends.begin(), sends.end(),
        [sendBus](const QPair<AudioBus*, float>& p) { return p.first == sendBus; }),
        sends.end());

    if (sends.isEmpty()) {
        m_trackSends.erase(it);
    }

    emit routingChanged();
}

QList<QPair<AudioBus*, float>> BusRouter::trackSends(int trackIndex) const
{
    auto it = m_trackSends.constFind(trackIndex);
    if (it != m_trackSends.constEnd()) {
        return it.value();
    }
    return {};
}

// ---------------------------------------------------------------------------
// Audio-thread processing (RT-safe)
// ---------------------------------------------------------------------------

void BusRouter::clearAll(int frames, int channels)
{
    for (auto* bus : m_buses) {
        bus->clearBuffer(frames, channels);
    }
}

void BusRouter::routeTrack(int trackIndex, const float* buffer, int frames, int channels)
{
    if (!buffer || frames <= 0 || channels <= 0) return;

    // Route to the track's primary output bus
    AudioBus* outputBus = trackOutput(trackIndex);
    if (outputBus) {
        outputBus->addInput(buffer, frames, channels, 1.0f);
    }

    // Route to all configured sends
    auto it = m_trackSends.constFind(trackIndex);
    if (it != m_trackSends.constEnd()) {
        for (const auto& pair : it.value()) {
            if (pair.first && pair.second > 0.0f) {
                pair.first->addInput(buffer, frames, channels, pair.second);
            }
        }
    }
}

void BusRouter::processAll(int frames, int channels)
{
    // Phase 1: Process all sub-group buses (they feed into master)
    for (auto* bus : m_buses) {
        if (bus->busType() == AudioBus::SubGroup) {
            bus->process(frames, channels);

            // Feed sub-group output into master
            const float* subOut = bus->output();
            if (subOut) {
                AudioBus* master = masterBus();
                if (master) {
                    master->addInput(subOut, frames, channels, 1.0f);
                }
            }
        }
    }

    // Phase 2: Process aux/send buses (parallel, do not feed master)
    for (auto* bus : m_buses) {
        if (bus->busType() == AudioBus::Aux || bus->busType() == AudioBus::Send) {
            bus->process(frames, channels);
        }
    }

    // Phase 3: Process the master bus last
    AudioBus* master = masterBus();
    if (master) {
        master->process(frames, channels);
    }
}

} // namespace dawcast
