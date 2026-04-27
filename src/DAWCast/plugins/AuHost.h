// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
//
// AuHost
// ──────
// Audio Unit (v2) inspector + live plugin host built on the Apple
// AudioToolbox / AudioUnit C APIs. Parallels Vst3Host.h.
//
// Phase 1 (this pass): inspect an installed AudioComponent and return
//                    metadata.  Instantiate a v2 effect unit, set a
//                    stereo non-interleaved float stream format, route
//                    audio through AudioUnitRender via a pull-style
//                    input render callback, enumerate and edit
//                    parameters, host the Cocoa UI (or a generic
//                    sliders fallback) inside a QDialog, and save /
//                    restore full classInfo state.
//
// Thread safety: bundle inspection / parameter queries / editor creation
// happen on the GUI thread. The audio thread calls process() only.
// AudioUnitRender is RT-safe.  Parameter writes from the GUI go through
// AudioUnitSetParameter which is documented as RT-safe at global scope.

#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QByteArray>
#include <memory>
#include <cstdint>

#ifdef __APPLE__
#include <AudioToolbox/AudioToolbox.h>
#endif

class QWidget;
class QDialog;

namespace dawcast::plugins {

/// Metadata for a single installed Audio Unit (effect or otherwise).
struct AuClassInfo {
    QString name;            ///< "Foo Reverb" (from AudioComponentCopyName)
    QString vendor;          ///< "Acme Audio" (split from name or from Info.plist)
    QString subCategories;   ///< "Effect|aufx|..." best-effort tag string
    QString version;         ///< "1.2.3" (from AudioComponent version)
    QString cid;             ///< "aufx:rev1:Acme" 4CC triple as a display id
    uint32_t type         = 0;  ///< componentType,       e.g. 'aufx'
    uint32_t subtype      = 0;  ///< componentSubType
    uint32_t manufacturer = 0;  ///< componentManufacturer
};

struct AuBundleInfo {
    QString path;
    QString name;
    bool    isComponent = false;
    QList<AuClassInfo> classes;
    QString error;           ///< populated if load failed
};

/// Parameter metadata surfaced from the Audio Unit's parameter list.
struct AuParamInfo {
    uint32_t paramId   = 0;   ///< AudioUnitParameterID (opaque, AU-defined)
    QString  name;            ///< e.g. "Gain"
    QString  units;            ///< e.g. "dB", empty if generic
    float    minValue     = 0.0f;
    float    maxValue     = 1.0f;
    float    defaultValue = 0.0f;
    bool     canAutomate  = true;
    bool     readOnly     = false;
};

#ifdef __APPLE__

/// Attempts to describe an installed AudioComponent and return its
/// metadata.  Returns an AuBundleInfo; check `error.isEmpty()` for
/// success.  Does not hold the unit open — the instance is discarded
/// as soon as this returns.
AuBundleInfo inspectAuComponent(const AudioComponentDescription& desc);

#endif // __APPLE__

/// Live Audio Unit (v2) audio-effect instance.
///
/// Holds an AudioUnit, the preallocated non-interleaved input / output
/// buffers and an input render callback that supplies the current block
/// to AudioUnitRender.  Use create() to open + activate, then process()
/// per block.
class AuPluginInstance {
public:
#ifdef __APPLE__
    /// Open the installed AudioUnit matching `desc`, set stereo float
    /// non-interleaved stream format, install an input render callback,
    /// and call AudioUnitInitialize.  Returns nullptr on failure (see
    /// `lastError()` static for details).
    static std::unique_ptr<AuPluginInstance> create(
        const AudioComponentDescription& desc,
        double sampleRate,
        int maxFrames);
#endif

    ~AuPluginInstance();

    /// Display name of the plugin (from AudioComponentCopyName).
    QString displayName() const;

    /// Render `frames` of stereo-interleaved float audio in-place.
    /// The buffer is deinterleaved into per-channel storage, the input
    /// render callback hands those buffers to the AU, AudioUnitRender
    /// writes the AU's output into our output buffers, and we
    /// reinterleave back.  channels must be 2.
    void process(float* buffer, int frames, int channels);

    // ── Parameters ─────────────────────────────────────────────────────
    /// Number of parameters surfaced by the AU.
    int parameterCount() const;

    /// Lookup by index.  Returns an empty info struct if index is out
    /// of range.
    AuParamInfo parameterInfo(int index) const;

    /// Read the current value for a parameter by index (global scope).
    float getParameter(int index) const;

    /// Write a parameter (global scope).  AudioUnitSetParameter is
    /// documented RT-safe so we go straight through — no ring buffer.
    void  setParameter(int index, float value);

    // ── Editor ─────────────────────────────────────────────────────────
    /// Create a QDialog that either embeds the plugin's Cocoa UI (via
    /// kAudioUnitProperty_CocoaUI), or falls back to a generic
    /// sliders-based parameter editor built from parameterInfo().
    /// Ownership is transferred to the caller.
    QDialog* openEditor(QWidget* parent);

    // ── State ──────────────────────────────────────────────────────────
    /// Serialize kAudioUnitProperty_ClassInfo into a self-describing
    /// binary-plist blob.  Returns true on success.
    bool saveState(QByteArray& out) const;

    /// Restore a blob produced by saveState().  Returns true on success.
    bool restoreState(const QByteArray& in);

    /// Last error from the most recent create() call.
    static QString lastError();

    // Implementation holder — public so the Objective-C++ callback
    // inside the .mm can reach into the buffers without forward-decl
    // gymnastics.
    struct Impl;

private:
    AuPluginInstance();
    std::unique_ptr<Impl> m_impl;
};

} // namespace dawcast::plugins
