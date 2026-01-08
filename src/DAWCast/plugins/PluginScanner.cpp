// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "PluginScanner.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QUuid>

namespace dawcast::plugins {

PluginScanner* PluginScanner::s_instance = nullptr;

PluginScanner::PluginScanner(QObject* parent)
    : QObject(parent)
{
}

PluginScanner::~PluginScanner() = default;

PluginScanner* PluginScanner::instance()
{
    if (!s_instance) {
        s_instance = new PluginScanner(nullptr);
    }
    return s_instance;
}

void PluginScanner::scanPaths()
{
    m_plugins.clear();
    emit scanStarted();

    // ── VST3 standard paths ────────────────────────────────────────────────
    // System-wide
    scanVST3(QStringLiteral("/Library/Audio/Plug-Ins/VST3"));

    // User-local
    QString homePath = QDir::homePath();
    scanVST3(homePath + QStringLiteral("/Library/Audio/Plug-Ins/VST3"));

#ifdef __APPLE__
    // ── AudioUnit enumeration ──────────────────────────────────────────────
    scanAU();
#endif

    emit scanComplete(m_plugins.size());
}

void PluginScanner::scanVST3(const QString& directory)
{
    QDir dir(directory);
    if (!dir.exists()) return;

    emit scanProgress(directory);

    QDirIterator it(directory, QStringList() << QStringLiteral("*.vst3"),
                    QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);

    while (it.hasNext()) {
        QString bundlePath = it.next();
        QFileInfo fi(bundlePath);

        PluginInfo info;
        info.path   = bundlePath;
        info.format = PluginInfo::VST3;
        info.id     = QUuid::createUuid().toString(QUuid::WithoutBraces);

        // Try to read name from the bundle's Info.plist
        QString plistPath = bundlePath + QStringLiteral("/Contents/Info.plist");
        QFileInfo plistFile(plistPath);
        if (plistFile.exists()) {
            QSettings plist(plistPath, QSettings::NativeFormat);
            info.name   = plist.value(QStringLiteral("CFBundleName"), fi.baseName()).toString();
            info.vendor = plist.value(QStringLiteral("CFBundleGetInfoString"), QString()).toString();

            // Check for instrument category in Info.plist
            QString category = plist.value(QStringLiteral("AudioComponentDescription/type"), QString()).toString();
            if (category == QLatin1String("aumu") || category == QLatin1String("aumi")) {
                info.isInstrument = true;
                info.isSynth = true;
            }
        } else {
            // Fallback: use bundle name
            info.name = fi.baseName();
        }

        if (info.name.isEmpty())
            info.name = fi.baseName();

        // Default I/O for effects
        info.numAudioInputs  = 2;
        info.numAudioOutputs = 2;

        m_plugins.append(info);
    }
}

QList<PluginInfo> PluginScanner::availablePlugins() const
{
    return m_plugins;
}

} // namespace dawcast::plugins
