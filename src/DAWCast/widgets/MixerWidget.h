// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QWidget>
#include <QHBoxLayout>
#include <QList>

class QFrame;

namespace dawcast { class AudioMixer; }
namespace dawcast { class BusRouter; }
namespace dawcast { class AudioBus; }

namespace dawcast::widgets {

class MixerWidget : public QWidget {
    Q_OBJECT

public:
    explicit MixerWidget(QWidget* parent = nullptr);
    ~MixerWidget() override;

    void setMixer(AudioMixer* mixer);
    void setBusRouter(BusRouter* router);
    void addStrip();
    void removeStrip(int index);

    int stripCount() const;

    /// Rebuild bus strips from the current BusRouter state.
    void rebuildBusStrips();

signals:
    void sendLevelChanged(int trackIndex, int busIndex, float level);
    void busEffectsRequested(int busIndex);

private:
    AudioMixer*  m_mixer     = nullptr;
    BusRouter*   m_busRouter = nullptr;
    int m_stripCount = 0;

    QHBoxLayout* m_stripLayout      = nullptr;
    QFrame*      m_masterSeparator  = nullptr;
    QWidget*     m_masterStrip      = nullptr;
    QFrame*      m_busSeparator     = nullptr;
    QList<QWidget*> m_busStripWidgets;
};

} // namespace dawcast::widgets
