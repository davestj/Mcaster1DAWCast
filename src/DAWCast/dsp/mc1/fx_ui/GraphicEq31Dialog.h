/*
 * Mcaster1AudioPipe — Virtual Audio Routing Application
 * fx_ui/GraphicEq31Dialog.h — 31-Band ISO 1/3-Octave Graphic EQ rack editor
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * QDialog for editing a live FxGraphicEq31 effect instance.
 * Styled to look like a broadcast-grade hardware rack unit:
 *   - EqCurveWidget showing stereo-linked curve (single teal line)
 *   - 31 narrow vertical sliders in a dense row
 *   - Alternating frequency labels to avoid crowding
 *   - Flat button to reset all bands to 0 dB
 *   - Clipping indicator with band identification
 *   - Preset combo + Apply/Reset/Close
 *
 * Header-only. All parameter changes propagate live to the DSP engine.
 */

#pragma once

#include "fx_ui/RackKnob.h"
#include "fx_ui/RackMeter.h"
#include "fx_ui/EqCurveWidget.h"

#include "patchbay/dsp/dsp_effect.h"
#include "patchbay/dsp/preset_manager.h"

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QSlider>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QTimer>
#include <QFrame>

#include <algorithm>
#include <cmath>
#include <cstdio>

class GraphicEq31Dialog : public QDialog {
    Q_OBJECT

public:
    static constexpr int NUM_BANDS = 31;

    explicit GraphicEq31Dialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : QDialog(parent)
        , fx_(fx)
    {
        setWindowTitle("31-Band Graphic EQ (ISO 1/3-Octave)");
        setFixedSize(1000, 500);
        setStyleSheet(kDialogQss);

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(10, 8, 10, 8);
        root->setSpacing(6);

        /* ── Preset row ───────────────────────────────────────────── */

        auto* presetRow = new QHBoxLayout;
        presetRow->setSpacing(8);
        auto* presetLabel = new QLabel("Preset:");
        presetLabel->setStyleSheet("color: #8090a8; font-size: 12px;");
        presetCombo_ = new QComboBox;
        presetCombo_->setMinimumWidth(200);
        populatePresets();
        presetRow->addWidget(presetLabel);
        presetRow->addWidget(presetCombo_);
        presetRow->addStretch();

        /* Clipping indicator */
        clipLabel_ = new QLabel;
        clipLabel_->setStyleSheet(
            "color: #ff3d00; font-size: 11px; font-weight: bold; font-family: Menlo;");
        clipLabel_->setVisible(false);
        presetRow->addWidget(clipLabel_);

        root->addLayout(presetRow);

        /* ── EQ curve plot ────────────────────────────────────────── */

        auto* curveGroup = makeGroupBox("Frequency Response");
        auto* curveLayout = new QVBoxLayout(curveGroup);
        curveLayout->setContentsMargins(6, 16, 6, 6);

        curve_ = new EqCurveWidget;
        curve_->setDbRange(12.0f);
        curve_->setFixedHeight(180);
        curveLayout->addWidget(curve_);
        root->addWidget(curveGroup);

        /* ── Band sliders (31 narrow columns) ─────────────────────── */

        auto* bandGroup = makeGroupBox("Band Gains");
        auto* bandGrid = new QGridLayout(bandGroup);
        bandGrid->setContentsMargins(4, 18, 4, 4);
        bandGrid->setHorizontalSpacing(1);
        bandGrid->setVerticalSpacing(1);

        for (int i = 0; i < NUM_BANDS; ++i) {
            /* Frequency label — alternate visibility to prevent crowding */
            freqLabels_[i] = new QLabel(kFreqLabels[i]);
            freqLabels_[i]->setAlignment(Qt::AlignCenter);
            freqLabels_[i]->setStyleSheet(
                "font-size: 8px; color: #6080a0; font-family: Menlo;");
            freqLabels_[i]->setVisible((i % 2) == 0);  /* show every other */
            bandGrid->addWidget(freqLabels_[i], 0, i, Qt::AlignCenter);

            /* Narrow vertical slider: -12 to +12 dB */
            sliders_[i] = new QSlider(Qt::Vertical);
            sliders_[i]->setRange(-120, 120);  /* x10 for 0.1 dB steps */
            sliders_[i]->setSingleStep(5);
            sliders_[i]->setPageStep(30);
            sliders_[i]->setTickPosition(QSlider::NoTicks);
            sliders_[i]->setMinimumHeight(80);
            sliders_[i]->setMaximumWidth(24);
            sliders_[i]->setStyleSheet(kSliderQss);
            bandGrid->addWidget(sliders_[i], 1, i, Qt::AlignCenter);

            /* Gain readout — alternate visibility */
            gainLabels_[i] = new QLabel("0.0");
            gainLabels_[i]->setAlignment(Qt::AlignCenter);
            gainLabels_[i]->setStyleSheet(
                "font-size: 7px; color: #00d4aa; font-family: Menlo;");
            gainLabels_[i]->setVisible((i % 2) == 0);
            bandGrid->addWidget(gainLabels_[i], 2, i, Qt::AlignCenter);

            /* Read initial parameter state */
            float norm = fx_->paramValue(i);
            float db   = norm * 24.0f - 12.0f;
            sliders_[i]->setValue(static_cast<int>(db * 10.0f));
            updateGainLabel(i, db);

            /* Connect slider changes */
            const int bandIdx = i;
            connect(sliders_[i], &QSlider::valueChanged, this,
                    [this, bandIdx](int val) { onSliderChanged(bandIdx, val); });
        }

        root->addWidget(bandGroup);

        /* ── Button row ───────────────────────────────────────────── */

        auto* btnRow = new QHBoxLayout;
        btnRow->setSpacing(8);

        auto* flatBtn = new QPushButton("Flat");
        flatBtn->setToolTip("Reset all bands to 0 dB");
        flatBtn->setStyleSheet(kFlatBtnQss);
        connect(flatBtn, &QPushButton::clicked, this, &GraphicEq31Dialog::onFlat);
        btnRow->addWidget(flatBtn);

        btnRow->addStretch();

        auto* applyBtn = new QPushButton("Apply");
        applyBtn->setStyleSheet(kBtnQss);
        connect(applyBtn, &QPushButton::clicked, this, &GraphicEq31Dialog::onApply);
        btnRow->addWidget(applyBtn);

        auto* resetBtn = new QPushButton("Reset");
        resetBtn->setStyleSheet(kBtnQss);
        connect(resetBtn, &QPushButton::clicked, this, &GraphicEq31Dialog::onReset);
        btnRow->addWidget(resetBtn);

        auto* closeBtn = new QPushButton("Close");
        closeBtn->setStyleSheet(kBtnQss);
        connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
        btnRow->addWidget(closeBtn);

        root->addLayout(btnRow);

        /* ── Initial curve sync ───────────────────────────────────── */

        refreshCurve();

        /* ── 20 FPS meter timer ───────────────────────────────────── */

        meterTimer_ = new QTimer(this);
        connect(meterTimer_, &QTimer::timeout, this, &GraphicEq31Dialog::onMeterTick);
        meterTimer_->start(50);
    }

private slots:
    void onSliderChanged(int band, int val)
    {
        float db   = static_cast<float>(val) / 10.0f;
        float norm = (db + 12.0f) / 24.0f;
        fx_->setParamValue(band, norm);
        updateGainLabel(band, db);
        refreshCurve();
    }

    void onFlat()
    {
        for (int i = 0; i < NUM_BANDS; ++i) {
            fx_->setParamValue(i, 0.5f);  /* 0 dB */
            sliders_[i]->setValue(0);
            updateGainLabel(i, 0.0f);
        }
        refreshCurve();
        clipLabel_->setVisible(false);
    }

    void onApply()
    {
        if (presetCombo_->currentIndex() < 0) return;
        mc1dsp::Preset preset = mc1dsp::PresetManager::capturePreset(
            fx_, presetCombo_->currentText());
        mc1dsp::PresetManager::savePreset(preset);
        populatePresets();
    }

    void onReset()
    {
        onFlat();  /* Reset is equivalent to Flat for a graphic EQ */
    }

    void onMeterTick()
    {
        if (fx_->isClipping()) {
            int band = fx_->clipBand();
            if (band >= 0 && band < NUM_BANDS) {
                char buf[48];
                snprintf(buf, sizeof(buf), "CLIP: Band %d (%s)",
                         band + 1, kFreqLabels[band]);
                clipLabel_->setText(QString::fromLatin1(buf));
                clipLabel_->setVisible(true);
            }
            clipFadeCount_ = 0;
            fx_->resetClip();
        } else {
            if (clipLabel_->isVisible()) {
                clipFadeCount_++;
                if (clipFadeCount_ > 40) {  /* 2 sec at 20fps */
                    clipLabel_->setVisible(false);
                    clipFadeCount_ = 0;
                }
            }
        }
    }

private:
    /* ── Band constants ───────────────────────────────────────────── */

    static constexpr float kFreqs[NUM_BANDS] = {
           20.0f,    25.0f,    31.5f,    40.0f,    50.0f,
           63.0f,    80.0f,   100.0f,   125.0f,   160.0f,
          200.0f,   250.0f,   315.0f,   400.0f,   500.0f,
          630.0f,   800.0f,  1000.0f,  1250.0f,  1600.0f,
         2000.0f,  2500.0f,  3150.0f,  4000.0f,  5000.0f,
         6300.0f,  8000.0f, 10000.0f, 12500.0f, 16000.0f,
        20000.0f
    };

    static constexpr const char* kFreqLabels[NUM_BANDS] = {
        "20",   "25",   "31",   "40",   "50",
        "63",   "80",   "100",  "125",  "160",
        "200",  "250",  "315",  "400",  "500",
        "630",  "800",  "1k",   "1.25k","1.6k",
        "2k",   "2.5k", "3.15k","4k",   "5k",
        "6.3k", "8k",   "10k",  "12.5k","16k",
        "20k"
    };

    /* ── Helpers ──────────────────────────────────────────────────── */

    void populatePresets()
    {
        presetCombo_->clear();
        auto presets = mc1dsp::PresetManager::listPresets(
            QString::fromUtf8(fx_->id()));
        for (const auto& p : presets)
            presetCombo_->addItem(p.name, p.filePath);
    }

    void updateGainLabel(int band, float db)
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%+.1f", static_cast<double>(db));
        gainLabels_[band]->setText(QString::fromLatin1(buf));
    }

    void refreshCurve()
    {
        float gains[NUM_BANDS];
        for (int i = 0; i < NUM_BANDS; ++i) {
            float norm = fx_->paramValue(i);
            gains[i] = norm * 24.0f - 12.0f;
        }
        curve_->setBands31(gains, fx_->sampleRate());
    }

    static QGroupBox* makeGroupBox(const QString& title)
    {
        auto* gb = new QGroupBox(title);
        gb->setStyleSheet(kGroupBoxQss);
        return gb;
    }

    /* ── Widgets ──────────────────────────────────────────────────── */

    mc1dsp::DspEffect* fx_ = nullptr;
    EqCurveWidget*     curve_ = nullptr;
    QComboBox*         presetCombo_ = nullptr;
    QLabel*            clipLabel_ = nullptr;
    QTimer*            meterTimer_ = nullptr;
    int                clipFadeCount_ = 0;
    QSlider*           sliders_[NUM_BANDS] = {};
    QLabel*            freqLabels_[NUM_BANDS] = {};
    QLabel*            gainLabels_[NUM_BANDS] = {};

    /* ── Stylesheet constants ─────────────────────────────────────── */

    static constexpr const char* kDialogQss =
        "QDialog {"
        "  background: #141828;"
        "  color: #d0e0f0;"
        "}"
        "QLabel {"
        "  color: #d0e0f0;"
        "  font-size: 12px;"
        "}"
        "QComboBox {"
        "  background: #1c2238;"
        "  color: #d0e0f0;"
        "  border: 1px solid #2a3a50;"
        "  border-radius: 3px;"
        "  padding: 3px 8px;"
        "  font-size: 12px;"
        "}"
        "QComboBox::drop-down {"
        "  border: none;"
        "  width: 20px;"
        "}"
        "QComboBox QAbstractItemView {"
        "  background: #1c2238;"
        "  color: #d0e0f0;"
        "  selection-background-color: #00d4aa;"
        "  selection-color: #141828;"
        "}";

    static constexpr const char* kGroupBoxQss =
        "QGroupBox {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 #1a2240, stop:1 #141828);"
        "  border: 1px solid #2a3a50;"
        "  border-left: 3px solid #00d4aa;"
        "  border-radius: 4px;"
        "  margin-top: 10px;"
        "  padding-top: 8px;"
        "  font-size: 11px;"
        "  color: #6080a0;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  left: 12px;"
        "  padding: 0 4px;"
        "  color: #8090a8;"
        "}";

    static constexpr const char* kSliderQss =
        "QSlider::groove:vertical {"
        "  background: #1a2240;"
        "  width: 2px;"
        "  border-radius: 1px;"
        "}"
        "QSlider::handle:vertical {"
        "  background: qlineargradient(x1:0,y1:0,x2:1,y2:1,"
        "    stop:0 #c0c0c0, stop:0.5 #909090, stop:1 #707070);"
        "  border: 1px solid #505050;"
        "  width: 14px;"
        "  height: 7px;"
        "  margin: 0 -6px;"
        "  border-radius: 2px;"
        "}"
        "QSlider::handle:vertical:hover {"
        "  background: qlineargradient(x1:0,y1:0,x2:1,y2:1,"
        "    stop:0 #d4d4d4, stop:0.5 #a0a0a0, stop:1 #808080);"
        "  border-color: #00d4aa;"
        "}"
        "QSlider::sub-page:vertical {"
        "  background: #1a2240;"
        "}"
        "QSlider::add-page:vertical {"
        "  background: #1a2240;"
        "}";

    static constexpr const char* kBtnQss =
        "QPushButton {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 #2a3a55, stop:1 #1a2540);"
        "  color: #d0e0f0;"
        "  border: 1px solid #3a4a60;"
        "  border-radius: 4px;"
        "  padding: 5px 16px;"
        "  font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 #3a4a65, stop:1 #2a3550);"
        "  border-color: #00d4aa;"
        "}"
        "QPushButton:pressed {"
        "  background: #141828;"
        "}";

    /* Flat button: distinct teal accent to stand out */
    static constexpr const char* kFlatBtnQss =
        "QPushButton {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 #1a3a40, stop:1 #142830);"
        "  color: #00d4aa;"
        "  border: 1px solid #00d4aa;"
        "  border-radius: 4px;"
        "  padding: 5px 16px;"
        "  font-size: 12px;"
        "  font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 #2a4a50, stop:1 #1a3840);"
        "  color: #ffffff;"
        "}"
        "QPushButton:pressed {"
        "  background: #00d4aa;"
        "  color: #141828;"
        "}";
};
