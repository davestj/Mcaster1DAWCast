// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ViewModeManager.h"
#include "../config/AppConfig.h"

#include <QMetaEnum>

namespace dawcast {

ViewModeManager* ViewModeManager::s_instance = nullptr;

ViewModeManager::ViewModeManager(QObject* parent)
    : QObject(parent)
{
    restoreMode();
}

ViewModeManager::~ViewModeManager() = default;

ViewModeManager* ViewModeManager::instance()
{
    if (!s_instance)
        s_instance = new ViewModeManager();
    return s_instance;
}

// ── Mode Access ────────────────────────────────────────────────────────────

void ViewModeManager::setMode(Mode mode)
{
    if (m_currentMode == mode)
        return;

    m_currentMode = mode;
    persistMode();
    emit modeChanged(mode);
}

ViewModeManager::Mode ViewModeManager::currentMode() const
{
    return m_currentMode;
}

// ── Display Strings ────────────────────────────────────────────────────────

QString ViewModeManager::modeName(Mode mode) const
{
    switch (mode) {
    case Podcaster:    return tr("Podcaster");
    case Producer:     return tr("Producer");
    case DJLive:       return tr("DJ / Live");
    case StudioArtist: return tr("Studio Artist");
    case VoiceOver:    return tr("Voice Over");
    case GuitarFX:     return tr("Guitar FX");
    }
    return tr("Unknown");
}

QString ViewModeManager::modeDescription(Mode mode) const
{
    switch (mode) {
    case Podcaster:
        return tr("Record interviews, edit episodes, manage chapters and publish podcasts.");
    case Producer:
        return tr("Full-featured production workspace with timeline, mixer, effects, and video.");
    case DJLive:
        return tr("Live streaming and DJ workspace with crossfader, broadcast clock, and stream monitor.");
    case StudioArtist:
        return tr("Recording and arranging workspace with mixer, piano roll, effects, and metronome.");
    case VoiceOver:
        return tr("Voice over workspace with script reader, noise reduction, waveform zoom, and punch-in/out.");
    case GuitarFX:
        return tr("Pedalboard-style effects chain for guitar processing and tone shaping.");
    }
    return {};
}

QIcon ViewModeManager::modeIcon(Mode mode) const
{
    // Return theme icons — the ViewModeSwitcher will fall back to text labels
    // if icons are not available in the current icon theme.
    switch (mode) {
    case Podcaster:    return QIcon::fromTheme(QStringLiteral("audio-input-microphone"));
    case Producer:     return QIcon::fromTheme(QStringLiteral("preferences-desktop-multimedia"));
    case DJLive:       return QIcon::fromTheme(QStringLiteral("network-wireless"));
    case StudioArtist: return QIcon::fromTheme(QStringLiteral("audio-x-generic"));
    case VoiceOver:    return QIcon::fromTheme(QStringLiteral("user-available"));
    case GuitarFX:     return QIcon::fromTheme(QStringLiteral("media-eq-symbolic"));
    }
    return {};
}

// ── Workspace Layouts ──────────────────────────────────────────────────────

ViewModeManager::WorkspaceLayout ViewModeManager::layoutForMode(Mode mode) const
{
    WorkspaceLayout layout;

    switch (mode) {

    case Podcaster:
        // Timeline + Chapters + Metadata/RSS + LUFS + Media Browser
        layout.showMixer          = false;
        layout.showVideoPreview   = false;
        layout.showMediaBrowser   = true;
        layout.showEffectsRack    = false;
        layout.showChapterWidget  = true;
        layout.showMetadataPanel  = true;
        layout.showLUFSMeter      = true;
        layout.showAIPanel        = false;
        layout.showPianoRoll      = false;
        layout.showSpectrumWidget = false;
        layout.showScriptReader   = false;
        layout.showPedalboard     = false;
        layout.showStreamMonitor  = false;
        layout.showBroadcastClock = false;
        break;

    case Producer:
        // Full-featured: mixer, effects, video, spectrum
        layout.showMixer          = true;
        layout.showVideoPreview   = true;
        layout.showMediaBrowser   = true;
        layout.showEffectsRack    = true;
        layout.showChapterWidget  = false;
        layout.showMetadataPanel  = false;
        layout.showLUFSMeter      = true;
        layout.showAIPanel        = true;
        layout.showPianoRoll      = false;
        layout.showSpectrumWidget = true;
        layout.showScriptReader   = false;
        layout.showPedalboard     = false;
        layout.showStreamMonitor  = false;
        layout.showBroadcastClock = false;
        break;

    case DJLive:
        // Crossfader + Stream monitor + Broadcast clock + LUFS
        layout.showMixer          = true;
        layout.showVideoPreview   = false;
        layout.showMediaBrowser   = true;
        layout.showEffectsRack    = false;
        layout.showChapterWidget  = false;
        layout.showMetadataPanel  = false;
        layout.showLUFSMeter      = true;
        layout.showAIPanel        = false;
        layout.showPianoRoll      = false;
        layout.showSpectrumWidget = false;
        layout.showScriptReader   = false;
        layout.showPedalboard     = false;
        layout.showStreamMonitor  = true;
        layout.showBroadcastClock = true;
        break;

    case StudioArtist:
        // Recording + Mixer + Effects + Piano Roll
        layout.showMixer          = true;
        layout.showVideoPreview   = false;
        layout.showMediaBrowser   = true;
        layout.showEffectsRack    = true;
        layout.showChapterWidget  = false;
        layout.showMetadataPanel  = false;
        layout.showLUFSMeter      = true;
        layout.showAIPanel        = false;
        layout.showPianoRoll      = true;
        layout.showSpectrumWidget = false;
        layout.showScriptReader   = false;
        layout.showPedalboard     = false;
        layout.showStreamMonitor  = false;
        layout.showBroadcastClock = false;
        break;

    case VoiceOver:
        // Script reader + Recording controls + Waveform zoom
        layout.showMixer          = false;
        layout.showVideoPreview   = false;
        layout.showMediaBrowser   = false;
        layout.showEffectsRack    = true;   // Noise reduction prominent
        layout.showChapterWidget  = false;
        layout.showMetadataPanel  = false;
        layout.showLUFSMeter      = true;
        layout.showAIPanel        = false;
        layout.showPianoRoll      = false;
        layout.showSpectrumWidget = false;
        layout.showScriptReader   = true;
        layout.showPedalboard     = false;
        layout.showStreamMonitor  = false;
        layout.showBroadcastClock = false;
        break;

    case GuitarFX:
        // Pedalboard-style effects chain
        layout.showMixer          = false;
        layout.showVideoPreview   = false;
        layout.showMediaBrowser   = false;
        layout.showEffectsRack    = false;  // Replaced by pedalboard
        layout.showChapterWidget  = false;
        layout.showMetadataPanel  = false;
        layout.showLUFSMeter      = true;
        layout.showAIPanel        = false;
        layout.showPianoRoll      = false;
        layout.showSpectrumWidget = true;   // Show spectrum for tone feedback
        layout.showScriptReader   = false;
        layout.showPedalboard     = true;
        layout.showStreamMonitor  = false;
        layout.showBroadcastClock = false;
        break;
    }

    return layout;
}

// ── Persistence ────────────────────────────────────────────────────────────

void ViewModeManager::persistMode()
{
    auto* cfg = config::AppConfig::instance();
    if (!cfg) return;

    QMetaEnum me = QMetaEnum::fromType<Mode>();
    cfg->setValue(QStringLiteral("viewMode/current"),
                  QString::fromLatin1(me.valueToKey(static_cast<int>(m_currentMode))));
    cfg->save();
}

void ViewModeManager::restoreMode()
{
    auto* cfg = config::AppConfig::instance();
    if (!cfg) return;

    QString stored = cfg->value(QStringLiteral("viewMode/current"),
                                QStringLiteral("Producer")).toString();

    QMetaEnum me = QMetaEnum::fromType<Mode>();
    bool ok = false;
    int val = me.keyToValue(stored.toLatin1().constData(), &ok);
    if (ok && val >= 0 && val <= static_cast<int>(GuitarFX))
        m_currentMode = static_cast<Mode>(val);
    else
        m_currentMode = Producer;
}

} // namespace dawcast
