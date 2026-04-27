// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
//
// AudioUnit enumeration — macOS only (Objective-C++)

#ifdef __APPLE__

#include "PluginScanner.h"

#import <AudioToolbox/AudioToolbox.h>
#import <CoreFoundation/CoreFoundation.h>

#include <QUuid>

namespace dawcast::plugins {

void PluginScanner::scanAU()
{
    emit scanProgress(QStringLiteral("Scanning AudioUnit components..."));

    AudioComponentDescription desc;
    desc.componentType         = 0; // match all
    desc.componentSubType      = 0;
    desc.componentManufacturer = 0;
    desc.componentFlags        = 0;
    desc.componentFlagsMask    = 0;

    AudioComponent comp = nullptr;
    while ((comp = AudioComponentFindNext(comp, &desc)) != nullptr) {
        CFStringRef cfName = nullptr;
        OSStatus status = AudioComponentCopyName(comp, &cfName);
        if (status != noErr || !cfName) continue;

        AudioComponentDescription compDesc;
        AudioComponentGetDescription(comp, &compDesc);

        PluginInfo info;
        info.format = PluginInfo::AudioUnit;
        info.id     = QUuid::createUuid().toString(QUuid::WithoutBraces);

        // Convert CFString to QString
        NSString* nsName = (__bridge NSString*)cfName;
        info.name = QString::fromNSString(nsName);
        CFRelease(cfName);

        // Extract vendor from "Vendor: Name" format common in AU
        int colonIdx = info.name.indexOf(QLatin1String(": "));
        if (colonIdx > 0) {
            info.vendor = info.name.left(colonIdx);
        }

        // Determine type
        if (compDesc.componentType == kAudioUnitType_MusicDevice ||
            compDesc.componentType == kAudioUnitType_MusicEffect) {
            info.isInstrument = true;
            info.isSynth = (compDesc.componentType == kAudioUnitType_MusicDevice);
        }

        // AU path is system-managed; record the 4-char codes instead.
        //
        // Apple's AudioComponentDescription fourcc fields use the compile-
        // time 'abcd' literal encoding: 'a' is the MOST-significant byte
        // of the 32-bit integer (big-endian in-integer order). On little-
        // endian ARM64, dumping those 4 bytes in MEMORY order produces the
        // reversed string ("xfua" instead of "aufx"), which the host then
        // packs back high-byte-first and ends up with a byte-swapped
        // identifier that AudioComponentFindNext rejects as "no matching
        // component". Emit character order (high byte first) explicitly.
        auto fourCcToQString = [](uint32_t v) {
            char buf[5] = {
                static_cast<char>((v >> 24) & 0xFF),
                static_cast<char>((v >> 16) & 0xFF),
                static_cast<char>((v >>  8) & 0xFF),
                static_cast<char>( v        & 0xFF),
                '\0'
            };
            return QString::fromLatin1(buf, 4);
        };
        info.path = QStringLiteral("AU:%1:%2:%3")
            .arg(fourCcToQString(compDesc.componentType))
            .arg(fourCcToQString(compDesc.componentSubType))
            .arg(fourCcToQString(compDesc.componentManufacturer));

        info.numAudioInputs  = 2;
        info.numAudioOutputs = 2;

        m_plugins.append(info);
    }
}

} // namespace dawcast::plugins

#endif // __APPLE__
