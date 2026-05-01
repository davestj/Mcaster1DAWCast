/*
 * Mcaster1AudioPipe — Virtual Audio Routing Application
 * fx_ui/DbxVoiceDialog.h — DBX 286S Voice Processor editor dialog
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Multi-section channel strip editor matching FxDbxVoice (10 params).
 * Four horizontal sections: Gate, Compressor, De-Esser, Enhancer.
 * GR meter for compressor gain reduction. Preset load/save support.
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
#include <QTimer>
#include <QInputDialog>

class DbxVoiceDialog : public QDialog {
    Q_OBJECT

public:
    explicit DbxVoiceDialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : QDialog(parent), fx_(fx)
    {
        setWindowTitle("DBX 286S Voice Processor");
        setMinimumSize(825, 360);
        resize(825, 360);
        setAttribute(Qt::WA_DeleteOnClose, false);
        setStyleSheet(kDialogStyle);

        auto* root = new QVBoxLayout(this);
        root->setSpacing(6);
        root->setContentsMargins(12, 10, 12, 10);

        /* ── Header ─────────────────────────────────────────────── */

        auto* headerLayout = new QHBoxLayout;

        auto* titleBlock = new QVBoxLayout;
        auto* titleLabel = new QLabel("DBX 286S");
        titleLabel->setStyleSheet(
            "font-size: 22px; font-weight: bold; color: #e0e8f0;"
            "font-family: 'Helvetica Neue', 'Arial Black', sans-serif;"
            "letter-spacing: 2px;");
        titleBlock->addWidget(titleLabel);

        auto* subtitleLabel = new QLabel("Voice Processor");
        subtitleLabel->setStyleSheet(
            "font-size: 12px; color: #708090; font-style: italic;"
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
        presetCombo_->setFixedWidth(120);
        presetCombo_->setStyleSheet(kComboStyle);
        headerLayout->addWidget(presetCombo_);

        auto* loadBtn = new QPushButton("Load");
        loadBtn->setFixedWidth(38);
        loadBtn->setStyleSheet(kButtonStyle);
        connect(loadBtn, &QPushButton::clicked, this, &DbxVoiceDialog::loadPreset);
        headerLayout->addWidget(loadBtn);

        auto* saveBtn = new QPushButton("Save");
        saveBtn->setFixedWidth(38);
        saveBtn->setStyleSheet(kButtonStyle);
        connect(saveBtn, &QPushButton::clicked, this, &DbxVoiceDialog::savePreset);
        headerLayout->addWidget(saveBtn);

        root->addLayout(headerLayout);

        /* ── Sections row ───────────────────────────────────────── */

        auto* sectionsLayout = new QHBoxLayout;
        sectionsLayout->setSpacing(6);

        /* Section 1: Gate/Expander */
        auto* gateGroup = createSection("GATE / EXPANDER");
        auto* gateLayout = new QHBoxLayout;
        gateLayout->setSpacing(10);
        gateLayout->setContentsMargins(8, 4, 8, 8);

        knobGateThresh_ = createKnob("Threshold", QColor("#00d4aa"));
        knobGateRatio_  = createKnob("Ratio", QColor("#00d4aa"));
        gateLayout->addWidget(knobGateThresh_);
        gateLayout->addWidget(knobGateRatio_);

        static_cast<QVBoxLayout*>(gateGroup->layout())->addLayout(gateLayout);
        sectionsLayout->addWidget(gateGroup);

        /* Section 2: Compressor + GR meter */
        auto* compGroup = createSection("COMPRESSOR");
        auto* compOuterLayout = new QHBoxLayout;
        compOuterLayout->setSpacing(6);
        compOuterLayout->setContentsMargins(8, 4, 8, 8);

        auto* compKnobLayout = new QHBoxLayout;
        compKnobLayout->setSpacing(6);

        knobCompThresh_  = createKnob("Threshold", QColor("#00d4aa"));
        knobCompRatio_   = createKnob("Ratio", QColor("#00d4aa"));
        knobCompAttack_  = createKnob("Attack", QColor("#00d4aa"));
        knobCompRelease_ = createKnob("Release", QColor("#00d4aa"));

        compKnobLayout->addWidget(knobCompThresh_);
        compKnobLayout->addWidget(knobCompRatio_);
        compKnobLayout->addWidget(knobCompAttack_);
        compKnobLayout->addWidget(knobCompRelease_);

        compOuterLayout->addLayout(compKnobLayout);

        /* GR Meter */
        auto* meterLayout = new QVBoxLayout;
        meterLayout->setSpacing(2);
        auto* grLabel = new QLabel("GR");
        grLabel->setAlignment(Qt::AlignCenter);
        grLabel->setStyleSheet("font-size: 9px; color: #FF9800; font-weight: bold;");
        meterLayout->addWidget(grLabel);

        grMeter_ = new RackMeter(RackMeter::GR_METER);
        grMeter_->setFixedWidth(21);
        meterLayout->addWidget(grMeter_, 1);
        compOuterLayout->addLayout(meterLayout);

        static_cast<QVBoxLayout*>(compGroup->layout())->addLayout(compOuterLayout);
        sectionsLayout->addWidget(compGroup);

        /* Section 3: De-Esser */
        auto* deessGroup = createSection("DE-ESSER");
        auto* deessLayout = new QHBoxLayout;
        deessLayout->setSpacing(10);
        deessLayout->setContentsMargins(8, 4, 8, 8);

        knobDeessFreq_   = createKnob("Frequency", QColor("#00d4aa"));
        knobDeessThresh_ = createKnob("Threshold", QColor("#00d4aa"));
        deessLayout->addWidget(knobDeessFreq_);
        deessLayout->addWidget(knobDeessThresh_);

        static_cast<QVBoxLayout*>(deessGroup->layout())->addLayout(deessLayout);
        sectionsLayout->addWidget(deessGroup);

        /* Section 4: Enhancer */
        auto* enhGroup = createSection("ENHANCER");
        auto* enhLayout = new QHBoxLayout;
        enhLayout->setSpacing(10);
        enhLayout->setContentsMargins(8, 4, 8, 8);

        knobLfGain_ = createKnob("LF Enhance", QColor("#FFA726"));  /* warm orange */
        knobHfGain_ = createKnob("HF Detail", QColor("#26C6DA"));   /* bright cyan */
        enhLayout->addWidget(knobLfGain_);
        enhLayout->addWidget(knobHfGain_);

        static_cast<QVBoxLayout*>(enhGroup->layout())->addLayout(enhLayout);
        sectionsLayout->addWidget(enhGroup);

        sectionsLayout->addStretch(1);
        root->addLayout(sectionsLayout, 0);
        root->addStretch(1);;

        /* ── Connect knobs to effect params ─────────────────────── */

        connectKnob(knobGateThresh_,  0);
        connectKnob(knobGateRatio_,   1);
        connectKnob(knobCompThresh_,  2);
        connectKnob(knobCompRatio_,   3);
        connectKnob(knobCompAttack_,  4);
        connectKnob(knobCompRelease_, 5);
        connectKnob(knobDeessFreq_,   6);
        connectKnob(knobDeessThresh_, 7);
        connectKnob(knobLfGain_,      8);
        connectKnob(knobHfGain_,      9);

        /* ── Read initial values from effect ────────────────────── */

        readAllParams();
        refreshPresetList();

        /* ── Meter update timer (20 FPS) ────────────────────────── */

        meterTimer_ = new QTimer(this);
        connect(meterTimer_, &QTimer::timeout, this, &DbxVoiceDialog::updateMeters);
        meterTimer_->start(50);
    }

private slots:
    void updateMeters()
    {
        if (!fx_) return;

        /* GR meter reads compressor gain reduction */
        float gr = fx_->meterGainReduction();
        grMeter_->setLevel(gr > 0.01f ? -gr : -60.0f);

        /* Refresh display strings */
        updateKnobText(knobGateThresh_,  0);
        updateKnobText(knobGateRatio_,   1);
        updateKnobText(knobCompThresh_,  2);
        updateKnobText(knobCompRatio_,   3);
        updateKnobText(knobCompAttack_,  4);
        updateKnobText(knobCompRelease_, 5);
        updateKnobText(knobDeessFreq_,   6);
        updateKnobText(knobDeessThresh_, 7);
        updateKnobText(knobLfGain_,      8);
        updateKnobText(knobHfGain_,      9);
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

    RackKnob* createKnob(const QString& title, const QColor& accent)
    {
        auto* knob = new RackKnob;
        knob->setStyle(RackKnob::Bellcap);
        knob->setTitle(title);
        knob->setAccentColor(accent);
        knob->setNotches(11);
        knob->setFixedSize(60, 82);
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

        readKnob(knobGateThresh_,  0);
        readKnob(knobGateRatio_,   1);
        readKnob(knobCompThresh_,  2);
        readKnob(knobCompRatio_,   3);
        readKnob(knobCompAttack_,  4);
        readKnob(knobCompRelease_, 5);
        readKnob(knobDeessFreq_,   6);
        readKnob(knobDeessThresh_, 7);
        readKnob(knobLfGain_,      8);
        readKnob(knobHfGain_,      9);
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

    /* ── Members ─────────────────────────────────────────────── */

    mc1dsp::DspEffect* fx_ = nullptr;
    QTimer* meterTimer_ = nullptr;
    QComboBox* presetCombo_ = nullptr;
    QVector<mc1dsp::Preset> presets_;

    /* Gate knobs */
    RackKnob* knobGateThresh_  = nullptr;
    RackKnob* knobGateRatio_   = nullptr;

    /* Compressor knobs */
    RackKnob* knobCompThresh_  = nullptr;
    RackKnob* knobCompRatio_   = nullptr;
    RackKnob* knobCompAttack_  = nullptr;
    RackKnob* knobCompRelease_ = nullptr;

    /* De-Esser knobs */
    RackKnob* knobDeessFreq_   = nullptr;
    RackKnob* knobDeessThresh_ = nullptr;

    /* Enhancer knobs */
    RackKnob* knobLfGain_ = nullptr;
    RackKnob* knobHfGain_ = nullptr;

    /* Meters */
    RackMeter* grMeter_ = nullptr;
};
