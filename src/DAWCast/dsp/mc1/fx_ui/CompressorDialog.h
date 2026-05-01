/*
 * Mcaster1AudioPipe — Virtual Audio Routing Application
 * fx_ui/CompressorDialog.h — Compressor / Gate / Limiter editor dialog
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Hardware rack unit editor for FxCompressor (9 params).
 * Three control sections: Input, Compressor, Output — plus level meters.
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

class CompressorDialog : public QDialog {
    Q_OBJECT

public:
    explicit CompressorDialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : QDialog(parent)
        , fx_(fx)
    {
        setWindowTitle("Compressor / Gate / Limiter");
        setMinimumSize(300, 200); resize(600, 300);
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
        root->setSpacing(8);

        /* ── Header ────────────────────────────────────────────────── */
        auto* titleLabel = new QLabel("Compressor / Gate / Limiter");
        titleLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #e0f0ff;");
        titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        root->addWidget(titleLabel);

        /* ── Control sections + meters row ─────────────────────────── */
        auto* mainRow = new QHBoxLayout;
        mainRow->setSpacing(8);

        /* — Input section — */
        auto* inputGroup = new QGroupBox("Input");
        auto* inputLayout = new QVBoxLayout(inputGroup);
        inputLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
        inputLayout->setSpacing(8);

        knobs_[0] = createKnob("Input Gain", 0);
        knobs_[7] = createKnob("Gate Thresh", 7);
        inputLayout->addWidget(knobs_[0], 0, Qt::AlignHCenter);
        inputLayout->addWidget(knobs_[7], 0, Qt::AlignHCenter);
        mainRow->addWidget(inputGroup);

        /* — Compressor section — */
        auto* compGroup = new QGroupBox("Compressor");
        auto* compOuter = new QVBoxLayout(compGroup);
        compOuter->setSpacing(4);

        auto* compRow1 = new QHBoxLayout;
        compRow1->setSpacing(4);
        knobs_[1] = createKnob("Threshold", 1);
        knobs_[2] = createKnob("Ratio", 2);
        knobs_[3] = createKnob("Attack", 3);
        compRow1->addWidget(knobs_[1]);
        compRow1->addWidget(knobs_[2]);
        compRow1->addWidget(knobs_[3]);
        compOuter->addLayout(compRow1);

        auto* compRow2 = new QHBoxLayout;
        compRow2->setSpacing(4);
        knobs_[4] = createKnob("Release", 4);
        knobs_[5] = createKnob("Knee", 5);
        knobs_[6] = createKnob("Makeup", 6);
        compRow2->addWidget(knobs_[4]);
        compRow2->addWidget(knobs_[5]);
        compRow2->addWidget(knobs_[6]);
        compOuter->addLayout(compRow2);

        mainRow->addWidget(compGroup, 1);

        /* — Output section — */
        auto* outGroup = new QGroupBox("Output");
        auto* outLayout = new QVBoxLayout(outGroup);
        outLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

        knobs_[8] = createKnob("Limiter Ceil", 8);
        outLayout->addWidget(knobs_[8], 0, Qt::AlignHCenter);
        mainRow->addWidget(outGroup);

        /* — Meters — */
        auto* meterGroup = new QGroupBox("Meters");
        auto* meterLayout = new QHBoxLayout(meterGroup);
        meterLayout->setSpacing(4);

        inputMeter_ = new RackMeter(RackMeter::INPUT_METER);
        grMeter_    = new RackMeter(RackMeter::GR_METER);
        outputMeter_= new RackMeter(RackMeter::OUTPUT_METER);

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
        addMeterCol(grMeter_,     "GR");
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

        connect(applyBtn, &QPushButton::clicked, this, &CompressorDialog::applyPreset);
        connect(resetBtn, &QPushButton::clicked, this, &CompressorDialog::resetParams);
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
            grMeter_->setLevel(fx_->meterGainReduction());
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

    static constexpr int kParamCount = 9;

    mc1dsp::DspEffect* fx_ = nullptr;
    RackKnob*   knobs_[kParamCount] = {};
    RackMeter*  inputMeter_  = nullptr;
    RackMeter*  grMeter_     = nullptr;
    RackMeter*  outputMeter_ = nullptr;
    QComboBox*  presetCombo_ = nullptr;
    QTimer*     meterTimer_  = nullptr;
};
