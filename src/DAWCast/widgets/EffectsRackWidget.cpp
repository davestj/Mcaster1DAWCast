// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "EffectsRackWidget.h"
#include "BevelButton.h"
#include "ParametricEQDialog.h"
#include "DspChain.h"
#include "IEffectUnit.h"
#include "ParametricEQ.h"
#include "NoiseReduction.h"
#include "Compressor.h"
#include "Limiter.h"
#include "NoiseGate.h"
#include "DeEsser.h"
#include "Reverb.h"
#include "GraphicEQ31.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QMenu>
#include <QListWidget>
#include <QFrame>
#include <QDialog>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>

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

namespace {
/// Open a minimal generic parameter editor for any IEffectUnit. Shows one
/// QDoubleSpinBox per parameter so the user can tweak values without needing
/// an effect-specific UI. Parameters are written live via setParameter().
void openGenericEffectEditor(IEffectUnit* effect, QWidget* parent)
{
    if (!effect) return;

    auto* dialog = new QDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(QObject::tr("%1 — Parameters").arg(effect->name()));

    auto* form = new QFormLayout;

    const int count = effect->parameterCount();
    for (int i = 0; i < count; ++i) {
        auto* spin = new QDoubleSpinBox(dialog);
        spin->setDecimals(3);
        spin->setRange(-1e6, 1e6);
        spin->setValue(effect->parameter(i));
        QObject::connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                         dialog, [effect, i](double v) {
            effect->setParameter(i, static_cast<float>(v));
        });
        form->addRow(QObject::tr("Param %1").arg(i + 1), spin);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::accept);

    auto* layout = new QVBoxLayout(dialog);
    layout->addLayout(form);
    layout->addWidget(buttons);

    dialog->show();
}
} // namespace

void EffectsRackWidget::addEffect(IEffectUnit* effect)
{
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
    QString effectName = effect ? effect->name() : tr("Effect %1").arg(m_effectCount + 1);
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

    // Connect bypass — directly drives the effect unit's bypass state.
    connect(bypassBtn, &BevelButton::toggled, this,
            [this, slot, nameLabel](bool bypassed) {
        // Resolve the effect dynamically by looking up the slot's current
        // position in the layout so that reordering / removes don't strand
        // the callback with a stale pointer.
        int idx = m_slotLayout->indexOf(slot);
        IEffectUnit* fx = (m_chain && idx >= 0 && idx < m_chain->effectCount())
                              ? m_chain->effect(idx) : nullptr;
        if (fx) fx->setBypassed(bypassed);
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

    connect(editBtn, &QPushButton::clicked, this, [this, slot] {
        int idx = m_slotLayout->indexOf(slot);
        IEffectUnit* fx = (m_chain && idx >= 0 && idx < m_chain->effectCount())
                              ? m_chain->effect(idx) : nullptr;
        if (!fx) return;

        // ParametricEQ gets its bespoke visual editor
        if (auto* peq = dynamic_cast<ParametricEQ*>(fx)) {
            auto* dialog = new ParametricEQDialog(peq, this);
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            dialog->show();
            return;
        }

        // Every other effect falls back to the generic param-per-spinbox dialog
        openGenericEffectEditor(fx, this);
    });

    // Remove button
    auto* removeBtn = new QPushButton(QStringLiteral("\u00D7"), slot); // multiplication sign (X)
    removeBtn->setFixedSize(24, 24);
    removeBtn->setToolTip(tr("Remove effect"));
    removeBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: transparent; color: #888; border: none; font-size: 14px; font-weight: bold; }"
        "QPushButton:hover { color: #e44; }"));
    layout->addWidget(removeBtn);

    connect(removeBtn, &QPushButton::clicked, this, [this, slot] {
        int idx = m_slotLayout->indexOf(slot);
        if (idx >= 0) removeEffect(idx);
    });

    // Insert before the "Add Effect" button
    int insertPos = m_effectCount;
    m_slotLayout->insertWidget(insertPos, slot);
    ++m_effectCount;
}

void EffectsRackWidget::removeEffect(int index)
{
    if (index < 0 || index >= m_effectCount) return;

    // Detach the effect from the chain first so the audio thread stops
    // calling into it, then delete it after the slot widget goes away.
    IEffectUnit* fx = nullptr;
    if (m_chain && index < m_chain->effectCount()) {
        fx = m_chain->effect(index);
        m_chain->removeEffect(index);
    }

    auto* item = m_slotLayout->takeAt(index);
    if (item && item->widget()) {
        delete item->widget();
    }
    delete item;
    --m_effectCount;

    // DspChain holds raw pointers — nothing else owns the effect, so we
    // need to free it here to avoid a leak.
    delete fx;
}

int EffectsRackWidget::effectCount() const
{
    return m_effectCount;
}

void EffectsRackWidget::showAddEffectMenu()
{
    // Nothing to add to if no chain is attached (i.e. no track selected).
    // Still show the menu so the user gets feedback, but do nothing on click.
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
            if (!m_chain) return;  // No track selected — nothing to append to

            // Instantiate the effect if we have a concrete implementation.
            IEffectUnit* effect = nullptr;

            if      (name == QLatin1String("Parametric EQ"))    effect = new ParametricEQ();
            else if (name == QLatin1String("Graphic EQ"))       effect = new GraphicEQ31();
            else if (name == QLatin1String("Compressor"))       effect = new Compressor();
            else if (name == QLatin1String("Limiter"))          effect = new Limiter();
            else if (name == QLatin1String("Gate"))             effect = new NoiseGate();
            else if (name == QLatin1String("De-Esser"))         effect = new DeEsser();
            else if (name == QLatin1String("Reverb"))           effect = new Reverb();
            else if (name == QLatin1String("Noise Reduction"))  effect = new NoiseReduction();
            // Remaining slots ("High/Low-Pass Filter", "Delay", "Chorus",
            // "Phaser", "Flanger", "Loudness Meter") have no dedicated DSP
            // class yet — skip them until implementation lands.

            if (!effect) return;

            m_chain->addEffect(effect);
            addEffect(effect);
        });
    }

    menu.exec(m_addEffectBtn->mapToGlobal(QPoint(0, m_addEffectBtn->height())));
}

} // namespace dawcast::widgets
