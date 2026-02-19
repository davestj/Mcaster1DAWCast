// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "MixerWidget.h"
#include "VUMeterWidget.h"
#include "EmbossedKnob.h"
#include "BevelButton.h"
#include "../audio_engine/AudioMixer.h"
#include "../audio_engine/BusRouter.h"
#include "../audio_engine/AudioBus.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSlider>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QFrame>
#include <QComboBox>

namespace dawcast::widgets {

namespace {
constexpr int kStripWidth     = 80;
constexpr int kStripMinHeight = 300;
constexpr int kMasterStripWidth = 90;
constexpr int kBusStripWidth    = 80;
constexpr int kMaxSendsPerStrip = 4;

const QString kStripStyle = QStringLiteral(
    "QWidget#channelStrip { background: #2a2a30; border: 1px solid #3a3a42; border-radius: 3px; }");
const QString kMasterStripStyle = QStringLiteral(
    "QWidget#masterStrip { background: #302a30; border: 1px solid #4a3a4a; border-radius: 3px; }");
const QString kBusStripStyle = QStringLiteral(
    "QWidget#busStrip { background: #2a302a; border: 1px solid #3a4a3a; border-radius: 3px; }");
const QString kLabelStyle = QStringLiteral(
    "QLabel { color: #ccc; font-size: 10px; font-weight: bold; }");
const QString kFaderStyle = QStringLiteral(
    "QSlider::groove:vertical { background: #1e1e24; width: 8px; border-radius: 4px; }"
    "QSlider::handle:vertical { background: #7090b0; height: 14px; margin: 0 -4px; border-radius: 4px; }");
const QString kSendLabelStyle = QStringLiteral(
    "QLabel { color: #8a8; font-size: 9px; }");

QWidget* createChannelStrip(QWidget* parent, const QString& name,
                            bool isMaster = false, bool isBus = false)
{
    auto* strip = new QWidget(parent);
    if (isMaster) {
        strip->setObjectName(QStringLiteral("masterStrip"));
        strip->setStyleSheet(kMasterStripStyle);
        strip->setFixedWidth(kMasterStripWidth);
    } else if (isBus) {
        strip->setObjectName(QStringLiteral("busStrip"));
        strip->setStyleSheet(kBusStripStyle);
        strip->setFixedWidth(kBusStripWidth);
    } else {
        strip->setObjectName(QStringLiteral("channelStrip"));
        strip->setStyleSheet(kStripStyle);
        strip->setFixedWidth(kStripWidth);
    }
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
        if (value == 0) {
            dbLabel->setText(QStringLiteral("-inf"));
            return;
        }
        float db = 20.0f * std::log10(static_cast<float>(value) / 100.0f);
        dbLabel->setText(QString::number(static_cast<double>(db), 'f', 1) + QStringLiteral(" dB"));
    });

    // Pan knob
    auto* panKnob = new EmbossedKnob(strip);
    panKnob->setRange(-1.0f, 1.0f);
    panKnob->setValue(0.0f);
    panKnob->setLabel(QStringLiteral("Pan"));
    panKnob->setKnobSize(28);
    if (isMaster)
        panKnob->setArcColor(QColor(180, 120, 200));
    else if (isBus)
        panKnob->setArcColor(QColor(120, 200, 140));
    else
        panKnob->setArcColor(QColor(80, 180, 255));
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

/// Create a channel strip with send controls below the pan knob.
QWidget* createChannelStripWithSends(QWidget* parent, const QString& name,
                                     int stripIndex, MixerWidget* mixer)
{
    auto* strip = new QWidget(parent);
    strip->setObjectName(QStringLiteral("channelStrip"));
    strip->setStyleSheet(kStripStyle);
    strip->setFixedWidth(kStripWidth);
    strip->setMinimumHeight(kStripMinHeight + 80);  // extra room for sends

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
    vuMeter->setMinimumHeight(60);
    vuMeter->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    layout->addWidget(vuMeter, 1);

    // Volume fader (vertical)
    auto* fader = new QSlider(Qt::Vertical, strip);
    fader->setRange(0, 127);
    fader->setValue(100);
    fader->setStyleSheet(kFaderStyle);
    fader->setMinimumHeight(50);
    layout->addWidget(fader, 0, Qt::AlignHCenter);

    // dB label
    auto* dbLabel = new QLabel(QStringLiteral("0.0 dB"), strip);
    dbLabel->setAlignment(Qt::AlignCenter);
    dbLabel->setStyleSheet(QStringLiteral("QLabel { color: #888; font-size: 9px; }"));
    layout->addWidget(dbLabel);

    QObject::connect(fader, &QSlider::valueChanged, dbLabel, [dbLabel](int value) {
        if (value == 0) {
            dbLabel->setText(QStringLiteral("-inf"));
            return;
        }
        float db = 20.0f * std::log10(static_cast<float>(value) / 100.0f);
        dbLabel->setText(QString::number(static_cast<double>(db), 'f', 1) + QStringLiteral(" dB"));
    });

    // Pan knob
    auto* panKnob = new EmbossedKnob(strip);
    panKnob->setRange(-1.0f, 1.0f);
    panKnob->setValue(0.0f);
    panKnob->setLabel(QStringLiteral("Pan"));
    panKnob->setKnobSize(28);
    panKnob->setArcColor(QColor(80, 180, 255));
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

    // ── Sends section ──────────────────────────────────────────────
    auto* sendsLabel = new QLabel(QStringLiteral("Sends"), strip);
    sendsLabel->setAlignment(Qt::AlignCenter);
    sendsLabel->setStyleSheet(kSendLabelStyle);
    layout->addWidget(sendsLabel);

    auto* sendsContainer = new QWidget(strip);
    auto* sendsLayout = new QVBoxLayout(sendsContainer);
    sendsLayout->setContentsMargins(2, 0, 2, 0);
    sendsLayout->setSpacing(2);

    // Create send slots (initially 2, up to kMaxSendsPerStrip)
    for (int s = 0; s < 2; ++s) {
        auto* sendRow = new QHBoxLayout;
        sendRow->setSpacing(2);

        // Bus selector combo
        auto* busSel = new QComboBox(sendsContainer);
        busSel->setFixedHeight(18);
        busSel->setStyleSheet(QStringLiteral(
            "QComboBox { background: #252840; color: #aab; border: 1px solid #3a3e55;"
            " border-radius: 2px; font-size: 9px; padding: 0 2px; }"
            "QComboBox::drop-down { width: 12px; }"));
        busSel->addItem(QStringLiteral("--"));
        sendRow->addWidget(busSel, 1);

        // Send level knob
        auto* sendKnob = new EmbossedKnob(sendsContainer);
        sendKnob->setRange(-96.0f, 0.0f);
        sendKnob->setValue(-96.0f);  // off by default
        sendKnob->setLabel(QString());
        sendKnob->setKnobSize(18);
        sendKnob->setArcColor(QColor(100, 200, 140));
        sendKnob->setFixedSize(22, 28);
        sendRow->addWidget(sendKnob);

        // Emit signal when send level changes
        QObject::connect(sendKnob, &EmbossedKnob::valueChanged, mixer,
            [mixer, stripIndex, s](float value) {
                emit mixer->sendLevelChanged(stripIndex, s, value);
            });

        sendsLayout->addLayout(sendRow);
    }

    // "Add Send" button
    auto* addSendBtn = new QPushButton(QStringLiteral("+"), sendsContainer);
    addSendBtn->setFixedSize(20, 16);
    addSendBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #2e3248; color: #8a8; border: 1px solid #3a4a3a;"
        " border-radius: 2px; font-size: 10px; font-weight: bold; }"
        "QPushButton:hover { background: #3a4060; }"));
    addSendBtn->setToolTip(QObject::tr("Add Send"));
    sendsLayout->addWidget(addSendBtn, 0, Qt::AlignHCenter);

    layout->addWidget(sendsContainer);

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

    // Bus separator (before bus strips)
    m_busSeparator = new QFrame(container);
    m_busSeparator->setFrameShape(QFrame::VLine);
    m_busSeparator->setStyleSheet(QStringLiteral("QFrame { color: #484; }"));
    m_busSeparator->hide();

    // Master strip on the right
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

    // Clear existing strips (but not master/bus)
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

    // Re-add bus separator, bus strips, master separator, and master
    m_stripLayout->addWidget(m_busSeparator);

    // Re-add any bus strips
    for (auto* busWidget : m_busStripWidgets) {
        m_stripLayout->addWidget(busWidget);
    }

    m_stripLayout->addWidget(m_masterSeparator);
    m_stripLayout->addWidget(m_masterStrip);
    m_stripLayout->addStretch();
}

void MixerWidget::setBusRouter(BusRouter* router)
{
    m_busRouter = router;

    if (m_busRouter) {
        connect(m_busRouter, &BusRouter::busAdded,
                this, &MixerWidget::rebuildBusStrips);
        connect(m_busRouter, &BusRouter::busRemoved,
                this, &MixerWidget::rebuildBusStrips);
    }

    rebuildBusStrips();
}

void MixerWidget::addStrip()
{
    QString name = tr("Ch %1").arg(m_stripCount + 1);
    auto* strip = createChannelStripWithSends(this, name, m_stripCount, this);

    // Insert before the bus separator
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

void MixerWidget::rebuildBusStrips()
{
    // Remove old bus strip widgets
    for (auto* bw : m_busStripWidgets) {
        m_stripLayout->removeWidget(bw);
        bw->deleteLater();
    }
    m_busStripWidgets.clear();

    if (!m_busRouter) {
        m_busSeparator->hide();
        return;
    }

    int busCount = m_busRouter->busCount();
    bool hasNonMasterBuses = (busCount > 1);

    m_busSeparator->setVisible(hasNonMasterBuses);

    // Create a strip for each non-master bus
    for (int i = 1; i < busCount; ++i) {
        AudioBus* bus = m_busRouter->bus(i);
        if (!bus) continue;

        // Determine bus type label
        QString typeTag;
        switch (bus->busType()) {
        case AudioBus::SubGroup: typeTag = QStringLiteral("GRP"); break;
        case AudioBus::Aux:      typeTag = QStringLiteral("AUX"); break;
        case AudioBus::Send:     typeTag = QStringLiteral("SND"); break;
        default:                 typeTag = QStringLiteral("BUS"); break;
        }

        auto* busStrip = createChannelStrip(this, bus->name(), false, true);

        // Add an "FX" button to open effects rack for this bus
        auto* fxBtn = new QPushButton(QStringLiteral("FX"), busStrip);
        fxBtn->setFixedSize(30, 20);
        fxBtn->setStyleSheet(QStringLiteral(
            "QPushButton { background: #2e4238; color: #8c8; border: 1px solid #3a5a3a;"
            " border-radius: 2px; font-size: 9px; font-weight: bold; }"
            "QPushButton:hover { background: #3a5248; }"));
        auto* busLayout = qobject_cast<QVBoxLayout*>(busStrip->layout());
        if (busLayout) {
            busLayout->addWidget(fxBtn, 0, Qt::AlignHCenter);
        }

        int busIndex = i;
        connect(fxBtn, &QPushButton::clicked, this, [this, busIndex]() {
            emit busEffectsRequested(busIndex);
        });

        // Add type tag label at bottom
        auto* tagLabel = new QLabel(typeTag, busStrip);
        tagLabel->setAlignment(Qt::AlignCenter);
        tagLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: #6a6; font-size: 8px; font-weight: bold;"
            " background: #1e2e1e; border-radius: 2px; padding: 1px 3px; }"));
        if (busLayout) {
            busLayout->addWidget(tagLabel, 0, Qt::AlignHCenter);
        }

        m_busStripWidgets.append(busStrip);

        // Insert before master separator: find master separator position
        int masterSepIdx = m_stripLayout->indexOf(m_masterSeparator);
        if (masterSepIdx >= 0) {
            m_stripLayout->insertWidget(masterSepIdx, busStrip);
        } else {
            m_stripLayout->addWidget(busStrip);
        }
    }
}

} // namespace dawcast::widgets
