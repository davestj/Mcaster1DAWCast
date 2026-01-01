// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QWidget>
#include <QVBoxLayout>

namespace dawcast::dsp { class DspChain; class IEffectUnit; }

namespace dawcast::widgets {

class EffectsRackWidget : public QWidget {
    Q_OBJECT

public:
    explicit EffectsRackWidget(QWidget* parent = nullptr);
    ~EffectsRackWidget() override;

    void setDspChain(dsp::DspChain* chain);
    void addEffect(dsp::IEffectUnit* effect);
    void removeEffect(int index);

    int effectCount() const;

private:
    dsp::DspChain* m_chain = nullptr;
    int m_effectCount = 0;

    QVBoxLayout* m_slotLayout = nullptr;
};

} // namespace dawcast::widgets
