// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "EffectsRackWidget.h"
#include "BevelButton.h"
#include "DspChain.h"
#include "IEffectUnit.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QMenu>
#include <QListWidget>
#include <QFrame>

namespace dawcast::widgets {

namespace {
const QString kSlotStyle = QStringLiteral(
    "QWidget#effectSlot { background: #2a2a30; border: 1px solid #3a3a42; "
    "border-radius: 3px; padding: 2px; }");
const QString kSlotBypassedStyle = QStringLiteral(
    "QWidget#effectSlot { background: #222228; border: 1px solid #333; "
    "border-radius: 3px; padding: 2px; }");
const QString kDragHandleStyle = QStringLiteral(
    "QLabel { color: #666; font-size: 14px; font-weight: bold; padding: 0 2px; }");
const QString kEffectNameStyle = QStringLiteral(
    "QLabel { color: #ccc; font-size: 11px; font-weight: bold; }");
const QString kEffectNameBypassedStyle = QStringLiteral(
    "QLabel { color: #666; font-size: 11px; font-weight: bold; font-style: italic; }");
const QString kAddBtnStyle = QStringLiteral(
    "QPushButton { background: #353540; color: #aaa; border: 1px dashed #555; "
    "border-radius: 3px; padding: 6px; font-size: 11px; }"
    "QPushButton:hover { background: #404050; color: #ccc; }");

// Available built-in effects for the Add Effect menu
struct EffectDef {
    const char* name;
    const char* category;
};

static const EffectDef kBuiltinEffects[] = {
    {"Parametric EQ",       "EQ"},
    {"Graphic EQ",          "EQ"},
    {"High-Pass Filter",    "EQ"},
    {"Low-Pass Filter",     "EQ"},
    {"Compressor",          "Dynamics"},
    {"Limiter",             "Dynamics"},
    {"Gate",                "Dynamics"},
    {"De-Esser",            "Dynamics"},
    {"Reverb",              "Spatial"},
    {"Delay",               "Spatial"},
    {"Chorus",              "Modulation"},
    {"Phaser",              "Modulation"},
    {"Flanger",             "Modulation"},
    {"Noise Reduction",     "Restoration"},
    {"Loudness Meter",      "Metering"},
};
constexpr int kBuiltinEffectCount = sizeof(kBuiltinEffects) / sizeof(kBuiltinEffects[0]);
} // anonymous namespace

EffectsRackWidget::EffectsRackWidget(QWidget* parent)
    : QWidget(parent)
{
    setStyleSheet(QStringLiteral("EffectsRackWidget { background: #222228; }"));

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet(QStringLiteral("QScrollArea { background: #222228; border: none; }"));

    auto* container = new QWidget(scrollArea);
    container->setStyleSheet(QStringLiteral("background: #222228;"));
    m_slotLayout = new QVBoxLayout(container);
    m_slotLayout->setContentsMargins(4, 4, 4, 4);
    m_slotLayout->setSpacing(3);
    m_slotLayout->setAlignment(Qt::AlignTop);

    // "Add Effect" button at the bottom (always last)
    m_addEffectBtn = new QPushButton(tr("+ Add Effect"), container);
    m_addEffectBtn->setStyleSheet(kAddBtnStyle);
    m_addEffectBtn->setCursor(Qt::PointingHandCursor);
    m_slotLayout->addWidget(m_addEffectBtn);

    connect(m_addEffectBtn, &QPushButton::clicked, this, &EffectsRackWidget::showAddEffectMenu);

    scrollArea->setWidget(container);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(scrollArea);
}

EffectsRackWidget::~EffectsRackWidget() = default;

void EffectsRackWidget::setDspChain(DspChain* chain)
{
    m_chain = chain;

    // Clear existing slots (keep the Add button)
    while (m_slotLayout->count() > 1) {
        auto* item = m_slotLayout->takeAt(0);
        if (item && item->widget() && item->widget() != m_addEffectBtn) {
            delete item->widget();
        }
        delete item;
    }
    m_effectCount = 0;

    // Rebuild from chain
    if (m_chain) {
        for (int i = 0; i < m_chain->effectCount(); ++i) {
            addEffect(m_chain->effect(i));
        }
    }
}

void EffectsRackWidget::addEffect(IEffectUnit* effect)
{
    int slotIndex = m_effectCount;

    auto* slot = new QWidget(this);
    slot->setObjectName(QStringLiteral("effectSlot"));
    slot->setStyleSheet(kSlotStyle);
    slot->setFixedHeight(36);

    auto* layout = new QHBoxLayout(slot);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(4);

    // Drag handle
    auto* dragHandle = new QLabel(QStringLiteral("\u2261"), slot); // triple bar character
    dragHandle->setStyleSheet(kDragHandleStyle);
    dragHandle->setCursor(Qt::OpenHandCursor);
    dragHandle->setFixedWidth(16);
    layout->addWidget(dragHandle);

    // Effect name
    QString effectName = effect ? effect->name() : tr("Effect %1").arg(slotIndex + 1);
    auto* nameLabel = new QLabel(effectName, slot);
    nameLabel->setStyleSheet(kEffectNameStyle);
    layout->addWidget(nameLabel, 1);

    // Bypass toggle
    auto* bypassBtn = new BevelButton(tr("BYP"), slot);
    bypassBtn->setCheckable(true);
    bypassBtn->setFixedSize(40, 24);
    bypassBtn->setCheckedFaceColor(QColor(180, 140, 50));
    if (effect) bypassBtn->setChecked(effect->isBypassed());
    layout->addWidget(bypassBtn);

    // Connect bypass
    connect(bypassBtn, &BevelButton::toggled, this, [this, effect, slot, nameLabel](bool bypassed) {
        if (effect) effect->setBypassed(bypassed);
        slot->setStyleSheet(bypassed ? kSlotBypassedStyle : kSlotStyle);
        nameLabel->setStyleSheet(bypassed ? kEffectNameBypassedStyle : kEffectNameStyle);
    });

    // Edit button
    auto* editBtn = new QPushButton(tr("Edit"), slot);
    editBtn->setFixedSize(40, 24);
    editBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #3a3a42; color: #bbb; border: 1px solid #555; "
        "border-radius: 2px; font-size: 10px; }"
        "QPushButton:hover { background: #4a4a52; }"));
    layout->addWidget(editBtn);

    connect(editBtn, &QPushButton::clicked, this, [slotIndex] {
        // In a full implementation, this would open an effect editor dialog
        Q_UNUSED(slotIndex);
    });

    // Remove button
    auto* removeBtn = new QPushButton(QStringLiteral("\u00D7"), slot); // multiplication sign (X)
    removeBtn->setFixedSize(24, 24);
    removeBtn->setToolTip(tr("Remove effect"));
    removeBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: transparent; color: #888; border: none; font-size: 14px; font-weight: bold; }"
        "QPushButton:hover { color: #e44; }"));
    layout->addWidget(removeBtn);

    connect(removeBtn, &QPushButton::clicked, this, [this, slotIndex] {
        removeEffect(slotIndex);
    });

    // Insert before the "Add Effect" button
    int insertPos = m_effectCount;
    m_slotLayout->insertWidget(insertPos, slot);
    ++m_effectCount;
}

void EffectsRackWidget::removeEffect(int index)
{
    if (index < 0 || index >= m_effectCount) return;

    auto* item = m_slotLayout->takeAt(index);
    if (item && item->widget()) {
        delete item->widget();
    }
    delete item;
    --m_effectCount;

    // Also remove from the DSP chain if connected
    if (m_chain && index < m_chain->effectCount()) {
        m_chain->removeEffect(index);
    }
}

int EffectsRackWidget::effectCount() const
{
    return m_effectCount;
}

void EffectsRackWidget::showAddEffectMenu()
{
    QMenu menu(this);

    // Build categorized submenus
    QMap<QString, QMenu*> categories;

    for (int i = 0; i < kBuiltinEffectCount; ++i) {
        QString category = QString::fromLatin1(kBuiltinEffects[i].category);
        QString name = QString::fromLatin1(kBuiltinEffects[i].name);

        if (!categories.contains(category)) {
            categories[category] = menu.addMenu(category);
        }

        categories[category]->addAction(name, this, [this, name] {
            // In a full implementation, this would instantiate the actual effect.
            // For now, we add a placeholder slot.
            addEffect(nullptr);

            // Update the name label of the just-added slot
            int lastSlotIdx = m_effectCount - 1;
            if (lastSlotIdx >= 0 && lastSlotIdx < m_slotLayout->count()) {
                auto* slotWidget = m_slotLayout->itemAt(lastSlotIdx)->widget();
                if (slotWidget) {
                    auto* nameLabel = slotWidget->findChild<QLabel*>(QString(), Qt::FindDirectChildrenOnly);
                    // Find the name label (second QLabel child, after drag handle)
                    auto labels = slotWidget->findChildren<QLabel*>();
                    if (labels.size() >= 2) {
                        labels[1]->setText(name);
                    }
                }
            }
        });
    }

    menu.exec(m_addEffectBtn->mapToGlobal(QPoint(0, m_addEffectBtn->height())));
}

} // namespace dawcast::widgets
