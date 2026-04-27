// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "PluginHost.h"

#include <QDebug>
#include <cstring>

namespace dawcast::plugins {

PluginHost::PluginHost(QObject* parent)
    : QObject(parent)
{
}

PluginHost::~PluginHost()
{
    unloadPlugin();
}

bool PluginHost::loadPlugin(const PluginInfo& info)
{
    // VST3 / AudioUnit loading is not implemented yet. Previously this
    // method pretended to succeed (returning true + emitting pluginLoaded
    // without actually instantiating the plugin), which let callers think
    // they had a working plugin chain. Return an honest false so UI code
    // can show a "plugin unavailable" message instead of silent failure.
    if (m_loaded) {
        unloadPlugin();
    }
    qWarning() << "PluginHost::loadPlugin: VST3/AudioUnit hosting is not "
                  "implemented yet. Requested:" << info.name << "at" << info.path;
    m_info = PluginInfo();
    m_loaded = false;
    m_pluginInstance = nullptr;
    return false;
}

void PluginHost::unloadPlugin()
{
    if (!m_loaded) return;

    // TODO: Actual plugin unloading
    //
    // For VST3:
    //   1. Deactivate processing
    //   2. Release IComponent and IEditController
    //   3. Close the module
    //
    // For AudioUnit:
    //   1. AudioUnitUninitialize()
    //   2. AudioComponentInstanceDispose()

    m_pluginInstance = nullptr;
    m_loaded = false;
    m_info = PluginInfo();

    emit pluginUnloaded();
}

void PluginHost::processBlock(float* buffer, int frames, int channels)
{
    // No hosted plugin — loadPlugin always returns false today. Leave the
    // buffer untouched so the upstream audio path passes through unchanged.
    Q_UNUSED(buffer);
    Q_UNUSED(frames);
    Q_UNUSED(channels);
}

} // namespace dawcast::plugins
