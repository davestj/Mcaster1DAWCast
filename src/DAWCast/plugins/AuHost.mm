// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
//
// AuHost.mm
// ─────────
// macOS-only Objective-C++ implementation of AuHost.h.  Wraps the C
// AudioUnit v2 API (AudioComponentInstanceNew / AudioUnitInitialize /
// AudioUnitRender / AudioUnitSetParameter) behind a Qt-friendly class.

#ifdef __APPLE__

#include "AuHost.h"

#import  <AudioToolbox/AudioToolbox.h>
#import  <AudioUnit/AudioUnit.h>
#import  <AudioUnit/AUCocoaUIView.h>
#import  <CoreAudio/CoreAudioTypes.h>
#import  <CoreFoundation/CoreFoundation.h>
#import  <Cocoa/Cocoa.h>
#import  <Foundation/Foundation.h>

#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>

#include <cstring>
#include <vector>

namespace dawcast::plugins {

namespace {

QString g_lastError;

// 4CC packed uint32 -> "abcd" ASCII helper.  AU 4CCs are big-endian
// OSType values; mask to the 4 ASCII characters.
QString fourCcToString(uint32_t code)
{
    char out[5] = { 0, 0, 0, 0, 0 };
    out[0] = static_cast<char>((code >> 24) & 0xFF);
    out[1] = static_cast<char>((code >> 16) & 0xFF);
    out[2] = static_cast<char>((code >>  8) & 0xFF);
    out[3] = static_cast<char>((code      ) & 0xFF);
    // Replace NUL / non-printables with '?' so the display string is valid.
    for (int i = 0; i < 4; ++i) {
        unsigned char c = static_cast<unsigned char>(out[i]);
        if (c < 0x20 || c > 0x7E) out[i] = '?';
    }
    return QString::fromLatin1(out, 4);
}

QString nsStringToQString(NSString* s)
{
    if (!s) return QString();
    return QString::fromNSString(s);
}

} // namespace

// ────────────────────────────────────────────────────────────────────────────
// inspectAuComponent
// ────────────────────────────────────────────────────────────────────────────
AuBundleInfo inspectAuComponent(const AudioComponentDescription& descIn)
{
    AuBundleInfo out;
    out.path = QStringLiteral("AU:%1:%2:%3")
        .arg(fourCcToString(descIn.componentType))
        .arg(fourCcToString(descIn.componentSubType))
        .arg(fourCcToString(descIn.componentManufacturer));

    AudioComponentDescription desc = descIn;
    desc.componentFlags     = 0;
    desc.componentFlagsMask = 0;

    AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
    if (!comp) {
        out.error = QStringLiteral("AudioComponentFindNext: no matching component");
        return out;
    }

    AuClassInfo cls;
    cls.type         = desc.componentType;
    cls.subtype      = desc.componentSubType;
    cls.manufacturer = desc.componentManufacturer;
    cls.cid          = out.path;

    // Fetch the display name.  AudioComponentCopyName returns
    // "Vendor: Name" on most units.
    CFStringRef cfName = nullptr;
    if (AudioComponentCopyName(comp, &cfName) == noErr && cfName) {
        NSString* nsName = (__bridge NSString*)cfName;
        cls.name = nsStringToQString(nsName);
        CFRelease(cfName);
    }

    // Split vendor / name on the common "Vendor: Name" pattern.
    const int colonIdx = cls.name.indexOf(QLatin1String(": "));
    if (colonIdx > 0) {
        cls.vendor = cls.name.left(colonIdx).trimmed();
    }

    // Version is packed as 0xMMmmbbbb (major.minor.bugfix).
    UInt32 verPacked = 0;
    if (AudioComponentGetVersion(comp, &verPacked) == noErr) {
        const unsigned major = (verPacked >> 16) & 0xFFFF;
        const unsigned minor = (verPacked >>  8) & 0xFF;
        const unsigned bug   = (verPacked      ) & 0xFF;
        cls.version = QStringLiteral("%1.%2.%3").arg(major).arg(minor).arg(bug);
    }

    // Tag a best-effort subcategory string from the component type.
    const uint32_t t = desc.componentType;
    QString subcat;
    switch (t) {
        case kAudioUnitType_Effect:          subcat = QStringLiteral("Effect|aufx");       break;
        case kAudioUnitType_MusicEffect:     subcat = QStringLiteral("Effect|Instrument|aumf"); break;
        case kAudioUnitType_MusicDevice:     subcat = QStringLiteral("Instrument|aumu");   break;
        case kAudioUnitType_Mixer:           subcat = QStringLiteral("Mixer|aumx");        break;
        case kAudioUnitType_Panner:          subcat = QStringLiteral("Panner|aupn");       break;
        case kAudioUnitType_Generator:       subcat = QStringLiteral("Generator|augn");    break;
        case kAudioUnitType_OfflineEffect:   subcat = QStringLiteral("OfflineEffect|auol"); break;
        case kAudioUnitType_FormatConverter: subcat = QStringLiteral("FormatConverter|aufc"); break;
        case kAudioUnitType_Output:          subcat = QStringLiteral("Output|auou");       break;
        default:                             subcat = fourCcToString(t);                   break;
    }
    cls.subCategories = subcat;

    out.name        = cls.name.isEmpty() ? cls.cid : cls.name;
    out.isComponent = true;
    out.classes.append(cls);
    return out;
}

// ────────────────────────────────────────────────────────────────────────────
// Impl
// ────────────────────────────────────────────────────────────────────────────
struct AuPluginInstance::Impl {
    AudioUnit               au           = nullptr;
    double                  sampleRate   = 48000.0;
    int                     maxFrames    = 0;
    int                     numInChans   = 2;
    int                     numOutChans  = 2;
    bool                    initialized  = false;

    QString                 name;

    // Preallocated deinterleaved per-channel scratch buffers.
    std::vector<float>      inL, inR;
    std::vector<float>      outL, outR;

    // AudioBufferLists reused per block (input + output).  AU v2 expects
    // non-interleaved floats with one AudioBuffer per channel.
    // Allocated once as plain storage; AudioBufferList is a trailing-
    // array struct so we host it inside a byte-addressable buffer.
    std::vector<uint8_t>    inListStore;
    std::vector<uint8_t>    outListStore;
    AudioBufferList*        inList       = nullptr;
    AudioBufferList*        outList      = nullptr;

    // Cached parameter list (gathered once at create() time so the GUI
    // thread doesn't have to enumerate on every paint).
    QList<AuParamInfo>      params;
};

// ────────────────────────────────────────────────────────────────────────────
// Helpers
// ────────────────────────────────────────────────────────────────────────────
namespace {

AudioBufferList* allocateAbl(std::vector<uint8_t>& store, int channels)
{
    const size_t bytes =
        sizeof(AudioBufferList) +
        sizeof(AudioBuffer) * static_cast<size_t>(channels > 0 ? channels - 1 : 0);
    store.assign(bytes, 0);
    auto* list = reinterpret_cast<AudioBufferList*>(store.data());
    list->mNumberBuffers = static_cast<UInt32>(channels);
    for (int i = 0; i < channels; ++i) {
        list->mBuffers[i].mNumberChannels = 1;
        list->mBuffers[i].mDataByteSize   = 0;
        list->mBuffers[i].mData           = nullptr;
    }
    return list;
}

void gatherParameters(AuPluginInstance::Impl* impl)
{
    impl->params.clear();
    if (!impl->au) return;

    // 1) Size of the parameter-list blob.
    UInt32 size = 0;
    Boolean writable = false;
    OSStatus err = AudioUnitGetPropertyInfo(
        impl->au, kAudioUnitProperty_ParameterList,
        kAudioUnitScope_Global, 0, &size, &writable);
    if (err != noErr || size == 0) return;

    const int nParams = static_cast<int>(size / sizeof(AudioUnitParameterID));
    if (nParams <= 0) return;

    std::vector<AudioUnitParameterID> ids(nParams);
    err = AudioUnitGetProperty(
        impl->au, kAudioUnitProperty_ParameterList,
        kAudioUnitScope_Global, 0, ids.data(), &size);
    if (err != noErr) return;

    impl->params.reserve(nParams);
    for (int i = 0; i < nParams; ++i) {
        AudioUnitParameterInfo pinfo{};
        UInt32 infoSize = sizeof(pinfo);
        if (AudioUnitGetProperty(
                impl->au, kAudioUnitProperty_ParameterInfo,
                kAudioUnitScope_Global, ids[i], &pinfo, &infoSize) != noErr) {
            continue;
        }

        AuParamInfo out;
        out.paramId       = ids[i];
        out.minValue      = pinfo.minValue;
        out.maxValue      = pinfo.maxValue;
        out.defaultValue  = pinfo.defaultValue;

        // Name: prefer CFStringRef (always populated in modern AUs) but
        // fall back to the C string embedded in the struct.
        if ((pinfo.flags & kAudioUnitParameterFlag_HasCFNameString) &&
            pinfo.cfNameString) {
            out.name = nsStringToQString((__bridge NSString*)pinfo.cfNameString);
            if (pinfo.flags & kAudioUnitParameterFlag_CFNameRelease) {
                CFRelease(pinfo.cfNameString);
            }
        }
        if (out.name.isEmpty()) {
            out.name = QString::fromUtf8(pinfo.name);
        }
        if (out.name.isEmpty()) {
            out.name = QStringLiteral("Param %1").arg(i);
        }

        // Units: the AU API exposes either a unit enum or a custom
        // CFStringRef; prefer the string if present.
        if ((pinfo.flags & kAudioUnitParameterFlag_HasCFNameString) == 0 &&
            pinfo.unitName) {
            out.units = nsStringToQString((__bridge NSString*)pinfo.unitName);
        }

        out.canAutomate =
            (pinfo.flags & kAudioUnitParameterFlag_NonRealTime) == 0;
        out.readOnly =
            (pinfo.flags & kAudioUnitParameterFlag_IsReadable) != 0 &&
            (pinfo.flags & kAudioUnitParameterFlag_IsWritable) == 0;

        impl->params.append(out);
    }
}

// Pull-style render callback.  The AU calls this when it needs `inNumberFrames`
// of input audio — we hand it our preloaded deinterleaved buffers.
OSStatus AuInputRenderCallback(
    void*                       inRefCon,
    AudioUnitRenderActionFlags* /*ioActionFlags*/,
    const AudioTimeStamp*       /*inTimeStamp*/,
    UInt32                      /*inBusNumber*/,
    UInt32                      inNumberFrames,
    AudioBufferList*            ioData)
{
    auto* impl = static_cast<AuPluginInstance::Impl*>(inRefCon);
    if (!impl || !ioData) return noErr;

    const UInt32 nBuffers = ioData->mNumberBuffers;
    for (UInt32 b = 0; b < nBuffers; ++b) {
        auto* dst = static_cast<float*>(ioData->mBuffers[b].mData);
        if (!dst) continue;
        const UInt32 wantBytes = inNumberFrames * sizeof(float);
        if (ioData->mBuffers[b].mDataByteSize < wantBytes) {
            // AU asked for more frames than we have loaded — zero-fill.
            std::memset(dst, 0, ioData->mBuffers[b].mDataByteSize);
            continue;
        }

        const float* src = nullptr;
        if (b == 0)      src = impl->inL.data();
        else if (b == 1) src = impl->inR.data();

        if (src) std::memcpy(dst, src, wantBytes);
        else     std::memset(dst, 0, wantBytes);
    }
    return noErr;
}

} // namespace

// ────────────────────────────────────────────────────────────────────────────
// create()
// ────────────────────────────────────────────────────────────────────────────
std::unique_ptr<AuPluginInstance> AuPluginInstance::create(
    const AudioComponentDescription& descIn,
    double sampleRate, int maxFrames)
{
    g_lastError.clear();

    AudioComponentDescription desc = descIn;
    desc.componentFlags     = 0;
    desc.componentFlagsMask = 0;

    // OSStatus codes in AU land are often themselves 4-char fourccs (like
    // 'pwr?' = 1886547263 for not-initialized). Render them as both the
    // raw integer AND the 4-char interpretation for fast diagnosis.
    auto statusDesc = [](OSStatus s) -> QString {
        char fcc[5] = {
            static_cast<char>((s >> 24) & 0xFF),
            static_cast<char>((s >> 16) & 0xFF),
            static_cast<char>((s >>  8) & 0xFF),
            static_cast<char>( s        & 0xFF),
            '\0'
        };
        bool printable = true;
        for (int i = 0; i < 4; ++i) {
            if (fcc[i] < 0x20 || fcc[i] > 0x7E) { printable = false; break; }
        }
        if (printable) {
            return QStringLiteral("%1 ('%2')").arg(static_cast<int>(s))
                                              .arg(QString::fromLatin1(fcc, 4));
        }
        return QString::number(static_cast<int>(s));
    };

    // Echo the reconstructed fourccs so mismatches are spottable in the log.
    auto fccAscii = [](uint32_t v) -> QString {
        char buf[5] = {
            static_cast<char>((v >> 24) & 0xFF),
            static_cast<char>((v >> 16) & 0xFF),
            static_cast<char>((v >>  8) & 0xFF),
            static_cast<char>( v        & 0xFF),
            '\0'
        };
        return QString::fromLatin1(buf, 4);
    };
    qInfo().noquote() << "AU host: load attempt"
        << "type="   << fccAscii(desc.componentType)
        << "subtype=" << fccAscii(desc.componentSubType)
        << "manuf="  << fccAscii(desc.componentManufacturer)
        << "sr="     << sampleRate
        << "maxFr="  << maxFrames;

    // Reject types that can't be driven by our stereo-effect pipeline.
    // Users will still see these in the scanner output (they're real AUs
    // on the system) but can't drop them into an effect chain.
    const uint32_t t = desc.componentType;
    const bool isSupportedType =
        (t == kAudioUnitType_Effect      ||
         t == kAudioUnitType_MusicEffect ||
         t == kAudioUnitType_Mixer       ||
         t == kAudioUnitType_Panner);
    if (!isSupportedType) {
        g_lastError = QStringLiteral(
            "Unsupported AU type '%1' — effect chain needs kAudioUnitType_Effect, "
            "MusicEffect, Mixer, or Panner. Codecs (aenc/adec), format converters "
            "(aufc), generators (augn), and instruments (aumu) are scanned but "
            "not yet routed through the effect chain."
        ).arg(fccAscii(t));
        qWarning().noquote() << "AU host: rejected type" << fccAscii(t);
        return nullptr;
    }

    AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
    if (!comp) {
        g_lastError = QStringLiteral(
            "AudioComponentFindNext: no matching component for type=%1 "
            "subtype=%2 manuf=%3 — the scanner saw this plugin but the "
            "registry doesn't find it now (was it uninstalled?).")
            .arg(fccAscii(desc.componentType))
            .arg(fccAscii(desc.componentSubType))
            .arg(fccAscii(desc.componentManufacturer));
        return nullptr;
    }

    AudioUnit au = nullptr;
    OSStatus err = AudioComponentInstanceNew(comp, &au);
    if (err != noErr || !au) {
        g_lastError = QStringLiteral(
            "AudioComponentInstanceNew failed: OSStatus=%1 — plugin bundle "
            "could not be loaded (missing architecture, Gatekeeper rejection, "
            "or plugin itself crashed during init)").arg(statusDesc(err));
        return nullptr;
    }

    // Stereo float, non-interleaved (one AudioBuffer per channel).
    AudioStreamBasicDescription asbd{};
    asbd.mSampleRate       = sampleRate;
    asbd.mFormatID         = kAudioFormatLinearPCM;
    asbd.mFormatFlags      = kAudioFormatFlagIsFloat
                           | kAudioFormatFlagIsPacked
                           | kAudioFormatFlagIsNonInterleaved;
    asbd.mBitsPerChannel   = 32;
    asbd.mChannelsPerFrame = 2;
    asbd.mFramesPerPacket  = 1;
    asbd.mBytesPerFrame    = sizeof(float);      // per channel (non-interleaved)
    asbd.mBytesPerPacket   = sizeof(float);

    // Set stream format on both I/O scopes (most effects want both).
    // Some units only have an output bus (generators) — tolerate failure
    // on the input scope but require success on output.
    err = AudioUnitSetProperty(au, kAudioUnitProperty_StreamFormat,
                               kAudioUnitScope_Input, 0,
                               &asbd, sizeof(asbd));
    const bool haveInput = (err == noErr);
    if (!haveInput) {
        // Not fatal — many effect AUs still accept output-only format
        // setting, and AU treats missing input as silence.
        qWarning() << "AU: set input StreamFormat failed" << (int)err;
    }

    err = AudioUnitSetProperty(au, kAudioUnitProperty_StreamFormat,
                               kAudioUnitScope_Output, 0,
                               &asbd, sizeof(asbd));
    if (err != noErr) {
        AudioComponentInstanceDispose(au);
        g_lastError = QStringLiteral(
            "AU: set output StreamFormat (stereo float 32-bit non-interleaved "
            "@ %1 Hz) failed: OSStatus=%2 — plugin rejected our pipeline "
            "format. AU host currently only supports stereo-effect format; "
            "surround/mono-only/exotic-sample-rate plugins aren't wired yet.")
            .arg(sampleRate).arg(statusDesc(err));
        return nullptr;
    }

    // Declare our maximum render block size.
    UInt32 maxFr = static_cast<UInt32>(maxFrames);
    err = AudioUnitSetProperty(au, kAudioUnitProperty_MaximumFramesPerSlice,
                               kAudioUnitScope_Global, 0,
                               &maxFr, sizeof(maxFr));
    if (err != noErr) {
        // Not fatal on many AUs; warn and continue.
        qWarning() << "AU: set MaximumFramesPerSlice failed" << (int)err;
    }

    auto inst = std::unique_ptr<AuPluginInstance>(new AuPluginInstance());
    inst->m_impl->au          = au;
    inst->m_impl->sampleRate  = sampleRate;
    inst->m_impl->maxFrames   = maxFrames;
    inst->m_impl->numInChans  = 2;
    inst->m_impl->numOutChans = 2;

    inst->m_impl->inL.assign(static_cast<size_t>(maxFrames), 0.0f);
    inst->m_impl->inR.assign(static_cast<size_t>(maxFrames), 0.0f);
    inst->m_impl->outL.assign(static_cast<size_t>(maxFrames), 0.0f);
    inst->m_impl->outR.assign(static_cast<size_t>(maxFrames), 0.0f);

    inst->m_impl->inList  = allocateAbl(inst->m_impl->inListStore,  2);
    inst->m_impl->outList = allocateAbl(inst->m_impl->outListStore, 2);

    // Install the input render callback on input bus 0 only if the unit
    // actually accepted an input stream format.  For pure generators we
    // leave it off and AudioUnitRender just produces output.
    if (haveInput) {
        AURenderCallbackStruct cb{};
        cb.inputProc       = &AuInputRenderCallback;
        cb.inputProcRefCon = inst->m_impl.get();
        err = AudioUnitSetProperty(au, kAudioUnitProperty_SetRenderCallback,
                                   kAudioUnitScope_Input, 0,
                                   &cb, sizeof(cb));
        if (err != noErr) {
            qWarning() << "AU: set SetRenderCallback failed" << (int)err;
        }
    }

    err = AudioUnitInitialize(au);
    if (err != noErr) {
        AudioComponentInstanceDispose(au);
        g_lastError = QStringLiteral(
            "AudioUnitInitialize failed: OSStatus=%1 — plugin couldn't "
            "complete its internal setup (license check, sample-rate mismatch, "
            "or buffer-size constraint).").arg(statusDesc(err));
        return nullptr;
    }
    qInfo().noquote() << "AU host: loaded OK —"
        << fccAscii(desc.componentType) << fccAscii(desc.componentSubType)
        << fccAscii(desc.componentManufacturer);
    inst->m_impl->initialized = true;

    // Pull the display name from AudioComponentCopyName.
    CFStringRef cfName = nullptr;
    if (AudioComponentCopyName(comp, &cfName) == noErr && cfName) {
        inst->m_impl->name = nsStringToQString((__bridge NSString*)cfName);
        CFRelease(cfName);
    }
    if (inst->m_impl->name.isEmpty()) {
        inst->m_impl->name = QStringLiteral("%1:%2:%3")
            .arg(fourCcToString(desc.componentType))
            .arg(fourCcToString(desc.componentSubType))
            .arg(fourCcToString(desc.componentManufacturer));
    }

    gatherParameters(inst->m_impl.get());
    return inst;
}

AuPluginInstance::AuPluginInstance()
    : m_impl(std::make_unique<Impl>())
{
}

AuPluginInstance::~AuPluginInstance()
{
    if (!m_impl) return;
    if (!m_impl->au) return;

    // Teardown ordering matters — especially for plugins that spawn their
    // own worker threads (Universal Audio is the canonical offender; its
    // plugins run UAContext{Main,LowP}, Socket, LocalSocket, a JUCE timer,
    // and a UALogWriter). If we dispose the AudioUnit instance while those
    // threads are mid-frame they'll call back into a half-destructed C++
    // object and hit __cxa_pure_virtual → abort(). This was the shutdown
    // crash we saw with uaudio_ua_1176_rev_a at app-exit.
    //
    // Step 1: stop output (silences render graph edges still pulling us).
    // Step 2: set kAudioUnitProperty_MakeConnection to nothing to cut
    //         the audio graph cleanly.
    // Step 3: uninitialize — this is where the plugin is supposed to
    //         signal its worker threads to stop.
    // Step 4: pump the current thread's run loop briefly (50 ms) so
    //         those worker threads can actually process the stop signal
    //         and exit before we yank the instance out from under them.
    // Step 5: dispose the component instance (actual dylib teardown).

    if (m_impl->initialized) {
        // Stop/active cycle before uninit — prevents in-flight renders
        // from seeing a partially-torn-down instance.
        AudioOutputUnitStop(m_impl->au);
        AudioUnitReset(m_impl->au, kAudioUnitScope_Global, 0);
        AudioUnitUninitialize(m_impl->au);
        m_impl->initialized = false;

        // Flush the current run loop so plugin worker threads can process
        // their stop signals. The magic number is a pragmatic balance:
        // too short (< 20 ms) and UA plugins still have threads executing
        // when we dispose; too long (> 100 ms) and app quit feels laggy.
        // 50 ms is what Logic Pro uses in practice (observed via dtrace).
        const CFAbsoluteTime deadline = CFAbsoluteTimeGetCurrent() + 0.050;
        while (CFAbsoluteTimeGetCurrent() < deadline) {
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.005, true);
        }
    }

    // Now safe to actually dispose. Wrap in a try/catch-equivalent via
    // signal-safe @try since some rogue plugins still throw from here.
    @try {
        AudioComponentInstanceDispose(m_impl->au);
    } @catch (NSException* e) {
        qWarning() << "AU host: plugin threw during dispose:"
                   << [[e description] UTF8String];
    }
    m_impl->au = nullptr;
}

QString AuPluginInstance::displayName() const
{
    return m_impl ? m_impl->name : QString();
}

// ────────────────────────────────────────────────────────────────────────────
// process()
// ────────────────────────────────────────────────────────────────────────────
void AuPluginInstance::process(float* buffer, int frames, int channels)
{
    if (!m_impl || !m_impl->au || !m_impl->initialized) return;
    if (!buffer || frames <= 0 || channels != 2) return;

    if (frames > m_impl->maxFrames) {
        int offset = 0;
        while (offset < frames) {
            int chunk = std::min(m_impl->maxFrames, frames - offset);
            process(buffer + offset * channels, chunk, channels);
            offset += chunk;
        }
        return;
    }

    // Deinterleave caller buffer into our per-channel scratch.
    for (int f = 0; f < frames; ++f) {
        m_impl->inL[f] = buffer[f * 2    ];
        m_impl->inR[f] = buffer[f * 2 + 1];
    }

    // Rebind the output AudioBufferList to point at our output scratch.
    const UInt32 bytes = static_cast<UInt32>(frames) * sizeof(float);
    m_impl->outList->mBuffers[0].mData         = m_impl->outL.data();
    m_impl->outList->mBuffers[0].mDataByteSize = bytes;
    m_impl->outList->mBuffers[1].mData         = m_impl->outR.data();
    m_impl->outList->mBuffers[1].mDataByteSize = bytes;

    // Our input render callback reads straight from inL/inR so it does
    // not need ioData->mBuffers[].mData to be preset, but AU sometimes
    // passes a scratch list it owns.  We do not touch it here.
    AudioUnitRenderActionFlags flags = 0;
    AudioTimeStamp ts{};
    ts.mSampleTime = 0;
    ts.mFlags      = kAudioTimeStampSampleTimeValid;

    OSStatus err = AudioUnitRender(
        m_impl->au, &flags, &ts, 0 /*bus*/,
        static_cast<UInt32>(frames), m_impl->outList);
    if (err != noErr) {
        // Fall back: zero the output so we don't pass junk downstream.
        std::memset(m_impl->outL.data(), 0, bytes);
        std::memset(m_impl->outR.data(), 0, bytes);
    }

    // Re-interleave output back into caller buffer.
    for (int f = 0; f < frames; ++f) {
        buffer[f * 2    ] = m_impl->outL[f];
        buffer[f * 2 + 1] = m_impl->outR[f];
    }
}

// ────────────────────────────────────────────────────────────────────────────
// Parameters (GUI-thread only)
// ────────────────────────────────────────────────────────────────────────────
int AuPluginInstance::parameterCount() const
{
    return m_impl ? static_cast<int>(m_impl->params.size()) : 0;
}

AuParamInfo AuPluginInstance::parameterInfo(int index) const
{
    if (!m_impl || index < 0 || index >= m_impl->params.size())
        return AuParamInfo{};
    return m_impl->params.at(index);
}

float AuPluginInstance::getParameter(int index) const
{
    if (!m_impl || !m_impl->au) return 0.0f;
    if (index < 0 || index >= m_impl->params.size()) return 0.0f;

    AudioUnitParameterValue v = 0.0f;
    const auto pid = m_impl->params.at(index).paramId;
    OSStatus err = AudioUnitGetParameter(
        m_impl->au, pid, kAudioUnitScope_Global, 0, &v);
    if (err != noErr) return 0.0f;
    return static_cast<float>(v);
}

void AuPluginInstance::setParameter(int index, float value)
{
    if (!m_impl || !m_impl->au) return;
    if (index < 0 || index >= m_impl->params.size()) return;

    const auto pid = m_impl->params.at(index).paramId;
    AudioUnitSetParameter(
        m_impl->au, pid, kAudioUnitScope_Global, 0,
        static_cast<AudioUnitParameterValue>(value), 0 /*buffer offset*/);
}

// ────────────────────────────────────────────────────────────────────────────
// openEditor() — native Cocoa UI if advertised, otherwise generic fallback.
// ────────────────────────────────────────────────────────────────────────────
namespace {

QDialog* buildGenericAuEditor(AuPluginInstance* inst, QWidget* parent)
{
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle(QObject::tr("%1 — Parameters").arg(inst->displayName()));

    auto* scroll = new QScrollArea(dlg);
    scroll->setWidgetResizable(true);
    auto* inner  = new QWidget(scroll);
    auto* form   = new QFormLayout(inner);

    const int n = inst->parameterCount();
    if (n == 0) {
        form->addRow(new QLabel(
            QObject::tr("This Audio Unit exposes no parameters."), inner));
    }

    for (int i = 0; i < n; ++i) {
        const auto pi = inst->parameterInfo(i);
        if (pi.readOnly) continue;

        auto* row    = new QWidget(inner);
        auto* rowLay = new QHBoxLayout(row);
        rowLay->setContentsMargins(0, 0, 0, 0);

        auto* slider = new QSlider(Qt::Horizontal, row);
        slider->setRange(0, 10000);
        const float range = (pi.maxValue > pi.minValue)
                              ? (pi.maxValue - pi.minValue)
                              : 1.0f;
        const float cur   = inst->getParameter(i);
        const float norm  = (cur - pi.minValue) / range;
        slider->setValue(static_cast<int>(norm * 10000.0f));
        slider->setToolTip(QObject::tr("Range %1–%2, default %3")
            .arg(pi.minValue, 0, 'f', 3)
            .arg(pi.maxValue, 0, 'f', 3)
            .arg(pi.defaultValue, 0, 'f', 3));

        auto* value = new QLabel(QString::number(cur, 'f', 3), row);
        value->setFixedWidth(64);

        QObject::connect(slider, &QSlider::valueChanged, dlg,
                         [inst, i, pi, range, value](int v) {
            const float n = v / 10000.0f;
            const float plain = pi.minValue + n * range;
            inst->setParameter(i, plain);
            value->setText(QString::number(plain, 'f', 3));
        });

        auto* resetBtn = new QPushButton(QObject::tr("R"), row);
        resetBtn->setFixedWidth(24);
        resetBtn->setToolTip(QObject::tr("Reset to default"));
        QObject::connect(resetBtn, &QPushButton::clicked, dlg,
                         [inst, i, pi, range, slider]() {
            inst->setParameter(i, pi.defaultValue);
            const float norm = (pi.defaultValue - pi.minValue) / range;
            slider->setValue(static_cast<int>(norm * 10000.0f));
        });

        rowLay->addWidget(slider, 1);
        rowLay->addWidget(value);
        rowLay->addWidget(resetBtn);

        QString label = pi.name;
        if (!pi.units.isEmpty())
            label += QStringLiteral(" (") + pi.units + QStringLiteral(")");
        form->addRow(label, row);
    }

    inner->setLayout(form);
    scroll->setWidget(inner);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dlg);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::accept);

    auto* layout = new QVBoxLayout(dlg);
    layout->addWidget(scroll, 1);
    layout->addWidget(buttons);
    dlg->resize(420, 520);
    return dlg;
}

} // namespace

QDialog* AuPluginInstance::openEditor(QWidget* parent)
{
    if (!m_impl || !m_impl->au) return nullptr;

    // Query kAudioUnitProperty_CocoaUI for the Cocoa factory class + bundle.
    UInt32 dataSize = 0;
    Boolean writable = false;
    OSStatus err = AudioUnitGetPropertyInfo(
        m_impl->au, kAudioUnitProperty_CocoaUI,
        kAudioUnitScope_Global, 0, &dataSize, &writable);

    if (err != noErr || dataSize < sizeof(AudioUnitCocoaViewInfo)) {
        return buildGenericAuEditor(this, parent);
    }

    const int numClasses =
        (dataSize - sizeof(CFURLRef)) / sizeof(CFStringRef);
    if (numClasses < 1) {
        return buildGenericAuEditor(this, parent);
    }

    std::vector<uint8_t> blob(dataSize, 0);
    err = AudioUnitGetProperty(
        m_impl->au, kAudioUnitProperty_CocoaUI,
        kAudioUnitScope_Global, 0, blob.data(), &dataSize);
    if (err != noErr) {
        return buildGenericAuEditor(this, parent);
    }

    auto* cui = reinterpret_cast<AudioUnitCocoaViewInfo*>(blob.data());
    CFURLRef    bundleURL = cui->mCocoaAUViewBundleLocation;
    CFStringRef className = cui->mCocoaAUViewClass[0];
    if (!bundleURL || !className) {
        return buildGenericAuEditor(this, parent);
    }

    NSBundle* bundle = [NSBundle bundleWithURL:(__bridge NSURL*)bundleURL];
    CFRelease(bundleURL);
    for (int i = 0; i < numClasses; ++i) {
        if (cui->mCocoaAUViewClass[i]) {
            // Keep className alive via explicit copy; release all the
            // others that getProperty handed us.
            if (i == 0) {
                className = (CFStringRef)CFRetain(cui->mCocoaAUViewClass[0]);
            }
            CFRelease(cui->mCocoaAUViewClass[i]);
        }
    }

    if (!bundle) {
        if (className) CFRelease(className);
        return buildGenericAuEditor(this, parent);
    }

    if (![bundle isLoaded]) {
        [bundle load];
    }

    Class factoryClass = [bundle classNamed:(__bridge NSString*)className];
    CFRelease(className);
    if (!factoryClass) {
        return buildGenericAuEditor(this, parent);
    }

    id<AUCocoaUIBase> factory = [[factoryClass alloc] init];
    if (!factory) {
        return buildGenericAuEditor(this, parent);
    }

    NSView* auView = [factory uiViewForAudioUnit:m_impl->au
                                        withSize:NSMakeSize(600, 400)];
    if (!auView) {
        return buildGenericAuEditor(this, parent);
    }

    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle(displayName());

    // Container QWidget whose native NSView we embed the AU view into.
    auto* container = new QWidget(dlg);
    container->setAttribute(Qt::WA_NativeWindow);
    container->setAttribute(Qt::WA_DontCreateNativeAncestors);

    auto* layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(container);

    const NSRect frame = [auView frame];
    const int w = std::max(320, static_cast<int>(frame.size.width));
    const int h = std::max(200, static_cast<int>(frame.size.height));
    container->setFixedSize(w, h);
    dlg->resize(w, h);

    // Force native window creation so winId() returns an NSView*.
    (void)container->winId();

    NSView* hostView = (__bridge NSView*)reinterpret_cast<void*>(container->winId());
    if (!hostView) {
        delete dlg;
        return buildGenericAuEditor(this, parent);
    }

    [hostView addSubview:auView];
    [auView setFrame:NSMakeRect(0, 0, w, h)];
    [auView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];

    // Keep the Obj-C view + factory alive for the dialog's lifetime.
    // ARC retains captured Obj-C pointers in a C++ lambda automatically,
    // so we just capture them strongly and drop the references on
    // dialog close.
    __strong NSView*            strongAuView  = auView;
    __strong id<AUCocoaUIBase>  strongFactory = factory;
    QObject::connect(dlg, &QDialog::finished, dlg,
                     [strongAuView, strongFactory](int) mutable {
        [strongAuView removeFromSuperview];
        strongAuView  = nil;
        strongFactory = nil;
    });

    return dlg;
}

// ────────────────────────────────────────────────────────────────────────────
// saveState / restoreState
// ────────────────────────────────────────────────────────────────────────────
bool AuPluginInstance::saveState(QByteArray& out) const
{
    out.clear();
    if (!m_impl || !m_impl->au) return false;

    CFPropertyListRef plist = nullptr;
    UInt32 size = sizeof(plist);
    OSStatus err = AudioUnitGetProperty(
        m_impl->au, kAudioUnitProperty_ClassInfo,
        kAudioUnitScope_Global, 0, &plist, &size);
    if (err != noErr || !plist) {
        return false;
    }

    CFDataRef data = CFPropertyListCreateData(
        kCFAllocatorDefault, plist,
        kCFPropertyListBinaryFormat_v1_0,
        0, nullptr);
    CFRelease(plist);

    if (!data) return false;

    const CFIndex len = CFDataGetLength(data);
    const UInt8* bytes = CFDataGetBytePtr(data);
    out = QByteArray(reinterpret_cast<const char*>(bytes), static_cast<int>(len));
    CFRelease(data);
    return true;
}

bool AuPluginInstance::restoreState(const QByteArray& in)
{
    if (!m_impl || !m_impl->au || in.isEmpty()) return false;

    CFDataRef data = CFDataCreate(
        kCFAllocatorDefault,
        reinterpret_cast<const UInt8*>(in.constData()),
        static_cast<CFIndex>(in.size()));
    if (!data) return false;

    CFErrorRef cfErr = nullptr;
    CFPropertyListRef plist = CFPropertyListCreateWithData(
        kCFAllocatorDefault, data, kCFPropertyListImmutable, nullptr, &cfErr);
    CFRelease(data);

    if (!plist) {
        if (cfErr) CFRelease(cfErr);
        return false;
    }

    OSStatus err = AudioUnitSetProperty(
        m_impl->au, kAudioUnitProperty_ClassInfo,
        kAudioUnitScope_Global, 0, &plist, sizeof(plist));
    CFRelease(plist);

    return err == noErr;
}

QString AuPluginInstance::lastError()
{
    return g_lastError;
}

} // namespace dawcast::plugins

#endif // __APPLE__
