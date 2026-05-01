/*
 * Mcaster1AudioPipe — Virtual Audio Routing Application
 * fx_ui/Dbx166xsDialog.h — DBX 166xs Compressor/Gate editor dialog
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Hardware rack unit editor for FxDbx166xs (10 params).
 * Two control sections: Compressor (with OverEasy) and Gate — plus level meters.
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

class Dbx166xsDialog : public QDialog {
    Q_OBJECT

public:
    explicit Dbx166xsDialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : QDialog(parent)
        , fx_(fx)
    {
        setWindowTitle("DBX 166xs Compressor / Gate");
        setMinimumSize(319, 200); resize(638, 300);
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

        auto* titleLabel = new QLabel("DBX 166xs");
        titleLabel->setStyleSheet(
            "font-size: 20px; font-weight: bold; color: #e0f0ff;"
            "padding: 0; margin: 0;");
        titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignBottom);

        auto* subtitleLabel = new QLabel("Compressor / Gate with OverEasy");
        subtitleLabel->setStyleSheet(
            "font-size: 11px; color: #8090a8; padding: 0; margin: 0;");
        subtitleLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);

        headerLayout->addWidget(titleLabel);
        headerLayout->addWidget(subtitleLabel);
        root->addLayout(headerLayout);

        /* ── Control sections + meters row ─────────────────────────── */
        auto* mainRow = new QHBoxLayout;
        mainRow->setSpacing(8);

        /* — Compressor section — */
        auto* compGroup = new QGroupBox("Compressor");
        auto* compOuter = new QVBoxLayout(compGroup);
        compOuter->setSpacing(4);

        auto* compRow1 = new QHBoxLayout;
        compRow1->setSpacing(4);
        knobs_[0] = createKnob("Threshold", 0);
        knobs_[1] = createKnob("Ratio", 1);
        knobs_[2] = createKnob("Attack", 2);
        compRow1->addWidget(knobs_[0]);
        compRow1->addWidget(knobs_[1]);
        compRow1->addWidget(knobs_[2]);
        compOuter->addLayout(compRow1);

        auto* compRow2 = new QHBoxLayout;
        compRow2->setSpacing(4);
        knobs_[3] = createKnob("Release", 3);
        knobs_[4] = createKnob("Output Gain", 4);
        knobs_[5] = createKnob("OverEasy", 5);
        knobs_[5]->setAccentColor(QColor("#FF9800"));
        compRow2->addWidget(knobs_[3]);
        compRow2->addWidget(knobs_[4]);
        compRow2->addWidget(knobs_[5]);
        compOuter->addLayout(compRow2);

        mainRow->addWidget(compGroup, 1);

        /* — Gate section — */
        auto* gateGroup = new QGroupBox("Gate");
        auto* gateOuter = new QVBoxLayout(gateGroup);
        gateOuter->setSpacing(4);

        auto* gateRow1 = new QHBoxLayout;
        gateRow1->setSpacing(4);
        knobs_[6] = createKnob("Gate Thresh", 6);
        knobs_[7] = createKnob("Gate Ratio", 7);
        gateRow1->addWidget(knobs_[6]);
        gateRow1->addWidget(knobs_[7]);
        gateOuter->addLayout(gateRow1);

        auto* gateRow2 = new QHBoxLayout;
        gateRow2->setSpacing(4);
        knobs_[8] = createKnob("Gate Attack", 8);
        knobs_[9] = createKnob("Gate Hold", 9);
        gateRow2->addWidget(knobs_[8]);
        gateRow2->addWidget(knobs_[9]);
        gateOuter->addLayout(gateRow2);

        mainRow->addWidget(gateGroup);

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

        connect(applyBtn, &QPushButton::clicked, this, &Dbx166xsDialog::applyPreset);
        connect(resetBtn, &QPushButton::clicked, this, &Dbx166xsDialog::resetParams);
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

    static constexpr int kParamCount = 10;

    mc1dsp::DspEffect* fx_ = nullptr;
    RackKnob*   knobs_[kParamCount] = {};
    RackMeter*  inputMeter_  = nullptr;
    RackMeter*  grMeter_     = nullptr;
    RackMeter*  outputMeter_ = nullptr;
    QComboBox*  presetCombo_ = nullptr;
    QTimer*     meterTimer_  = nullptr;
};
