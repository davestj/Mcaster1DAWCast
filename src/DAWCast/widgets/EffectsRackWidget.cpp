// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "EffectsRackWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>

namespace dawcast::widgets {

EffectsRackWidget::EffectsRackWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);

    auto* container = new QWidget(scrollArea);
    m_slotLayout = new QVBoxLayout(container);
    m_slotLayout->setAlignment(Qt::AlignTop);
    scrollArea->setWidget(container);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(scrollArea);
}

EffectsRackWidget::~EffectsRackWidget() = default;

void EffectsRackWidget::setDspChain(dsp::DspChain* chain)
{
    m_chain = chain;
    // TODO: rebuild slots from chain
}

void EffectsRackWidget::addEffect(dsp::IEffectUnit* /*effect*/)
{
    auto* slot = new QWidget(this);
    auto* layout = new QHBoxLayout(slot);

    // Drag handle
    auto* dragHandle = new QLabel(QStringLiteral("::"), slot);
    layout->addWidget(dragHandle);

    // Effect name
    auto* nameLabel = new QLabel(tr("Effect %1").arg(m_effectCount + 1), slot);
    layout->addWidget(nameLabel, 1);

    // Bypass button
    auto* bypassBtn = new QPushButton(tr("Bypass"), slot);
    bypassBtn->setCheckable(true);
    layout->addWidget(bypassBtn);

    // Edit button
    auto* editBtn = new QPushButton(tr("Edit"), slot);
    layout->addWidget(editBtn);

    m_slotLayout->addWidget(slot);
    ++m_effectCount;
}

void EffectsRackWidget::removeEffect(int index)
{
    if (index < 0 || index >= m_slotLayout->count()) return;

    auto* item = m_slotLayout->takeAt(index);
    if (item && item->widget()) {
        delete item->widget();
    }
    delete item;
    --m_effectCount;
}

int EffectsRackWidget::effectCount() const
{
    return m_effectCount;
}

} // namespace dawcast::widgets
