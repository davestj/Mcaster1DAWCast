// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef __APPLE__

#include "macos_audio.h"

#import <CoreAudio/CoreAudio.h>

namespace dawcast::platform {

QStringList enumerateAudioDevices()
{
    // TODO: enumerate CoreAudio devices via AudioObjectGetPropertyData
    return {};
}

int defaultInputDevice()
{
    // TODO: query kAudioHardwarePropertyDefaultInputDevice
    return -1;
}

int defaultOutputDevice()
{
    // TODO: query kAudioHardwarePropertyDefaultOutputDevice
    return -1;
}

} // namespace dawcast::platform

#endif // __APPLE__
