/*
 * Mcaster1PatchBay — DSP Effects Engine
 * dsp/preset_manager.h — Load/save/list YAML presets per effect
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Preset storage layout:
 *   ~/.mcaster1/Mcaster1DAWCast/presets/<effect-id>/
 *   ├── factory/         ← shipped defaults (read-only by convention)
 *   │   ├── flat.yaml
 *   │   └── warm.yaml
 *   └── custom/          ← user-created presets
 *       └── my-show.yaml
 */

#pragma once

#include "dsp_effect.h"

#include <QString>
#include <QVector>
#include <QMap>

namespace mc1dsp {

/* ── Preset data ────────────────────────────────────────────────── */

struct Preset {
    QString name;           // "Broadcast Voice"
    QString effectId;       // "mc1.eq.parametric10"
    QString version;        // "1.0.0"
    QString author;         // "Mcaster1" or "User"
    QString filePath;       // full path to .yaml on disk
    bool    isFactory = false;
    QMap<QString, float> parameters;  // param_name → normalized value (0-1)
};

/* ── PresetManager — static utility class ───────────────────────── */

class PresetManager {
public:
    /* Base directory for all presets (~/.mcaster1/Mcaster1DAWCast/presets/) */
    static QString presetsDir();

    /* List all presets for an effect (factory first, then custom, alphabetical) */
    static QVector<Preset> listPresets(const QString& effectId);

    /* Read a single YAML file into a Preset struct */
    static Preset loadPreset(const QString& filePath);

    /* Write a preset to disk (custom/ directory). Returns true on success. */
    static bool savePreset(const Preset& preset);

    /* Delete a custom preset. Refuses to delete factory presets. */
    static bool deletePreset(const QString& filePath);

    /* Set all parameters on an effect from a preset's parameter map */
    static void applyPreset(const Preset& preset, DspEffect* fx);

    /* Capture the current state of an effect into a Preset struct */
    static Preset capturePreset(const DspEffect* fx, const QString& name);

    /* Create factory presets for all known effects (skips existing files) */
    static void ensureFactoryPresets();

    /* Convert a human-readable name to a filesystem-safe slug */
    static QString slugify(const QString& name);

    /* Write a Preset to a specific file path (factory or custom) */
    static bool writePresetFile(const Preset& preset, const QString& path);

private:
    PresetManager() = default;

    /* Internal: create factory presets for a specific effect */
    static void createFactoryPresetsForEffect(const QString& effectId);

    /* Internal: top-up any plugin below 10 factory presets with
     * category-aware generated variants. Called from the end of
     * createFactoryPresetsForEffect. */
    static void fillThematicPresetsTo10(const QString& effectId,
                                         DspEffect* defaultsFx,
                                         const QString& version);

    /* Internal: helper to build a preset from an effect with given param map */
    static Preset buildPreset(const QString& name,
                              const QString& effectId,
                              const QString& version,
                              const QMap<QString, float>& params,
                              bool factory);
};

} // namespace mc1dsp
