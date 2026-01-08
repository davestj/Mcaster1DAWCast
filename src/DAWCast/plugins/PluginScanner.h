// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QList>
#include <QString>

namespace dawcast::plugins {

struct PluginInfo {
    QString id;
    QString name;
    QString vendor;
    QString path;
    enum Format { VST3, AudioUnit, LV2 };
    Format format      = VST3;
    int numAudioInputs  = 0;
    int numAudioOutputs = 2;
    bool isInstrument   = false;
    bool isSynth        = false;
};

class PluginScanner : public QObject
{
    Q_OBJECT

public:
    static PluginScanner* instance();

    void scanPaths();
    [[nodiscard]] QList<PluginInfo> availablePlugins() const;
    [[nodiscard]] int pluginCount() const { return m_plugins.size(); }

signals:
    void scanStarted();
    void scanProgress(const QString& currentPath);
    void scanComplete(int pluginCount);

private:
    explicit PluginScanner(QObject* parent = nullptr);
    ~PluginScanner() override;

    void scanVST3(const QString& directory);
#ifdef __APPLE__
    void scanAU();
#endif

    QList<PluginInfo> m_plugins;
    static PluginScanner* s_instance;
};

} // namespace dawcast::plugins
