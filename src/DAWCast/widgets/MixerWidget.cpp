// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "MixerWidget.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSlider>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>

namespace dawcast::widgets {

MixerWidget::MixerWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    auto* container = new QWidget(scrollArea);
    m_stripLayout = new QHBoxLayout(container);
    m_stripLayout->setAlignment(Qt::AlignLeft);
    scrollArea->setWidget(container);

    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(scrollArea);
}

MixerWidget::~MixerWidget() = default;

void MixerWidget::setMixer(audio_engine::AudioMixer* mixer)
{
    m_mixer = mixer;
    // TODO: rebuild strips from mixer state
}

void MixerWidget::addStrip()
{
    auto* strip = new QWidget(this);
    auto* layout = new QVBoxLayout(strip);

    layout->addWidget(new QLabel(tr("Ch %1").arg(m_stripCount + 1), strip));

    auto* fader = new QSlider(Qt::Vertical, strip);
    fader->setRange(0, 127);
    fader->setValue(100);
    layout->addWidget(fader);

    auto* muteBtn = new QPushButton(tr("M"), strip);
    muteBtn->setCheckable(true);
    layout->addWidget(muteBtn);

    auto* soloBtn = new QPushButton(tr("S"), strip);
    soloBtn->setCheckable(true);
    layout->addWidget(soloBtn);

    // TODO: pan knob, VU meter

    m_stripLayout->addWidget(strip);
    ++m_stripCount;
}

void MixerWidget::removeStrip(int index)
{
    if (index < 0 || index >= m_stripLayout->count()) return;

    auto* item = m_stripLayout->takeAt(index);
    if (item && item->widget()) {
        delete item->widget();
    }
    delete item;
    --m_stripCount;
}

int MixerWidget::stripCount() const
{
    return m_stripCount;
}

} // namespace dawcast::widgets
