// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "PedalboardWidget.h"
#include "EmbossedKnob.h"
#include "../core/IEffectUnit.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QPushButton>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QFont>

namespace dawcast::widgets {

// ── Color Palette for Default Pedals ───────────────────────────────────────

struct PedalPreset {
    const char* name;
    QColor      faceplate;
};

static const PedalPreset kDefaultPedals[] = {
    { "Tuner",      QColor(60,  70,  80)  },   // Dark charcoal
    { "Overdrive",  QColor(180, 60,  30)  },   // Deep orange
    { "Chorus",     QColor(40,  110, 170) },   // Ocean blue
    { "Delay",      QColor(50,  140, 80)  },   // Forest green
    { "Reverb",     QColor(100, 60,  150) },   // Purple
    { "Compressor", QColor(170, 150, 40)  },   // Gold
};
static constexpr int kDefaultPedalCount = 6;

// ═══════════════════════════════════════════════════════════════════════════
//  PedalWidget
// ═══════════════════════════════════════════════════════════════════════════

PedalWidget::PedalWidget(const QString& effectName,
                         const QColor& faceplateColor,
                         QWidget* parent)
    : QWidget(parent)
    , m_effectName(effectName)
    , m_faceplateColor(faceplateColor)
{
    setMinimumSize(140, 260);
    setMaximumWidth(160);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

    buildUI();
}

PedalWidget::~PedalWidget() = default;

void PedalWidget::buildUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 8, 6, 8);
    mainLayout->setSpacing(4);

    // Effect name label
    auto* nameLabel = new QLabel(m_effectName, this);
    nameLabel->setAlignment(Qt::AlignCenter);
    nameLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #e0e5e8; font-size: 12px; font-weight: bold; "
        "letter-spacing: 1px; text-transform: uppercase; }"));
    mainLayout->addWidget(nameLabel);

    // LED indicator
    auto* ledWidget = new QWidget(this);
    ledWidget->setFixedSize(10, 10);
    ledWidget->setStyleSheet(QStringLiteral(
        "QWidget { background: %1; border-radius: 5px; border: 1px solid #333; }")
        .arg(m_bypassed ? QStringLiteral("#333333") : QStringLiteral("#00ff44")));
    auto* ledLayout = new QHBoxLayout();
    ledLayout->addStretch();
    ledLayout->addWidget(ledWidget);
    ledLayout->addStretch();
    mainLayout->addLayout(ledLayout);

    // Knobs — 4 parameter knobs per pedal
    // In a real implementation these would map to IEffectUnit parameters
    static const char* knobLabels[] = { "Drive", "Tone", "Level", "Mix" };
    static const QColor knobColors[] = {
        QColor(255, 140, 60),  // Warm orange
        QColor(80, 180, 255),  // Sky blue
        QColor(100, 220, 100), // Green
        QColor(200, 160, 255), // Lavender
    };

    auto* knobGrid = new QHBoxLayout();
    knobGrid->setSpacing(2);
    for (int i = 0; i < 4; ++i) {
        auto* col = new QVBoxLayout();
        col->setSpacing(0);

        auto* knob = new EmbossedKnob(this);
        knob->setRange(0.0f, 1.0f);
        knob->setValue(0.5f);
        knob->setKnobSize(32);
        knob->setArcColor(knobColors[i]);
        knob->setLabel(QString::fromLatin1(knobLabels[i]));
        col->addWidget(knob, 0, Qt::AlignHCenter);

        connect(knob, &EmbossedKnob::valueChanged,
                this, [this, i](float val) {
            if (m_effect && !m_bypassed)
                m_effect->setParameter(i, val);
            emit parameterChanged(i, val);
        });

        m_knobs.append(knob);
        knobGrid->addLayout(col);
    }
    mainLayout->addLayout(knobGrid);

    mainLayout->addStretch();

    // Footswitch — big bypass button at the bottom
    m_footswitch = new QPushButton(this);
    m_footswitch->setCheckable(true);
    m_footswitch->setChecked(!m_bypassed);
    m_footswitch->setFixedSize(56, 56);
    m_footswitch->setToolTip(tr("Bypass — click to toggle effect on/off"));
    m_footswitch->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: qradialgradient(cx:0.5, cy:0.5, radius:0.5, "
        "    fx:0.5, fy:0.3, stop:0 #555, stop:1 #222);"
        "  border: 3px solid #444;"
        "  border-radius: 28px;"
        "  color: #ccc;"
        "  font-size: 10px;"
        "  font-weight: bold;"
        "}"
        "QPushButton:checked {"
        "  border-color: #00bcb4;"
        "  background: qradialgradient(cx:0.5, cy:0.5, radius:0.5, "
        "    fx:0.5, fy:0.3, stop:0 #666, stop:1 #333);"
        "}"));

    connect(m_footswitch, &QPushButton::toggled, this, [this, ledWidget](bool on) {
        m_bypassed = !on;
        if (m_effect)
            m_effect->setBypassed(m_bypassed);

        // Update LED color
        ledWidget->setStyleSheet(QStringLiteral(
            "QWidget { background: %1; border-radius: 5px; border: 1px solid #333; }")
            .arg(m_bypassed ? QStringLiteral("#333333") : QStringLiteral("#00ff44")));

        emit bypassToggled(m_bypassed);
    });

    auto* footLayout = new QHBoxLayout();
    footLayout->addStretch();
    footLayout->addWidget(m_footswitch);
    footLayout->addStretch();
    mainLayout->addLayout(footLayout);
}

void PedalWidget::setEffectUnit(IEffectUnit* effect)
{
    m_effect = effect;
    if (effect) {
        m_bypassed = effect->isBypassed();
        m_footswitch->setChecked(!m_bypassed);
        syncKnobsFromEffect();
    }
}

IEffectUnit* PedalWidget::effectUnit() const
{
    return m_effect;
}

bool PedalWidget::isBypassed() const
{
    return m_bypassed;
}

void PedalWidget::setBypassed(bool bypassed)
{
    m_footswitch->setChecked(!bypassed);
}

QString PedalWidget::effectName() const
{
    return m_effectName;
}

QSize PedalWidget::sizeHint() const
{
    return {150, 280};
}

QSize PedalWidget::minimumSizeHint() const
{
    return {130, 240};
}

void PedalWidget::syncKnobsFromEffect()
{
    if (!m_effect) return;
    int count = qMin(m_effect->parameterCount(), m_knobs.size());
    for (int i = 0; i < count; ++i) {
        m_knobs[i]->blockSignals(true);
        m_knobs[i]->setValue(m_effect->parameter(i));
        m_knobs[i]->blockSignals(false);
    }
}

void PedalWidget::syncKnobsToEffect()
{
    if (!m_effect) return;
    int count = qMin(m_effect->parameterCount(), m_knobs.size());
    for (int i = 0; i < count; ++i) {
        m_effect->setParameter(i, m_knobs[i]->value());
    }
}

// ── Custom Paint (skeuomorphic enclosure) ──────────────────────────────────

void PedalWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    QRect r = rect().adjusted(2, 2, -2, -2);

    // Enclosure body — dark metal
    QLinearGradient bodyGrad(0, 0, 0, r.height());
    bodyGrad.setColorAt(0.0, QColor(50, 55, 60));
    bodyGrad.setColorAt(0.5, QColor(35, 40, 45));
    bodyGrad.setColorAt(1.0, QColor(25, 28, 32));
    p.setBrush(bodyGrad);
    p.setPen(QPen(QColor(70, 75, 80), 1.5));

    QPainterPath enclosure;
    enclosure.addRoundedRect(QRectF(r), 8, 8);
    p.drawPath(enclosure);

    // Faceplate accent strip
    QRect faceRect(r.left() + 6, r.top() + 4, r.width() - 12, 36);
    QLinearGradient faceGrad(0, faceRect.top(), 0, faceRect.bottom());
    faceGrad.setColorAt(0.0, m_faceplateColor.lighter(120));
    faceGrad.setColorAt(1.0, m_faceplateColor.darker(110));
    p.setBrush(faceGrad);
    p.setPen(Qt::NoPen);

    QPainterPath face;
    face.addRoundedRect(QRectF(faceRect), 4, 4);
    p.drawPath(face);

    // Subtle edge screws (decorative)
    p.setBrush(QColor(90, 95, 100));
    p.setPen(QPen(QColor(60, 65, 70), 0.5));
    int screwR = 3;
    p.drawEllipse(QPoint(r.left() + 10, r.bottom() - 10), screwR, screwR);
    p.drawEllipse(QPoint(r.right() - 10, r.bottom() - 10), screwR, screwR);
    p.drawEllipse(QPoint(r.left() + 10, r.top() + 44), screwR, screwR);
    p.drawEllipse(QPoint(r.right() - 10, r.top() + 44), screwR, screwR);
}

// ═══════════════════════════════════════════════════════════════════════════
//  PedalboardWidget
// ═══════════════════════════════════════════════════════════════════════════

PedalboardWidget::PedalboardWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    // Header label
    auto* header = new QLabel(tr("  PEDALBOARD  "), this);
    header->setAlignment(Qt::AlignCenter);
    header->setStyleSheet(QStringLiteral(
        "QLabel { background: #161e24; color: #00bcb4; font-size: 13px; "
        "font-weight: bold; letter-spacing: 2px; padding: 6px; "
        "border-bottom: 2px solid #00bcb4; }"));
    outerLayout->addWidget(header);

    // Scrollable pedal chain
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setStyleSheet(QStringLiteral(
        "QScrollArea { background: #0e1418; border: none; }"
        "QScrollBar:horizontal {"
        "  background: #161e24; height: 10px; margin: 0; }"
        "QScrollBar::handle:horizontal {"
        "  background: #3a4a52; border-radius: 5px; min-width: 30px; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
        "  width: 0; }"));

    m_chainWidget = new QWidget(m_scrollArea);
    m_chainLayout = new QHBoxLayout(m_chainWidget);
    m_chainLayout->setContentsMargins(12, 12, 12, 12);
    m_chainLayout->setSpacing(16);

    m_scrollArea->setWidget(m_chainWidget);
    outerLayout->addWidget(m_scrollArea, 1);

    buildDefaultChain();
    addPedalButton();
}

PedalboardWidget::~PedalboardWidget() = default;

// ── Chain Management ───────────────────────────────────────────────────────

PedalWidget* PedalboardWidget::addPedal(const QString& effectName,
                                         const QColor& color)
{
    auto* pedal = new PedalWidget(effectName, color, m_chainWidget);

    // Insert before the "Add" button
    int insertIdx = m_chainLayout->count();
    if (m_addButton)
        insertIdx = m_chainLayout->indexOf(m_addButton);

    m_chainLayout->insertWidget(insertIdx, pedal);
    m_pedals.append(pedal);

    // Wire signal arrows between pedals (visual connection lines in paintEvent)
    emit chainChanged();
    return pedal;
}

void PedalboardWidget::removePedal(int index)
{
    if (index < 0 || index >= m_pedals.size())
        return;

    PedalWidget* pedal = m_pedals.takeAt(index);
    m_chainLayout->removeWidget(pedal);
    pedal->deleteLater();

    emit chainChanged();
}

int PedalboardWidget::pedalCount() const
{
    return m_pedals.size();
}

PedalWidget* PedalboardWidget::pedal(int index) const
{
    if (index < 0 || index >= m_pedals.size())
        return nullptr;
    return m_pedals[index];
}

void PedalboardWidget::setDspChain(dawcast::DspChain* chain)
{
    m_dspChain = chain;
    // Future: bind each PedalWidget to an IEffectUnit from the DspChain
}

// ── Default Chain ──────────────────────────────────────────────────────────

void PedalboardWidget::buildDefaultChain()
{
    for (int i = 0; i < kDefaultPedalCount; ++i) {
        addPedal(QString::fromLatin1(kDefaultPedals[i].name),
                 kDefaultPedals[i].faceplate);
    }
}

void PedalboardWidget::addPedalButton()
{
    m_addButton = new QPushButton(QStringLiteral("+  Add Pedal"), m_chainWidget);
    m_addButton->setFixedSize(120, 260);
    m_addButton->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: rgba(0, 188, 180, 0.08);"
        "  border: 2px dashed #3a5a5a;"
        "  border-radius: 8px;"
        "  color: #5a8a8a;"
        "  font-size: 13px;"
        "  font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "  background: rgba(0, 188, 180, 0.15);"
        "  border-color: #00bcb4;"
        "  color: #00bcb4;"
        "}"));

    connect(m_addButton, &QPushButton::clicked, this, [this]() {
        // Default to a generic compressor pedal when adding
        addPedal(tr("New Effect"), QColor(80, 90, 100));
    });

    m_chainLayout->addWidget(m_addButton);
    m_chainLayout->addStretch();
}

} // namespace dawcast::widgets
