// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "TrackHeaderWidget.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QLabel>

namespace dawcast::widgets {

TrackHeaderWidget::TrackHeaderWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);

    auto* nameEdit = new QLineEdit(this);
    nameEdit->setPlaceholderText(tr("Track Name"));
    layout->addWidget(nameEdit);

    auto* btnLayout = new QHBoxLayout;
    auto* recBtn   = new QPushButton(tr("R"), this);
    auto* muteBtn  = new QPushButton(tr("M"), this);
    auto* soloBtn  = new QPushButton(tr("S"), this);
    recBtn->setCheckable(true);
    muteBtn->setCheckable(true);
    soloBtn->setCheckable(true);
    btnLayout->addWidget(recBtn);
    btnLayout->addWidget(muteBtn);
    btnLayout->addWidget(soloBtn);
    layout->addLayout(btnLayout);

    auto* volumeSlider = new QSlider(Qt::Horizontal, this);
    volumeSlider->setRange(0, 100);
    volumeSlider->setValue(80);
    layout->addWidget(new QLabel(tr("Vol"), this));
    layout->addWidget(volumeSlider);

    // TODO: pan knob widget

    connect(nameEdit, &QLineEdit::textChanged, this, &TrackHeaderWidget::setTrackName);
    connect(recBtn, &QPushButton::toggled, this, &TrackHeaderWidget::setRecordArmed);
    connect(muteBtn, &QPushButton::toggled, this, &TrackHeaderWidget::setMuted);
    connect(soloBtn, &QPushButton::toggled, this, &TrackHeaderWidget::setSolo);
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
