// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>

namespace dawcast {

class AudioEngine;
class ThemeEngine;
class ProjectManager;
class AppConfig;

class App : public QObject
{
    Q_OBJECT

public:
    static App* instance();

    AudioEngine*    audioEngine()    const { return m_audioEngine; }
    ThemeEngine*    themeEngine()    const { return m_themeEngine; }
    ProjectManager* projectManager() const { return m_projectManager; }
    AppConfig*      appConfig()      const { return m_appConfig; }

private:
    explicit App(QObject* parent = nullptr);
    ~App() override;

    static App* s_instance;

    AudioEngine*    m_audioEngine    = nullptr;
    ThemeEngine*    m_themeEngine    = nullptr;
    ProjectManager* m_projectManager = nullptr;
    AppConfig*      m_appConfig      = nullptr;
};

} // namespace dawcast
