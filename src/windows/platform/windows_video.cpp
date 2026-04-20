// Mcaster1DAWCast — Windows video hardware acceleration hooks
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "windows_video.h"
#include "compat_windows.h"

#include <dxgi.h>

namespace {

QString query_primary_gpu() {
    QString name;
    IDXGIFactory1* factory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1),
                                  reinterpret_cast<void**>(&factory)))) return name;
    IDXGIAdapter1* adapter = nullptr;
    if (SUCCEEDED(factory->EnumAdapters1(0, &adapter))) {
        DXGI_ADAPTER_DESC1 desc = {};
        if (SUCCEEDED(adapter->GetDesc1(&desc))) {
            name = QString::fromWCharArray(desc.Description);
        }
        adapter->Release();
    }
    factory->Release();
    return name;
}

// Rough vendor sniff from DXGI description string.
bool is_nvidia(const QString& s) { return s.contains("NVIDIA",   Qt::CaseInsensitive); }
bool is_intel (const QString& s) { return s.contains("Intel",    Qt::CaseInsensitive); }
bool is_amd   (const QString& s) { return s.contains("AMD",      Qt::CaseInsensitive)
                                       || s.contains("Radeon",   Qt::CaseInsensitive); }

} // namespace

namespace dawcast {

WindowsHwAccelProbe WindowsVideo::probeDecoder() {
    WindowsHwAccelProbe p;
    p.deviceName = query_primary_gpu();

    // D3D11VA is the baseline for Win8+ with any DX11-capable adapter.
    // FFmpeg will fall back to software if the bitstream isn't supported.
    p.kind           = WindowsHwAccel::D3D11VA;
    p.supportsDecode = true;
    p.supportsEncode = false;
    p.description    = QStringLiteral("D3D11VA on %1").arg(p.deviceName.isEmpty()
                                                             ? QStringLiteral("unknown GPU")
                                                             : p.deviceName);
    return p;
}

WindowsHwAccelProbe WindowsVideo::probeEncoder() {
    WindowsHwAccelProbe p;
    p.deviceName = query_primary_gpu();

    if (is_nvidia(p.deviceName)) {
        p.kind           = WindowsHwAccel::CUDA;
        p.description    = QStringLiteral("NVENC on %1").arg(p.deviceName);
        p.supportsEncode = true;
    } else if (is_intel(p.deviceName)) {
        p.kind           = WindowsHwAccel::QSV;
        p.description    = QStringLiteral("Intel QuickSync on %1").arg(p.deviceName);
        p.supportsEncode = true;
    } else if (is_amd(p.deviceName)) {
        // AMF is not in FFmpeg's hwcontext list — route through D3D11 + amf encoders.
        p.kind           = WindowsHwAccel::D3D11VA;
        p.description    = QStringLiteral("AMF on %1 (via D3D11)").arg(p.deviceName);
        p.supportsEncode = true;
    } else {
        p.kind           = WindowsHwAccel::None;
        p.description    = QStringLiteral("No HW encoder detected — CPU (x264 / libvpx / SVT-AV1)");
        p.supportsEncode = false;
    }
    p.supportsDecode = true;
    return p;
}

AVHWDeviceType WindowsVideo::toAVDeviceType(WindowsHwAccel k) {
    switch (k) {
        case WindowsHwAccel::D3D11VA: return AV_HWDEVICE_TYPE_D3D11VA;
        case WindowsHwAccel::DXVA2:   return AV_HWDEVICE_TYPE_DXVA2;
        case WindowsHwAccel::CUDA:    return AV_HWDEVICE_TYPE_CUDA;
        case WindowsHwAccel::QSV:     return AV_HWDEVICE_TYPE_QSV;
        case WindowsHwAccel::None:
        default:                       return AV_HWDEVICE_TYPE_NONE;
    }
}

} // namespace dawcast
