/*
 * Mcaster1DAWCast — Official MC1 (Mediacast One) DSP Effects
 * fx_ui/LexiconPcm96Dialog.h — Lexicon PCM 96 editor faceplate
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "fx_ui/RackKnob.h"
#include "patchbay/dsp/dsp_effect.h"
#include "patchbay/dsp/fx_lexicon_pcm96.h"

#include <QDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QVector>

class LexiconPcm96Dialog : public QDialog {
    Q_OBJECT

public:
    explicit LexiconPcm96Dialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : QDialog(parent)
        , fx_(fx)
    {
        setWindowTitle("Lexicon PCM 96 Stereo Reverb");
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
        auto* title = new QLabel("LEXICON  PCM 96  STEREO  REVERB");
        title->setStyleSheet(
            "font-size: 16px; font-weight: bold; color: #ffb020; letter-spacing: 4px;");
        headerRow->addWidget(title);
        headerRow->addStretch();
        algoDisplay_ = new QLabel("CONCERT HALL");
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

        // Algorithm + Size
        auto* progGroup = new QGroupBox("ALGORITHM");
        auto* progLayout = new QVBoxLayout(progGroup);
        algoCombo_ = new QComboBox;
        algoCombo_->addItems({ "Concert Hall", "Random Hall", "Random Ambience" });
        progLayout->addWidget(algoCombo_);
        progLayout->addSpacing(6);
        knobs_[mc1dsp::FxLexiconPcm96::ParamSize] =
            createKnob("SIZE", mc1dsp::FxLexiconPcm96::ParamSize);
        progLayout->addWidget(knobs_[mc1dsp::FxLexiconPcm96::ParamSize], 0, Qt::AlignHCenter);
        progLayout->addStretch();
        mainRow->addWidget(progGroup);

        // Time
        auto* timeGroup = new QGroupBox("DECAY");
        auto* timeLayout = new QHBoxLayout(timeGroup);
        timeLayout->setSpacing(2);
        knobs_[mc1dsp::FxLexiconPcm96::ParamPreDelay] =
            createKnob("PRE DELAY", mc1dsp::FxLexiconPcm96::ParamPreDelay);
        knobs_[mc1dsp::FxLexiconPcm96::ParamRtMid] =
            createKnob("RT MID", mc1dsp::FxLexiconPcm96::ParamRtMid);
        knobs_[mc1dsp::FxLexiconPcm96::ParamShape] =
            createKnob("SHAPE", mc1dsp::FxLexiconPcm96::ParamShape);
        timeLayout->addWidget(knobs_[mc1dsp::FxLexiconPcm96::ParamPreDelay]);
        timeLayout->addWidget(knobs_[mc1dsp::FxLexiconPcm96::ParamRtMid]);
        timeLayout->addWidget(knobs_[mc1dsp::FxLexiconPcm96::ParamShape]);
        mainRow->addWidget(timeGroup);

        // Early reflections
        auto* erGroup = new QGroupBox("EARLY REFLECTIONS");
        auto* erLayout = new QHBoxLayout(erGroup);
        erLayout->setSpacing(2);
        knobs_[mc1dsp::FxLexiconPcm96::ParamErLevel] =
            createKnob("ER LEVEL", mc1dsp::FxLexiconPcm96::ParamErLevel);
        knobs_[mc1dsp::FxLexiconPcm96::ParamErTime] =
            createKnob("ER TIME", mc1dsp::FxLexiconPcm96::ParamErTime);
        knobs_[mc1dsp::FxLexiconPcm96::ParamLateLevel] =
            createKnob("LATE LVL", mc1dsp::FxLexiconPcm96::ParamLateLevel);
        erLayout->addWidget(knobs_[mc1dsp::FxLexiconPcm96::ParamErLevel]);
        erLayout->addWidget(knobs_[mc1dsp::FxLexiconPcm96::ParamErTime]);
        erLayout->addWidget(knobs_[mc1dsp::FxLexiconPcm96::ParamLateLevel]);
        mainRow->addWidget(erGroup);

        // Tone / Width / Mix
        auto* toneGroup = new QGroupBox("TONE  &  IMAGE");
        auto* toneLayout = new QHBoxLayout(toneGroup);
        toneLayout->setSpacing(2);
        knobs_[mc1dsp::FxLexiconPcm96::ParamDiffusion] =
            createKnob("DIFFUSION", mc1dsp::FxLexiconPcm96::ParamDiffusion);
        knobs_[mc1dsp::FxLexiconPcm96::ParamHfDamping] =
            createKnob("HF DAMP", mc1dsp::FxLexiconPcm96::ParamHfDamping);
        knobs_[mc1dsp::FxLexiconPcm96::ParamLfCut] =
            createKnob("LF CUT", mc1dsp::FxLexiconPcm96::ParamLfCut);
        knobs_[mc1dsp::FxLexiconPcm96::ParamStereoWidth] =
            createKnob("WIDTH", mc1dsp::FxLexiconPcm96::ParamStereoWidth);
        knobs_[mc1dsp::FxLexiconPcm96::ParamMix] =
            createKnob("MIX", mc1dsp::FxLexiconPcm96::ParamMix);
        toneLayout->addWidget(knobs_[mc1dsp::FxLexiconPcm96::ParamDiffusion]);
        toneLayout->addWidget(knobs_[mc1dsp::FxLexiconPcm96::ParamHfDamping]);
        toneLayout->addWidget(knobs_[mc1dsp::FxLexiconPcm96::ParamLfCut]);
        toneLayout->addWidget(knobs_[mc1dsp::FxLexiconPcm96::ParamStereoWidth]);
        toneLayout->addWidget(knobs_[mc1dsp::FxLexiconPcm96::ParamMix]);
        mainRow->addWidget(toneGroup, 1);

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
        connect(applyBtn, &QPushButton::clicked, this, &LexiconPcm96Dialog::applyPreset);
        connect(resetBtn, &QPushButton::clicked, this, &LexiconPcm96Dialog::resetParams);
        connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
        bottomRow->addWidget(applyBtn);
        bottomRow->addWidget(resetBtn);
        bottomRow->addWidget(closeBtn);
        root->addLayout(bottomRow);

        connect(algoCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int idx) {
            if (!fx_) return;
            float v = static_cast<float>(idx) / 2.0f;
            fx_->setParamValue(mc1dsp::FxLexiconPcm96::ParamAlgo, v);
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
        for (int i = 0; i < mc1dsp::FxLexiconPcm96::kParamCount; ++i) {
            if (knobs_[i]) knobs_[i]->setValue(fx_->paramValue(i));
        }
        int algo = static_cast<int>(
            fx_->paramValue(mc1dsp::FxLexiconPcm96::ParamAlgo) * 2.999f);
        if (algo < 0) algo = 0;
        if (algo > 2) algo = 2;
        algoCombo_->setCurrentIndex(algo);
        updateAlgoDisplay();
    }

    void updateAlgoDisplay()
    {
        static const char* names[3] = { "CONCERT HALL", "RANDOM HALL", "RANDOM AMBIENCE" };
        int idx = algoCombo_->currentIndex();
        if (idx < 0) idx = 0;
        if (idx > 2) idx = 2;
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
        presets_.append({ "True Hall A",
            { 0.0f/2, 0.55f, 0.65f, 0.06f, 0.40f, 0.30f, 0.55f, 0.70f, 0.50f, 0.40f, 0.15f, 0.75f, 0.32f } });
        presets_.append({ "True Hall B",
            { 0.0f/2, 0.65f, 0.75f, 0.08f, 0.45f, 0.40f, 0.60f, 0.72f, 0.45f, 0.42f, 0.20f, 0.78f, 0.33f } });
        presets_.append({ "Random Ambience",
            { 2.0f/2, 0.30f, 0.45f, 0.04f, 0.55f, 0.35f, 0.30f, 0.65f, 0.50f, 0.35f, 0.20f, 0.65f, 0.30f } });
        presets_.append({ "Concert",
            { 0.0f/2, 0.60f, 0.70f, 0.10f, 0.50f, 0.45f, 0.55f, 0.75f, 0.55f, 0.40f, 0.18f, 0.70f, 0.34f } });
        presets_.append({ "Random Hall Lux",
            { 1.0f/2, 0.55f, 0.65f, 0.06f, 0.45f, 0.40f, 0.60f, 0.72f, 0.65f, 0.45f, 0.18f, 0.78f, 0.34f } });
        for (const auto& p : presets_) presetCombo_->addItem(p.name);
    }

    void applyPreset()
    {
        int idx = presetCombo_->currentIndex() - 1;
        if (idx < 0 || idx >= presets_.size() || !fx_) return;
        const Preset& p = presets_[idx];
        for (int i = 0; i < mc1dsp::FxLexiconPcm96::kParamCount && i < (int)p.values.size(); ++i)
            fx_->setParamValue(i, p.values[i]);
        loadFromEffect();
    }

    void resetParams()
    {
        if (!fx_) return;
        const float def[mc1dsp::FxLexiconPcm96::kParamCount] = {
            0.0f, 0.45f, 0.55f, 0.05f, 0.4f, 0.3f, 0.55f, 0.7f, 0.5f, 0.45f, 0.15f, 0.75f, 0.32f
        };
        for (int i = 0; i < mc1dsp::FxLexiconPcm96::kParamCount; ++i)
            fx_->setParamValue(i, def[i]);
        loadFromEffect();
    }

    mc1dsp::DspEffect* fx_ = nullptr;
    QLabel*    algoDisplay_ = nullptr;
    QComboBox* algoCombo_   = nullptr;
    QComboBox* presetCombo_ = nullptr;
    RackKnob*  knobs_[mc1dsp::FxLexiconPcm96::kParamCount] = {};
    QVector<Preset> presets_;
};
