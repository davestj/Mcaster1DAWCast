// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "app.h"
// TODO: #include "audio_engine/AudioEngine.h"
// TODO: #include "config/ThemeEngine.h"
// TODO: #include "core/ProjectManager.h"
// TODO: #include "config/AppConfig.h"

namespace dawcast {

App* App::s_instance = nullptr;

App* App::instance()
{
    if (!s_instance) {
        s_instance = new App();
    }
    return s_instance;
}

App::App(QObject* parent)
    : QObject(parent)
{
    // TODO: Initialize subsystems
    // m_appConfig      = new AppConfig(this);
    // m_audioEngine    = new AudioEngine(this);
    // m_themeEngine    = new ThemeEngine(this);
    // m_projectManager = new ProjectManager(this);
}

App::~App()
{
    s_instance = nullptr;
}

} // namespace dawcast
