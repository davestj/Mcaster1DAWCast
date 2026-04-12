/*
 * Mcaster1AudioPipe — Virtual Audio Routing Application
 * fx_ui/ParametricEqDialog.h — 10-Band Parametric EQ hardware rack editor
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * QDialog for editing a live FxParametricEq effect instance.
 * Styled to look like a broadcast-grade hardware rack unit:
 *   - EqCurveWidget Bode plot (full width, 280px)
 *   - 10 vertical slider columns (80/150/400/800/1.5k/3k/5k/8k/12k/16k Hz)
 *   - Selected band detail row (frequency / Q factor)
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
#include <QDoubleSpinBox>
#include <QTimer>
#include <QFrame>

#include <algorithm>
#include <cmath>
#include <cstdio>

class ParametricEqDialog : public QDialog {
    Q_OBJECT

public:
    static constexpr int NUM_BANDS = 10;

    explicit ParametricEqDialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : QDialog(parent)
        , fx_(fx)
    {
        setWindowTitle("10-Band Parametric EQ");
        setFixedSize(750, 500);
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
        root->addLayout(presetRow);

        /* ── EQ curve plot ────────────────────────────────────────── */

        auto* curveGroup = makeGroupBox("Frequency Response");
        auto* curveLayout = new QVBoxLayout(curveGroup);
        curveLayout->setContentsMargins(6, 16, 6, 6);

        curve_ = new EqCurveWidget;
        curve_->setDbRange(24.0f);
        curve_->setFixedHeight(280);
        curveLayout->addWidget(curve_);
        root->addWidget(curveGroup);

        /* ── Band sliders ─────────────────────────────────────────── */

        auto* bandGroup = makeGroupBox("Band Gains");
        auto* bandGrid = new QGridLayout(bandGroup);
        bandGrid->setContentsMargins(8, 18, 8, 6);
        bandGrid->setHorizontalSpacing(4);
        bandGrid->setVerticalSpacing(2);

        for (int i = 0; i < NUM_BANDS; ++i) {
            /* Frequency label */
            freqLabels_[i] = new QLabel(kFreqLabels[i]);
            freqLabels_[i]->setAlignment(Qt::AlignCenter);
            freqLabels_[i]->setStyleSheet(
                "font-size: 10px; color: #8090a8; font-family: Menlo;");
            bandGrid->addWidget(freqLabels_[i], 0, i, Qt::AlignCenter);

            /* Vertical slider: -24 to +24 dB, tick every 6 dB */
            sliders_[i] = new QSlider(Qt::Vertical);
            sliders_[i]->setRange(-240, 240);   /* x10 for 0.1 dB steps */
            sliders_[i]->setSingleStep(5);
            sliders_[i]->setPageStep(60);
            sliders_[i]->setTickPosition(QSlider::TicksBothSides);
            sliders_[i]->setTickInterval(60);
            sliders_[i]->setMinimumHeight(100);
            sliders_[i]->setStyleSheet(kSliderQss);
            bandGrid->addWidget(sliders_[i], 1, i, Qt::AlignCenter);

            /* Gain readout */
            gainLabels_[i] = new QLabel("0.0");
            gainLabels_[i]->setAlignment(Qt::AlignCenter);
            gainLabels_[i]->setStyleSheet(
                "font-size: 10px; color: #00d4aa; font-family: Menlo;");
            bandGrid->addWidget(gainLabels_[i], 2, i, Qt::AlignCenter);

            /* Read initial parameter state */
            float norm = fx_->paramValue(i);
            float db   = norm * 48.0f - 24.0f;
            sliders_[i]->setValue(static_cast<int>(db * 10.0f));
            updateGainLabel(i, db);

            /* Connect slider changes */
            const int bandIdx = i;
            connect(sliders_[i], &QSlider::valueChanged, this,
                    [this, bandIdx](int val) { onSliderChanged(bandIdx, val); });

            /* Click to select band */
            sliders_[i]->installEventFilter(this);
        }

        root->addWidget(bandGroup);

        /* ── Selected band detail ─────────────────────────────────── */

        auto* detailRow = new QHBoxLayout;
        detailRow->setSpacing(10);

        selectedLabel_ = new QLabel("Band: --");
        selectedLabel_->setStyleSheet("color: #d0e0f0; font-size: 12px;");
        detailRow->addWidget(selectedLabel_);

        detailRow->addSpacing(10);

        auto* freqLabel = new QLabel("Freq:");
        freqLabel->setStyleSheet("color: #8090a8; font-size: 11px;");
        detailRow->addWidget(freqLabel);

        freqSpin_ = new QDoubleSpinBox;
        freqSpin_->setRange(20.0, 20000.0);
        freqSpin_->setSuffix(" Hz");
        freqSpin_->setDecimals(0);
        freqSpin_->setEnabled(false);
        freqSpin_->setStyleSheet(kSpinQss);
        detailRow->addWidget(freqSpin_);

        auto* qLabel = new QLabel("Q:");
        qLabel->setStyleSheet("color: #8090a8; font-size: 11px;");
        detailRow->addWidget(qLabel);

        qSpin_ = new QDoubleSpinBox;
        qSpin_->setRange(0.1, 20.0);
        qSpin_->setSingleStep(0.1);
        qSpin_->setValue(1.0);
        qSpin_->setDecimals(1);
        qSpin_->setEnabled(false);
        qSpin_->setStyleSheet(kSpinQss);
        detailRow->addWidget(qSpin_);

        detailRow->addStretch();
        root->addLayout(detailRow);

        /* ── Button row ───────────────────────────────────────────── */

        auto* btnRow = new QHBoxLayout;
        btnRow->setSpacing(8);
        btnRow->addStretch();

        auto* applyBtn = new QPushButton("Apply");
        applyBtn->setStyleSheet(kBtnQss);
        connect(applyBtn, &QPushButton::clicked, this, &ParametricEqDialog::onApply);
        btnRow->addWidget(applyBtn);

        auto* resetBtn = new QPushButton("Reset");
        resetBtn->setStyleSheet(kBtnQss);
        connect(resetBtn, &QPushButton::clicked, this, &ParametricEqDialog::onReset);
        btnRow->addWidget(resetBtn);

        auto* closeBtn = new QPushButton("Close");
        closeBtn->setStyleSheet(kBtnQss);
        connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
        btnRow->addWidget(closeBtn);

        root->addLayout(btnRow);

        /* ── Initial curve sync ───────────────────────────────────── */

        refreshCurve();
        selectBand(0);

        /* ── 20 FPS meter timer ───────────────────────────────────── */

        meterTimer_ = new QTimer(this);
        connect(meterTimer_, &QTimer::timeout, this, &ParametricEqDialog::onMeterTick);
        meterTimer_->start(50);
    }

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override
    {
        if (ev->type() == QEvent::MouseButtonPress) {
            for (int i = 0; i < NUM_BANDS; ++i) {
                if (obj == sliders_[i]) {
                    selectBand(i);
                    break;
                }
            }
        }
        return QDialog::eventFilter(obj, ev);
    }

private slots:
    void onSliderChanged(int band, int val)
    {
        float db   = static_cast<float>(val) / 10.0f;
        float norm = (db + 24.0f) / 48.0f;
        fx_->setParamValue(band, norm);
        updateGainLabel(band, db);
        refreshCurve();
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
        for (int i = 0; i < NUM_BANDS; ++i) {
            fx_->setParamValue(i, 0.5f);  /* 0 dB */
            sliders_[i]->setValue(0);
            updateGainLabel(i, 0.0f);
        }
        refreshCurve();
    }

    void onMeterTick()
    {
        /* Check clipping status from DSP engine */
        if (fx_->isClipping()) {
            /* Could flash a clipping indicator here in the future */
            fx_->resetClip();
        }
    }

private:
    /* ── Band constants ───────────────────────────────────────────── */

    static constexpr float kFreqs[NUM_BANDS] = {
        80, 150, 400, 800, 1500, 3000, 5000, 8000, 12000, 16000
    };

    static constexpr const char* kFreqLabels[NUM_BANDS] = {
        "80", "150", "400", "800", "1.5k", "3k", "5k", "8k", "12k", "16k"
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

    void selectBand(int band)
    {
        if (band < 0 || band >= NUM_BANDS) return;
        selectedBand_ = band;
        curve_->setSelectedBand(band);

        char buf[32];
        snprintf(buf, sizeof(buf), "Band %d: %s Hz",
                 band + 1, kFreqLabels[band]);
        selectedLabel_->setText(QString::fromLatin1(buf));

        freqSpin_->setValue(static_cast<double>(kFreqs[band]));
        freqSpin_->setEnabled(true);
        qSpin_->setEnabled(true);

        /* Highlight selected slider */
        for (int i = 0; i < NUM_BANDS; ++i) {
            freqLabels_[i]->setStyleSheet(
                (i == band)
                    ? "font-size: 10px; color: #00d4aa; font-weight: bold; font-family: Menlo;"
                    : "font-size: 10px; color: #8090a8; font-family: Menlo;");
        }
    }

    void refreshCurve()
    {
        float gains[NUM_BANDS];
        float freqs[NUM_BANDS];
        for (int i = 0; i < NUM_BANDS; ++i) {
            float norm = fx_->paramValue(i);
            gains[i] = norm * 48.0f - 24.0f;
            freqs[i] = kFreqs[i];
        }
        curve_->setBands(gains, freqs, fx_->sampleRate());
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
    QSlider*           sliders_[NUM_BANDS] = {};
    QLabel*            freqLabels_[NUM_BANDS] = {};
    QLabel*            gainLabels_[NUM_BANDS] = {};
    QLabel*            selectedLabel_ = nullptr;
    QDoubleSpinBox*    freqSpin_ = nullptr;
    QDoubleSpinBox*    qSpin_ = nullptr;
    QTimer*            meterTimer_ = nullptr;
    int                selectedBand_ = -1;

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
        "  width: 4px;"
        "  border-radius: 2px;"
        "}"
        "QSlider::handle:vertical {"
        "  background: qlineargradient(x1:0,y1:0,x2:1,y2:1,"
        "    stop:0 #c0c0c0, stop:0.5 #909090, stop:1 #707070);"
        "  border: 1px solid #505050;"
        "  width: 18px;"
        "  height: 10px;"
        "  margin: 0 -7px;"
        "  border-radius: 3px;"
        "}"
        "QSlider::handle:vertical:hover {"
        "  background: qlineargradient(x1:0,y1:0,x2:1,y2:1,"
        "    stop:0 #d4d4d4, stop:0.5 #a0a0a0, stop:1 #808080);"
        "}"
        "QSlider::sub-page:vertical {"
        "  background: #1a2240;"
        "}"
        "QSlider::add-page:vertical {"
        "  background: #1a2240;"
        "}";

    static constexpr const char* kSpinQss =
        "QDoubleSpinBox {"
        "  background: #1c2238;"
        "  color: #d0e0f0;"
        "  border: 1px solid #2a3a50;"
        "  border-radius: 3px;"
        "  padding: 2px 6px;"
        "  font-size: 11px;"
        "  font-family: Menlo;"
        "}"
        "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {"
        "  width: 14px;"
        "  background: #1a2240;"
        "  border: 1px solid #2a3a50;"
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
};
