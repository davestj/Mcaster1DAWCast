/*
 * Mcaster1PatchBay — DSP Effects Engine
 * dsp/preset_manager.cpp — YAML preset load/save/factory generation
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "preset_manager.h"
#include "effect_factory.h"
#include "../../config/AppConfig.h"

#include <yaml-cpp/yaml.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTextStream>

#include <fstream>
#include <algorithm>

namespace mc1dsp {

/* ═══════════════════════════════════════════════════════════════════
 *  Directory helpers
 * ═══════════════════════════════════════════════════════════════════ */

QString PresetManager::presetsDir()
{
    return dawcast::config::AppConfig::presetsDir();
}

QString PresetManager::slugify(const QString& name)
{
    QString slug = name.toLower().trimmed();
    QString result;
    result.reserve(slug.size());

    for (const QChar& ch : slug) {
        if (ch.isLetterOrNumber())
            result.append(ch);
        else if (ch == QLatin1Char(' ') || ch == QLatin1Char('_'))
            result.append(QLatin1Char('-'));
        /* Drop everything else (punctuation, special chars) */
    }

    /* Collapse consecutive hyphens */
    while (result.contains(QStringLiteral("--")))
        result.replace(QStringLiteral("--"), QStringLiteral("-"));

    /* Trim leading/trailing hyphens */
    while (result.startsWith(QLatin1Char('-')))
        result.remove(0, 1);
    while (result.endsWith(QLatin1Char('-')))
        result.chop(1);

    return result.isEmpty() ? QStringLiteral("preset") : result;
}

/* ═══════════════════════════════════════════════════════════════════
 *  List presets (factory first, then custom, alphabetical within)
 * ═══════════════════════════════════════════════════════════════════ */

QVector<Preset> PresetManager::listPresets(const QString& effectId)
{
    QVector<Preset> factory, custom;
    QString base = presetsDir() + QStringLiteral("/") + effectId;

    /* Scan factory/ */
    QDir factoryDir(base + QStringLiteral("/factory"));
    if (factoryDir.exists()) {
        const QFileInfoList files = factoryDir.entryInfoList(
            QStringList() << QStringLiteral("*.yaml") << QStringLiteral("*.yml"),
            QDir::Files, QDir::Name);
        for (const QFileInfo& fi : files) {
            Preset p = loadPreset(fi.absoluteFilePath());
            if (!p.name.isEmpty()) {
                p.isFactory = true;
                factory.append(p);
            }
        }
    }

    /* Scan custom/ */
    QDir customDir(base + QStringLiteral("/custom"));
    if (customDir.exists()) {
        const QFileInfoList files = customDir.entryInfoList(
            QStringList() << QStringLiteral("*.yaml") << QStringLiteral("*.yml"),
            QDir::Files, QDir::Name);
        for (const QFileInfo& fi : files) {
            Preset p = loadPreset(fi.absoluteFilePath());
            if (!p.name.isEmpty()) {
                p.isFactory = false;
                custom.append(p);
            }
        }
    }

    /* Sort each group alphabetically by name */
    auto byName = [](const Preset& a, const Preset& b) {
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    };
    std::sort(factory.begin(), factory.end(), byName);
    std::sort(custom.begin(), custom.end(), byName);

    /* Factory first, then custom */
    QVector<Preset> result;
    result.reserve(factory.size() + custom.size());
    result.append(factory);
    result.append(custom);
    return result;
}

/* ═══════════════════════════════════════════════════════════════════
 *  Load a YAML preset file
 * ═══════════════════════════════════════════════════════════════════ */

Preset PresetManager::loadPreset(const QString& filePath)
{
    Preset p;
    p.filePath = filePath;

    /* Determine factory vs custom from path */
    p.isFactory = filePath.contains(QStringLiteral("/factory/"));

    try {
        YAML::Node doc = YAML::LoadFile(filePath.toStdString());

        if (doc["name"])
            p.name = QString::fromStdString(doc["name"].as<std::string>());
        if (doc["effect_id"])
            p.effectId = QString::fromStdString(doc["effect_id"].as<std::string>());
        if (doc["version"])
            p.version = QString::fromStdString(doc["version"].as<std::string>());
        if (doc["author"])
            p.author = QString::fromStdString(doc["author"].as<std::string>());

        if (doc["parameters"] && doc["parameters"].IsMap()) {
            for (auto it = doc["parameters"].begin();
                 it != doc["parameters"].end(); ++it)
            {
                QString key = QString::fromStdString(it->first.as<std::string>());
                float   val = it->second.as<float>(0.0f);
                /* Clamp to valid normalized range */
                if (val < 0.0f) val = 0.0f;
                if (val > 1.0f) val = 1.0f;
                p.parameters.insert(key, val);
            }
        }
    } catch (const YAML::Exception& /*e*/) {
        /* Return empty preset on parse failure — caller checks name */
        p.name.clear();
    }

    return p;
}

/* ═══════════════════════════════════════════════════════════════════
 *  Save a preset (always to the custom/ directory)
 * ═══════════════════════════════════════════════════════════════════ */

bool PresetManager::savePreset(const Preset& preset)
{
    if (preset.name.isEmpty() || preset.effectId.isEmpty())
        return false;

    QString dir = presetsDir() + QStringLiteral("/")
                + preset.effectId + QStringLiteral("/custom");
    QDir().mkpath(dir);

    QString filename = slugify(preset.name) + QStringLiteral(".yaml");
    QString path = dir + QStringLiteral("/") + filename;

    return writePresetFile(preset, path);
}

/* ═══════════════════════════════════════════════════════════════════
 *  Delete a custom preset (refuses factory presets)
 * ═══════════════════════════════════════════════════════════════════ */

bool PresetManager::deletePreset(const QString& filePath)
{
    if (filePath.contains(QStringLiteral("/factory/")))
        return false;  /* Never delete factory presets */

    QFile file(filePath);
    return file.exists() && file.remove();
}

/* ═══════════════════════════════════════════════════════════════════
 *  Apply preset parameters to a live effect
 * ═══════════════════════════════════════════════════════════════════ */

void PresetManager::applyPreset(const Preset& preset, DspEffect* fx)
{
    if (!fx) return;

    for (int i = 0; i < fx->paramCount(); ++i) {
        QString pname = QString::fromUtf8(fx->paramName(i));
        auto it = preset.parameters.find(pname);
        if (it != preset.parameters.end()) {
            fx->setParamValue(i, it.value());
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════
 *  Capture current effect state into a Preset
 * ═══════════════════════════════════════════════════════════════════ */

Preset PresetManager::capturePreset(const DspEffect* fx, const QString& name)
{
    Preset p;
    if (!fx) return p;

    p.name     = name;
    p.effectId = QString::fromUtf8(fx->id());
    p.version  = QString::fromUtf8(fx->version());
    p.author   = QStringLiteral("User");
    p.isFactory = false;

    for (int i = 0; i < fx->paramCount(); ++i) {
        QString pname = QString::fromUtf8(fx->paramName(i));
        p.parameters.insert(pname, fx->paramValue(i));
    }

    return p;
}

/* ═══════════════════════════════════════════════════════════════════
 *  Write a Preset to a specific file path (YAML emitter)
 * ═══════════════════════════════════════════════════════════════════ */

bool PresetManager::writePresetFile(const Preset& preset, const QString& path)
{
    /* Ensure parent directory exists */
    QFileInfo fi(path);
    QDir().mkpath(fi.absolutePath());

    try {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "name"      << YAML::Value << preset.name.toStdString();
        out << YAML::Key << "effect_id" << YAML::Value << preset.effectId.toStdString();
        out << YAML::Key << "version"   << YAML::Value << preset.version.toStdString();
        out << YAML::Key << "author"    << YAML::Value << preset.author.toStdString();

        out << YAML::Key << "parameters" << YAML::Value;
        out << YAML::BeginMap;
        for (auto it = preset.parameters.constBegin();
             it != preset.parameters.constEnd(); ++it)
        {
            out << YAML::Key   << it.key().toStdString();
            out << YAML::Value << YAML::FloatPrecision(4) << it.value();
        }
        out << YAML::EndMap;
        out << YAML::EndMap;

        /* Atomic write: write to temp file, then rename */
        QString tmpPath = path + QStringLiteral(".tmp");
        {
            std::ofstream fout(tmpPath.toStdString());
            if (!fout.is_open())
                return false;
            fout << out.c_str() << std::endl;
            fout.close();
            if (fout.fail())
                return false;
        }

        /* Remove existing, rename temp */
        QFile::remove(path);
        return QFile::rename(tmpPath, path);

    } catch (const YAML::Exception& /*e*/) {
        return false;
    }
}

/* ═══════════════════════════════════════════════════════════════════
 *  Build a Preset struct from raw data (helper)
 * ═══════════════════════════════════════════════════════════════════ */

Preset PresetManager::buildPreset(const QString& name,
                                  const QString& effectId,
                                  const QString& version,
                                  const QMap<QString, float>& params,
                                  bool factory)
{
    Preset p;
    p.name       = name;
    p.effectId   = effectId;
    p.version    = version;
    p.author     = factory ? QStringLiteral("Mcaster1") : QStringLiteral("User");
    p.isFactory  = factory;
    p.parameters = params;
    return p;
}

/* ═══════════════════════════════════════════════════════════════════
 *  Factory preset generation
 * ═══════════════════════════════════════════════════════════════════ */

void PresetManager::ensureFactoryPresets()
{
    auto effects = EffectFactory::availableEffects();
    for (const auto& info : effects) {
        createFactoryPresetsForEffect(QString::fromUtf8(info.id));
    }
}

/*
 * Helper: capture defaults from a freshly-constructed effect,
 * then write the factory YAML file (only if it does not already exist).
 */
static bool writeFactoryIfMissing(const QString& effectId,
                                  const QString& name,
                                  const QMap<QString, float>& params,
                                  const QString& version)
{
    QString dir = PresetManager::presetsDir() + QStringLiteral("/")
                + effectId + QStringLiteral("/factory");
    QDir().mkpath(dir);

    QString slug = PresetManager::slugify(name);
    QString path = dir + QStringLiteral("/") + slug + QStringLiteral(".yaml");

    if (QFile::exists(path))
        return true;  /* Already exists — don't overwrite */

    Preset p;
    p.name       = name;
    p.effectId   = effectId;
    p.version    = version;
    p.author     = QStringLiteral("Mcaster1");
    p.isFactory  = true;
    p.filePath   = path;
    p.parameters = params;

    return PresetManager::writePresetFile(p, path);
}

/*
 * Helper: capture the default (constructor) state of an effect.
 */
static QMap<QString, float> captureDefaults(DspEffect* fx)
{
    QMap<QString, float> params;
    for (int i = 0; i < fx->paramCount(); ++i)
        params.insert(QString::fromUtf8(fx->paramName(i)), fx->paramValue(i));
    return params;
}

/*
 * Helper: capture state after setting specific parameters.
 * Takes pairs of (paramIndex, normalizedValue) to override.
 */
static QMap<QString, float> captureWithOverrides(
    DspEffect* fx,
    const std::initializer_list<std::pair<int, float>>& overrides)
{
    for (const auto& [idx, val] : overrides)
        fx->setParamValue(idx, val);

    QMap<QString, float> params;
    for (int i = 0; i < fx->paramCount(); ++i)
        params.insert(QString::fromUtf8(fx->paramName(i)), fx->paramValue(i));
    return params;
}

/* ─── Per-effect factory preset definitions ─────────────────────── */

void PresetManager::createFactoryPresetsForEffect(const QString& effectId)
{
    auto fx = EffectFactory::create(effectId.toStdString());
    if (!fx) return;

    QString ver = QString::fromUtf8(fx->version());
    std::string eid = effectId.toStdString();

    /* ────────────────────────────────────────────────────────────────
     * mc1.eq.parametric10 — 10-Band Parametric EQ
     * Params (0-9): "80 Hz", "150 Hz", "400 Hz", "800 Hz", "1.5 kHz",
     *               "3 kHz", "5 kHz", "8 kHz", "12 kHz", "16 kHz"
     * Normalized: 0.5 = 0 dB, 0 = -24 dB, 1 = +24 dB
     * ──────────────────────────────────────────────────────────────── */
    if (eid == "mc1.eq.parametric10") {

        /* Flat — all bands at 0 dB (constructor defaults) */
        writeFactoryIfMissing(effectId, QStringLiteral("Flat"),
                              captureDefaults(fx.get()), ver);

        /* Broadcast Voice — presence boost at 3k/5k, cut rumble at 80Hz */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Broadcast Voice"),
            captureWithOverrides(fx.get(), {
                {0, 0.4375f},   /* 80 Hz:   -3.0 dB  (0.4375 = (-3+24)/48) */
                {1, 0.4792f},   /* 150 Hz:  -1.0 dB  */
                {2, 0.5f},      /* 400 Hz:   0.0 dB  */
                {3, 0.5f},      /* 800 Hz:   0.0 dB  */
                {4, 0.5208f},   /* 1.5 kHz: +1.0 dB  */
                {5, 0.5625f},   /* 3 kHz:   +3.0 dB  */
                {6, 0.5417f},   /* 5 kHz:   +2.0 dB  */
                {7, 0.5208f},   /* 8 kHz:   +1.0 dB  */
                {8, 0.5f},      /* 12 kHz:   0.0 dB  */
                {9, 0.4792f},   /* 16 kHz:  -1.0 dB  */
            }), ver);

        /* Warm Music — low shelf boost, high shelf cut */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Warm Music"),
            captureWithOverrides(fx.get(), {
                {0, 0.5625f},   /* 80 Hz:   +3.0 dB  */
                {1, 0.5417f},   /* 150 Hz:  +2.0 dB  */
                {2, 0.5208f},   /* 400 Hz:  +1.0 dB  */
                {3, 0.5f},      /* 800 Hz:   0.0 dB  */
                {4, 0.5f},      /* 1.5 kHz:  0.0 dB  */
                {5, 0.5f},      /* 3 kHz:    0.0 dB  */
                {6, 0.4792f},   /* 5 kHz:   -1.0 dB  */
                {7, 0.4583f},   /* 8 kHz:   -2.0 dB  */
                {8, 0.4375f},   /* 12 kHz:  -3.0 dB  */
                {9, 0.4375f},   /* 16 kHz:  -3.0 dB  */
            }), ver);
    }

    /* ────────────────────────────────────────────────────────────────
     * mc1.eq.dual15 — Dual 15-Band EQ (L/R independent)
     * Params 0-14 = L bands, 15-29 = R bands
     * Normalized: 0.5 = 0 dB, 0 = -12 dB, 1 = +12 dB
     * ──────────────────────────────────────────────────────────────── */
    else if (eid == "mc1.eq.dual15") {

        /* Flat */
        writeFactoryIfMissing(effectId, QStringLiteral("Flat"),
                              captureDefaults(fx.get()), ver);

        /* Mic L / Music R — L voiced for speech, R gentle bass boost */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Mic L / Music R"),
            captureWithOverrides(fx.get(), {
                /* Left (speech): cut lows, presence boost */
                {0,  0.375f},   /* L 25 Hz:   -3 dB */
                {1,  0.375f},   /* L 40 Hz:   -3 dB */
                {2,  0.4167f},  /* L 63 Hz:   -2 dB */
                {3,  0.4583f},  /* L 100 Hz:  -1 dB */
                {8,  0.5417f},  /* L 1 kHz:   +1 dB */
                {9,  0.5833f},  /* L 1.6 kHz: +2 dB */
                {10, 0.625f},   /* L 2.5 kHz: +3 dB */
                {11, 0.5833f},  /* L 4 kHz:   +2 dB */
                {12, 0.5417f},  /* L 6.3 kHz: +1 dB */
                /* Right (music): gentle low-end warmth */
                {15, 0.5833f},  /* R 25 Hz:   +2 dB */
                {16, 0.5833f},  /* R 40 Hz:   +2 dB */
                {17, 0.5417f},  /* R 63 Hz:   +1 dB */
                {18, 0.5417f},  /* R 100 Hz:  +1 dB */
            }), ver);
    }

    /* ────────────────────────────────────────────────────────────────
     * mc1.eq.graphic31 — 31-Band Graphic EQ
     * Normalized: 0.5 = 0 dB, 0 = -12 dB, 1 = +12 dB
     * ──────────────────────────────────────────────────────────────── */
    else if (eid == "mc1.eq.graphic31") {

        /* Flat */
        writeFactoryIfMissing(effectId, QStringLiteral("Flat"),
                              captureDefaults(fx.get()), ver);

        /* Broadcast Polish — gentle smile curve (low/high subtle boost, mid flat) */
        fx = EffectFactory::create(eid);
        {
            /* Build a gentle smile: +2 dB at ends, 0 dB in middle */
            std::initializer_list<std::pair<int, float>> overrides = {
                { 0, 0.5833f},  /* 20 Hz:     +2 dB */
                { 1, 0.5833f},  /* 25 Hz:     +2 dB */
                { 2, 0.5625f},  /* 31.5 Hz:  +1.5 dB */
                { 3, 0.5417f},  /* 40 Hz:     +1 dB */
                { 4, 0.5208f},  /* 50 Hz:    +0.5 dB */
                /* 5-25 stay at 0.5 (0 dB) */
                {26, 0.5208f},  /* 8 kHz:    +0.5 dB */
                {27, 0.5417f},  /* 10 kHz:    +1 dB */
                {28, 0.5625f},  /* 12.5 kHz: +1.5 dB */
                {29, 0.5833f},  /* 16 kHz:    +2 dB */
                {30, 0.5833f},  /* 20 kHz:    +2 dB */
            };
            writeFactoryIfMissing(effectId, QStringLiteral("Broadcast Polish"),
                captureWithOverrides(fx.get(), overrides), ver);
        }
    }

    /* ────────────────────────────────────────────────────────────────
     * mc1.dynamics.compressor — Compressor / Gate / Limiter
     * Params: Input Gain, Threshold, Ratio, Attack, Release,
     *         Knee, Makeup Gain, Gate Threshold, Limiter Ceiling
     * ──────────────────────────────────────────────────────────────── */
    else if (eid == "mc1.dynamics.compressor") {

        /* Gentle 2:1 — light compression for transparent leveling */
        writeFactoryIfMissing(effectId, QStringLiteral("Gentle 2:1"),
            captureWithOverrides(fx.get(), {
                {0, 0.5f},      /* Input Gain:    0 dB */
                {1, 0.5f},      /* Threshold:   -30 dBFS */
                {2, 0.0526f},   /* Ratio:         2:1    (2-1)/19 */
                {3, 0.1992f},   /* Attack:       20 ms   (20-0.1)/99.9 */
                {4, 0.1919f},   /* Release:     200 ms   (200-10)/990 */
                {5, 0.5f},      /* Knee:          6 dB */
                {6, 0.3889f},   /* Makeup:       +2 dB   (2+12)/36 */
                {7, 0.25f},     /* Gate Thresh: -60 dBFS */
                {8, 0.8333f},   /* Limiter:      -1 dBFS */
            }), ver);

        /* Broadcast 4:1 — standard broadcast compression */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Broadcast 4:1"),
            captureWithOverrides(fx.get(), {
                {0, 0.5f},      /* Input Gain:    0 dB */
                {1, 0.7f},      /* Threshold:   -18 dBFS */
                {2, 0.1579f},   /* Ratio:         4:1    (4-1)/19 */
                {3, 0.0991f},   /* Attack:       10 ms */
                {4, 0.1919f},   /* Release:     200 ms */
                {5, 0.5f},      /* Knee:          6 dB */
                {6, 0.4444f},   /* Makeup:       +4 dB   (4+12)/36 */
                {7, 0.25f},     /* Gate Thresh: -60 dBFS */
                {8, 0.8333f},   /* Limiter:      -1 dBFS */
            }), ver);

        /* Heavy Limiting — aggressive with high ratio */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Heavy Limiting"),
            captureWithOverrides(fx.get(), {
                {0, 0.5f},      /* Input Gain:    0 dB */
                {1, 0.8333f},   /* Threshold:   -10 dBFS */
                {2, 0.7368f},   /* Ratio:        15:1   (15-1)/19 */
                {3, 0.0290f},   /* Attack:        3 ms */
                {4, 0.0909f},   /* Release:     100 ms  (100-10)/990 */
                {5, 0.1667f},   /* Knee:          2 dB */
                {6, 0.5f},      /* Makeup:       +6 dB   (6+12)/36 */
                {7, 0.25f},     /* Gate Thresh: -60 dBFS */
                {8, 0.6667f},   /* Limiter:      -2 dBFS */
            }), ver);
    }

    /* ────────────────────────────────────────────────────────────────
     * mc1.dynamics.dbx166xs — DBX 166xs Comp/Gate (OverEasy)
     * Params: Comp Threshold, Comp Ratio, Comp Attack, Comp Release,
     *         Comp Output Gain, OverEasy, Gate Threshold, Gate Ratio,
     *         Gate Attack, Gate Hold
     * ──────────────────────────────────────────────────────────────── */
    else if (eid == "mc1.dynamics.dbx166xs") {

        /* Gentle Comp — light compression with soft knee */
        writeFactoryIfMissing(effectId, QStringLiteral("Gentle Comp"),
            captureWithOverrides(fx.get(), {
                {0, 0.5f},      /* Comp Threshold: -10 dBu */
                {1, 0.0526f},   /* Comp Ratio:       2:1 */
                {2, 0.1005f},   /* Comp Attack:     21 ms  (21-1)/199 */
                {3, 0.1304f},   /* Comp Release:   200 ms  (200-50)/1150 */
                {4, 0.5f},      /* Comp Output:      0 dB */
                {5, 0.7f},      /* OverEasy:         7 (soft knee) */
                {6, 0.3333f},   /* Gate Threshold: -40 dBFS */
                {7, 0.3333f},   /* Gate Ratio:       4:1 */
                {8, 0.0361f},   /* Gate Attack:      1 ms */
                {9, 0.0226f},   /* Gate Hold:       50 ms */
            }), ver);

        /* Broadcast Comp/Gate — medium compression + active gate */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Broadcast Comp/Gate"),
            captureWithOverrides(fx.get(), {
                {0, 0.5f},      /* Comp Threshold: -10 dBu */
                {1, 0.1579f},   /* Comp Ratio:       4:1 */
                {2, 0.0704f},   /* Comp Attack:     15 ms */
                {3, 0.1304f},   /* Comp Release:   200 ms */
                {4, 0.575f},    /* Comp Output:     +3 dB */
                {5, 0.5f},      /* OverEasy:         5 (moderate soft knee) */
                {6, 0.5f},      /* Gate Threshold: -30 dBFS */
                {7, 0.3333f},   /* Gate Ratio:       4:1 */
                {8, 0.0361f},   /* Gate Attack:      1 ms */
                {9, 0.0476f},   /* Gate Hold:      100 ms */
            }), ver);

        /* Heavy OverEasy — aggressive comp with max soft knee */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Heavy OverEasy"),
            captureWithOverrides(fx.get(), {
                {0, 0.3333f},   /* Comp Threshold: -20 dBu */
                {1, 0.4211f},   /* Comp Ratio:       9:1 */
                {2, 0.0201f},   /* Comp Attack:      5 ms */
                {3, 0.0870f},   /* Comp Release:   150 ms */
                {4, 0.625f},    /* Comp Output:     +5 dB */
                {5, 1.0f},      /* OverEasy:        10 (maximum soft knee) */
                {6, 0.5f},      /* Gate Threshold: -30 dBFS */
                {7, 0.5556f},   /* Gate Ratio:       6:1 */
                {8, 0.0361f},   /* Gate Attack:      1 ms */
                {9, 0.0226f},   /* Gate Hold:       50 ms */
            }), ver);
    }

    /* ────────────────────────────────────────────────────────────────
     * mc1.dynamics.agc — Broadcast AGC
     * Params: Target Level, Max Gain, Max Reduction,
     *         Attack, Release, Gate Threshold, RMS Window
     * ──────────────────────────────────────────────────────────────── */
    else if (eid == "mc1.dynamics.agc") {

        /* Broadcast Music — moderate target, wide gain range */
        writeFactoryIfMissing(effectId, QStringLiteral("Broadcast Music"),
            captureWithOverrides(fx.get(), {
                {0, 0.4286f},   /* Target Level:  -14 dBFS  (-14+20)/14 */
                {1, 0.6667f},   /* Max Gain:      +20 dB */
                {2, 0.6667f},   /* Max Reduction: -20 dB */
                {3, 0.0982f},   /* Attack:         50 ms    (50-1)/499 */
                {4, 0.0909f},   /* Release:       500 ms    (500-50)/4950 */
                {5, 0.5f},      /* Gate Threshold: -50 dBFS */
                {6, 0.1837f},   /* RMS Window:    100 ms    (100-10)/490 */
            }), ver);

        /* Broadcast Voice — tighter target, faster response */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Broadcast Voice"),
            captureWithOverrides(fx.get(), {
                {0, 0.5714f},   /* Target Level:  -12 dBFS  (-12+20)/14 */
                {1, 0.5f},      /* Max Gain:      +15 dB */
                {2, 0.5f},      /* Max Reduction: -15 dB */
                {3, 0.0581f},   /* Attack:         30 ms    (30-1)/499 */
                {4, 0.0606f},   /* Release:       350 ms    (350-50)/4950 */
                {5, 0.5833f},   /* Gate Threshold: -45 dBFS  (-45+80)/60 */
                {6, 0.1020f},   /* RMS Window:     60 ms    (60-10)/490 */
            }), ver);

        /* Gentle Leveling — slow, subtle gain riding */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Gentle Leveling"),
            captureWithOverrides(fx.get(), {
                {0, 0.4286f},   /* Target Level:  -14 dBFS */
                {1, 0.3333f},   /* Max Gain:      +10 dB */
                {2, 0.3333f},   /* Max Reduction: -10 dB */
                {3, 0.1984f},   /* Attack:        100 ms   (100-1)/499 */
                {4, 0.1919f},   /* Release:      1000 ms   (1000-50)/4950 */
                {5, 0.5f},      /* Gate Threshold: -50 dBFS */
                {6, 0.3878f},   /* RMS Window:    200 ms   (200-10)/490 */
            }), ver);
    }

    /* ────────────────────────────────────────────────────────────────
     * mc1.channel.dbx286s — DBX 286S Voice Processor
     * Params: Gate Threshold, Gate Ratio, Comp Threshold, Comp Ratio,
     *         Comp Attack, Comp Release, De-Ess Frequency,
     *         De-Ess Threshold, LF Enhance, HF Detail
     * ──────────────────────────────────────────────────────────────── */
    else if (eid == "mc1.channel.dbx286s") {

        /* Voice Default — balanced defaults (constructor values) */
        writeFactoryIfMissing(effectId, QStringLiteral("Voice Default"),
                              captureDefaults(fx.get()), ver);

        /* Heavy Processing — aggressive gate + comp + enhancement */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Heavy Processing"),
            captureWithOverrides(fx.get(), {
                {0, 0.625f},    /* Gate Threshold: -30 dBFS  (-30+80)/80 */
                {1, 0.5556f},   /* Gate Ratio:       6:1     (6-1)/9 */
                {2, 0.5f},      /* Comp Threshold: -30 dBFS  (-30+60)/60 */
                {3, 0.3684f},   /* Comp Ratio:       8:1     (8-1)/19 */
                {4, 0.0290f},   /* Comp Attack:      3 ms    (3-0.1)/99.9 */
                {5, 0.0909f},   /* Comp Release:   100 ms    (100-10)/990 */
                {6, 0.5f},      /* De-Ess Freq:   7000 Hz   (7000-2000)/10000 */
                {7, 0.5f},      /* De-Ess Thresh:  -30 dBFS  (-30+60)/60 */
                {8, 0.5f},      /* LF Enhance:     +6 dB     6/12 */
                {9, 0.5833f},   /* HF Detail:      +7 dB     7/12 */
            }), ver);

        /* Light Touch — minimal processing, subtle enhancement */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Light Touch"),
            captureWithOverrides(fx.get(), {
                {0, 0.375f},    /* Gate Threshold: -50 dBFS  (-50+80)/80 */
                {1, 0.2222f},   /* Gate Ratio:       3:1     (3-1)/9 */
                {2, 0.6667f},   /* Comp Threshold: -20 dBFS  (-20+60)/60 */
                {3, 0.0526f},   /* Comp Ratio:       2:1     (2-1)/19 */
                {4, 0.0991f},   /* Comp Attack:     10 ms    (10-0.1)/99.9 */
                {5, 0.1919f},   /* Comp Release:   200 ms    (200-10)/990 */
                {6, 0.4f},      /* De-Ess Freq:   6000 Hz   (6000-2000)/10000 */
                {7, 0.5833f},   /* De-Ess Thresh:  -25 dBFS  (-25+60)/60 */
                {8, 0.1667f},   /* LF Enhance:     +2 dB     2/12 */
                {9, 0.1667f},   /* HF Detail:      +2 dB     2/12 */
            }), ver);
    }

    /* ────────────────────────────────────────────────────────────────
     * mc1.channel.xenyx — Mackie Xenyx Preamp
     * Params: Input Gain, HPF Enable, HPF Frequency, Comp Amount,
     *         EQ Low, EQ Mid, EQ Mid Freq, EQ High, Output Level
     * ──────────────────────────────────────────────────────────────── */
    else if (eid == "mc1.channel.xenyx") {

        /* Clean Preamp — moderate gain, HPF on, no comp, flat EQ */
        writeFactoryIfMissing(effectId, QStringLiteral("Clean Preamp"),
            captureWithOverrides(fx.get(), {
                {0, 0.3333f},   /* Input Gain:   20 dB   20/60 */
                {1, 1.0f},      /* HPF Enable:   On */
                {2, 0.1111f},   /* HPF Freq:    100 Hz   (100-75)/225 */
                {3, 0.0f},      /* Comp Amount:   0% */
                {4, 0.5f},      /* EQ Low:        0 dB */
                {5, 0.5f},      /* EQ Mid:        0 dB */
                {6, 0.3038f},   /* EQ Mid Freq: 2500 Hz  (2500-100)/7900 */
                {7, 0.5f},      /* EQ High:       0 dB */
                {8, 0.8571f},   /* Output:        0 dB   (0+60)/70 */
            }), ver);

        /* Warm Vocal — gentle low boost, presence lift, light comp */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Warm Vocal"),
            captureWithOverrides(fx.get(), {
                {0, 0.4167f},   /* Input Gain:   25 dB */
                {1, 1.0f},      /* HPF Enable:   On */
                {2, 0.3333f},   /* HPF Freq:    150 Hz */
                {3, 0.3f},      /* Comp Amount:  30% */
                {4, 0.6f},      /* EQ Low:       +3 dB   (3+15)/30 */
                {5, 0.5833f},   /* EQ Mid:      +2.5 dB  (2.5+15)/30 */
                {6, 0.4177f},   /* EQ Mid Freq: 3400 Hz  (3400-100)/7900 */
                {7, 0.5667f},   /* EQ High:      +2 dB   (2+15)/30 */
                {8, 0.8571f},   /* Output:        0 dB */
            }), ver);

        /* Podcast Setup — HPF + moderate comp + slight presence */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Podcast Setup"),
            captureWithOverrides(fx.get(), {
                {0, 0.5f},      /* Input Gain:   30 dB */
                {1, 1.0f},      /* HPF Enable:   On */
                {2, 0.2222f},   /* HPF Freq:    125 Hz   (125-75)/225 */
                {3, 0.45f},     /* Comp Amount:  45% */
                {4, 0.4667f},   /* EQ Low:       -1 dB   (-1+15)/30 */
                {5, 0.5333f},   /* EQ Mid:       +1 dB   (1+15)/30 */
                {6, 0.3671f},   /* EQ Mid Freq: 3000 Hz  (3000-100)/7900 */
                {7, 0.55f},     /* EQ High:     +1.5 dB  (1.5+15)/30 */
                {8, 0.8571f},   /* Output:        0 dB */
            }), ver);
    }

    /* ────────────────────────────────────────────────────────────────
     * mc1.enhancer.sonic — Sonic Enhancer (BBE 882I)
     * Params: Lo Contour (0-10), Process (0-10), Output Gain (-12..+6 dB)
     * ──────────────────────────────────────────────────────────────── */
    else if (eid == "mc1.enhancer.sonic") {

        /* Moderate — balanced enhancement (constructor defaults: 5/5/0dB) */
        writeFactoryIfMissing(effectId, QStringLiteral("Moderate"),
                              captureDefaults(fx.get()), ver);

        /* Heavy Enhancement — strong lo contour + process */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Heavy Enhancement"),
            captureWithOverrides(fx.get(), {
                {0, 0.8f},      /* Lo Contour: 8.0   8/10 */
                {1, 0.8f},      /* Process:    8.0 */
                {2, 0.6667f},   /* Output:     0 dB  (0+12)/18 */
            }), ver);

        /* Subtle Warmth — gentle low end, minimal highs */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Subtle Warmth"),
            captureWithOverrides(fx.get(), {
                {0, 0.4f},      /* Lo Contour: 4.0 */
                {1, 0.2f},      /* Process:    2.0 */
                {2, 0.6667f},   /* Output:     0 dB */
            }), ver);
    }

    /* ────────────────────────────────────────────────────────────────
     * mc1.analog.tube_preamp — Tube Mic Preamp
     * Params: 0=InputGain,1=Drive,2=Warmth,3=Presence,4=LowCut,
     *         5=Transformer,6=Bias,7=Sag,8=Air,9=Output
     * ──────────────────────────────────────────────────────────────── */
    else if (eid == "mc1.analog.tube_preamp") {

        /* Clean Preamp — low drive, minimal coloring */
        writeFactoryIfMissing(effectId, QStringLiteral("Clean Preamp"),
            captureWithOverrides(fx.get(), {
                {0, 0.333f},    /* Input: +20 dB */
                {1, 0.10f},     /* Drive: 10% */
                {2, 0.20f},     /* Warmth: 20% */
                {3, 0.667f},    /* Presence: +2 dB */
                {5, 0.10f},     /* Transformer: 10% */
                {7, 0.10f},     /* Sag: 10% */
            }), ver);

        /* Warm Vocal — smooth tube warmth for voice */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Warm Vocal"),
            captureWithOverrides(fx.get(), {
                {0, 0.417f},    /* Input: +25 dB */
                {1, 0.40f},     /* Drive: 40% */
                {2, 0.65f},     /* Warmth: 65% */
                {3, 0.750f},    /* Presence: +3 dB */
                {5, 0.45f},     /* Transformer: 45% */
                {6, 0.55f},     /* Bias: +5% */
                {7, 0.25f},     /* Sag: 25% */
                {8, 0.385f},    /* Air: +2 dB */
            }), ver);

        /* Vintage Crunch — heavy saturation, old-school warmth */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Vintage Crunch"),
            captureWithOverrides(fx.get(), {
                {0, 0.50f},     /* Input: +30 dB */
                {1, 0.75f},     /* Drive: 75% */
                {2, 0.80f},     /* Warmth: 80% */
                {3, 0.583f},    /* Presence: +1 dB */
                {5, 0.70f},     /* Transformer: 70% */
                {6, 0.60f},     /* Bias: +10% */
                {7, 0.50f},     /* Sag: 50% */
                {8, 0.167f},    /* Air: 0 dB */
            }), ver);

        /* Radio Broadcast — broadcast-optimized tube sound */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Radio Broadcast"),
            captureWithOverrides(fx.get(), {
                {0, 0.333f},    /* Input: +20 dB */
                {1, 0.30f},     /* Drive: 30% */
                {2, 0.50f},     /* Warmth: 50% */
                {3, 0.833f},    /* Presence: +4 dB */
                {4, 0.214f},    /* LowCut: 100 Hz */
                {5, 0.30f},     /* Transformer: 30% */
                {7, 0.15f},     /* Sag: 15% */
                {8, 0.5f},      /* Air: +3 dB */
            }), ver);

        /* Acoustic Guitar — gentle enhancement */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Acoustic Guitar"),
            captureWithOverrides(fx.get(), {
                {0, 0.25f},     /* Input: +15 dB */
                {1, 0.20f},     /* Drive: 20% */
                {2, 0.35f},     /* Warmth: 35% */
                {3, 0.667f},    /* Presence: +2 dB */
                {5, 0.25f},     /* Transformer: 25% */
                {8, 0.583f},    /* Air: +4 dB */
            }), ver);
    }

    /* ────────────────────────────────────────────────────────────────
     * mc1.analog.castertube — CasterTube Vocal Tone Shaper
     * Params: 0=InputGain,1=TubeDrive,2=Character,3=Sustain,
     *         4=VocalRange,5=Depth,6=Warmth,7=Presence,8=Silk,
     *         9=DeEss,10=Air,11=LowCut,12=Transformer,13=Output
     * ──────────────────────────────────────────────────────────────── */
    else if (eid == "mc1.analog.castertube") {

        /* Clean Voice — transparent, minimal processing */
        writeFactoryIfMissing(effectId, QStringLiteral("Clean Voice"),
            captureWithOverrides(fx.get(), {
                {0, 0.333f},    /* Input: +20 dB */
                {1, 0.15f},     /* Drive: 15% */
                {2, 0.20f},     /* Character: 20% (clean side) */
                {3, 0.20f},     /* Sustain: 20% */
                {4, 0.50f},     /* Range: Tenor */
                {5, 0.15f},     /* Depth: 15% */
                {6, 0.30f},     /* Warmth: 30% */
                {7, 0.30f},     /* Presence: 30% */
                {8, 0.20f},     /* Silk: 20% */
                {9, 0.25f},     /* De-Ess: 25% */
            }), ver);

        /* Warm Baritone — rich, full male vocal */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Warm Baritone"),
            captureWithOverrides(fx.get(), {
                {0, 0.417f},    /* Input: +25 dB */
                {1, 0.40f},     /* Drive: 40% */
                {2, 0.60f},     /* Character: 60% (warm) */
                {3, 0.50f},     /* Sustain: 50% */
                {4, 0.25f},     /* Range: Baritone */
                {5, 0.55f},     /* Depth: 55% */
                {6, 0.70f},     /* Warmth: 70% */
                {7, 0.35f},     /* Presence: 35% */
                {8, 0.40f},     /* Silk: 40% */
                {9, 0.30f},     /* De-Ess: 30% */
                {10, 0.375f},   /* Air: +2 dB */
                {12, 0.45f},    /* Transformer: 45% */
            }), ver);

        /* Bright Soprano — clear, articulate female vocal */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Bright Soprano"),
            captureWithOverrides(fx.get(), {
                {0, 0.333f},    /* Input: +20 dB */
                {1, 0.25f},     /* Drive: 25% */
                {2, 0.30f},     /* Character: 30% (cleaner) */
                {3, 0.45f},     /* Sustain: 45% */
                {4, 1.0f},      /* Range: Soprano */
                {5, 0.20f},     /* Depth: 20% */
                {6, 0.35f},     /* Warmth: 35% */
                {7, 0.60f},     /* Presence: 60% */
                {8, 0.50f},     /* Silk: 50% */
                {9, 0.45f},     /* De-Ess: 45% */
                {10, 0.50f},    /* Air: +3 dB */
            }), ver);

        /* Podcast Host — broadcast voice with smooth sustain */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Podcast Host"),
            captureWithOverrides(fx.get(), {
                {0, 0.417f},    /* Input: +25 dB */
                {1, 0.30f},     /* Drive: 30% */
                {2, 0.45f},     /* Character: 45% */
                {3, 0.60f},     /* Sustain: 60% (smooth leveling) */
                {4, 0.50f},     /* Range: Tenor */
                {5, 0.40f},     /* Depth: 40% */
                {6, 0.55f},     /* Warmth: 55% */
                {7, 0.50f},     /* Presence: 50% */
                {8, 0.35f},     /* Silk: 35% */
                {9, 0.35f},     /* De-Ess: 35% */
                {10, 0.375f},   /* Air: +2 dB */
                {11, 0.143f},   /* LowCut: 80 Hz */
                {12, 0.35f},    /* Transformer: 35% */
            }), ver);

        /* Vintage Radio — old-school warm, compressed, intimate */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Vintage Radio"),
            captureWithOverrides(fx.get(), {
                {0, 0.50f},     /* Input: +30 dB */
                {1, 0.55f},     /* Drive: 55% */
                {2, 0.85f},     /* Character: 85% (vintage) */
                {3, 0.70f},     /* Sustain: 70% */
                {4, 0.50f},     /* Range: Tenor */
                {5, 0.60f},     /* Depth: 60% */
                {6, 0.75f},     /* Warmth: 75% */
                {7, 0.30f},     /* Presence: 30% */
                {8, 0.60f},     /* Silk: 60% */
                {9, 0.20f},     /* De-Ess: 20% */
                {11, 0.286f},   /* LowCut: 120 Hz */
                {12, 0.60f},    /* Transformer: 60% */
            }), ver);

        /* Streaming Live — optimized for live broadcast */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Streaming Live"),
            captureWithOverrides(fx.get(), {
                {0, 0.383f},    /* Input: +23 dB */
                {1, 0.25f},     /* Drive: 25% */
                {2, 0.35f},     /* Character: 35% */
                {3, 0.55f},     /* Sustain: 55% */
                {4, 0.50f},     /* Range: Tenor */
                {5, 0.35f},     /* Depth: 35% */
                {6, 0.45f},     /* Warmth: 45% */
                {7, 0.55f},     /* Presence: 55% */
                {8, 0.30f},     /* Silk: 30% */
                {9, 0.40f},     /* De-Ess: 40% */
                {10, 0.375f},   /* Air: +2 dB */
                {11, 0.214f},   /* LowCut: 100 Hz */
                {12, 0.30f},    /* Transformer: 30% */
            }), ver);
    }
    /* ────────────────────────────────────────────────────────────────
     * mc1.modeling.mic — Mic Modeler
     * Params: 0=Model,1=Proximity,2=Axis,3=InputGain,4=Fat,
     *         5=HFContour,6=LowCut,7=TubeColor,8=BodyRes,9=Output
     * Model: 0-11 (normalized 0-1 as index/11)
     * ──────────────────────────────────────────────────────────────── */
    else if (eid == "mc1.modeling.mic") {

        /* Bock 167 Tube Condenser — lush, warm, present */
        writeFactoryIfMissing(effectId, QStringLiteral("Bock 167 Tube"),
            captureWithOverrides(fx.get(), {
                {0, 0.0f},      /* Model: 0 (Bock 167) */
                {1, 0.30f},     /* Proximity: 30% */
                {3, 0.25f},     /* Input: +10 dB */
                {4, 0.0f},      /* Fat: Off */
                {5, 0.667f},    /* HF Contour: Flat */
                {7, 0.35f},     /* Tube Color: 35% */
                {8, 0.50f},     /* Body Res: 50% */
            }), ver);

        /* U47 Vocal — the classic vocal mic sound */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("U47 Vocal"),
            captureWithOverrides(fx.get(), {
                {0, 0.0909f},   /* Model: 1 (U47) */
                {1, 0.35f},     /* Proximity: 35% */
                {3, 0.25f},     /* Input: +10 dB */
                {7, 0.40f},     /* Tube Color: 40% */
                {8, 0.55f},     /* Body Res: 55% */
            }), ver);

        /* C12 Bright — airy, detailed top end */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("C12 Bright"),
            captureWithOverrides(fx.get(), {
                {0, 0.1818f},   /* Model: 2 (C12) */
                {1, 0.25f},     /* Proximity: 25% */
                {5, 1.0f},      /* HF Contour: Boost +2dB@10k */
                {7, 0.30f},     /* Tube Color: 30% */
                {8, 0.60f},     /* Body Res: 60% */
            }), ver);

        /* SM7B Broadcast — flat, broadcast standard */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("SM7B Broadcast"),
            captureWithOverrides(fx.get(), {
                {0, 0.3636f},   /* Model: 4 (DN-7/SM7B) */
                {1, 0.20f},     /* Proximity: 20% */
                {3, 0.375f},    /* Input: +15 dB */
                {6, 0.158f},    /* LowCut: 80 Hz */
                {8, 0.40f},     /* Body Res: 40% */
            }), ver);

        /* RE20 Podcast — broadcast dynamic, minimal proximity */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("RE20 Podcast"),
            captureWithOverrides(fx.get(), {
                {0, 0.4545f},   /* Model: 5 (DN-20/RE20) */
                {1, 0.10f},     /* Proximity: 10% */
                {3, 0.375f},    /* Input: +15 dB */
                {6, 0.158f},    /* LowCut: 80 Hz */
                {8, 0.45f},     /* Body Res: 45% */
            }), ver);

        /* RCA 77 Vintage — warm ribbon broadcast */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("RCA 77 Vintage"),
            captureWithOverrides(fx.get(), {
                {0, 0.7273f},   /* Model: 8 (RB-77DX) */
                {1, 0.40f},     /* Proximity: 40% */
                {7, 0.0f},      /* No tube (ribbon) */
                {8, 0.35f},     /* Body Res: 35% */
            }), ver);

        /* Coles 4038 Rock — mid-forward ribbon for instruments */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Coles 4038 Rock"),
            captureWithOverrides(fx.get(), {
                {0, 0.8182f},   /* Model: 9 (RB-160) */
                {1, 0.35f},     /* Proximity: 35% */
                {3, 0.25f},     /* Input: +10 dB */
                {8, 0.50f},     /* Body Res: 50% */
            }), ver);

        /* MD421 Instrument — versatile dynamic for instruments */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("MD421 Instrument"),
            captureWithOverrides(fx.get(), {
                {0, 1.0f},      /* Model: 11 (DN-421) */
                {1, 0.20f},     /* Proximity: 20% */
                {3, 0.25f},     /* Input: +10 dB */
                {8, 0.55f},     /* Body Res: 55% */
            }), ver);
    }

    /* ────────────────────────────────────────────────────────────────
     * mc1.lexicon.l224 — Lexicon 224 Digital Reverb
     * Params: 0=Program,1=PreDelay,2=Decay,3=Size,4=Diffusion,
     *         5=HfDamping,6=LfCut,7=BassMult,8=TrebleDecay,9=ModDepth,10=Mix
     * ──────────────────────────────────────────────────────────────── */
    else if (eid == "mc1.lexicon.l224") {

        /* Default */
        writeFactoryIfMissing(effectId, QStringLiteral("Default"),
                              captureDefaults(fx.get()), ver);

        /* Small Room — short decay, small size */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Small Room"),
            captureWithOverrides(fx.get(), {
                {0, 0.75f},     /* Program: Room */
                {1, 0.02f},     /* PreDelay: ~5 ms */
                {2, 0.15f},     /* Decay: ~0.8 s */
                {3, 0.25f},     /* Size: small */
                {4, 0.50f},     /* Diffusion: moderate */
                {5, 0.60f},     /* HfDamping: moderate */
                {10, 0.25f},    /* Mix: 25% */
            }), ver);

        /* Large Hall — long decay, big size */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Large Hall"),
            captureWithOverrides(fx.get(), {
                {0, 0.25f},     /* Program: Hall B */
                {1, 0.12f},     /* PreDelay: ~30 ms */
                {2, 0.65f},     /* Decay: ~5 s */
                {3, 0.80f},     /* Size: large */
                {4, 0.75f},     /* Diffusion: high */
                {5, 0.35f},     /* HfDamping: gentle */
                {7, 0.60f},     /* BassMult: 1.2 */
                {10, 0.35f},    /* Mix: 35% */
            }), ver);

        /* Plate — classic 224 plate */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Plate"),
            captureWithOverrides(fx.get(), {
                {0, 0.50f},     /* Program: Plate */
                {1, 0.04f},     /* PreDelay: ~10 ms */
                {2, 0.40f},     /* Decay: ~2.0 s */
                {3, 0.50f},     /* Size: 1.0 */
                {4, 0.80f},     /* Diffusion: high (dense) */
                {5, 0.30f},     /* HfDamping: low */
                {9, 0.45f},     /* ModDepth: subtle shimmer */
                {10, 0.30f},    /* Mix: 30% */
            }), ver);
    }

    /* ────────────────────────────────────────────────────────────────
     * mc1.lexicon.pcm70 — Lexicon PCM 70 Multi-FX
     * Params: 0=Algo,1=Size,2=Decay,3=PreDelay,4=Diffusion,5=Shape,
     *         6=Spread,7=HfCut,8=LfCut,9=ModRate,10=ModDepth,11=Mix
     * ──────────────────────────────────────────────────────────────── */
    else if (eid == "mc1.lexicon.pcm70") {

        /* Default */
        writeFactoryIfMissing(effectId, QStringLiteral("Default"),
                              captureDefaults(fx.get()), ver);

        /* Small Room — Chamber algorithm, tight */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Small Room"),
            captureWithOverrides(fx.get(), {
                {0, 0.20f},     /* Algo: Chamber */
                {1, 0.30f},     /* Size: small */
                {2, 0.20f},     /* Decay: ~0.8 s */
                {3, 0.02f},     /* PreDelay: ~4 ms */
                {6, 0.40f},     /* Spread: narrow */
                {11, 0.25f},    /* Mix: 25% */
            }), ver);

        /* Large Hall — Plate algo, long tail */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Large Hall"),
            captureWithOverrides(fx.get(), {
                {0, 0.0f},      /* Algo: Plate */
                {1, 0.70f},     /* Size: large */
                {2, 0.60f},     /* Decay: ~4 s */
                {3, 0.15f},     /* PreDelay: ~30 ms */
                {4, 0.80f},     /* Diffusion: high */
                {6, 0.80f},     /* Spread: wide */
                {7, 0.50f},     /* HfCut: moderate */
                {11, 0.35f},    /* Mix: 35% */
            }), ver);

        /* Plate — dense, fast, classic */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Plate"),
            captureWithOverrides(fx.get(), {
                {0, 0.0f},      /* Algo: Plate */
                {1, 0.50f},     /* Size: medium */
                {2, 0.45f},     /* Decay: ~2.5 s */
                {3, 0.05f},     /* PreDelay: ~10 ms */
                {4, 0.75f},     /* Diffusion: high */
                {5, 0.40f},     /* Shape: quick build */
                {7, 0.35f},     /* HfCut: gentle */
                {11, 0.30f},    /* Mix: 30% */
            }), ver);
    }

    /* ────────────────────────────────────────────────────────────────
     * mc1.lexicon.pcm96 — Lexicon PCM 96 Stereo Reverb
     * Params: 0=Algo,1=RtMid,2=Size,3=PreDelay,4=ErLevel,5=ErTime,
     *         6=LateLevel,7=Diffusion,8=Shape,9=HfDamping,10=LfCut,
     *         11=StereoWidth,12=Mix
     * ──────────────────────────────────────────────────────────────── */
    else if (eid == "mc1.lexicon.pcm96") {

        /* Default */
        writeFactoryIfMissing(effectId, QStringLiteral("Default"),
                              captureDefaults(fx.get()), ver);

        /* Small Room — Random Ambience, short */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Small Room"),
            captureWithOverrides(fx.get(), {
                {0, 1.0f},      /* Algo: RandomAmbience */
                {1, 0.15f},     /* RtMid: ~0.6 s */
                {2, 0.30f},     /* Size: small */
                {4, 0.60f},     /* ErLevel: prominent ERs */
                {5, 0.20f},     /* ErTime: short spread */
                {6, 0.30f},     /* LateLevel: subtle */
                {12, 0.25f},    /* Mix: 25% */
            }), ver);

        /* Large Hall — Concert Hall, lush */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Large Hall"),
            captureWithOverrides(fx.get(), {
                {0, 0.0f},      /* Algo: ConcertHall */
                {1, 0.60f},     /* RtMid: ~5 s */
                {2, 0.75f},     /* Size: large */
                {3, 0.10f},     /* PreDelay: ~20 ms */
                {4, 0.30f},     /* ErLevel: subtle ERs */
                {6, 0.65f},     /* LateLevel: prominent */
                {7, 0.80f},     /* Diffusion: high */
                {11, 0.85f},    /* StereoWidth: wide */
                {12, 0.35f},    /* Mix: 35% */
            }), ver);

        /* Plate — dense, bright, vocal plate */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Plate"),
            captureWithOverrides(fx.get(), {
                {0, 0.0f},      /* Algo: ConcertHall (plate-like high diffusion) */
                {1, 0.40f},     /* RtMid: ~2.0 s */
                {2, 0.45f},     /* Size: medium */
                {3, 0.03f},     /* PreDelay: ~6 ms */
                {4, 0.15f},     /* ErLevel: minimal */
                {6, 0.60f},     /* LateLevel: rich */
                {7, 0.85f},     /* Diffusion: very high */
                {9, 0.25f},     /* HfDamping: low (bright) */
                {12, 0.30f},    /* Mix: 30% */
            }), ver);
    }

    /* ────────────────────────────────────────────────────────────────
     * mc1.lexicon.l480l — Lexicon 480L Random Hall
     * Params: 0=Algo,1=RtMid,2=Size,3=Shape,4=Spread,5=PreDelay,
     *         6=ErTime,7=Diffusion,8=HfCut,9=BassBoost,10=ModRate,
     *         11=ModDepth,12=TailDensity,13=Mix
     * ──────────────────────────────────────────────────────────────── */
    else if (eid == "mc1.lexicon.l480l") {

        /* Default */
        writeFactoryIfMissing(effectId, QStringLiteral("Default"),
                              captureDefaults(fx.get()), ver);

        /* Small Room — Random Ambience, tight */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Small Room"),
            captureWithOverrides(fx.get(), {
                {0, 1.0f},      /* Algo: RandomAmbience */
                {1, 0.20f},     /* RtMid: ~0.8 s */
                {2, 0.30f},     /* Size: small */
                {4, 0.40f},     /* Spread: narrow */
                {5, 0.02f},     /* PreDelay: ~5 ms */
                {6, 0.15f},     /* ErTime: short */
                {13, 0.25f},    /* Mix: 25% */
            }), ver);

        /* Large Hall — Random Hall, Hollywood scoring stage */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Large Hall"),
            captureWithOverrides(fx.get(), {
                {0, 0.0f},      /* Algo: RandomHall */
                {1, 0.65f},     /* RtMid: ~6 s */
                {2, 0.80f},     /* Size: large */
                {3, 0.60f},     /* Shape: slow build */
                {4, 0.85f},     /* Spread: very wide */
                {5, 0.10f},     /* PreDelay: ~25 ms */
                {7, 0.80f},     /* Diffusion: high */
                {9, 0.60f},     /* BassBoost: warm */
                {12, 0.40f},    /* TailDensity: moderate saturation */
                {13, 0.35f},    /* Mix: 35% */
            }), ver);

        /* Plate — dense tail, bright character */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Plate"),
            captureWithOverrides(fx.get(), {
                {0, 0.0f},      /* Algo: RandomHall */
                {1, 0.40f},     /* RtMid: ~2 s */
                {2, 0.50f},     /* Size: medium */
                {3, 0.35f},     /* Shape: quick */
                {5, 0.03f},     /* PreDelay: ~7 ms */
                {7, 0.85f},     /* Diffusion: very high */
                {8, 0.25f},     /* HfCut: minimal (bright) */
                {12, 0.50f},    /* TailDensity: dense */
                {13, 0.30f},    /* Mix: 30% */
            }), ver);
    }

    /* ────────────────────────────────────────────────────────────────
     * mc1.lexicon.mpx1 — Lexicon MPX 1 Pitch + Delay
     * Params: 0=Pitch1,1=Pitch2,2=P1Delay,3=P2Delay,4=Tap1,5=Tap2,
     *         6=Tap3,7=Tap4,8=Feedback,9=DuckThresh,10=DuckRatio,11=Mix
     * ──────────────────────────────────────────────────────────────── */
    else if (eid == "mc1.lexicon.mpx1") {

        /* Default */
        writeFactoryIfMissing(effectId, QStringLiteral("Default"),
                              captureDefaults(fx.get()), ver);

        /* Small Room — short delays, narrow pitch, tight feedback */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Small Room"),
            captureWithOverrides(fx.get(), {
                {0, 0.5f},      /* Pitch1: unity */
                {1, 0.5f},      /* Pitch2: unity */
                {4, 0.05f},     /* Tap1: ~75 ms */
                {5, 0.10f},     /* Tap2: ~150 ms */
                {6, 0.15f},     /* Tap3: ~225 ms */
                {7, 0.20f},     /* Tap4: ~300 ms */
                {8, 0.20f},     /* Feedback: low */
                {11, 0.25f},    /* Mix: 25% */
            }), ver);

        /* Large Hall — long delays, wide feedback */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Large Hall"),
            captureWithOverrides(fx.get(), {
                {0, 0.48f},     /* Pitch1: slightly flat (-0.5 semi) */
                {1, 0.52f},     /* Pitch2: slightly sharp (+0.5 semi) */
                {4, 0.30f},     /* Tap1: ~450 ms */
                {5, 0.50f},     /* Tap2: ~750 ms */
                {6, 0.70f},     /* Tap3: ~1050 ms */
                {7, 0.90f},     /* Tap4: ~1350 ms */
                {8, 0.55f},     /* Feedback: moderate */
                {11, 0.35f},    /* Mix: 35% */
            }), ver);

        /* Plate — pitch doubler + short slapback */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Plate"),
            captureWithOverrides(fx.get(), {
                {0, 0.46f},     /* Pitch1: -1 semi */
                {1, 0.54f},     /* Pitch2: +1 semi */
                {2, 0.10f},     /* P1Delay: ~20 ms */
                {3, 0.15f},     /* P2Delay: ~30 ms */
                {4, 0.08f},     /* Tap1: ~120 ms */
                {5, 0.16f},     /* Tap2: ~240 ms */
                {8, 0.35f},     /* Feedback: moderate */
                {11, 0.30f},    /* Mix: 30% */
            }), ver);
    }

    /* ════════════════════════════════════════════════════════════════
     * MC1 Podcast Series — Default only (constructor defaults)
     * ════════════════════════════════════════════════════════════════ */

    else if (eid == "mc1.podcast.voice_lift") {
        writeFactoryIfMissing(effectId, QStringLiteral("Default"),
                              captureDefaults(fx.get()), ver);
    }
    else if (eid == "mc1.podcast.plosive") {
        writeFactoryIfMissing(effectId, QStringLiteral("Default"),
                              captureDefaults(fx.get()), ver);
    }
    else if (eid == "mc1.podcast.mouth_click") {
        writeFactoryIfMissing(effectId, QStringLiteral("Default"),
                              captureDefaults(fx.get()), ver);
    }
    else if (eid == "mc1.podcast.bleed") {
        writeFactoryIfMissing(effectId, QStringLiteral("Default"),
                              captureDefaults(fx.get()), ver);
    }
    else if (eid == "mc1.podcast.phone_line") {
        writeFactoryIfMissing(effectId, QStringLiteral("Default"),
                              captureDefaults(fx.get()), ver);
    }
    else if (eid == "mc1.podcast.remote_restore") {
        writeFactoryIfMissing(effectId, QStringLiteral("Default"),
                              captureDefaults(fx.get()), ver);
    }
    else if (eid == "mc1.podcast.loudness_match") {
        writeFactoryIfMissing(effectId, QStringLiteral("Default"),
                              captureDefaults(fx.get()), ver);
    }
    else if (eid == "mc1.podcast.stinger") {
        writeFactoryIfMissing(effectId, QStringLiteral("Default"),
                              captureDefaults(fx.get()), ver);
    }
    else if (eid == "mc1.podcast.vodcast_lipsync") {
        writeFactoryIfMissing(effectId, QStringLiteral("Default"),
                              captureDefaults(fx.get()), ver);
    }

    /* ════════════════════════════════════════════════════════════════
     * MC1 Studios — Signal Hill / Tidemark / Granite
     * ════════════════════════════════════════════════════════════════ */

    /* ────────────────────────────────────────────────────────────────
     * mc1.studio.signal_hill_a — Signal Hill Broadcasting A
     * Params: 0=BoothType,1=MicCharacter,2=VoiceLift,3=PlosiveAmount,
     *         4=ClickRemoval,5=Compression,6=DeEsser,7=Enhancer,
     *         8=BleedGate,9=PhoneLine,10=LoudnessTarget,11=Warmth,
     *         12=Mix,13=Output
     * ──────────────────────────────────────────────────────────────── */
    else if (eid == "mc1.studio.signal_hill_a") {

        /* Default */
        writeFactoryIfMissing(effectId, QStringLiteral("Default"),
                              captureDefaults(fx.get()), ver);

        /* Podcast Pro — tight booth, heavy processing chain */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Podcast Pro"),
            captureWithOverrides(fx.get(), {
                {0, 0.0f},      /* BoothType: TightBooth */
                {2, 0.65f},     /* VoiceLift: strong */
                {3, 0.60f},     /* PlosiveAmount: 60% */
                {4, 0.55f},     /* ClickRemoval: 55% */
                {5, 0.65f},     /* Compression: heavy */
                {6, 0.55f},     /* DeEsser: moderate */
                {7, 0.50f},     /* Enhancer: medium */
                {11, 0.40f},    /* Warmth: warm */
            }), ver);

        /* Interview Casual — treated room, lighter touch */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Interview Casual"),
            captureWithOverrides(fx.get(), {
                {0, 1.0f},      /* BoothType: InterviewLounge */
                {2, 0.45f},     /* VoiceLift: gentle */
                {3, 0.40f},     /* PlosiveAmount: 40% */
                {4, 0.35f},     /* ClickRemoval: 35% */
                {5, 0.40f},     /* Compression: light */
                {7, 0.30f},     /* Enhancer: subtle */
                {11, 0.20f},    /* Warmth: minimal */
            }), ver);
    }

    /* ────────────────────────────────────────────────────────────────
     * mc1.studio.tidemark_a — Tidemark Studios A
     * Params: 0=SourcePosition,1=MicSelect,2=MicProximity,3=ConsoleDrive,
     *         4=ConsoleEQ,5=Compression,6=RoomTone,7=ChamberSend,
     *         8=ChamberDecay,9=Polish,10=Mix,11=Output
     * ──────────────────────────────────────────────────────────────── */
    else if (eid == "mc1.studio.tidemark_a") {

        /* Default */
        writeFactoryIfMissing(effectId, QStringLiteral("Default"),
                              captureDefaults(fx.get()), ver);

        /* Vocal Session — U47, close, warm console, chamber send */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Vocal Session"),
            captureWithOverrides(fx.get(), {
                {0, 0.20f},     /* SourcePosition: VocalBooth */
                {1, 0.0f},      /* MicSelect: U47 */
                {2, 0.70f},     /* MicProximity: close */
                {3, 0.55f},     /* ConsoleDrive: warm */
                {5, 0.60f},     /* Compression: medium */
                {7, 0.40f},     /* ChamberSend: present */
                {8, 0.50f},     /* ChamberDecay: moderate */
                {9, 0.45f},     /* Polish: medium */
            }), ver);

        /* Drum Room — riser position, big room tone, 1176 comp */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Drum Room"),
            captureWithOverrides(fx.get(), {
                {0, 0.0f},      /* SourcePosition: DrumRiser */
                {1, 0.50f},     /* MicSelect: SM57 */
                {2, 0.40f},     /* MicProximity: back */
                {3, 0.60f},     /* ConsoleDrive: driven */
                {5, 0.70f},     /* Compression: heavy */
                {6, 0.75f},     /* RoomTone: lots of room */
                {7, 0.20f},     /* ChamberSend: subtle */
            }), ver);
    }

    /* ────────────────────────────────────────────────────────────────
     * mc1.studio.tidemark_b — Tidemark Studios B
     * Params: same indices as Tidemark A (12 params)
     * ──────────────────────────────────────────────────────────────── */
    else if (eid == "mc1.studio.tidemark_b") {

        /* Default */
        writeFactoryIfMissing(effectId, QStringLiteral("Default"),
                              captureDefaults(fx.get()), ver);

        /* Intimate Vocal — close, dry, warm */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Intimate Vocal"),
            captureWithOverrides(fx.get(), {
                {0, 0.0f},      /* SourcePosition: VocalClose */
                {2, 0.75f},     /* MicProximity: very close */
                {3, 0.50f},     /* ConsoleDrive: medium tube */
                {5, 0.55f},     /* Compression: moderate */
                {6, 0.30f},     /* RoomTone: dry */
                {7, 0.20f},     /* ChamberSend: whisper */
                {9, 0.35f},     /* Polish: subtle */
            }), ver);

        /* Acoustic Guitar — instrument position, chamber send */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Acoustic Guitar"),
            captureWithOverrides(fx.get(), {
                {0, 1.0f},      /* SourcePosition: InstrumentSpot */
                {1, 0.75f},     /* MicSelect: Ribbon121 */
                {2, 0.55f},     /* MicProximity: moderate */
                {3, 0.35f},     /* ConsoleDrive: clean */
                {5, 0.40f},     /* Compression: light */
                {6, 0.50f},     /* RoomTone: balanced */
                {7, 0.35f},     /* ChamberSend: present */
                {8, 0.50f},     /* ChamberDecay: moderate */
            }), ver);
    }

    /* ────────────────────────────────────────────────────────────────
     * mc1.studio.tidemark_vault — Tidemark Vault Chambers
     * Params: 0=Chamber,1=Decay,2=PreDelay,3=Size,4=Damping,
     *         5=LowCut,6=Mix,7=Output
     * ──────────────────────────────────────────────────────────────── */
    else if (eid == "mc1.studio.tidemark_vault") {

        /* Default */
        writeFactoryIfMissing(effectId, QStringLiteral("Default"),
                              captureDefaults(fx.get()), ver);

        /* Vocal Chamber — long bright (Chamber A), open */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Vocal Chamber"),
            captureWithOverrides(fx.get(), {
                {0, 0.0f},      /* Chamber: A (long bright) */
                {1, 0.60f},     /* Decay: ~3.5 s */
                {2, 0.08f},     /* PreDelay: ~16 ms */
                {3, 0.60f},     /* Size: medium-large */
                {4, 0.30f},     /* Damping: low */
                {6, 0.35f},     /* Mix: 35% */
            }), ver);

        /* Drum Plate — short dark (Chamber B), tight */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Drum Plate"),
            captureWithOverrides(fx.get(), {
                {0, 0.50f},     /* Chamber: B (short dark) */
                {1, 0.30f},     /* Decay: ~1.5 s */
                {2, 0.04f},     /* PreDelay: ~8 ms */
                {3, 0.40f},     /* Size: smaller */
                {4, 0.65f},     /* Damping: high (dark) */
                {6, 0.25f},     /* Mix: 25% */
            }), ver);
    }

    /* ────────────────────────────────────────────────────────────────
     * mc1.studio.granite_a — Granite Hall Studios A
     * Params: 0=SourcePosition,1=MicSelect,2=MicProximity,3=ConsoleDrive,
     *         4=ConsoleEQ,5=FETLimiter,6=DolbyA,7=RoomTone,8=ChamberSend,
     *         9=ChamberDecay,10=Polish,11=Mix,12=Output
     * ──────────────────────────────────────────────────────────────── */
    else if (eid == "mc1.studio.granite_a") {

        /* Default */
        writeFactoryIfMissing(effectId, QStringLiteral("Default"),
                              captureDefaults(fx.get()), ver);

        /* Rock Drums — drum riser, big room, FET limiter cranked */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Rock Drums"),
            captureWithOverrides(fx.get(), {
                {0, 0.0f},      /* SourcePosition: DrumRiser */
                {1, 0.40f},     /* MicSelect: SM57 */
                {2, 0.35f},     /* MicProximity: backed off */
                {3, 0.65f},     /* ConsoleDrive: hot */
                {5, 0.75f},     /* FETLimiter: smashing */
                {6, 0.40f},     /* DolbyA: medium expander */
                {7, 0.80f},     /* RoomTone: lots of room */
                {8, 0.30f},     /* ChamberSend: subtle */
            }), ver);

        /* Guitar Wall — dual stacks, compressed, dense */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Guitar Wall"),
            captureWithOverrides(fx.get(), {
                {0, 0.25f},     /* SourcePosition: GuitarAmpLeft */
                {1, 0.40f},     /* MicSelect: SM57 */
                {2, 0.50f},     /* MicProximity: medium */
                {3, 0.55f},     /* ConsoleDrive: warm */
                {5, 0.60f},     /* FETLimiter: moderate */
                {6, 0.25f},     /* DolbyA: subtle */
                {7, 0.55f},     /* RoomTone: moderate */
                {8, 0.40f},     /* ChamberSend: present */
                {9, 0.70f},     /* ChamberDecay: long */
            }), ver);
    }

    /* ════════════════════════════════════════════════════════════════
     * BBE Series — D82 / H82 / L82 / Mach3 Bass
     * ════════════════════════════════════════════════════════════════ */

    /* ────────────────────────────────────────────────────────────────
     * mc1.bbe.d82 — BBE D82 Sonic Maximizer
     * Params: 0=LoContour,1=Process,2=Drive,3=StereoWidth,
     *         4=LoXOver,5=HiXOver,6=Mix,7=Output
     * ──────────────────────────────────────────────────────────────── */
    else if (eid == "mc1.bbe.d82") {

        /* Default */
        writeFactoryIfMissing(effectId, QStringLiteral("Default"),
                              captureDefaults(fx.get()), ver);

        /* Bright Presence — emphasis on high end, minimal lows */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Bright Presence"),
            captureWithOverrides(fx.get(), {
                {0, 0.30f},     /* LoContour: 3.0 */
                {1, 0.70f},     /* Process: 7.0 */
                {2, 0.40f},     /* Drive: 4.0 */
                {6, 1.0f},      /* Mix: 100% */
                {7, 0.667f},    /* Output: 0 dB */
            }), ver);

        /* Warm Bass — bottom-up enhancement */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Warm Bass"),
            captureWithOverrides(fx.get(), {
                {0, 0.75f},     /* LoContour: 7.5 */
                {1, 0.30f},     /* Process: 3.0 */
                {2, 0.25f},     /* Drive: 2.5 */
                {6, 1.0f},      /* Mix: 100% */
                {7, 0.667f},    /* Output: 0 dB */
            }), ver);

        /* Subtle Enhancement — gentle overall polish */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Subtle Enhancement"),
            captureWithOverrides(fx.get(), {
                {0, 0.35f},     /* LoContour: 3.5 */
                {1, 0.35f},     /* Process: 3.5 */
                {2, 0.15f},     /* Drive: 1.5 */
                {3, 0.50f},     /* StereoWidth: 50% */
                {6, 0.70f},     /* Mix: 70% */
                {7, 0.667f},    /* Output: 0 dB */
            }), ver);
    }

    /* ────────────────────────────────────────────────────────────────
     * mc1.bbe.h82 — BBE H82 Harmonic Maximizer
     * Params: 0=LoContour,1=Process,2=Harmonics,3=LoRestore,4=Mix,5=Output
     * ──────────────────────────────────────────────────────────────── */
    else if (eid == "mc1.bbe.h82") {

        /* Default */
        writeFactoryIfMissing(effectId, QStringLiteral("Default"),
                              captureDefaults(fx.get()), ver);

        /* Warm Harmonics — even harmonic emphasis, bass warmth */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Warm Harmonics"),
            captureWithOverrides(fx.get(), {
                {0, 0.65f},     /* LoContour: 6.5 */
                {1, 0.55f},     /* Process: 5.5 */
                {2, 0.20f},     /* Harmonics: mostly even */
                {3, 0.40f},     /* LoRestore: 40% */
                {4, 1.0f},      /* Mix: 100% */
            }), ver);

        /* Bright Excite — odd harmonic emphasis, presence */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Bright Excite"),
            captureWithOverrides(fx.get(), {
                {0, 0.30f},     /* LoContour: 3.0 */
                {1, 0.70f},     /* Process: 7.0 */
                {2, 0.80f},     /* Harmonics: mostly odd */
                {3, 0.15f},     /* LoRestore: 15% */
                {4, 1.0f},      /* Mix: 100% */
            }), ver);

        /* Sub Restore — subharmonic restoration focus */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Sub Restore"),
            captureWithOverrides(fx.get(), {
                {0, 0.50f},     /* LoContour: 5.0 */
                {1, 0.30f},     /* Process: 3.0 */
                {2, 0.30f},     /* Harmonics: even-leaning */
                {3, 0.80f},     /* LoRestore: 80% */
                {4, 1.0f},      /* Mix: 100% */
            }), ver);
    }

    /* ────────────────────────────────────────────────────────────────
     * mc1.bbe.l82 — BBE L82 Loudness Maximizer
     * Params: 0=Sensitivity,1=LoThresh,2=MidThresh,3=HiThresh,
     *         4=Release,5=Ceiling,6=Mix,7=Output
     * ──────────────────────────────────────────────────────────────── */
    else if (eid == "mc1.bbe.l82") {

        /* Default */
        writeFactoryIfMissing(effectId, QStringLiteral("Default"),
                              captureDefaults(fx.get()), ver);

        /* Broadcast Loud — aggressive loudness for on-air */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Broadcast Loud"),
            captureWithOverrides(fx.get(), {
                {0, 0.75f},     /* Sensitivity: 7.5 (aggressive) */
                {1, 0.60f},     /* LoThresh: -10 dB offset */
                {2, 0.55f},     /* MidThresh: -11 dB offset */
                {3, 0.50f},     /* HiThresh: -12 dB offset */
                {4, 0.30f},     /* Release: ~185 ms */
                {5, 0.83f},     /* Ceiling: -1 dBFS */
                {6, 1.0f},      /* Mix: 100% */
            }), ver);

        /* Gentle Squeeze — transparent loudness increase */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Gentle Squeeze"),
            captureWithOverrides(fx.get(), {
                {0, 0.40f},     /* Sensitivity: 4.0 (gentle) */
                {1, 0.45f},     /* LoThresh: -13 dB offset */
                {2, 0.40f},     /* MidThresh: -14 dB offset */
                {3, 0.40f},     /* HiThresh: -14 dB offset */
                {4, 0.55f},     /* Release: ~300 ms */
                {5, 0.67f},     /* Ceiling: -2 dBFS */
                {6, 0.80f},     /* Mix: 80% */
            }), ver);

        /* Slam — maximum loudness, dense and hot */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Slam"),
            captureWithOverrides(fx.get(), {
                {0, 0.90f},     /* Sensitivity: 9.0 */
                {1, 0.75f},     /* LoThresh: -6 dB offset */
                {2, 0.70f},     /* MidThresh: -7 dB offset */
                {3, 0.65f},     /* HiThresh: -8 dB offset */
                {4, 0.15f},     /* Release: ~115 ms (fast) */
                {5, 1.0f},      /* Ceiling: 0 dBFS */
                {6, 1.0f},      /* Mix: 100% */
            }), ver);
    }

    /* ────────────────────────────────────────────────────────────────
     * mc1.bbe.mach3bass — BBE Mach 3 Bass
     * Params: 0=Frequency,1=BassBoost,2=Drive,3=Tightness,4=Mix,5=Output
     * ──────────────────────────────────────────────────────────────── */
    else if (eid == "mc1.bbe.mach3bass") {

        /* Default */
        writeFactoryIfMissing(effectId, QStringLiteral("Default"),
                              captureDefaults(fx.get()), ver);

        /* Deep Sub — low frequency, strong bass boost */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Deep Sub"),
            captureWithOverrides(fx.get(), {
                {0, 0.15f},     /* Frequency: ~64 Hz */
                {1, 0.75f},     /* BassBoost: +9 dB */
                {2, 0.50f},     /* Drive: 5.0 */
                {3, 0.70f},     /* Tightness: 70% */
                {4, 1.0f},      /* Mix: 100% */
            }), ver);

        /* Punch — mid-bass focus, tight low end */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Punch"),
            captureWithOverrides(fx.get(), {
                {0, 0.50f},     /* Frequency: ~120 Hz */
                {1, 0.55f},     /* BassBoost: +6.5 dB */
                {2, 0.65f},     /* Drive: 6.5 */
                {3, 0.85f},     /* Tightness: 85% */
                {4, 1.0f},      /* Mix: 100% */
            }), ver);

        /* Rumble — wide bass, loose */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Rumble"),
            captureWithOverrides(fx.get(), {
                {0, 0.25f},     /* Frequency: ~80 Hz */
                {1, 0.85f},     /* BassBoost: +10 dB */
                {2, 0.40f},     /* Drive: 4.0 */
                {3, 0.30f},     /* Tightness: 30% (loose) */
                {4, 1.0f},      /* Mix: 100% */
            }), ver);
    }

    /* ════════════════════════════════════════════════════════════════
     * dbx 500 Series — 676 / 580 / 266xs / 560A / 520 / 510 / 530
     * ════════════════════════════════════════════════════════════════ */

    /* ────────────────────────────────────────────────────────────────
     * mc1.dbx.676 — dbx 676 Tube Mic Preamp
     * Params: 0=Gain,1=Drive,2=HPF,3=LoGain,4=MidFreq,5=MidGain,
     *         6=HiGain,7=CompThresh,8=CompRatio,9=TubeMix,10=Mix,11=Output
     * ──────────────────────────────────────────────────────────────── */
    else if (eid == "mc1.dbx.676") {

        /* Default */
        writeFactoryIfMissing(effectId, QStringLiteral("Default"),
                              captureDefaults(fx.get()), ver);

        /* Warm Vocal — tube driven, presence lift, gentle comp */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Warm Vocal"),
            captureWithOverrides(fx.get(), {
                {0, 0.45f},     /* Gain: 27 dB */
                {1, 0.50f},     /* Drive: 5.0 (warm tube) */
                {2, 0.375f},    /* HPF: 100 Hz */
                {3, 0.55f},     /* LoGain: +1.2 dB */
                {4, 0.50f},     /* MidFreq: ~2.5 kHz */
                {5, 0.58f},     /* MidGain: +2 dB */
                {6, 0.54f},     /* HiGain: +1 dB */
                {7, 0.65f},     /* CompThresh: -14 dB */
                {8, 0.33f},     /* CompRatio: ~4:1 */
                {9, 0.75f},     /* TubeMix: 75% */
                {10, 1.0f},     /* Mix: 100% */
            }), ver);

        /* Clean Instrument — minimal tube, flat EQ, light comp */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Clean Instrument"),
            captureWithOverrides(fx.get(), {
                {0, 0.40f},     /* Gain: 24 dB */
                {1, 0.15f},     /* Drive: 1.5 (clean) */
                {2, 0.0f},      /* HPF: 40 Hz (lowest) */
                {3, 0.50f},     /* LoGain: flat */
                {5, 0.50f},     /* MidGain: flat */
                {6, 0.50f},     /* HiGain: flat */
                {7, 0.80f},     /* CompThresh: -8 dB (light) */
                {8, 0.11f},     /* CompRatio: ~2:1 */
                {9, 0.25f},     /* TubeMix: 25% */
                {10, 1.0f},     /* Mix: 100% */
            }), ver);
    }

    /* ────────────────────────────────────────────────────────────────
     * mc1.dbx.580 — dbx 580 Mic Preamp
     * Params: 0=Gain,1=Pad,2=Phase,3=Phantom,4=HPF,5=Output
     * ──────────────────────────────────────────────────────────────── */
    else if (eid == "mc1.dbx.580") {

        /* Default — clean preamp at constructor defaults */
        writeFactoryIfMissing(effectId, QStringLiteral("Default"),
                              captureDefaults(fx.get()), ver);
    }

    /* ────────────────────────────────────────────────────────────────
     * mc1.dbx.266xs — dbx 266xs Compressor / Gate
     * Params: 0=Threshold,1=Ratio,2=Attack,3=Release,4=Knee,
     *         5=GateThresh,6=GateRatio,7=Output,8=Mix
     * ──────────────────────────────────────────────────────────────── */
    else if (eid == "mc1.dbx.266xs") {

        /* Default */
        writeFactoryIfMissing(effectId, QStringLiteral("Default"),
                              captureDefaults(fx.get()), ver);

        /* Gentle Comp — soft knee, low ratio */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Gentle Comp"),
            captureWithOverrides(fx.get(), {
                {0, 0.60f},     /* Threshold: -16 dB */
                {1, 0.10f},     /* Ratio: ~2:1 */
                {2, 0.20f},     /* Attack: ~21 ms */
                {3, 0.40f},     /* Release: ~230 ms */
                {4, 1.0f},      /* Knee: Auto/Soft */
                {7, 0.667f},    /* Output: 0 dB */
                {8, 1.0f},      /* Mix: 100% */
            }), ver);

        /* Hard Squeeze — aggressive compression with gate */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Hard Squeeze"),
            captureWithOverrides(fx.get(), {
                {0, 0.40f},     /* Threshold: -24 dB */
                {1, 0.60f},     /* Ratio: ~7.5:1 */
                {2, 0.05f},     /* Attack: ~6 ms (fast) */
                {3, 0.20f},     /* Release: ~140 ms */
                {4, 0.0f},      /* Knee: Hard */
                {5, 0.50f},     /* GateThresh: -50 dB */
                {6, 0.50f},     /* GateRatio: ~50:1 */
                {7, 0.75f},     /* Output: +1.5 dB */
                {8, 1.0f},      /* Mix: 100% */
            }), ver);
    }

    /* ────────────────────────────────────────────────────────────────
     * mc1.dbx.560a — dbx 560A Compressor / Limiter
     * Params: 0=Threshold,1=Ratio,2=Attack,3=Release,4=Knee,
     *         5=ScHPF,6=Auto,7=Output,8=Mix
     * ──────────────────────────────────────────────────────────────── */
    else if (eid == "mc1.dbx.560a") {

        /* Default */
        writeFactoryIfMissing(effectId, QStringLiteral("Default"),
                              captureDefaults(fx.get()), ver);

        /* Mastering Glue — gentle OverEasy comp with auto-release */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Mastering Glue"),
            captureWithOverrides(fx.get(), {
                {0, 0.55f},     /* Threshold: -18 dB */
                {1, 0.10f},     /* Ratio: ~3:1 */
                {2, 0.30f},     /* Attack: ~30 ms */
                {3, 0.50f},     /* Release: ~275 ms */
                {4, 0.80f},     /* Knee: OverEasy (soft) */
                {5, 0.30f},     /* ScHPF: ~145 Hz */
                {6, 1.0f},      /* Auto: on */
                {8, 1.0f},      /* Mix: 100% */
            }), ver);

        /* Drum Bus — fast attack, aggressive, sidechain HPF */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Drum Bus"),
            captureWithOverrides(fx.get(), {
                {0, 0.40f},     /* Threshold: -24 dB */
                {1, 0.35f},     /* Ratio: ~7.5:1 */
                {2, 0.05f},     /* Attack: ~6 ms */
                {3, 0.25f},     /* Release: ~160 ms */
                {4, 0.40f},     /* Knee: moderate */
                {5, 0.50f},     /* ScHPF: ~190 Hz */
                {6, 0.0f},      /* Auto: off */
                {7, 0.75f},     /* Output: +1.5 dB */
                {8, 1.0f},      /* Mix: 100% */
            }), ver);
    }

    /* ────────────────────────────────────────────────────────────────
     * mc1.dbx.520 — dbx 520 De-Esser
     * Params: 0=Frequency,1=Range,2=Threshold,3=Width,4=Listen,
     *         5=Mix,6=Output
     * ──────────────────────────────────────────────────────────────── */
    else if (eid == "mc1.dbx.520") {

        /* Default */
        writeFactoryIfMissing(effectId, QStringLiteral("Default"),
                              captureDefaults(fx.get()), ver);

        /* Vocal De-Ess — standard sibilance taming for voice */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Vocal De-Ess"),
            captureWithOverrides(fx.get(), {
                {0, 0.50f},     /* Frequency: ~6500 Hz */
                {1, 0.50f},     /* Range: -12 dB max reduction */
                {2, 0.55f},     /* Threshold: -18 dB */
                {3, 0.40f},     /* Width: Q ~3.5 */
                {5, 1.0f},      /* Mix: 100% */
            }), ver);

        /* Bright Tame — wider band, taming harsh highs */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Bright Tame"),
            captureWithOverrides(fx.get(), {
                {0, 0.70f},     /* Frequency: ~8700 Hz */
                {1, 0.35f},     /* Range: -8.4 dB */
                {2, 0.45f},     /* Threshold: -22 dB */
                {3, 0.25f},     /* Width: Q ~2.4 (wider) */
                {5, 1.0f},      /* Mix: 100% */
            }), ver);
    }

    /* ────────────────────────────────────────────────────────────────
     * mc1.dbx.510 — dbx 510 Subharmonic Synthesizer
     * Params: 0=Band1Level,1=Band2Level,2=SynthLevel,3=Crossover,
     *         4=Tightness,5=Mix,6=Output
     * ──────────────────────────────────────────────────────────────── */
    else if (eid == "mc1.dbx.510") {

        /* Default */
        writeFactoryIfMissing(effectId, QStringLiteral("Default"),
                              captureDefaults(fx.get()), ver);

        /* Deep Sub — emphasis on lowest band, strong synth */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Deep Sub"),
            captureWithOverrides(fx.get(), {
                {0, 0.80f},     /* Band1Level: 80% (24-36 Hz) */
                {1, 0.40f},     /* Band2Level: 40% (36-56 Hz) */
                {2, 0.70f},     /* SynthLevel: 70% */
                {3, 0.35f},     /* Crossover: ~75 Hz */
                {4, 0.75f},     /* Tightness: 75% */
                {5, 1.0f},      /* Mix: 100% */
            }), ver);

        /* Gentle Bottom — subtle sub enhancement */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Gentle Bottom"),
            captureWithOverrides(fx.get(), {
                {0, 0.45f},     /* Band1Level: 45% */
                {1, 0.55f},     /* Band2Level: 55% */
                {2, 0.40f},     /* SynthLevel: 40% */
                {3, 0.50f},     /* Crossover: ~85 Hz */
                {4, 0.60f},     /* Tightness: 60% */
                {5, 0.70f},     /* Mix: 70% */
            }), ver);
    }

    /* ────────────────────────────────────────────────────────────────
     * mc1.dbx.530 — dbx 530 Parametric EQ
     * Params: 0=HPF,1=LFFreq,2=LFGain,3=LMFFreq,4=LMFGain,5=LMFQ,
     *         6=HMFFreq,7=HMFGain,8=HMFQ,9=HFFreq,10=HFGain,
     *         11=LPF,12=Mix,13=Output
     * ──────────────────────────────────────────────────────────────── */
    else if (eid == "mc1.dbx.530") {

        /* Default */
        writeFactoryIfMissing(effectId, QStringLiteral("Default"),
                              captureDefaults(fx.get()), ver);

        /* Vocal EQ — HPF, presence boost, gentle low warm */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Vocal EQ"),
            captureWithOverrides(fx.get(), {
                {0, 0.22f},     /* HPF: ~104 Hz */
                {1, 0.30f},     /* LFFreq: ~178 Hz */
                {2, 0.54f},     /* LFGain: +1 dB */
                {3, 0.40f},     /* LMFFreq: ~920 Hz */
                {4, 0.46f},     /* LMFGain: -1 dB (scoop) */
                {5, 0.40f},     /* LMFQ: ~3.5 */
                {6, 0.35f},     /* HMFFreq: ~3300 Hz */
                {7, 0.58f},     /* HMFGain: +2 dB (presence) */
                {8, 0.35f},     /* HMFQ: ~3.1 */
                {9, 0.60f},     /* HFFreq: ~10400 Hz */
                {10, 0.55f},    /* HFGain: +1.2 dB (air) */
                {12, 1.0f},     /* Mix: 100% */
            }), ver);

        /* Guitar Scoop — mid cut, low/high presence */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Guitar Scoop"),
            captureWithOverrides(fx.get(), {
                {0, 0.10f},     /* HPF: ~58 Hz */
                {1, 0.20f},     /* LFFreq: ~132 Hz */
                {2, 0.58f},     /* LFGain: +2 dB */
                {3, 0.30f},     /* LMFFreq: ~740 Hz */
                {4, 0.42f},     /* LMFGain: -2 dB */
                {5, 0.30f},     /* LMFQ: ~2.75 */
                {6, 0.55f},     /* HMFFreq: ~4800 Hz */
                {7, 0.54f},     /* HMFGain: +1 dB */
                {8, 0.40f},     /* HMFQ: ~3.5 */
                {9, 0.50f},     /* HFFreq: ~9000 Hz */
                {10, 0.56f},    /* HFGain: +1.5 dB */
                {12, 1.0f},     /* Mix: 100% */
            }), ver);
    }

    /* ════════════════════════════════════════════════════════════════
     * MC1 Flagship — Vocal Producer Pro & Topline Key Finder
     * ════════════════════════════════════════════════════════════════ */

    /* ────────────────────────────────────────────────────────────────
     * mc1.studio.vocal_producer — MC1 Vocal Producer Pro
     * Params: 0=PitchCorrect,1=PitchSpeed,2=Key,3=Drive,4=Compression,
     *         5=EQLow,6=EQMid,7=EQHigh,8=Delay,9=DelayTime,
     *         10=Reverb,11=ReverbDecay,12=Mix,13=Output
     * ──────────────────────────────────────────────────────────────── */
    else if (eid == "mc1.studio.vocal_producer") {

        /* Default */
        writeFactoryIfMissing(effectId, QStringLiteral("Default"),
                              captureDefaults(fx.get()), ver);

        /* Pop Vocal — strong pitch correction, punchy comp, bright */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Pop Vocal"),
            captureWithOverrides(fx.get(), {
                {0, 0.80f},     /* PitchCorrect: 80% */
                {1, 0.20f},     /* PitchSpeed: fast (0=instant) */
                {3, 0.40f},     /* Drive: warm tube */
                {4, 0.60f},     /* Compression: moderate-heavy */
                {5, 0.45f},     /* EQLow: slight cut */
                {6, 0.48f},     /* EQMid: slight scoop */
                {7, 0.58f},     /* EQHigh: +2 dB presence */
                {8, 0.10f},     /* Delay: subtle slapback */
                {9, 0.25f},     /* DelayTime: ~160 ms */
                {10, 0.30f},    /* Reverb: moderate send */
                {11, 0.35f},    /* ReverbDecay: short-medium */
            }), ver);

        /* R&B Smooth — moderate pitch, heavy tube, delay+reverb */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("R&B Smooth"),
            captureWithOverrides(fx.get(), {
                {0, 0.55f},     /* PitchCorrect: moderate */
                {1, 0.50f},     /* PitchSpeed: natural */
                {3, 0.55f},     /* Drive: rich tube warmth */
                {4, 0.55f},     /* Compression: moderate */
                {5, 0.55f},     /* EQLow: warm low boost */
                {6, 0.50f},     /* EQMid: flat */
                {7, 0.52f},     /* EQHigh: gentle air */
                {8, 0.20f},     /* Delay: present slapback */
                {9, 0.35f},     /* DelayTime: ~210 ms */
                {10, 0.35f},    /* Reverb: lush send */
                {11, 0.50f},    /* ReverbDecay: medium */
            }), ver);

        /* Natural — minimal pitch correction, gentle processing */
        fx = EffectFactory::create(eid);
        writeFactoryIfMissing(effectId, QStringLiteral("Natural"),
            captureWithOverrides(fx.get(), {
                {0, 0.25f},     /* PitchCorrect: subtle */
                {1, 0.75f},     /* PitchSpeed: slow (natural) */
                {3, 0.20f},     /* Drive: gentle */
                {4, 0.40f},     /* Compression: light */
                {5, 0.50f},     /* EQLow: flat */
                {6, 0.50f},     /* EQMid: flat */
                {7, 0.50f},     /* EQHigh: flat */
                {8, 0.05f},     /* Delay: barely there */
                {10, 0.15f},    /* Reverb: subtle */
                {11, 0.30f},    /* ReverbDecay: short */
            }), ver);
    }

    /* ────────────────────────────────────────────────────────────────
     * mc1.analyzer.key_finder — MC1 Topline Key Finder
     * Params: 0=Sensitivity,1=Smoothing,2=ConcertPitch,3=Output
     * ──────────────────────────────────────────────────────────────── */
    else if (eid == "mc1.analyzer.key_finder") {

        /* Default */
        writeFactoryIfMissing(effectId, QStringLiteral("Default"),
                              captureDefaults(fx.get()), ver);
    }
}

} // namespace mc1dsp
