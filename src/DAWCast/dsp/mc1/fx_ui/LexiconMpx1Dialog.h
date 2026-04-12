/*
 * Mcaster1DAWCast — Official MC1 (Mediacast One) DSP Effects
 * fx_ui/LexiconMpx1Dialog.h — Lexicon MPX 1 editor faceplate
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "fx_ui/RackKnob.h"
#include "patchbay/dsp/dsp_effect.h"
#include "patchbay/dsp/fx_lexicon_mpx1.h"

#include <QDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QVector>

class LexiconMpx1Dialog : public QDialog {
    Q_OBJECT

public:
    explicit LexiconMpx1Dialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : QDialog(parent)
        , fx_(fx)
    {
        setWindowTitle("Lexicon MPX 1 Pitch + Delay");
        setFixedSize(940, 380);
        applyLexiconStyle();
        buildUi();
        loadFromEffect();
    }

private:
    void applyLexiconStyle()
    {
        setStyleSheet(
            "QDialog { background: #0c1422; color: #d6e4f0; }"
            "QGroupBox {"
            "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
            "      stop:0 #102036, stop:1 #0a1626);"
            "  border: 1px solid #20406a; border-left: 3px solid #ffb020;"
            "  border-radius: 6px; margin-top: 14px;"
            "  padding: 10px 6px 6px 6px; font-size: 11px; color: #6088b0;"
            "}"
            "QGroupBox::title {"
            "  subcontrol-origin: margin; subcontrol-position: top left;"
            "  padding: 2px 8px; color: #ffb020; font-weight: bold;"
            "  letter-spacing: 1px;"
            "}"
            "QLabel { color: #d6e4f0; }"
            "QComboBox {"
            "  background: #0a1426; color: #ffb020;"
            "  border: 1px solid #20406a; border-radius: 3px;"
            "  padding: 4px 8px; font-family: 'Menlo', 'Monaco', monospace;"
            "  font-weight: bold; min-width: 160px;"
            "}"
            "QComboBox::drop-down { border: none; }"
            "QComboBox QAbstractItemView {"
            "  background: #0a1426; color: #ffb020;"
            "  selection-background-color: #20406a;"
            "}"
            "QPushButton {"
            "  background: #102036; color: #d6e4f0;"
            "  border: 1px solid #20406a; border-radius: 3px;"
            "  padding: 6px 16px; min-width: 70px;"
            "}"
            "QPushButton:hover { background: #1a2c4a; }"
            "QPushButton:pressed { background: #ffb020; color: #0c1422; }"
        );
    }

    void buildUi()
    {
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(14, 10, 14, 10);
        root->setSpacing(8);

        auto* headerRow = new QHBoxLayout;
        auto* title = new QLabel("LEXICON  MPX 1  PITCH  +  DELAY");
        title->setStyleSheet(
            "font-size: 16px; font-weight: bold; color: #ffb020; letter-spacing: 4px;");
        headerRow->addWidget(title);
        headerRow->addStretch();
        statusDisplay_ = new QLabel("PITCH + 4-TAP + DUCK");
        statusDisplay_->setAlignment(Qt::AlignCenter);
        statusDisplay_->setFixedSize(220, 32);
        statusDisplay_->setStyleSheet(
            "QLabel { background: #0a1426; color: #ffb020;"
            " border: 1px solid #20406a; border-radius: 3px;"
            " font-family: 'Menlo', 'Monaco', monospace;"
            " font-size: 12px; font-weight: bold; letter-spacing: 2px; }");
        headerRow->addWidget(statusDisplay_);
        root->addLayout(headerRow);

        auto* mainRow = new QHBoxLayout;
        mainRow->setSpacing(8);

        // Pitch group
        auto* pitchGroup = new QGroupBox("PITCH");
        auto* pitchLayout = new QHBoxLayout(pitchGroup);
        pitchLayout->setSpacing(2);
        knobs_[mc1dsp::FxLexiconMpx1::ParamPitch1] =
            createKnob("PITCH 1", mc1dsp::FxLexiconMpx1::ParamPitch1);
        knobs_[mc1dsp::FxLexiconMpx1::ParamP1Delay] =
            createKnob("P1 DELAY", mc1dsp::FxLexiconMpx1::ParamP1Delay);
        knobs_[mc1dsp::FxLexiconMpx1::ParamPitch2] =
            createKnob("PITCH 2", mc1dsp::FxLexiconMpx1::ParamPitch2);
        knobs_[mc1dsp::FxLexiconMpx1::ParamP2Delay] =
            createKnob("P2 DELAY", mc1dsp::FxLexiconMpx1::ParamP2Delay);
        pitchLayout->addWidget(knobs_[mc1dsp::FxLexiconMpx1::ParamPitch1]);
        pitchLayout->addWidget(knobs_[mc1dsp::FxLexiconMpx1::ParamP1Delay]);
        pitchLayout->addWidget(knobs_[mc1dsp::FxLexiconMpx1::ParamPitch2]);
        pitchLayout->addWidget(knobs_[mc1dsp::FxLexiconMpx1::ParamP2Delay]);
        mainRow->addWidget(pitchGroup);

        // Tap delays
        auto* tapGroup = new QGroupBox("4-TAP DELAY");
        auto* tapLayout = new QHBoxLayout(tapGroup);
        tapLayout->setSpacing(2);
        knobs_[mc1dsp::FxLexiconMpx1::ParamTap1] =
            createKnob("TAP 1", mc1dsp::FxLexiconMpx1::ParamTap1);
        knobs_[mc1dsp::FxLexiconMpx1::ParamTap2] =
            createKnob("TAP 2", mc1dsp::FxLexiconMpx1::ParamTap2);
        knobs_[mc1dsp::FxLexiconMpx1::ParamTap3] =
            createKnob("TAP 3", mc1dsp::FxLexiconMpx1::ParamTap3);
        knobs_[mc1dsp::FxLexiconMpx1::ParamTap4] =
            createKnob("TAP 4", mc1dsp::FxLexiconMpx1::ParamTap4);
        knobs_[mc1dsp::FxLexiconMpx1::ParamFeedback] =
            createKnob("FEEDBACK", mc1dsp::FxLexiconMpx1::ParamFeedback);
        tapLayout->addWidget(knobs_[mc1dsp::FxLexiconMpx1::ParamTap1]);
        tapLayout->addWidget(knobs_[mc1dsp::FxLexiconMpx1::ParamTap2]);
        tapLayout->addWidget(knobs_[mc1dsp::FxLexiconMpx1::ParamTap3]);
        tapLayout->addWidget(knobs_[mc1dsp::FxLexiconMpx1::ParamTap4]);
        tapLayout->addWidget(knobs_[mc1dsp::FxLexiconMpx1::ParamFeedback]);
        mainRow->addWidget(tapGroup, 1);

        // Ducker + mix
        auto* duckGroup = new QGroupBox("DUCKER  &  MIX");
        auto* duckLayout = new QHBoxLayout(duckGroup);
        duckLayout->setSpacing(2);
        knobs_[mc1dsp::FxLexiconMpx1::ParamDuckThresh] =
            createKnob("DUCK THR", mc1dsp::FxLexiconMpx1::ParamDuckThresh);
        knobs_[mc1dsp::FxLexiconMpx1::ParamDuckRatio] =
            createKnob("DUCK RAT", mc1dsp::FxLexiconMpx1::ParamDuckRatio);
        knobs_[mc1dsp::FxLexiconMpx1::ParamMix] =
            createKnob("MIX", mc1dsp::FxLexiconMpx1::ParamMix);
        duckLayout->addWidget(knobs_[mc1dsp::FxLexiconMpx1::ParamDuckThresh]);
        duckLayout->addWidget(knobs_[mc1dsp::FxLexiconMpx1::ParamDuckRatio]);
        duckLayout->addWidget(knobs_[mc1dsp::FxLexiconMpx1::ParamMix]);
        mainRow->addWidget(duckGroup);

        root->addLayout(mainRow, 1);

        auto* bottomRow = new QHBoxLayout;
        auto* presetLabel = new QLabel("PRESET:");
        presetLabel->setStyleSheet("color: #6088b0; letter-spacing: 1px;");
        bottomRow->addWidget(presetLabel);
        presetCombo_ = new QComboBox;
        populatePresets();
        bottomRow->addWidget(presetCombo_, 1);
        auto* applyBtn = new QPushButton("APPLY");
        auto* resetBtn = new QPushButton("RESET");
        auto* closeBtn = new QPushButton("CLOSE");
        connect(applyBtn, &QPushButton::clicked, this, &LexiconMpx1Dialog::applyPreset);
        connect(resetBtn, &QPushButton::clicked, this, &LexiconMpx1Dialog::resetParams);
        connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
        bottomRow->addWidget(applyBtn);
        bottomRow->addWidget(resetBtn);
        bottomRow->addWidget(closeBtn);
        root->addLayout(bottomRow);
    }

    RackKnob* createKnob(const QString& label, int paramIdx)
    {
        auto* k = new RackKnob;
        k->setTitle(label);
        k->setFixedSize(78, 100);
        connect(k, &RackKnob::valueChanged, this, [this, paramIdx](float v) {
            if (fx_) fx_->setParamValue(paramIdx, v);
        });
        return k;
    }

    void loadFromEffect()
    {
        if (!fx_) return;
        for (int i = 0; i < mc1dsp::FxLexiconMpx1::kParamCount; ++i) {
            if (knobs_[i]) knobs_[i]->setValue(fx_->paramValue(i));
        }
    }

    struct Preset {
        QString name;
        QVector<float> values;
    };

    void populatePresets()
    {
        presetCombo_->clear();
        presetCombo_->addItem("— Factory Bank —");
        presets_.clear();
        // Iconic MPX 1 patches
        presets_.append({ "Detune Lead",
            { 0.55f, 0.45f, 0.05f, 0.05f, 0.10f, 0.20f, 0.30f, 0.40f, 0.20f, 0.55f, 0.30f, 0.35f } });
        presets_.append({ "Octaver Up/Down",
            { 1.00f, 0.00f, 0.05f, 0.05f, 0.05f, 0.10f, 0.15f, 0.20f, 0.10f, 0.50f, 0.20f, 0.40f } });
        presets_.append({ "Slap Tap",
            { 0.50f, 0.50f, 0.00f, 0.00f, 0.08f, 0.16f, 0.24f, 0.32f, 0.30f, 0.50f, 0.30f, 0.30f } });
        presets_.append({ "Dub Echo",
            { 0.50f, 0.50f, 0.00f, 0.00f, 0.30f, 0.50f, 0.70f, 0.90f, 0.65f, 0.45f, 0.25f, 0.45f } });
        presets_.append({ "Vocal Doubler",
            { 0.51f, 0.49f, 0.05f, 0.07f, 0.04f, 0.08f, 0.10f, 0.12f, 0.10f, 0.55f, 0.30f, 0.30f } });
        for (const auto& p : presets_) presetCombo_->addItem(p.name);
    }

    void applyPreset()
    {
        int idx = presetCombo_->currentIndex() - 1;
        if (idx < 0 || idx >= presets_.size() || !fx_) return;
        const Preset& p = presets_[idx];
        for (int i = 0; i < mc1dsp::FxLexiconMpx1::kParamCount && i < (int)p.values.size(); ++i)
            fx_->setParamValue(i, p.values[i]);
        loadFromEffect();
    }

    void resetParams()
    {
        if (!fx_) return;
        const float def[mc1dsp::FxLexiconMpx1::kParamCount] = {
            0.5f, 0.55f, 0.05f, 0.10f, 0.20f, 0.40f, 0.60f, 0.80f, 0.40f, 0.50f, 0.30f, 0.30f
        };
        for (int i = 0; i < mc1dsp::FxLexiconMpx1::kParamCount; ++i)
            fx_->setParamValue(i, def[i]);
        loadFromEffect();
    }

    mc1dsp::DspEffect* fx_ = nullptr;
    QLabel*    statusDisplay_ = nullptr;
    QComboBox* presetCombo_   = nullptr;
    RackKnob*  knobs_[mc1dsp::FxLexiconMpx1::kParamCount] = {};
    QVector<Preset> presets_;
};
