// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef __APPLE__

#include "device_discovery.h"

#import <CoreAudio/CoreAudio.h>
#import <AVFoundation/AVFoundation.h>

namespace dawcast::platform {

QStringList audioInputDevices()
{
    // TODO: enumerate CoreAudio input devices
    return {};
}

QStringList audioOutputDevices()
{
    // TODO: enumerate CoreAudio output devices
    return {};
}

QStringList videoInputDevices()
{
    // TODO: enumerate AVCaptureDevice video devices
    return {};
}

} // namespace dawcast::platform

#endif // __APPLE__
