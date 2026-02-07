// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QIcon>

namespace dawcast {

class ViewModeManager : public QObject {
    Q_OBJECT

public:
    enum Mode {
        Podcaster,
        Producer,
        DJLive,
        StudioArtist,
        VoiceOver,
        GuitarFX
    };
    Q_ENUM(Mode)

    static ViewModeManager* instance();

    void setMode(Mode mode);
    Mode currentMode() const;

    QString modeName(Mode mode) const;
    QString modeDescription(Mode mode) const;
    QIcon   modeIcon(Mode mode) const;

    static constexpr int modeCount() { return 6; }

    // Panel visibility configuration for each view mode.
    // MainWindow reads this to show/hide dock widgets when the mode changes.
    struct WorkspaceLayout {
        bool showMixer          = true;
        bool showVideoPreview   = false;
        bool showMediaBrowser   = true;
        bool showEffectsRack    = true;
        bool showChapterWidget  = false;
        bool showMetadataPanel  = false;
        bool showLUFSMeter      = true;
        bool showAIPanel        = false;
        bool showPianoRoll      = false;
        bool showSpectrumWidget = false;

        // Mode-specific panels
        bool showScriptReader   = false;  // Voice Over
        bool showPedalboard     = false;  // Guitar FX
        bool showStreamMonitor  = false;  // DJ/Live
        bool showBroadcastClock = false;  // DJ/Live
    };

    WorkspaceLayout layoutForMode(Mode mode) const;

signals:
    void modeChanged(Mode mode);

private:
    explicit ViewModeManager(QObject* parent = nullptr);
    ~ViewModeManager() override;

    void persistMode();
    void restoreMode();

    Mode m_currentMode = Producer;

    static ViewModeManager* s_instance;
};

} // namespace dawcast
