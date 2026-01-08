// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QString>

#include "PluginScanner.h"

namespace dawcast::plugins {

class PluginHost : public QObject
{
    Q_OBJECT

public:
    explicit PluginHost(QObject* parent = nullptr);
    ~PluginHost() override;

    bool loadPlugin(const PluginInfo& info);
    void unloadPlugin();

    // Process audio through the loaded plugin
    void processBlock(float* buffer, int frames, int channels);

    [[nodiscard]] bool isLoaded() const { return m_loaded; }
    [[nodiscard]] QString pluginName() const { return m_info.name; }
    [[nodiscard]] const PluginInfo& pluginInfo() const { return m_info; }

signals:
    void pluginLoaded(const QString& name);
    void pluginUnloaded();
    void processingError(const QString& message);

private:
    PluginInfo m_info;
    bool       m_loaded         = false;
    void*      m_pluginInstance  = nullptr;  // Opaque: VST3 IComponent* or AudioComponentInstance
};

} // namespace dawcast::plugins
