// Mcaster1DAWCast — Windows audio device enumeration (WASAPI)
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Parallels src/DAWCast/platform/device_discovery.mm on macOS.
// PortAudio already opens WASAPI streams; this module exposes richer device
// metadata (friendly name, default sample rate, channel layout, whether it's
// a Mcaster1AudioPipe virtual loopback) that PortAudio's device info doesn't
// surface consistently on Windows.
#pragma once

#include <QList>
#include <QString>
#include <cstdint>

namespace dawcast {

struct AudioDeviceInfo {
    QString     id;                // WASAPI endpoint id (stable across boots)
    QString     name;              // Friendly name shown in UI
    bool        isInput       = false;
    bool        isOutput      = false;
    bool        isDefault     = false;
    bool        isLoopback    = false; // WASAPI loopback / AudioPipe virtual
    uint32_t    maxChannels   = 0;
    double      defaultSampleRate = 0.0;
    QString     driverName;        // "WASAPI Shared", "WASAPI Exclusive", "ASIO"
};

class WindowsAudioDevices {
public:
    // Enumerate all render + capture endpoints via MMDeviceEnumerator.
    // Returns empty list on failure and logs via DebugLogger.
    static QList<AudioDeviceInfo> enumerate();

    // Returns the WASAPI endpoint id of the system default render device.
    // Empty QString if none available.
    static QString defaultRenderDeviceId();
    static QString defaultCaptureDeviceId();
};

} // namespace dawcast
