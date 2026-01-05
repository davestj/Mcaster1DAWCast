// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QWidget>
#include <QVBoxLayout>

class QPushButton;

namespace dawcast { class DspChain; class IEffectUnit; }

namespace dawcast::widgets {

class EffectsRackWidget : public QWidget {
    Q_OBJECT

public:
    explicit EffectsRackWidget(QWidget* parent = nullptr);
    ~EffectsRackWidget() override;

    void setDspChain(DspChain* chain);
    void addEffect(IEffectUnit* effect);
    void removeEffect(int index);

    int effectCount() const;

private:
    void showAddEffectMenu();

    DspChain* m_chain = nullptr;
    int m_effectCount = 0;

    QVBoxLayout* m_slotLayout   = nullptr;
    QPushButton* m_addEffectBtn = nullptr;
};

} // namespace dawcast::widgets
