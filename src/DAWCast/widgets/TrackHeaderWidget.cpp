// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "TrackHeaderWidget.h"
#include "EmbossedKnob.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QLabel>

namespace dawcast::widgets {

namespace {
constexpr int kHeaderWidth = 180;

const QString kRecActiveStyle  = QStringLiteral("QPushButton { background-color: #c03030; color: white; font-weight: bold; border-radius: 2px; padding: 2px 6px; }");
const QString kRecInactiveStyle = QStringLiteral("QPushButton { background-color: #4a3030; color: #aaa; font-weight: bold; border-radius: 2px; padding: 2px 6px; }");
const QString kMuteActiveStyle  = QStringLiteral("QPushButton { background-color: #c07820; color: white; font-weight: bold; border-radius: 2px; padding: 2px 6px; }");
const QString kMuteInactiveStyle = QStringLiteral("QPushButton { background-color: #4a3c20; color: #aaa; font-weight: bold; border-radius: 2px; padding: 2px 6px; }");
const QString kSoloActiveStyle  = QStringLiteral("QPushButton { background-color: #c0b830; color: white; font-weight: bold; border-radius: 2px; padding: 2px 6px; }");
const QString kSoloInactiveStyle = QStringLiteral("QPushButton { background-color: #4a4830; color: #aaa; font-weight: bold; border-radius: 2px; padding: 2px 6px; }");
} // anonymous namespace

TrackHeaderWidget::TrackHeaderWidget(QWidget* parent)
    : QWidget(parent)
{
    setFixedWidth(kHeaderWidth);
    setStyleSheet(QStringLiteral("TrackHeaderWidget { background-color: #2a2a30; }"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(4);

    // Track name (editable)
    auto* nameEdit = new QLineEdit(this);
    nameEdit->setPlaceholderText(tr("Track Name"));
    nameEdit->setStyleSheet(QStringLiteral(
        "QLineEdit { background: #1e1e24; color: #ddd; border: 1px solid #555; "
        "border-radius: 2px; padding: 2px 4px; font-size: 11px; }"));
    layout->addWidget(nameEdit);

    // R / M / S buttons row
    auto* btnLayout = new QHBoxLayout;
    btnLayout->setSpacing(3);

    auto* recBtn  = new QPushButton(tr("R"), this);
    auto* muteBtn = new QPushButton(tr("M"), this);
    auto* soloBtn = new QPushButton(tr("S"), this);

    recBtn->setCheckable(true);
    muteBtn->setCheckable(true);
    soloBtn->setCheckable(true);

    recBtn->setFixedSize(28, 22);
    muteBtn->setFixedSize(28, 22);
    soloBtn->setFixedSize(28, 22);

    recBtn->setStyleSheet(kRecInactiveStyle);
    muteBtn->setStyleSheet(kMuteInactiveStyle);
    soloBtn->setStyleSheet(kSoloInactiveStyle);

    btnLayout->addWidget(recBtn);
    btnLayout->addWidget(muteBtn);
    btnLayout->addWidget(soloBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    // Volume slider
    auto* volRow = new QHBoxLayout;
    volRow->setSpacing(4);
    auto* volLabel = new QLabel(tr("Vol"), this);
    volLabel->setStyleSheet(QStringLiteral("QLabel { color: #999; font-size: 10px; }"));
    volLabel->setFixedWidth(22);
    auto* volumeSlider = new QSlider(Qt::Horizontal, this);
    volumeSlider->setRange(0, 100);
    volumeSlider->setValue(80);
    volumeSlider->setStyleSheet(QStringLiteral(
        "QSlider::groove:horizontal { background: #1e1e24; height: 6px; border-radius: 3px; }"
        "QSlider::handle:horizontal { background: #7090b0; width: 12px; margin: -3px 0; border-radius: 6px; }"));
    volRow->addWidget(volLabel);
    volRow->addWidget(volumeSlider);
    layout->addLayout(volRow);

    // Pan knob
    auto* panRow = new QHBoxLayout;
    panRow->setSpacing(4);
    auto* panLabel = new QLabel(tr("Pan"), this);
    panLabel->setStyleSheet(QStringLiteral("QLabel { color: #999; font-size: 10px; }"));
    panLabel->setFixedWidth(22);

    auto* panKnob = new EmbossedKnob(this);
    panKnob->setRange(-1.0f, 1.0f);
    panKnob->setValue(0.0f);
    panKnob->setLabel(QString());
    panKnob->setKnobSize(32);
    panKnob->setArcColor(QColor(100, 180, 220));
    panKnob->setFixedSize(36, 40);
    panRow->addWidget(panLabel);
    panRow->addWidget(panKnob);
    panRow->addStretch();
    layout->addLayout(panRow);

    layout->addStretch();

    // Connections: name
    connect(nameEdit, &QLineEdit::textChanged, this, &TrackHeaderWidget::setTrackName);

    // Record arm button with color toggle
    connect(recBtn, &QPushButton::toggled, this, [this, recBtn](bool armed) {
        recBtn->setStyleSheet(armed ? kRecActiveStyle : kRecInactiveStyle);
        setRecordArmed(armed);
    });

    // Mute button with color toggle
    connect(muteBtn, &QPushButton::toggled, this, [this, muteBtn](bool muted) {
        muteBtn->setStyleSheet(muted ? kMuteActiveStyle : kMuteInactiveStyle);
        setMuted(muted);
    });

    // Solo button with color toggle
    connect(soloBtn, &QPushButton::toggled, this, [this, soloBtn](bool solo) {
        soloBtn->setStyleSheet(solo ? kSoloActiveStyle : kSoloInactiveStyle);
        setSolo(solo);
    });

    // Volume slider -> volumeChanged signal
    connect(volumeSlider, &QSlider::valueChanged, this, [this](int value) {
        m_volume = static_cast<float>(value) / 100.0f;
        emit volumeChanged(m_volume);
    });

    // Pan knob -> panChanged signal
    connect(panKnob, &EmbossedKnob::valueChanged, this, [this](float value) {
        m_pan = value;
        emit panChanged(m_pan);
    });
}

TrackHeaderWidget::~TrackHeaderWidget() = default;

void TrackHeaderWidget::setTrackName(const QString& name) { m_trackName = name; }
void TrackHeaderWidget::setRecordArmed(bool armed) { m_recordArmed = armed; emit recordArmToggled(armed); }
void TrackHeaderWidget::setMuted(bool muted) { m_muted = muted; emit muteToggled(muted); }
void TrackHeaderWidget::setSolo(bool solo) { m_solo = solo; emit soloToggled(solo); }

QString TrackHeaderWidget::trackName() const { return m_trackName; }
bool TrackHeaderWidget::isRecordArmed() const { return m_recordArmed; }
bool TrackHeaderWidget::isMuted() const { return m_muted; }
bool TrackHeaderWidget::isSolo() const { return m_solo; }

} // namespace dawcast::widgets
