// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef __APPLE__

#include "macos_video.h"

#import <AVFoundation/AVFoundation.h>

namespace dawcast::platform {

bool hasVideoToolbox()
{
    // TODO: check for VideoToolbox availability
    return false;
}

QStringList supportedHWCodecs()
{
    // TODO: enumerate hardware-accelerated codecs via VideoToolbox
    return {};
}

} // namespace dawcast::platform

#endif // __APPLE__
