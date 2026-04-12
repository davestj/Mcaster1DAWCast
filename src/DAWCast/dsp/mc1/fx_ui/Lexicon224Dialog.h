/*
 * Mcaster1DAWCast — Official MC1 (Mediacast One) DSP Effects
 * fx_ui/Lexicon224Dialog.h — Lexicon 224 Digital Reverb editor
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Editor faceplate for FxLexicon224. Models the dark blue / amber LCD
 * look of the original 224 hardware. Header-only with inline implementation
 * so it can be MOC'd from a single header (matches the rest of the
 * fx_ui pack).
 */

#pragma once

#include "fx_ui/RackKnob.h"
#include "patchbay/dsp/dsp_effect.h"
#include "patchbay/dsp/fx_lexicon_224.h"
#include "patchbay/dsp/preset_manager.h"

#include <QDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QFrame>
#include <QVector>

#include <string>

class Lexicon224Dialog : public QDialog {
    Q_OBJECT

public:
    explicit Lexicon224Dialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : QDialog(parent)
        , fx_(fx)
    {
        setWindowTitle("Lexicon 224 Digital Reverb");
        setFixedSize(880, 360);
        buildUi();
        loadFromEffect();
    }

private:
    void buildUi()
    {
        // Faceplate stylesheet — deep midnight blue / amber LCD
        setStyleSheet(
            "QDialog { background: #0c1422; color: #d6e4f0; }"
            "QGroupBox {"
            "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
            "      stop:0 #102036, stop:1 #0a1626);"
            "  border: 1px solid #20406a;"
            "  border-left: 3px solid #ffb020;"
            "  border-radius: 6px;"
            "  margin-top: 14px;"
            "  padding: 10px 6px 6px 6px;"
            "  font-size: 11px;"
            "  color: #6088b0;"
            "}"
            "QGroupBox::title {"
            "  subcontrol-origin: margin;"
            "  subcontrol-position: top left;"
            "  padding: 2px 8px;"
            "  color: #ffb020;"
            "  font-weight: bold;"
            "  letter-spacing: 1px;"
            "}"
            "QLabel { color: #d6e4f0; }"
            "QComboBox {"
            "  background: #0a1426; color: #ffb020;"
            "  border: 1px solid #20406a;"
            "  border-radius: 3px; padding: 4px 8px;"
            "  font-family: 'Menlo', 'Monaco', monospace; font-weight: bold;"
            "  min-width: 160px;"
            "}"
            "QComboBox::drop-down { border: none; }"
            "QComboBox QAbstractItemView {"
            "  background: #0a1426; color: #ffb020;"
            "  selection-background-color: #20406a;"
            "}"
            "QPushButton {"
            "  background: #102036; color: #d6e4f0;"
            "  border: 1px solid #20406a;"
            "  border-radius: 3px; padding: 6px 16px; min-width: 70px;"
            "}"
            "QPushButton:hover { background: #1a2c4a; }"
            "QPushButton:pressed { background: #ffb020; color: #0c1422; }"
        );

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(14, 10, 14, 10);
        root->setSpacing(8);

        // Header — title + LCD-style program readout
        auto* headerRow = new QHBoxLayout;
        headerRow->setSpacing(12);

        auto* title = new QLabel("LEXICON  224  DIGITAL  REVERB");
        title->setStyleSheet(
            "font-size: 16px; font-weight: bold; color: #ffb020; "
            "letter-spacing: 4px;");
        headerRow->addWidget(title);
        headerRow->addStretch();

        programDisplay_ = new QLabel("HALL A");
        programDisplay_->setAlignment(Qt::AlignCenter);
        programDisplay_->setFixedSize(180, 32);
        programDisplay_->setStyleSheet(
            "QLabel {"
            "  background: #0a1426; color: #ffb020;"
            "  border: 1px solid #20406a; border-radius: 3px;"
            "  font-family: 'Menlo', 'Monaco', monospace;"
            "  font-size: 16px; font-weight: bold; letter-spacing: 3px;"
            "}");
        headerRow->addWidget(programDisplay_);
        root->addLayout(headerRow);

        // Three control groups + program selector
        auto* mainRow = new QHBoxLayout;
        mainRow->setSpacing(8);

        // Program group
        auto* progGroup = new QGroupBox("PROGRAM");
        auto* progLayout = new QVBoxLayout(progGroup);
        progLayout->setSpacing(6);
        progCombo_ = new QComboBox;
        progCombo_->addItems({ "Concert Hall A", "Concert Hall B", "Plate", "Room" });
        progCombo_->setMinimumWidth(140);
        progLayout->addWidget(progCombo_);
        progLayout->addSpacing(6);
        knobs_[mc1dsp::FxLexicon224::ParamSize] =
            createKnob("SIZE", mc1dsp::FxLexicon224::ParamSize);
        progLayout->addWidget(knobs_[mc1dsp::FxLexicon224::ParamSize], 0, Qt::AlignHCenter);
        progLayout->addStretch();
        mainRow->addWidget(progGroup);

        // Time group
        auto* timeGroup = new QGroupBox("TIME");
        auto* timeLayout = new QHBoxLayout(timeGroup);
        timeLayout->setSpacing(2);
        knobs_[mc1dsp::FxLexicon224::ParamPreDelay] =
            createKnob("PRE DELAY", mc1dsp::FxLexicon224::ParamPreDelay);
        knobs_[mc1dsp::FxLexicon224::ParamDecay] =
            createKnob("DECAY", mc1dsp::FxLexicon224::ParamDecay);
        knobs_[mc1dsp::FxLexicon224::ParamBassMult] =
            createKnob("BASS MULT", mc1dsp::FxLexicon224::ParamBassMult);
        knobs_[mc1dsp::FxLexicon224::ParamTrebleDecay] =
            createKnob("TREBLE", mc1dsp::FxLexicon224::ParamTrebleDecay);
        timeLayout->addWidget(knobs_[mc1dsp::FxLexicon224::ParamPreDelay]);
        timeLayout->addWidget(knobs_[mc1dsp::FxLexicon224::ParamDecay]);
        timeLayout->addWidget(knobs_[mc1dsp::FxLexicon224::ParamBassMult]);
        timeLayout->addWidget(knobs_[mc1dsp::FxLexicon224::ParamTrebleDecay]);
        mainRow->addWidget(timeGroup, 1);

        // Tone / Diffusion / Mod group
        auto* toneGroup = new QGroupBox("CHARACTER");
        auto* toneLayout = new QHBoxLayout(toneGroup);
        toneLayout->setSpacing(2);
        knobs_[mc1dsp::FxLexicon224::ParamDiffusion] =
            createKnob("DIFFUSION", mc1dsp::FxLexicon224::ParamDiffusion);
        knobs_[mc1dsp::FxLexicon224::ParamHfDamping] =
            createKnob("HF DAMP", mc1dsp::FxLexicon224::ParamHfDamping);
        knobs_[mc1dsp::FxLexicon224::ParamLfCut] =
            createKnob("LF CUT", mc1dsp::FxLexicon224::ParamLfCut);
        knobs_[mc1dsp::FxLexicon224::ParamModDepth] =
            createKnob("MOD DEPTH", mc1dsp::FxLexicon224::ParamModDepth);
        toneLayout->addWidget(knobs_[mc1dsp::FxLexicon224::ParamDiffusion]);
        toneLayout->addWidget(knobs_[mc1dsp::FxLexicon224::ParamHfDamping]);
        toneLayout->addWidget(knobs_[mc1dsp::FxLexicon224::ParamLfCut]);
        toneLayout->addWidget(knobs_[mc1dsp::FxLexicon224::ParamModDepth]);
        mainRow->addWidget(toneGroup, 1);

        // Mix group
        auto* mixGroup = new QGroupBox("MIX");
        auto* mixLayout = new QVBoxLayout(mixGroup);
        knobs_[mc1dsp::FxLexicon224::ParamMix] =
            createKnob("WET / DRY", mc1dsp::FxLexicon224::ParamMix);
        mixLayout->addWidget(knobs_[mc1dsp::FxLexicon224::ParamMix], 0, Qt::AlignCenter);
        mainRow->addWidget(mixGroup);

        root->addLayout(mainRow, 1);

        // Preset row
        auto* bottomRow = new QHBoxLayout;
        bottomRow->setSpacing(8);
        auto* presetLabel = new QLabel("PRESET:");
        presetLabel->setStyleSheet("font-size: 11px; color: #6088b0; letter-spacing: 1px;");
        bottomRow->addWidget(presetLabel);
        presetCombo_ = new QComboBox;
        populatePresets();
        bottomRow->addWidget(presetCombo_, 1);
        auto* applyBtn = new QPushButton("APPLY");
        auto* resetBtn = new QPushButton("RESET");
        auto* closeBtn = new QPushButton("CLOSE");
        connect(applyBtn, &QPushButton::clicked, this, &Lexicon224Dialog::applyPreset);
        connect(resetBtn, &QPushButton::clicked, this, &Lexicon224Dialog::resetParams);
        connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
        bottomRow->addWidget(applyBtn);
        bottomRow->addWidget(resetBtn);
        bottomRow->addWidget(closeBtn);
        root->addLayout(bottomRow);

        connect(progCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int idx) {
            if (!fx_) return;
            float v = static_cast<float>(idx) / 3.0f;
            fx_->setParamValue(mc1dsp::FxLexicon224::ParamProgram, v);
            updateProgramDisplay();
        });
    }

    RackKnob* createKnob(const QString& label, int paramIdx)
    {
        auto* k = new RackKnob;
        k->setTitle(label);
        k->setFixedSize(78, 100);
        connect(k, &RackKnob::valueChanged, this, [this, paramIdx](float v) {
            if (!fx_) return;
            fx_->setParamValue(paramIdx, v);
            if (paramIdx == mc1dsp::FxLexicon224::ParamProgram)
                updateProgramDisplay();
        });
        return k;
    }

    void loadFromEffect()
    {
        if (!fx_) return;
        for (int i = 0; i < mc1dsp::FxLexicon224::kParamCount; ++i) {
            if (knobs_[i]) knobs_[i]->setValue(fx_->paramValue(i));
        }
        int prog = static_cast<int>(
            fx_->paramValue(mc1dsp::FxLexicon224::ParamProgram) * 3.999f);
        if (prog < 0) prog = 0;
        if (prog > 3) prog = 3;
        progCombo_->setCurrentIndex(prog);
        updateProgramDisplay();
    }

    void updateProgramDisplay()
    {
        static const char* names[4] = { "HALL A", "HALL B", "PLATE", "ROOM" };
        int idx = progCombo_->currentIndex();
        if (idx < 0) idx = 0;
        if (idx > 3) idx = 3;
        programDisplay_->setText(names[idx]);
    }

    void populatePresets()
    {
        presetCombo_->clear();
        presetCombo_->addItem("— Factory Bank —");
        presets_.clear();
        // Iconic 224 presets — store the parameter snapshot inline so the
        // bank works even before PresetManager has scanned the disk.
        presets_.append({ "Bowie Heroes Plate",
            { 2, 0.04f, 0.55f, 0.55f, 0.78f, 0.30f, 0.10f, 0.55f, 0.50f, 0.30f, 0.40f } });
        presets_.append({ "Phil Collins Gate Snare",
            { 0, 0.00f, 0.18f, 0.30f, 0.85f, 0.25f, 0.20f, 0.50f, 0.45f, 0.20f, 0.50f } });
        presets_.append({ "Cathedral",
            { 1, 0.10f, 0.92f, 0.95f, 0.65f, 0.40f, 0.10f, 0.65f, 0.50f, 0.50f, 0.45f } });
        presets_.append({ "Vocal Plate",
            { 2, 0.04f, 0.40f, 0.40f, 0.75f, 0.50f, 0.30f, 0.50f, 0.55f, 0.45f, 0.32f } });
        presets_.append({ "Drum Room",
            { 3, 0.02f, 0.20f, 0.30f, 0.55f, 0.55f, 0.40f, 0.50f, 0.40f, 0.20f, 0.28f } });
        presets_.append({ "Concert Hall",
            { 0, 0.08f, 0.65f, 0.70f, 0.72f, 0.40f, 0.15f, 0.55f, 0.55f, 0.45f, 0.35f } });
        for (const auto& p : presets_)
            presetCombo_->addItem(p.name);
    }

    void applyPreset()
    {
        int idx = presetCombo_->currentIndex() - 1;  // skip header item
        if (idx < 0 || idx >= presets_.size() || !fx_) return;
        const Preset& p = presets_[idx];
        for (int i = 0; i < mc1dsp::FxLexicon224::kParamCount && i < (int)p.values.size(); ++i) {
            float v = p.values[i];
            if (i == 0) v = v / 3.0f;  // program is stored as int 0..3
            fx_->setParamValue(i, v);
        }
        loadFromEffect();
    }

    void resetParams()
    {
        if (!fx_) return;
        // Hard reset — re-instantiating would be cleaner but we don't
        // own the effect; just push known defaults.
        const float def[mc1dsp::FxLexicon224::kParamCount] = {
            0.0f, 0.08f, 0.45f, 0.5f, 0.7f, 0.45f, 0.15f, 0.5f, 0.55f, 0.35f, 0.30f
        };
        for (int i = 0; i < mc1dsp::FxLexicon224::kParamCount; ++i) {
            fx_->setParamValue(i, def[i]);
        }
        loadFromEffect();
    }

    struct Preset {
        QString name;
        QVector<float> values;
    };

    mc1dsp::DspEffect* fx_ = nullptr;
    QLabel*    programDisplay_ = nullptr;
    QComboBox* progCombo_      = nullptr;
    QComboBox* presetCombo_    = nullptr;
    RackKnob*  knobs_[mc1dsp::FxLexicon224::kParamCount] = {};
    QVector<Preset> presets_;
};
