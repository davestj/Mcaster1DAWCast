// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "app.h"
#include "audio_engine/AudioEngine.h"
#include "widgets/ThemeEngine.h"
#include "core/ProjectManager.h"
#include "config/AppConfig.h"

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
    // Initialize subsystems in dependency order:
    // 1. Configuration first (other subsystems may read config values)
    m_appConfig = dawcast::config::AppConfig::instance();

    // 2. Theme engine (UI styling, loaded before widgets are created)
    m_themeEngine = dawcast::widgets::ThemeEngine::instance();

    // 3. Audio engine (PortAudio initialization, sample rate, buffer size)
    m_audioEngine = new AudioEngine(this);

    // 4. Project manager (depends on audio engine for sample rate defaults)
    m_projectManager = new ProjectManager(this);
}

App::~App()
{
    // Audio engine and project manager are parented to this QObject,
    // so they will be destroyed automatically by Qt's parent-child system.
    // ThemeEngine and AppConfig are singletons managed by their own static lifetime.
    s_instance = nullptr;
}

} // namespace dawcast
