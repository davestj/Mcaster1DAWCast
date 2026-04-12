/*
 * Mcaster1DAWCast — Official MC1 (Mediacast One) DSP Effects
 * fx_ui/Lexicon480LDialog.h — Lexicon 480L editor faceplate
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "fx_ui/RackKnob.h"
#include "patchbay/dsp/dsp_effect.h"
#include "patchbay/dsp/fx_lexicon_480l.h"

#include <QDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QVector>

class Lexicon480LDialog : public QDialog {
    Q_OBJECT

public:
    explicit Lexicon480LDialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : QDialog(parent)
        , fx_(fx)
    {
        setWindowTitle("Lexicon 480L Random Hall");
        setFixedSize(960, 380);
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
        auto* title = new QLabel("LEXICON  480L  RANDOM  HALL");
        title->setStyleSheet(
            "font-size: 16px; font-weight: bold; color: #ffb020; letter-spacing: 4px;");
        headerRow->addWidget(title);
        headerRow->addStretch();
        algoDisplay_ = new QLabel("RANDOM HALL");
        algoDisplay_->setAlignment(Qt::AlignCenter);
        algoDisplay_->setFixedSize(220, 32);
        algoDisplay_->setStyleSheet(
            "QLabel { background: #0a1426; color: #ffb020;"
            " border: 1px solid #20406a; border-radius: 3px;"
            " font-family: 'Menlo', 'Monaco', monospace;"
            " font-size: 14px; font-weight: bold; letter-spacing: 2px; }");
        headerRow->addWidget(algoDisplay_);
        root->addLayout(headerRow);

        auto* mainRow = new QHBoxLayout;
        mainRow->setSpacing(8);

        // Algorithm + size + spread
        auto* progGroup = new QGroupBox("ALGORITHM");
        auto* progLayout = new QVBoxLayout(progGroup);
        algoCombo_ = new QComboBox;
        algoCombo_->addItems({ "Random Hall", "Random Ambience" });
        progLayout->addWidget(algoCombo_);
        progLayout->addSpacing(6);
        knobs_[mc1dsp::FxLexicon480L::ParamSize] =
            createKnob("SIZE", mc1dsp::FxLexicon480L::ParamSize);
        progLayout->addWidget(knobs_[mc1dsp::FxLexicon480L::ParamSize], 0, Qt::AlignHCenter);
        progLayout->addStretch();
        mainRow->addWidget(progGroup);

        // Decay
        auto* decayGroup = new QGroupBox("DECAY");
        auto* decayLayout = new QHBoxLayout(decayGroup);
        decayLayout->setSpacing(2);
        knobs_[mc1dsp::FxLexicon480L::ParamPreDelay] =
            createKnob("PRE DELAY", mc1dsp::FxLexicon480L::ParamPreDelay);
        knobs_[mc1dsp::FxLexicon480L::ParamRtMid] =
            createKnob("RT MID", mc1dsp::FxLexicon480L::ParamRtMid);
        knobs_[mc1dsp::FxLexicon480L::ParamShape] =
            createKnob("SHAPE", mc1dsp::FxLexicon480L::ParamShape);
        knobs_[mc1dsp::FxLexicon480L::ParamSpread] =
            createKnob("SPREAD", mc1dsp::FxLexicon480L::ParamSpread);
        knobs_[mc1dsp::FxLexicon480L::ParamErTime] =
            createKnob("ER TIME", mc1dsp::FxLexicon480L::ParamErTime);
        decayLayout->addWidget(knobs_[mc1dsp::FxLexicon480L::ParamPreDelay]);
        decayLayout->addWidget(knobs_[mc1dsp::FxLexicon480L::ParamRtMid]);
        decayLayout->addWidget(knobs_[mc1dsp::FxLexicon480L::ParamShape]);
        decayLayout->addWidget(knobs_[mc1dsp::FxLexicon480L::ParamSpread]);
        decayLayout->addWidget(knobs_[mc1dsp::FxLexicon480L::ParamErTime]);
        mainRow->addWidget(decayGroup, 1);

        // Tone
        auto* toneGroup = new QGroupBox("CHARACTER");
        auto* toneLayout = new QHBoxLayout(toneGroup);
        toneLayout->setSpacing(2);
        knobs_[mc1dsp::FxLexicon480L::ParamDiffusion] =
            createKnob("DIFFUSION", mc1dsp::FxLexicon480L::ParamDiffusion);
        knobs_[mc1dsp::FxLexicon480L::ParamHfCut] =
            createKnob("HF CUT", mc1dsp::FxLexicon480L::ParamHfCut);
        knobs_[mc1dsp::FxLexicon480L::ParamBassBoost] =
            createKnob("BASS", mc1dsp::FxLexicon480L::ParamBassBoost);
        knobs_[mc1dsp::FxLexicon480L::ParamTailDensity] =
            createKnob("DENSITY", mc1dsp::FxLexicon480L::ParamTailDensity);
        toneLayout->addWidget(knobs_[mc1dsp::FxLexicon480L::ParamDiffusion]);
        toneLayout->addWidget(knobs_[mc1dsp::FxLexicon480L::ParamHfCut]);
        toneLayout->addWidget(knobs_[mc1dsp::FxLexicon480L::ParamBassBoost]);
        toneLayout->addWidget(knobs_[mc1dsp::FxLexicon480L::ParamTailDensity]);
        mainRow->addWidget(toneGroup, 1);

        // Modulation + mix
        auto* modGroup = new QGroupBox("MOD  &  MIX");
        auto* modLayout = new QHBoxLayout(modGroup);
        modLayout->setSpacing(2);
        knobs_[mc1dsp::FxLexicon480L::ParamModRate] =
            createKnob("MOD RATE", mc1dsp::FxLexicon480L::ParamModRate);
        knobs_[mc1dsp::FxLexicon480L::ParamModDepth] =
            createKnob("MOD DEPTH", mc1dsp::FxLexicon480L::ParamModDepth);
        knobs_[mc1dsp::FxLexicon480L::ParamMix] =
            createKnob("MIX", mc1dsp::FxLexicon480L::ParamMix);
        modLayout->addWidget(knobs_[mc1dsp::FxLexicon480L::ParamModRate]);
        modLayout->addWidget(knobs_[mc1dsp::FxLexicon480L::ParamModDepth]);
        modLayout->addWidget(knobs_[mc1dsp::FxLexicon480L::ParamMix]);
        mainRow->addWidget(modGroup);

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
        connect(applyBtn, &QPushButton::clicked, this, &Lexicon480LDialog::applyPreset);
        connect(resetBtn, &QPushButton::clicked, this, &Lexicon480LDialog::resetParams);
        connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
        bottomRow->addWidget(applyBtn);
        bottomRow->addWidget(resetBtn);
        bottomRow->addWidget(closeBtn);
        root->addLayout(bottomRow);

        connect(algoCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int idx) {
            if (!fx_) return;
            float v = static_cast<float>(idx);
            fx_->setParamValue(mc1dsp::FxLexicon480L::ParamAlgo, v);
            updateAlgoDisplay();
        });
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
        for (int i = 0; i < mc1dsp::FxLexicon480L::kParamCount; ++i) {
            if (knobs_[i]) knobs_[i]->setValue(fx_->paramValue(i));
        }
        int algo = static_cast<int>(
            fx_->paramValue(mc1dsp::FxLexicon480L::ParamAlgo) * 1.999f);
        if (algo < 0) algo = 0;
        if (algo > 1) algo = 1;
        algoCombo_->setCurrentIndex(algo);
        updateAlgoDisplay();
    }

    void updateAlgoDisplay()
    {
        static const char* names[2] = { "RANDOM HALL", "RANDOM AMBIENCE" };
        int idx = algoCombo_->currentIndex();
        if (idx < 0) idx = 0;
        if (idx > 1) idx = 1;
        algoDisplay_->setText(names[idx]);
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
        presets_.append({ "Random Hall Mid",
            { 0.0f, 0.55f, 0.65f, 0.50f, 0.70f, 0.06f, 0.30f, 0.75f, 0.40f, 0.45f, 0.40f, 0.40f, 0.30f, 0.32f } });
        presets_.append({ "Hollywood Scoring",
            { 0.0f, 0.75f, 0.85f, 0.55f, 0.85f, 0.10f, 0.45f, 0.78f, 0.40f, 0.55f, 0.35f, 0.50f, 0.35f, 0.35f } });
        presets_.append({ "Drum Room",
            { 1.0f, 0.30f, 0.50f, 0.40f, 0.50f, 0.04f, 0.30f, 0.60f, 0.45f, 0.40f, 0.20f, 0.20f, 0.20f, 0.30f } });
        presets_.append({ "Vocal Hall",
            { 0.0f, 0.55f, 0.55f, 0.55f, 0.65f, 0.06f, 0.25f, 0.72f, 0.50f, 0.45f, 0.30f, 0.30f, 0.20f, 0.30f } });
        presets_.append({ "Cathedral 480",
            { 0.0f, 0.85f, 0.90f, 0.55f, 0.85f, 0.10f, 0.50f, 0.78f, 0.40f, 0.55f, 0.30f, 0.50f, 0.30f, 0.36f } });
        for (const auto& p : presets_) presetCombo_->addItem(p.name);
    }

    void applyPreset()
    {
        int idx = presetCombo_->currentIndex() - 1;
        if (idx < 0 || idx >= presets_.size() || !fx_) return;
        const Preset& p = presets_[idx];
        for (int i = 0; i < mc1dsp::FxLexicon480L::kParamCount && i < (int)p.values.size(); ++i)
            fx_->setParamValue(i, p.values[i]);
        loadFromEffect();
    }

    void resetParams()
    {
        if (!fx_) return;
        const float def[mc1dsp::FxLexicon480L::kParamCount] = {
            0.0f, 0.5f, 0.65f, 0.5f, 0.7f, 0.06f, 0.3f, 0.75f, 0.4f, 0.5f, 0.4f, 0.4f, 0.3f, 0.30f
        };
        for (int i = 0; i < mc1dsp::FxLexicon480L::kParamCount; ++i)
            fx_->setParamValue(i, def[i]);
        loadFromEffect();
    }

    mc1dsp::DspEffect* fx_ = nullptr;
    QLabel*    algoDisplay_ = nullptr;
    QComboBox* algoCombo_   = nullptr;
    QComboBox* presetCombo_ = nullptr;
    RackKnob*  knobs_[mc1dsp::FxLexicon480L::kParamCount] = {};
    QVector<Preset> presets_;
};
