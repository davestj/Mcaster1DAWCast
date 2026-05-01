/*
 * Mcaster1AudioPipe — Virtual Audio Routing Application
 * fx_ui/SonicEnhancerDialog.h — BBE 882I Sonic Maximizer editor dialog
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Clean, minimal front-panel editor matching FxSonicEnhancer (3 params).
 * Three large knobs: Lo Contour, Process, Output Gain.
 * Preset load/save support.
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

class SonicEnhancerDialog : public QDialog {
    Q_OBJECT

public:
    explicit SonicEnhancerDialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : QDialog(parent), fx_(fx)
    {
        setWindowTitle("BBE 882I Sonic Maximizer");
        setMinimumSize(300, 200); resize(375, 262);
        setAttribute(Qt::WA_DeleteOnClose, false);
        setStyleSheet(kDialogStyle);

        auto* root = new QVBoxLayout(this);
        root->setSpacing(8);
        root->setContentsMargins(16, 14, 16, 14);

        /* ── Header ─────────────────────────────────────────────── */

        auto* headerLayout = new QVBoxLayout;
        headerLayout->setSpacing(0);
        headerLayout->setAlignment(Qt::AlignCenter);

        auto* titleLabel = new QLabel("BBE 882I");
        titleLabel->setAlignment(Qt::AlignCenter);
        titleLabel->setStyleSheet(
            "font-size: 28px; font-weight: bold; color: #e0e8f0;"
            "font-family: 'Helvetica Neue', 'Arial Black', sans-serif;"
            "letter-spacing: 4px;");
        headerLayout->addWidget(titleLabel);

        auto* subtitleLabel = new QLabel("Sonic Maximizer");
        subtitleLabel->setAlignment(Qt::AlignCenter);
        subtitleLabel->setStyleSheet(
            "font-size: 13px; color: #00d4aa; font-weight: bold;"
            "letter-spacing: 2px;");
        headerLayout->addWidget(subtitleLabel);

        /* Preset row */
        auto* presetRow = new QHBoxLayout;
        presetRow->setSpacing(6);
        presetRow->addStretch();

        auto* presetLabel = new QLabel("Preset:");
        presetLabel->setStyleSheet("font-size: 11px; color: #90a0b0;");
        presetRow->addWidget(presetLabel);

        presetCombo_ = new QComboBox;
        presetCombo_->setFixedWidth(105);
        presetCombo_->setStyleSheet(kComboStyle);
        presetRow->addWidget(presetCombo_);

        auto* loadBtn = new QPushButton("Load");
        loadBtn->setFixedWidth(34);
        loadBtn->setStyleSheet(kButtonStyle);
        connect(loadBtn, &QPushButton::clicked, this, &SonicEnhancerDialog::loadPreset);
        presetRow->addWidget(loadBtn);

        auto* saveBtn = new QPushButton("Save");
        saveBtn->setFixedWidth(34);
        saveBtn->setStyleSheet(kButtonStyle);
        connect(saveBtn, &QPushButton::clicked, this, &SonicEnhancerDialog::savePreset);
        presetRow->addWidget(saveBtn);

        presetRow->addStretch();
        headerLayout->addSpacing(6);
        headerLayout->addLayout(presetRow);

        root->addLayout(headerLayout);

        /* ── Knobs panel ────────────────────────────────────────── */

        auto* panelGroup = new QGroupBox;
        panelGroup->setStyleSheet(kPanelStyle);
        auto* panelLayout = new QHBoxLayout(panelGroup);
        panelLayout->setSpacing(30);
        panelLayout->setContentsMargins(20, 16, 20, 12);
        panelLayout->setAlignment(Qt::AlignCenter);

        knobLoContour_ = createLargeKnob("Lo Contour", QColor("#FFA726"));
        knobProcess_   = createLargeKnob("Process",    QColor("#00d4aa"));
        knobOutput_    = createLargeKnob("Output Gain", QColor("#e0e8f0"));

        panelLayout->addWidget(knobLoContour_);
        panelLayout->addWidget(knobProcess_);
        panelLayout->addWidget(knobOutput_);

        root->addWidget(panelGroup, 0);
        root->addStretch(1);;

        /* ── Description text ───────────────────────────────────── */

        auto* descLayout = new QHBoxLayout;
        descLayout->setSpacing(16);
        descLayout->setContentsMargins(8, 0, 8, 0);

        auto* descLo = new QLabel("Bass impact\nand warmth");
        descLo->setAlignment(Qt::AlignCenter);
        descLo->setStyleSheet(kDescStyle);
        descLayout->addWidget(descLo, 1);

        auto* descProc = new QLabel("High-frequency\ndefinition and clarity");
        descProc->setAlignment(Qt::AlignCenter);
        descProc->setStyleSheet(kDescStyle);
        descLayout->addWidget(descProc, 1);

        auto* descOut = new QLabel("Overall\noutput level");
        descOut->setAlignment(Qt::AlignCenter);
        descOut->setStyleSheet(kDescStyle);
        descLayout->addWidget(descOut, 1);

        root->addLayout(descLayout);

        /* ── Connect knobs to effect params ─────────────────────── */

        connectKnob(knobLoContour_, 0);
        connectKnob(knobProcess_,   1);
        connectKnob(knobOutput_,    2);

        /* ── Read initial values from effect ────────────────────── */

        readAllParams();
        refreshPresetList();

        /* ── Meter update timer (20 FPS) for value text refresh ── */

        meterTimer_ = new QTimer(this);
        connect(meterTimer_, &QTimer::timeout, this, &SonicEnhancerDialog::updateMeters);
        meterTimer_->start(50);
    }

private slots:
    void updateMeters()
    {
        if (!fx_) return;

        updateKnobText(knobLoContour_, 0);
        updateKnobText(knobProcess_,   1);
        updateKnobText(knobOutput_,    2);
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

    RackKnob* createLargeKnob(const QString& title, const QColor& accent)
    {
        auto* knob = new RackKnob;
        knob->setTitle(title);
        knob->setAccentColor(accent);
        knob->setNotches(11);
        knob->setFixedSize(75, 98);
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

        readKnob(knobLoContour_, 0);
        readKnob(knobProcess_,   1);
        readKnob(knobOutput_,    2);
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

    static constexpr const char* kPanelStyle =
        "QGroupBox {"
        "  background: qlineargradient(y1:0, y2:1,"
        "    stop:0 #1e2a3a, stop:1 #161e2c);"
        "  border: 1px solid #2a3a4c;"
        "  border-radius: 6px;"
        "  margin: 0px;"
        "  padding: 0px;"
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

    static constexpr const char* kDescStyle =
        "font-size: 10px; color: #607080;"
        "line-height: 1.3;";

    /* ── Members ─────────────────────────────────────────────── */

    mc1dsp::DspEffect* fx_ = nullptr;
    QTimer* meterTimer_ = nullptr;
    QComboBox* presetCombo_ = nullptr;
    QVector<mc1dsp::Preset> presets_;

    RackKnob* knobLoContour_ = nullptr;
    RackKnob* knobProcess_   = nullptr;
    RackKnob* knobOutput_    = nullptr;
};
