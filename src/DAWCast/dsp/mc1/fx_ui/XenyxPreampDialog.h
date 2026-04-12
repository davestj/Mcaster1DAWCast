/*
 * Mcaster1AudioPipe — Virtual Audio Routing Application
 * fx_ui/XenyxPreampDialog.h — Mackie Xenyx Mic Preamp editor dialog
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Channel strip editor matching FxXenyxPreamp (9 params).
 * Left-to-right signal flow: Gain -> HPF -> Comp -> 3-Band EQ -> Output.
 * EQ section uses vertical stack (High/Mid/Low) like a real Mackie strip.
 * Output meter. Preset load/save support.
 * Header-only implementation with hardware rack unit aesthetic.
 */

#pragma once

#include "fx_ui/RackKnob.h"
#include "fx_ui/RackMeter.h"
#include "patchbay/dsp/dsp_effect.h"
#include "patchbay/dsp/preset_manager.h"

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QTimer>
#include <QInputDialog>

class XenyxPreampDialog : public QDialog {
    Q_OBJECT

public:
    explicit XenyxPreampDialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : QDialog(parent), fx_(fx)
    {
        setWindowTitle("Mackie Xenyx Preamp");
        setFixedSize(750, 450);
        setAttribute(Qt::WA_DeleteOnClose, false);
        setStyleSheet(kDialogStyle);

        auto* root = new QVBoxLayout(this);
        root->setSpacing(6);
        root->setContentsMargins(12, 10, 12, 10);

        /* ── Header ─────────────────────────────────────────────── */

        auto* headerLayout = new QHBoxLayout;

        auto* titleBlock = new QVBoxLayout;
        auto* titleLabel = new QLabel("XENYX");
        titleLabel->setStyleSheet(
            "font-size: 24px; font-weight: bold; color: #e0e8f0;"
            "font-family: 'Helvetica Neue', 'Arial Black', sans-serif;"
            "letter-spacing: 3px;");
        titleBlock->addWidget(titleLabel);

        auto* subtitleLabel = new QLabel("Mic Preamp + Channel Strip");
        subtitleLabel->setStyleSheet(
            "font-size: 11px; color: #708090; font-style: italic;"
            "letter-spacing: 1px;");
        titleBlock->addWidget(subtitleLabel);
        titleBlock->setSpacing(0);

        headerLayout->addLayout(titleBlock);
        headerLayout->addStretch();

        /* Preset controls */
        auto* presetLabel = new QLabel("Preset:");
        presetLabel->setStyleSheet("font-size: 11px; color: #90a0b0;");
        headerLayout->addWidget(presetLabel);

        presetCombo_ = new QComboBox;
        presetCombo_->setFixedWidth(160);
        presetCombo_->setStyleSheet(kComboStyle);
        headerLayout->addWidget(presetCombo_);

        auto* loadBtn = new QPushButton("Load");
        loadBtn->setFixedWidth(50);
        loadBtn->setStyleSheet(kButtonStyle);
        connect(loadBtn, &QPushButton::clicked, this, &XenyxPreampDialog::loadPreset);
        headerLayout->addWidget(loadBtn);

        auto* saveBtn = new QPushButton("Save");
        saveBtn->setFixedWidth(50);
        saveBtn->setStyleSheet(kButtonStyle);
        connect(saveBtn, &QPushButton::clicked, this, &XenyxPreampDialog::savePreset);
        headerLayout->addWidget(saveBtn);

        root->addLayout(headerLayout);

        /* ── Channel strip (left to right) ──────────────────────── */

        auto* stripLayout = new QHBoxLayout;
        stripLayout->setSpacing(6);

        /* --- Gain Stage --- */
        auto* gainGroup = createSection("GAIN");
        auto* gainInner = new QVBoxLayout;
        gainInner->setAlignment(Qt::AlignCenter);
        gainInner->setContentsMargins(8, 4, 8, 8);

        knobInputGain_ = createKnob("Input Gain", QColor("#00d4aa"), 90, 120);
        gainInner->addWidget(knobInputGain_, 0, Qt::AlignCenter);

        static_cast<QVBoxLayout*>(gainGroup->layout())->addLayout(gainInner);
        stripLayout->addWidget(gainGroup);

        /* --- HPF Section --- */
        auto* hpfGroup = createSection("HPF");
        auto* hpfInner = new QVBoxLayout;
        hpfInner->setSpacing(6);
        hpfInner->setContentsMargins(8, 4, 8, 8);

        hpfEnableCheck_ = new QCheckBox("Enable");
        hpfEnableCheck_->setStyleSheet(kCheckStyle);
        hpfEnableCheck_->setChecked(fx_->paramValue(1) >= 0.5f);
        connect(hpfEnableCheck_, &QCheckBox::toggled, [this](bool on) {
            if (fx_) fx_->setParamValue(1, on ? 1.0f : 0.0f);
            knobHpfFreq_->setEnabled(on);
        });
        hpfInner->addWidget(hpfEnableCheck_, 0, Qt::AlignCenter);

        knobHpfFreq_ = createKnob("Frequency", QColor("#00d4aa"));
        knobHpfFreq_->setEnabled(fx_->paramValue(1) >= 0.5f);
        hpfInner->addWidget(knobHpfFreq_, 0, Qt::AlignCenter);

        static_cast<QVBoxLayout*>(hpfGroup->layout())->addLayout(hpfInner);
        stripLayout->addWidget(hpfGroup);

        /* --- Compressor --- */
        auto* compGroup = createSection("COMP");
        auto* compInner = new QVBoxLayout;
        compInner->setAlignment(Qt::AlignCenter);
        compInner->setContentsMargins(8, 4, 8, 8);

        knobCompAmount_ = createKnob("Amount", QColor("#FFA726"));
        compInner->addWidget(knobCompAmount_, 0, Qt::AlignCenter);

        static_cast<QVBoxLayout*>(compGroup->layout())->addLayout(compInner);
        stripLayout->addWidget(compGroup);

        /* --- 3-Band EQ (vertical stack: High, Mid+Freq, Low) --- */
        auto* eqGroup = createSection("3-BAND EQ");
        auto* eqInner = new QVBoxLayout;
        eqInner->setSpacing(4);
        eqInner->setContentsMargins(8, 4, 8, 8);

        /* EQ High (top) */
        auto* eqHighRow = new QHBoxLayout;
        auto* highLabel = new QLabel("HI");
        highLabel->setFixedWidth(22);
        highLabel->setAlignment(Qt::AlignCenter);
        highLabel->setStyleSheet("font-size: 9px; font-weight: bold; color: #26C6DA;");
        eqHighRow->addWidget(highLabel);
        knobEqHigh_ = createKnob("12 kHz", QColor("#26C6DA"));
        eqHighRow->addWidget(knobEqHigh_);
        eqInner->addLayout(eqHighRow);

        /* EQ Mid + Mid Freq (middle) */
        auto* eqMidRow = new QHBoxLayout;
        eqMidRow->setSpacing(4);
        auto* midLabel = new QLabel("MID");
        midLabel->setFixedWidth(22);
        midLabel->setAlignment(Qt::AlignCenter);
        midLabel->setStyleSheet("font-size: 9px; font-weight: bold; color: #00d4aa;");
        eqMidRow->addWidget(midLabel);
        knobEqMid_ = createKnob("Gain", QColor("#00d4aa"));
        eqMidRow->addWidget(knobEqMid_);
        knobEqMidFreq_ = createKnob("Freq", QColor("#00d4aa"), 66, 95);
        eqMidRow->addWidget(knobEqMidFreq_);
        eqInner->addLayout(eqMidRow);

        /* EQ Low (bottom) */
        auto* eqLowRow = new QHBoxLayout;
        auto* lowLabel = new QLabel("LO");
        lowLabel->setFixedWidth(22);
        lowLabel->setAlignment(Qt::AlignCenter);
        lowLabel->setStyleSheet("font-size: 9px; font-weight: bold; color: #FFA726;");
        eqLowRow->addWidget(lowLabel);
        knobEqLow_ = createKnob("80 Hz", QColor("#FFA726"));
        eqLowRow->addWidget(knobEqLow_);
        eqInner->addLayout(eqLowRow);

        static_cast<QVBoxLayout*>(eqGroup->layout())->addLayout(eqInner);
        stripLayout->addWidget(eqGroup);

        /* --- Output stage + meter --- */
        auto* outGroup = createSection("OUTPUT");
        auto* outInner = new QHBoxLayout;
        outInner->setSpacing(6);
        outInner->setContentsMargins(8, 4, 8, 8);

        knobOutputLevel_ = createKnob("Level", QColor("#e0e8f0"), 90, 120);
        outInner->addWidget(knobOutputLevel_);

        auto* meterCol = new QVBoxLayout;
        meterCol->setSpacing(2);
        auto* meterLabel = new QLabel("OUT");
        meterLabel->setAlignment(Qt::AlignCenter);
        meterLabel->setStyleSheet("font-size: 9px; color: #00d4aa; font-weight: bold;");
        meterCol->addWidget(meterLabel);

        outputMeter_ = new RackMeter(RackMeter::OUTPUT_METER);
        outputMeter_->setFixedWidth(28);
        meterCol->addWidget(outputMeter_, 1);
        outInner->addLayout(meterCol);

        static_cast<QVBoxLayout*>(outGroup->layout())->addLayout(outInner);
        stripLayout->addWidget(outGroup);

        root->addLayout(stripLayout, 1);

        /* ── Connect knobs to effect params ─────────────────────── */

        connectKnob(knobInputGain_,   0);
        /* param 1 (HPF Enable) handled by checkbox */
        connectKnob(knobHpfFreq_,     2);
        connectKnob(knobCompAmount_,  3);
        connectKnob(knobEqLow_,       4);
        connectKnob(knobEqMid_,       5);
        connectKnob(knobEqMidFreq_,   6);
        connectKnob(knobEqHigh_,      7);
        connectKnob(knobOutputLevel_, 8);

        /* ── Read initial values from effect ────────────────────── */

        readAllParams();
        refreshPresetList();

        /* ── Meter update timer (20 FPS) ────────────────────────── */

        meterTimer_ = new QTimer(this);
        connect(meterTimer_, &QTimer::timeout, this, &XenyxPreampDialog::updateMeters);
        meterTimer_->start(50);
    }

private slots:
    void updateMeters()
    {
        if (!fx_) return;

        /* Output meter */
        float outPeak = fx_->meterOutputPeak();
        outputMeter_->setLevel(outPeak);

        /* Refresh display strings */
        updateKnobText(knobInputGain_,   0);
        updateKnobText(knobHpfFreq_,     2);
        updateKnobText(knobCompAmount_,  3);
        updateKnobText(knobEqLow_,       4);
        updateKnobText(knobEqMid_,       5);
        updateKnobText(knobEqMidFreq_,   6);
        updateKnobText(knobEqHigh_,      7);
        updateKnobText(knobOutputLevel_, 8);
    }

    void loadPreset()
    {
        int idx = presetCombo_->currentIndex();
        if (idx < 0 || idx >= presets_.size()) return;

        mc1dsp::PresetManager::applyPreset(presets_[idx], fx_);
        readAllParams();
    }

    void savePreset()
    {
        bool ok = false;
        QString name = QInputDialog::getText(this, "Save Preset",
            "Preset name:", QLineEdit::Normal, "", &ok);
        if (!ok || name.trimmed().isEmpty()) return;

        auto preset = mc1dsp::PresetManager::capturePreset(fx_, name.trimmed());
        mc1dsp::PresetManager::savePreset(preset);
        refreshPresetList();
    }

private:
    /* ── Helpers ─────────────────────────────────────────────── */

    QGroupBox* createSection(const QString& title)
    {
        auto* group = new QGroupBox(title);
        group->setStyleSheet(kGroupStyle);
        auto* layout = new QVBoxLayout(group);
        layout->setSpacing(4);
        layout->setContentsMargins(4, 18, 4, 4);
        return group;
    }

    RackKnob* createKnob(const QString& title, const QColor& accent,
                          int w = 80, int h = 110)
    {
        auto* knob = new RackKnob;
        knob->setTitle(title);
        knob->setAccentColor(accent);
        knob->setNotches(11);
        knob->setFixedSize(w, h);
        return knob;
    }

    void connectKnob(RackKnob* knob, int paramIndex)
    {
        connect(knob, &RackKnob::valueChanged, [this, paramIndex](float v) {
            if (fx_) {
                fx_->setParamValue(paramIndex, v);
            }
        });
    }

    void updateKnobText(RackKnob* knob, int paramIndex)
    {
        if (!fx_) return;
        std::string text = fx_->paramDisplayValue(paramIndex);
        knob->setValueText(QString::fromStdString(text));
    }

    void readAllParams()
    {
        if (!fx_) return;

        auto readKnob = [this](RackKnob* knob, int idx) {
            knob->blockSignals(true);
            knob->setValue(fx_->paramValue(idx));
            knob->blockSignals(false);
            updateKnobText(knob, idx);
        };

        readKnob(knobInputGain_,   0);
        readKnob(knobHpfFreq_,     2);
        readKnob(knobCompAmount_,  3);
        readKnob(knobEqLow_,       4);
        readKnob(knobEqMid_,       5);
        readKnob(knobEqMidFreq_,   6);
        readKnob(knobEqHigh_,      7);
        readKnob(knobOutputLevel_, 8);

        /* HPF enable checkbox */
        hpfEnableCheck_->blockSignals(true);
        hpfEnableCheck_->setChecked(fx_->paramValue(1) >= 0.5f);
        hpfEnableCheck_->blockSignals(false);
        knobHpfFreq_->setEnabled(fx_->paramValue(1) >= 0.5f);
    }

    void refreshPresetList()
    {
        presetCombo_->clear();
        presets_ = mc1dsp::PresetManager::listPresets(
            QString::fromLatin1(fx_->id()));
        for (const auto& p : presets_) {
            QString label = p.isFactory ? p.name + "  [F]" : p.name;
            presetCombo_->addItem(label);
        }
    }

    /* ── Style constants ────────────────────────────────────── */

    static constexpr const char* kDialogStyle =
        "QDialog {"
        "  background: qlineargradient(y1:0, y2:1,"
        "    stop:0 #1a2233, stop:0.5 #141c28, stop:1 #101820);"
        "}"
        "QLabel { color: #c0d0e0; }";

    static constexpr const char* kGroupStyle =
        "QGroupBox {"
        "  background: qlineargradient(y1:0, y2:1,"
        "    stop:0 #1e2a3a, stop:1 #161e2c);"
        "  border: 1px solid #2a3a4c;"
        "  border-radius: 4px;"
        "  font-size: 10px;"
        "  font-weight: bold;"
        "  color: #00d4aa;"
        "  padding-top: 14px;"
        "  margin-top: 4px;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  subcontrol-position: top center;"
        "  padding: 2px 10px;"
        "  background: #141c28;"
        "  border: 1px solid #2a3a4c;"
        "  border-radius: 3px;"
        "  letter-spacing: 1px;"
        "}";

    static constexpr const char* kComboStyle =
        "QComboBox {"
        "  background: #1a2233;"
        "  border: 1px solid #2a3a4c;"
        "  border-radius: 3px;"
        "  color: #c0d0e0;"
        "  padding: 3px 8px;"
        "  font-size: 11px;"
        "}"
        "QComboBox::drop-down { border: none; width: 20px; }"
        "QComboBox QAbstractItemView {"
        "  background: #1a2233;"
        "  border: 1px solid #2a3a4c;"
        "  color: #c0d0e0;"
        "  selection-background-color: #00d4aa;"
        "  selection-color: #0a0e14;"
        "}";

    static constexpr const char* kButtonStyle =
        "QPushButton {"
        "  background: #1e2a3a;"
        "  border: 1px solid #2a3a4c;"
        "  border-radius: 3px;"
        "  color: #c0d0e0;"
        "  padding: 3px 8px;"
        "  font-size: 11px;"
        "}"
        "QPushButton:hover {"
        "  background: #2a3a4c;"
        "  border-color: #00d4aa;"
        "}"
        "QPushButton:pressed {"
        "  background: #00d4aa;"
        "  color: #0a0e14;"
        "}";

    static constexpr const char* kCheckStyle =
        "QCheckBox {"
        "  color: #c0d0e0;"
        "  font-size: 11px;"
        "  spacing: 6px;"
        "}"
        "QCheckBox::indicator {"
        "  width: 16px; height: 16px;"
        "  border: 1px solid #2a3a4c;"
        "  border-radius: 3px;"
        "  background: #1a2233;"
        "}"
        "QCheckBox::indicator:checked {"
        "  background: #00d4aa;"
        "  border-color: #00d4aa;"
        "}"
        "QCheckBox::indicator:hover {"
        "  border-color: #00d4aa;"
        "}";

    /* ── Members ─────────────────────────────────────────────── */

    mc1dsp::DspEffect* fx_ = nullptr;
    QTimer* meterTimer_ = nullptr;
    QComboBox* presetCombo_ = nullptr;
    QVector<mc1dsp::Preset> presets_;

    /* Gain */
    RackKnob* knobInputGain_ = nullptr;

    /* HPF */
    QCheckBox* hpfEnableCheck_ = nullptr;
    RackKnob* knobHpfFreq_ = nullptr;

    /* Compressor */
    RackKnob* knobCompAmount_ = nullptr;

    /* 3-Band EQ */
    RackKnob* knobEqHigh_    = nullptr;
    RackKnob* knobEqMid_     = nullptr;
    RackKnob* knobEqMidFreq_ = nullptr;
    RackKnob* knobEqLow_     = nullptr;

    /* Output */
    RackKnob* knobOutputLevel_ = nullptr;
    RackMeter* outputMeter_ = nullptr;
};
