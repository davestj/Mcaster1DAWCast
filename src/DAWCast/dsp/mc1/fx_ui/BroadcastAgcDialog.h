/*
 * Mcaster1AudioPipe — Virtual Audio Routing Application
 * fx_ui/BroadcastAgcDialog.h — Broadcast AGC editor dialog
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Hardware rack unit editor for FxBroadcastAgc (7 params).
 * Three control sections: Level Control, Timing, Gate+Window — plus meters.
 * The gain meter shows applied gain (both boost and cut), not just reduction.
 * Header-only with inline implementation.
 */

#pragma once

#include "fx_ui/RackKnob.h"
#include "fx_ui/RackMeter.h"
#include "patchbay/dsp/dsp_effect.h"
#include "patchbay/dsp/preset_manager.h"

#include <QDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QTimer>

#include <string>

class BroadcastAgcDialog : public QDialog {
    Q_OBJECT

public:
    explicit BroadcastAgcDialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : QDialog(parent)
        , fx_(fx)
    {
        setWindowTitle("Broadcast AGC");
        setMinimumSize(300, 200); resize(600, 285);
        buildUi();
        loadFromEffect();
        startMetering();
    }

private:
    /* ── UI construction ────────────────────────────────────────────── */

    void buildUi()
    {
        /* Dialog-wide dark stylesheet */
        setStyleSheet(
            "QDialog { background: #141828; color: #d0e0f0; }"
            "QGroupBox {"
            "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
            "      stop:0 #1c2240, stop:1 #141828);"
            "  border: 1px solid #2a3450;"
            "  border-left: 3px solid #00d4aa;"
            "  border-radius: 6px;"
            "  margin-top: 14px;"
            "  padding: 10px 6px 6px 6px;"
            "  font-size: 11px;"
            "  color: #8090a8;"
            "}"
            "QGroupBox::title {"
            "  subcontrol-origin: margin;"
            "  subcontrol-position: top left;"
            "  padding: 2px 8px;"
            "  color: #00d4aa;"
            "}"
            "QLabel { color: #d0e0f0; }"
            "QComboBox {"
            "  background: #1c2240; color: #d0e0f0; border: 1px solid #2a3450;"
            "  border-radius: 3px; padding: 4px 8px; min-width: 160px;"
            "}"
            "QComboBox::drop-down { border: none; }"
            "QComboBox QAbstractItemView {"
            "  background: #1c2240; color: #d0e0f0; selection-background-color: #00d4aa;"
            "}"
            "QPushButton {"
            "  background: #1c2240; color: #d0e0f0; border: 1px solid #2a3450;"
            "  border-radius: 3px; padding: 6px 16px; min-width: 70px;"
            "}"
            "QPushButton:hover { background: #243060; }"
            "QPushButton:pressed { background: #00d4aa; color: #141828; }"
        );

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(12, 8, 12, 8);
        root->setSpacing(6);

        /* ── Header ────────────────────────────────────────────────── */
        auto* headerLayout = new QVBoxLayout;
        headerLayout->setSpacing(0);

        auto* titleLabel = new QLabel("Broadcast AGC");
        titleLabel->setStyleSheet(
            "font-size: 20px; font-weight: bold; color: #e0f0ff;"
            "padding: 0; margin: 0;");
        titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignBottom);

        auto* subtitleLabel = new QLabel("Automatic Gain Control");
        subtitleLabel->setStyleSheet(
            "font-size: 11px; color: #8090a8; padding: 0; margin: 0;");
        subtitleLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);

        headerLayout->addWidget(titleLabel);
        headerLayout->addWidget(subtitleLabel);
        root->addLayout(headerLayout);

        /* ── Control sections + meters row ─────────────────────────── */
        auto* mainRow = new QHBoxLayout;
        mainRow->setSpacing(8);

        /* — Level Control section — */
        auto* levelGroup = new QGroupBox("Level Control");
        auto* levelLayout = new QVBoxLayout(levelGroup);
        levelLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
        levelLayout->setSpacing(6);

        knobs_[0] = createKnob("Target Level", 0);
        knobs_[1] = createKnob("Max Gain", 1);
        knobs_[2] = createKnob("Max Reduce", 2);

        levelLayout->addWidget(knobs_[0], 0, Qt::AlignHCenter);

        auto* gainRow = new QHBoxLayout;
        gainRow->setSpacing(4);
        gainRow->addWidget(knobs_[1]);
        gainRow->addWidget(knobs_[2]);
        levelLayout->addLayout(gainRow);

        mainRow->addWidget(levelGroup);

        /* — Timing section — */
        auto* timingGroup = new QGroupBox("Timing");
        auto* timingLayout = new QVBoxLayout(timingGroup);
        timingLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
        timingLayout->setSpacing(6);

        knobs_[3] = createKnob("Attack", 3);
        knobs_[4] = createKnob("Release", 4);
        timingLayout->addWidget(knobs_[3], 0, Qt::AlignHCenter);
        timingLayout->addWidget(knobs_[4], 0, Qt::AlignHCenter);

        mainRow->addWidget(timingGroup);

        /* — Gate + Window section — */
        auto* gateGroup = new QGroupBox("Gate + Window");
        auto* gateLayout = new QVBoxLayout(gateGroup);
        gateLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
        gateLayout->setSpacing(6);

        knobs_[5] = createKnob("Gate Thresh", 5);
        knobs_[6] = createKnob("RMS Window", 6);
        gateLayout->addWidget(knobs_[5], 0, Qt::AlignHCenter);
        gateLayout->addWidget(knobs_[6], 0, Qt::AlignHCenter);

        mainRow->addWidget(gateGroup);

        /* — Meters — */
        auto* meterGroup = new QGroupBox("Meters");
        auto* meterLayout = new QHBoxLayout(meterGroup);
        meterLayout->setSpacing(4);

        inputMeter_  = new RackMeter(RackMeter::INPUT_METER);
        gainMeter_   = new RackMeter(RackMeter::GR_METER);
        outputMeter_ = new RackMeter(RackMeter::OUTPUT_METER);

        auto addMeterCol = [&](RackMeter* m, const QString& label) {
            auto* col = new QVBoxLayout;
            col->setSpacing(2);
            auto* lbl = new QLabel(label);
            lbl->setStyleSheet("font-size: 9px; color: #8090a8;");
            lbl->setAlignment(Qt::AlignCenter);
            col->addWidget(lbl);
            col->addWidget(m, 1);
            meterLayout->addLayout(col);
        };

        addMeterCol(inputMeter_,  "IN");
        addMeterCol(gainMeter_,   "GAIN");
        addMeterCol(outputMeter_, "OUT");

        mainRow->addWidget(meterGroup);
        root->addLayout(mainRow, 0);

        root->addStretch(1);

        /* ── Preset row + buttons ──────────────────────────────────── */
        auto* bottomRow = new QHBoxLayout;
        bottomRow->setSpacing(8);

        auto* presetLabel = new QLabel("Preset:");
        presetLabel->setStyleSheet("font-size: 11px;");
        bottomRow->addWidget(presetLabel);

        presetCombo_ = new QComboBox;
        populatePresets();
        bottomRow->addWidget(presetCombo_, 1);

        auto* applyBtn = new QPushButton("Apply");
        auto* resetBtn = new QPushButton("Reset");
        auto* closeBtn = new QPushButton("Close");

        connect(applyBtn, &QPushButton::clicked, this, &BroadcastAgcDialog::applyPreset);
        connect(resetBtn, &QPushButton::clicked, this, &BroadcastAgcDialog::resetParams);
        connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);

        bottomRow->addWidget(applyBtn);
        bottomRow->addWidget(resetBtn);
        bottomRow->addWidget(closeBtn);

        root->addLayout(bottomRow);
    }

    /* ── Knob factory ───────────────────────────────────────────────── */

    RackKnob* createKnob(const QString& title, int paramIndex)
    {
        auto* k = new RackKnob;
        k->setStyle(RackKnob::Bellcap);
        k->setTitle(title);

        connect(k, &RackKnob::valueChanged, this, [this, paramIndex](float val) {
            if (!fx_) return;
            fx_->setParamValue(paramIndex, val);
            updateKnobText(paramIndex);
        });

        return k;
    }

    /* ── Load state from effect ─────────────────────────────────────── */

    void loadFromEffect()
    {
        if (!fx_) return;
        for (int i = 0; i < kParamCount; ++i) {
            if (!knobs_[i]) continue;
            knobs_[i]->blockSignals(true);
            knobs_[i]->setValue(fx_->paramValue(i));
            knobs_[i]->blockSignals(false);
            updateKnobText(i);
        }
    }

    void updateKnobText(int index)
    {
        if (!fx_ || !knobs_[index]) return;
        knobs_[index]->setValueText(
            QString::fromStdString(fx_->paramDisplayValue(index)));
    }

    /* ── Metering ───────────────────────────────────────────────────── */

    void startMetering()
    {
        meterTimer_ = new QTimer(this);
        connect(meterTimer_, &QTimer::timeout, this, [this]() {
            if (!fx_) return;
            inputMeter_->setLevel(fx_->meterInputPeak());
            /* AGC meterGainReduction() returns the applied gain in dB
               (positive = boost, negative = cut). Feed it to the GR_METER
               which renders downward from 0 — absolute value gives the
               magnitude of gain change in either direction. */
            gainMeter_->setLevel(fx_->meterGainReduction());
            outputMeter_->setLevel(fx_->meterOutputPeak());
        });
        meterTimer_->start(50);  /* 20 FPS */
    }

    /* ── Presets ─────────────────────────────────────────────────────── */

    void populatePresets()
    {
        presetCombo_->clear();
        if (!fx_) return;
        auto presets = mc1dsp::PresetManager::listPresets(
            QString::fromUtf8(fx_->id()));
        for (const auto& p : presets)
            presetCombo_->addItem(p.name, p.filePath);
    }

    void applyPreset()
    {
        if (!fx_ || presetCombo_->currentIndex() < 0) return;
        QString path = presetCombo_->currentData().toString();
        if (path.isEmpty()) return;
        auto preset = mc1dsp::PresetManager::loadPreset(path);
        mc1dsp::PresetManager::applyPreset(preset, fx_);
        loadFromEffect();
    }

    void resetParams()
    {
        if (!fx_) return;
        fx_->reset();
        loadFromEffect();
    }

    /* ── Data ────────────────────────────────────────────────────────── */

    static constexpr int kParamCount = 7;

    mc1dsp::DspEffect* fx_ = nullptr;
    RackKnob*   knobs_[kParamCount] = {};
    RackMeter*  inputMeter_  = nullptr;
    RackMeter*  gainMeter_   = nullptr;
    RackMeter*  outputMeter_ = nullptr;
    QComboBox*  presetCombo_ = nullptr;
    QTimer*     meterTimer_  = nullptr;
};
