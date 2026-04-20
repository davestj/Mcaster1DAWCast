// Mcaster1DAWCast — Windows video hardware acceleration hooks
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Parallels src/DAWCast/platform/macos_video.mm (VideoToolbox).
// Heavy lifting is done by FFmpeg's hwaccel API — this module picks the best
// available decoder/encoder for the current GPU and returns AV_HWDEVICE_TYPE_*
// values the video pipeline can plug into avcodec contexts.
#pragma once

#include <QString>
#include <cstdint>

extern "C" {
#include <libavutil/hwcontext.h>
}

namespace dawcast {

enum class WindowsHwAccel : int {
    None      = 0,
    D3D11VA,  // Windows 8+ baseline — widest compat
    DXVA2,    // Legacy fallback for Win7-era drivers
    CUDA,     // NVIDIA NVDEC path
    QSV,      // Intel QuickSync
};

struct WindowsHwAccelProbe {
    WindowsHwAccel kind = WindowsHwAccel::None;
    QString        description;   // Human-readable, for UI display
    QString        deviceName;    // GPU adapter name via DXGI
    bool           supportsDecode = false;
    bool           supportsEncode = false;
};

class WindowsVideo {
public:
    // Probe GPU + driver + FFmpeg availability. Returns the best decoder.
    static WindowsHwAccelProbe probeDecoder();

    // Best encoder — prefers NVENC > QSV > D3D11 > CPU-only.
    static WindowsHwAccelProbe probeEncoder();

    // Map our enum onto FFmpeg's device type for av_hwdevice_ctx_create.
    static AVHWDeviceType toAVDeviceType(WindowsHwAccel k);
};

} // namespace dawcast
