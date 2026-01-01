// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QWidget>
#include <QHBoxLayout>

namespace dawcast::audio_engine { class AudioMixer; }

namespace dawcast::widgets {

class MixerWidget : public QWidget {
    Q_OBJECT

public:
    explicit MixerWidget(QWidget* parent = nullptr);
    ~MixerWidget() override;

    void setMixer(audio_engine::AudioMixer* mixer);
    void addStrip();
    void removeStrip(int index);

    int stripCount() const;

private:
    audio_engine::AudioMixer* m_mixer = nullptr;
    int m_stripCount = 0;

    QHBoxLayout* m_stripLayout = nullptr;
};

} // namespace dawcast::widgets
