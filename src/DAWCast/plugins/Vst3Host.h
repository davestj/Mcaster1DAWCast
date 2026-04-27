// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Vst3Host
// ────────
// VST3 bundle inspector + live plugin host built on the Steinberg VST3
// SDK's VST3::Hosting::Module helper.
//
// Phase 1 (shipped): opens the bundle, reads the plugin factory,
//                    enumerates audio-effect classes, exposes metadata.
// Phase 2 (shipped): instantiates IComponent + IAudioProcessor, activates
//                    stereo bus arrangements, processes audio RT-safely.
// Phase 3 (this pass): IEditController parameter bridge, lock-free
//                    parameter-change queue, IPlugView editor hosting,
//                    IComponent / IEditController state save/load.
//
// Thread safety: bundle loading / parameter queries / editor creation
// happen on the GUI thread. The audio thread calls process() only.
// Parameter writes from GUI are queued into a lock-free ring buffer that
// the audio thread drains into ProcessData::inputParameterChanges.

#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QByteArray>
#include <memory>

class QWidget;
class QDialog;

namespace dawcast::plugins {

/// Metadata for a single audio-effect class inside a VST3 bundle.
struct Vst3ClassInfo {
    QString name;
    QString category;         // "Audio Module Class" etc.
    QString subCategories;    // "Fx|Modulation" etc.
    QString vendor;
    QString version;
    QString sdkVersion;
    QString cid;              // class UID hex string
};

struct Vst3BundleInfo {
    QString path;
    QString name;
    bool isBundle = false;
    QList<Vst3ClassInfo> classes;
    QString error;            // populated if load failed
};

/// Parameter metadata surfaced from the plugin's IEditController.
/// All values are in the VST3 normalized [0,1] domain for the raw
/// parameter. The host is responsible for converting to/from plain
/// units through the controller when it needs a display value.
struct Vst3ParameterInfo {
    quint32 id = 0;           ///< VST3 ParamID (opaque, plugin-defined)
    QString title;            ///< e.g. "Gain"
    QString shortTitle;       ///< e.g. "Gn"
    QString units;            ///< e.g. "dB"
    double  defaultNormalized = 0.0;
    int     stepCount = 0;    ///< 0 = continuous, 1 = toggle, >1 = discrete
    bool    canAutomate = true;
    bool    readOnly = false;
    bool    isBypass = false;
};

/// Attempts to load the VST3 bundle at `path` and return its metadata.
/// Returns a Vst3BundleInfo; check `error.isEmpty()` for success.
/// The Module's dylib mapping is released as soon as this returns —
/// we don't hold instances open from this helper yet.
Vst3BundleInfo inspectVst3Bundle(const QString& path);

/// Live VST3 audio-effect instance.
///
/// Holds the Module (keeps dylib mapped), the plugin's IComponent +
/// IAudioProcessor + IEditController, and preallocated ProcessData
/// buffers so process() is RT-safe. Use create() to load + activate,
/// then process() per block.
class Vst3PluginInstance {
public:
    /// Load the bundle at `path`, instantiate the class with `cid` (32-char
    /// hex), and activate for stereo in/out at the given sample rate.
    /// Returns nullptr on failure (see `lastError()` static for details).
    static std::unique_ptr<Vst3PluginInstance> create(const QString& bundlePath,
                                                       const QString& cidHex,
                                                       double sampleRate,
                                                       int maxFrames);

    ~Vst3PluginInstance();

    /// Display name of the plugin (from its class info).
    QString displayName() const;

    /// Render `frames` of stereo-interleaved float audio in-place. The
    /// buffer is split into stereo deinterleaved in → processor->process()
    /// → interleaved back. channels must be 2.
    void process(float* buffer, int frames, int channels);

    // ── Parameters ─────────────────────────────────────────────────────
    /// Number of parameters exposed by the plugin's IEditController.
    /// Returns 0 if no controller is available.
    int  parameterCount() const;

    /// Lookup by index. Returns an empty info struct if index is out of
    /// range or no controller is available.
    Vst3ParameterInfo parameterInfo(int index) const;

    /// Read the current normalized value for a parameter by index.
    double getParameterNormalized(int index) const;

    /// Enqueue a normalized parameter write. The change is drained by the
    /// audio thread into ProcessData::inputParameterChanges on the next
    /// process() block. Also updates the controller-side cache so the
    /// editor stays in sync.
    void   setParameterNormalized(int index, double value);

    // ── Editor ─────────────────────────────────────────────────────────
    /// Create (or recreate) a QDialog that either embeds the plugin's
    /// native IPlugView editor, or falls back to a generic sliders-based
    /// parameter editor. Ownership is transferred to the caller.
    QDialog* openEditor(QWidget* parent);

    // ── State ──────────────────────────────────────────────────────────
    /// Serialize IComponent::getState + IEditController::getState into
    /// a self-describing blob. Returns true on success.
    bool saveState(QByteArray& out) const;

    /// Restore a blob produced by saveState(). Returns true on success.
    bool restoreState(const QByteArray& in);

    /// Last error from the most recent create() call (static because the
    /// instance may not exist on failure).
    static QString lastError();

    // Implementation holder — public so SDK-side callbacks inside the .cpp
    // can touch the queues without forward-declaration gymnastics. Actual
    // struct definition is private to Vst3Host.cpp.
    struct Impl;

private:
    Vst3PluginInstance();
    std::unique_ptr<Impl> m_impl;
};

} // namespace dawcast::plugins
