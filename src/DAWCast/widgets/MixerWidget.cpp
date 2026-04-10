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
#include <QTimer>
#include <cmath>

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

/// Convert the 0..127 slider value into a working dB value for the audio
/// engine. 100 -> 0 dB, 0 -> -inf (treated as -96 dB).
inline float faderValueToDb(int value)
{
    if (value <= 0) return -96.0f;
    return 20.0f * std::log10(static_cast<float>(value) / 100.0f);
}

QWidget* createChannelStrip(QWidget* parent, const QString& name,
                            bool isMaster = false, bool isBus = false,
                            VUMeterWidget** vuMeterOut = nullptr,
                            QSlider** faderOut = nullptr)
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
    if (vuMeterOut) *vuMeterOut = vuMeter;

    // Volume fader (vertical)
    auto* fader = new QSlider(Qt::Vertical, strip);
    fader->setRange(0, 127);
    fader->setValue(100);
    fader->setStyleSheet(kFaderStyle);
    fader->setMinimumHeight(60);
    layout->addWidget(fader, 0, Qt::AlignHCenter);
    if (faderOut) *faderOut = fader;

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

const QString kEqLabelStyle = QStringLiteral(
    "QLabel { color: #8aa; font-size: 8px; font-weight: bold; }");

/// Create a channel strip with inline 3-band EQ and send controls.
QWidget* createChannelStripWithSends(QWidget* parent, const QString& name,
                                     int stripIndex, MixerWidget* mixer,
                                     AudioMixer* audioMixer,
                                     ChannelEQKnobs* eqKnobsOut = nullptr,
                                     VUMeterWidget** vuMeterOut = nullptr)
{
    auto* strip = new QWidget(parent);
    strip->setObjectName(QStringLiteral("channelStrip"));
    strip->setStyleSheet(kStripStyle);
    strip->setFixedWidth(kStripWidth);
    strip->setMinimumHeight(kStripMinHeight + 140);  // extra room for EQ + sends

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
    if (vuMeterOut) *vuMeterOut = vuMeter;

    // ── Inline 3-Band EQ section ──────────────────────────────────
    auto* eqLabel = new QLabel(QStringLiteral("EQ"), strip);
    eqLabel->setAlignment(Qt::AlignCenter);
    eqLabel->setStyleSheet(kEqLabelStyle);
    layout->addWidget(eqLabel);

    auto* eqGroup = new QWidget(strip);
    auto* eqLayout = new QHBoxLayout(eqGroup);
    eqLayout->setContentsMargins(0, 0, 0, 0);
    eqLayout->setSpacing(2);

    // LF knob — Low shelf at 200 Hz, +/-12 dB
    auto* lfKnob = new EmbossedKnob(eqGroup);
    lfKnob->setRange(-12.0f, 12.0f);
    lfKnob->setValue(0.0f);
    lfKnob->setLabel(QStringLiteral("LF"));
    lfKnob->setKnobSize(22);
    lfKnob->setArcColor(QColor(200, 140, 80));
    lfKnob->setFixedSize(24, 36);
    eqLayout->addWidget(lfKnob);

    // MF knob — Peaking at 1 kHz, Q=1.0, +/-12 dB
    auto* mfKnob = new EmbossedKnob(eqGroup);
    mfKnob->setRange(-12.0f, 12.0f);
    mfKnob->setValue(0.0f);
    mfKnob->setLabel(QStringLiteral("MF"));
    mfKnob->setKnobSize(22);
    mfKnob->setArcColor(QColor(140, 200, 80));
    mfKnob->setFixedSize(24, 36);
    eqLayout->addWidget(mfKnob);

    // HF knob — High shelf at 8 kHz, +/-12 dB
    auto* hfKnob = new EmbossedKnob(eqGroup);
    hfKnob->setRange(-12.0f, 12.0f);
    hfKnob->setValue(0.0f);
    hfKnob->setLabel(QStringLiteral("HF"));
    hfKnob->setKnobSize(22);
    hfKnob->setArcColor(QColor(80, 140, 200));
    hfKnob->setFixedSize(24, 36);
    eqLayout->addWidget(hfKnob);

    layout->addWidget(eqGroup);

    // Return EQ knob references so the mixer widget can wire them up
    if (eqKnobsOut) {
        eqKnobsOut->lfKnob = lfKnob;
        eqKnobsOut->mfKnob = mfKnob;
        eqKnobsOut->hfKnob = hfKnob;
    }

    // Emit channelEQChanged when any EQ knob moves
    auto emitEQ = [mixer, stripIndex, lfKnob, mfKnob, hfKnob]() {
        emit mixer->channelEQChanged(stripIndex,
                                     lfKnob->value(), mfKnob->value(), hfKnob->value());
    };
    QObject::connect(lfKnob, &EmbossedKnob::valueChanged, mixer, emitEQ);
    QObject::connect(mfKnob, &EmbossedKnob::valueChanged, mixer, emitEQ);
    QObject::connect(hfKnob, &EmbossedKnob::valueChanged, mixer, emitEQ);

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

    // Fader drives both the dB label AND the actual mixer strip volume.
    // Without the setStripVolume() call, moving the fader was purely cosmetic
    // and audio kept playing at whatever gain the track was last set to.
    QObject::connect(fader, &QSlider::valueChanged, mixer,
        [dbLabel, audioMixer, stripIndex](int value) {
            float db = faderValueToDb(value);
            if (value == 0) {
                dbLabel->setText(QStringLiteral("-inf"));
            } else {
                dbLabel->setText(QString::number(static_cast<double>(db), 'f', 1)
                                 + QStringLiteral(" dB"));
            }
            if (audioMixer) {
                audioMixer->setStripVolume(stripIndex, db);
            }
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

    // Pan knob -> mixer strip pan
    QObject::connect(panKnob, &EmbossedKnob::valueChanged, mixer,
        [audioMixer, stripIndex](float value) {
            if (audioMixer) audioMixer->setStripPan(stripIndex, value);
        });

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

    // Mute / solo buttons -> mixer strip state
    QObject::connect(muteBtn, &BevelButton::toggled, mixer,
        [audioMixer, stripIndex](bool muted) {
            if (audioMixer) audioMixer->setStripMuted(stripIndex, muted);
        });
    QObject::connect(soloBtn, &BevelButton::toggled, mixer,
        [audioMixer, stripIndex](bool solo) {
            if (audioMixer) audioMixer->setStripSolo(stripIndex, solo);
        });

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

    // Create the master strip, capturing the fader + VU meter so we can
    // hook them up to the master bus and meter poller later.
    QSlider* masterFader = nullptr;
    m_masterStrip = createChannelStrip(container, tr("Master"), true, false,
                                       &m_masterVuMeter, &masterFader);

    // Master fader -> master bus volume (once a BusRouter is set).
    // We connect by capturing `this` so we can look up the router dynamically;
    // the router is not necessarily available at construction time.
    if (masterFader) {
        connect(masterFader, &QSlider::valueChanged, this, [this](int value) {
            if (!m_busRouter) return;
            AudioBus* master = m_busRouter->masterBus();
            if (!master) return;
            master->setVolume(faderValueToDb(value));
        });
    }

    scrollArea->setWidget(container);

    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(scrollArea);

    // ── Meter poll timer ─────────────────────────────────────────────
    // Drains the AudioMixer's atomic peak accessors and repaints each
    // VU meter at ~33 Hz. Running on the GUI thread is safe — the mixer
    // writes into atomics from the audio thread.
    m_meterTimer = new QTimer(this);
    m_meterTimer->setInterval(30);
    connect(m_meterTimer, &QTimer::timeout, this, [this]() {
        if (!m_mixer) return;

        auto linearToDb = [](float lin) -> float {
            if (lin <= 1e-6f) return -96.0f;
            return 20.0f * std::log10(lin);
        };

        for (int i = 0; i < m_vuMeters.size(); ++i) {
            auto* meter = m_vuMeters[i];
            if (!meter) continue;
            float peakL = m_mixer->stripPeakL(i);
            float peakR = m_mixer->stripPeakR(i);
            meter->setLevel(linearToDb(peakL), linearToDb(peakR));
        }

        if (m_masterVuMeter) {
            float mL = m_mixer->masterPeakL();
            float mR = m_mixer->masterPeakR();
            m_masterVuMeter->setLevel(linearToDb(mL), linearToDb(mR));
        }
    });
    m_meterTimer->start();
}

MixerWidget::~MixerWidget() = default;

void MixerWidget::setMixer(AudioMixer* mixer)
{
    m_mixer = mixer;

    // Clear existing strips (but not master/bus). The bus separator, bus
    // strips, master separator and master strip are re-parented out of the
    // layout first so takeAt() can safely wipe only the channel strips.
    m_stripLayout->removeWidget(m_busSeparator);
    for (auto* bw : m_busStripWidgets) {
        m_stripLayout->removeWidget(bw);
    }
    m_stripLayout->removeWidget(m_masterSeparator);
    m_stripLayout->removeWidget(m_masterStrip);

    while (m_stripLayout->count() > 0) {
        auto* item = m_stripLayout->takeAt(0);
        if (item && item->widget()) {
            delete item->widget();
        }
        delete item;
    }
    m_stripCount = 0;
    m_vuMeters.clear();
    m_channelEQKnobs.clear();

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
    ChannelEQKnobs eqKnobs;
    VUMeterWidget* vuMeter = nullptr;
    auto* strip = createChannelStripWithSends(this, name, m_stripCount, this,
                                              m_mixer, &eqKnobs, &vuMeter);

    // Store EQ knob references for this channel
    m_channelEQKnobs[m_stripCount] = eqKnobs;
    m_vuMeters.append(vuMeter);

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

    if (index < m_vuMeters.size()) {
        m_vuMeters.removeAt(index);
    }
    m_channelEQKnobs.remove(index);
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
