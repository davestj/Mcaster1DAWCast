// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "MixerWidget.h"
#include "VUMeterWidget.h"
#include "EmbossedKnob.h"
#include "BevelButton.h"
#include "AudioMixer.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSlider>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QFrame>

namespace dawcast::widgets {

namespace {
constexpr int kStripWidth     = 80;
constexpr int kStripMinHeight = 300;
constexpr int kMasterStripWidth = 90;

const QString kStripStyle = QStringLiteral(
    "QWidget#channelStrip { background: #2a2a30; border: 1px solid #3a3a42; border-radius: 3px; }");
const QString kMasterStripStyle = QStringLiteral(
    "QWidget#masterStrip { background: #302a30; border: 1px solid #4a3a4a; border-radius: 3px; }");
const QString kLabelStyle = QStringLiteral(
    "QLabel { color: #ccc; font-size: 10px; font-weight: bold; }");
const QString kFaderStyle = QStringLiteral(
    "QSlider::groove:vertical { background: #1e1e24; width: 8px; border-radius: 4px; }"
    "QSlider::handle:vertical { background: #7090b0; height: 14px; margin: 0 -4px; border-radius: 4px; }");

QWidget* createChannelStrip(QWidget* parent, const QString& name, bool isMaster = false)
{
    auto* strip = new QWidget(parent);
    strip->setObjectName(isMaster ? QStringLiteral("masterStrip") : QStringLiteral("channelStrip"));
    strip->setStyleSheet(isMaster ? kMasterStripStyle : kStripStyle);
    strip->setFixedWidth(isMaster ? kMasterStripWidth : kStripWidth);
    strip->setMinimumHeight(kStripMinHeight);

    auto* layout = new QVBoxLayout(strip);
    layout->setContentsMargins(4, 6, 4, 6);
    layout->setSpacing(4);

    // Channel name label
    auto* nameLabel = new QLabel(name, strip);
    nameLabel->setAlignment(Qt::AlignCenter);
    nameLabel->setStyleSheet(kLabelStyle);
    layout->addWidget(nameLabel);

    // VU Meter (vertical)
    auto* vuMeter = new VUMeterWidget(strip);
    vuMeter->setOrientation(Qt::Vertical);
    vuMeter->setMinimumHeight(80);
    vuMeter->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    layout->addWidget(vuMeter, 1);

    // Volume fader (vertical)
    auto* fader = new QSlider(Qt::Vertical, strip);
    fader->setRange(0, 127);
    fader->setValue(100);
    fader->setStyleSheet(kFaderStyle);
    fader->setMinimumHeight(60);
    layout->addWidget(fader, 0, Qt::AlignHCenter);

    // dB label below fader
    auto* dbLabel = new QLabel(QStringLiteral("0.0 dB"), strip);
    dbLabel->setAlignment(Qt::AlignCenter);
    dbLabel->setStyleSheet(QStringLiteral("QLabel { color: #888; font-size: 9px; }"));
    layout->addWidget(dbLabel);

    // Update dB label on fader change
    QObject::connect(fader, &QSlider::valueChanged, dbLabel, [dbLabel](int value) {
        // Map 0-127 to roughly -inf to +6 dB
        float db;
        if (value == 0) {
            dbLabel->setText(QStringLiteral("-inf"));
            return;
        }
        db = 20.0f * std::log10(static_cast<float>(value) / 100.0f);
        dbLabel->setText(QString::number(static_cast<double>(db), 'f', 1) + QStringLiteral(" dB"));
    });

    // Pan knob
    auto* panKnob = new EmbossedKnob(strip);
    panKnob->setRange(-1.0f, 1.0f);
    panKnob->setValue(0.0f);
    panKnob->setLabel(QStringLiteral("Pan"));
    panKnob->setKnobSize(28);
    panKnob->setArcColor(isMaster ? QColor(180, 120, 200) : QColor(80, 180, 255));
    panKnob->setFixedSize(36, 44);
    layout->addWidget(panKnob, 0, Qt::AlignHCenter);

    // Mute / Solo buttons
    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(3);

    auto* muteBtn = new BevelButton(QStringLiteral("M"), strip);
    muteBtn->setCheckable(true);
    muteBtn->setFixedSize(30, 22);
    muteBtn->setCheckedFaceColor(QColor(200, 120, 30));
    btnRow->addWidget(muteBtn);

    auto* soloBtn = new BevelButton(QStringLiteral("S"), strip);
    soloBtn->setCheckable(true);
    soloBtn->setFixedSize(30, 22);
    soloBtn->setCheckedFaceColor(QColor(200, 190, 50));
    btnRow->addWidget(soloBtn);

    layout->addLayout(btnRow);

    return strip;
}
} // anonymous namespace

MixerWidget::MixerWidget(QWidget* parent)
    : QWidget(parent)
{
    setStyleSheet(QStringLiteral("MixerWidget { background: #222228; }"));

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet(QStringLiteral("QScrollArea { background: #222228; border: none; }"));

    auto* container = new QWidget(scrollArea);
    container->setStyleSheet(QStringLiteral("background: #222228;"));
    m_stripLayout = new QHBoxLayout(container);
    m_stripLayout->setContentsMargins(6, 6, 6, 6);
    m_stripLayout->setSpacing(4);
    m_stripLayout->setAlignment(Qt::AlignLeft);

    // Master strip on the right -- we add a separator and the master strip
    // These persist even when channel strips are added/removed
    m_masterSeparator = new QFrame(container);
    m_masterSeparator->setFrameShape(QFrame::VLine);
    m_masterSeparator->setStyleSheet(QStringLiteral("QFrame { color: #555; }"));

    m_masterStrip = createChannelStrip(container, tr("Master"), true);

    scrollArea->setWidget(container);

    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(scrollArea);
}

MixerWidget::~MixerWidget() = default;

void MixerWidget::setMixer(AudioMixer* mixer)
{
    m_mixer = mixer;

    // Clear existing strips (but not master)
    while (m_stripLayout->count() > 0) {
        auto* item = m_stripLayout->takeAt(0);
        if (item && item->widget()) {
            delete item->widget();
        }
        delete item;
    }
    m_stripCount = 0;

    // Rebuild from mixer state
    if (m_mixer) {
        int count = m_mixer->stripCount();
        for (int i = 0; i < count; ++i) {
            addStrip();
        }
    }

    // Re-add separator and master at the end
    m_stripLayout->addWidget(m_masterSeparator);
    m_stripLayout->addWidget(m_masterStrip);
    m_stripLayout->addStretch();
}

void MixerWidget::addStrip()
{
    QString name = tr("Ch %1").arg(m_stripCount + 1);
    auto* strip = createChannelStrip(this, name);

    // Insert before the master separator (which is at count - 2 if present)
    // For safety, just insert at position m_stripCount
    int insertPos = m_stripCount;
    m_stripLayout->insertWidget(insertPos, strip);
    ++m_stripCount;
}

void MixerWidget::removeStrip(int index)
{
    if (index < 0 || index >= m_stripCount) return;

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
