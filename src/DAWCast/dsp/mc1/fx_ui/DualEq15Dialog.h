/*
 * Mcaster1AudioPipe — Virtual Audio Routing Application
 * fx_ui/DualEq15Dialog.h — Dual 15-Band Graphic EQ (L/R) hardware rack editor
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * QDialog for editing a live FxDualEq15 effect instance.
 * Styled to look like a broadcast-grade hardware rack unit:
 *   - EqCurveWidget in dual mode — L (teal) and R (orange) curves
 *   - Two side-by-side 15-band slider groups (Left / Right channels)
 *   - Clipping indicator with band identification
 *   - Preset combo + Apply/Reset/Close
 *
 * Parameters 0-14  = Left channel bands
 * Parameters 15-29 = Right channel bands
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
#include <QScrollArea>

#include <algorithm>
#include <cmath>
#include <cstdio>

class DualEq15Dialog : public QDialog {
    Q_OBJECT

public:
    static constexpr int NUM_BANDS = 15;

    explicit DualEq15Dialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : QDialog(parent)
        , fx_(fx)
    {
        setWindowTitle("Dual 15-Band Graphic EQ (L/R)");
        setMinimumSize(337, 206); resize(675, 412);
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
        presetCombo_->setMinimumWidth(150);
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

        /* ── EQ curve plot (dual mode) ────────────────────────────── */

        auto* curveGroup = makeGroupBox("Frequency Response (L/R)");
        auto* curveLayout = new QVBoxLayout(curveGroup);
        curveLayout->setContentsMargins(6, 16, 6, 6);

        curve_ = new EqCurveWidget;
        curve_->setDbRange(12.0f);
        curve_->setFixedHeight(150);
        curveLayout->addWidget(curve_);
        root->addWidget(curveGroup);

        /* ── Dual channel slider groups ───────────────────────────── */

        auto* channelRow = new QHBoxLayout;
        channelRow->setSpacing(8);

        /* Left channel */
        auto* leftGroup = makeGroupBox("Left Channel");
        leftGroup->setStyleSheet(kGroupBoxQssLeft);
        auto* leftGrid = new QGridLayout(leftGroup);
        leftGrid->setContentsMargins(6, 18, 6, 4);
        leftGrid->setHorizontalSpacing(2);
        leftGrid->setVerticalSpacing(1);
        buildSliderBank(leftGrid, slidersL_, freqLabelsL_, gainLabelsL_,
                        0, QColor("#00d4aa"));
        channelRow->addWidget(leftGroup);

        /* Right channel */
        auto* rightGroup = makeGroupBox("Right Channel");
        rightGroup->setStyleSheet(kGroupBoxQssRight);
        auto* rightGrid = new QGridLayout(rightGroup);
        rightGrid->setContentsMargins(6, 18, 6, 4);
        rightGrid->setHorizontalSpacing(2);
        rightGrid->setVerticalSpacing(1);
        buildSliderBank(rightGrid, slidersR_, freqLabelsR_, gainLabelsR_,
                        NUM_BANDS, QColor("#FF9800"));
        channelRow->addWidget(rightGroup);

        root->addLayout(channelRow);

        /* ── Button row ───────────────────────────────────────────── */

        auto* btnRow = new QHBoxLayout;
        btnRow->setSpacing(8);
        btnRow->addStretch();

        auto* applyBtn = new QPushButton("Apply");
        applyBtn->setStyleSheet(kBtnQss);
        connect(applyBtn, &QPushButton::clicked, this, &DualEq15Dialog::onApply);
        btnRow->addWidget(applyBtn);

        auto* resetBtn = new QPushButton("Reset");
        resetBtn->setStyleSheet(kBtnQss);
        connect(resetBtn, &QPushButton::clicked, this, &DualEq15Dialog::onReset);
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
        connect(meterTimer_, &QTimer::timeout, this, &DualEq15Dialog::onMeterTick);
        meterTimer_->start(50);
    }

private slots:
    void onSliderChanged(int paramIndex, int val)
    {
        float db   = static_cast<float>(val) / 10.0f;
        float norm = (db + 12.0f) / 24.0f;
        fx_->setParamValue(paramIndex, norm);

        /* Update the correct gain label */
        if (paramIndex < NUM_BANDS) {
            updateGainLabel(gainLabelsL_[paramIndex], db);
        } else {
            updateGainLabel(gainLabelsR_[paramIndex - NUM_BANDS], db);
        }

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
        for (int i = 0; i < NUM_BANDS * 2; ++i) {
            fx_->setParamValue(i, 0.5f);  /* 0 dB */
        }
        for (int i = 0; i < NUM_BANDS; ++i) {
            slidersL_[i]->setValue(0);
            slidersR_[i]->setValue(0);
            updateGainLabel(gainLabelsL_[i], 0.0f);
            updateGainLabel(gainLabelsR_[i], 0.0f);
        }
        refreshCurve();
        clipLabel_->setVisible(false);
    }

    void onMeterTick()
    {
        if (fx_->isClipping()) {
            int band = fx_->clipBand();
            const char* ch = (band < NUM_BANDS) ? "L" : "R";
            int bIdx = (band < NUM_BANDS) ? band : (band - NUM_BANDS);

            char buf[48];
            snprintf(buf, sizeof(buf), "CLIP: %s %s",
                     ch, kFreqLabels[bIdx]);
            clipLabel_->setText(QString::fromLatin1(buf));
            clipLabel_->setVisible(true);
            fx_->resetClip();
        } else {
            /* Fade out after 2 seconds */
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
        25, 40, 63, 100, 160, 250, 400, 630, 1000, 1600,
        2500, 4000, 6300, 10000, 16000
    };

    static constexpr const char* kFreqLabels[NUM_BANDS] = {
        "25",  "40",  "63",  "100", "160",
        "250", "400", "630", "1k",  "1.6k",
        "2.5k","4k",  "6.3k","10k", "16k"
    };

    /* ── Helpers ──────────────────────────────────────────────────── */

    void buildSliderBank(QGridLayout* grid,
                         QSlider* sliders[], QLabel* freqLbls[], QLabel* gainLbls[],
                         int paramOffset, const QColor& accent)
    {
        QString accentHex = accent.name();

        for (int i = 0; i < NUM_BANDS; ++i) {
            /* Frequency label */
            freqLbls[i] = new QLabel(kFreqLabels[i]);
            freqLbls[i]->setAlignment(Qt::AlignCenter);
            freqLbls[i]->setStyleSheet(
                QString("font-size: 9px; color: %1; font-family: Menlo;")
                    .arg(accentHex));
            grid->addWidget(freqLbls[i], 0, i, Qt::AlignCenter);

            /* Vertical slider: -12 to +12 dB */
            sliders[i] = new QSlider(Qt::Vertical);
            sliders[i]->setRange(-120, 120);  /* x10 for 0.1 dB steps */
            sliders[i]->setSingleStep(5);
            sliders[i]->setPageStep(30);
            sliders[i]->setTickPosition(QSlider::NoTicks);
            sliders[i]->setMinimumHeight(60);
            sliders[i]->setMaximumWidth(21);
            sliders[i]->setStyleSheet(
                buildSliderQss(accent));
            grid->addWidget(sliders[i], 1, i, Qt::AlignCenter);

            /* Gain readout */
            gainLbls[i] = new QLabel("0.0");
            gainLbls[i]->setAlignment(Qt::AlignCenter);
            gainLbls[i]->setStyleSheet(
                QString("font-size: 9px; color: %1; font-family: Menlo;")
                    .arg(accentHex));
            grid->addWidget(gainLbls[i], 2, i, Qt::AlignCenter);

            /* Read initial state */
            int paramIdx = paramOffset + i;
            float norm = fx_->paramValue(paramIdx);
            float db   = norm * 24.0f - 12.0f;
            sliders[i]->setValue(static_cast<int>(db * 10.0f));
            updateGainLabel(gainLbls[i], db);

            /* Connect slider */
            connect(sliders[i], &QSlider::valueChanged, this,
                    [this, paramIdx](int val) { onSliderChanged(paramIdx, val); });
        }
    }

    void populatePresets()
    {
        presetCombo_->clear();
        auto presets = mc1dsp::PresetManager::listPresets(
            QString::fromUtf8(fx_->id()));
        for (const auto& p : presets)
            presetCombo_->addItem(p.name, p.filePath);
    }

    static void updateGainLabel(QLabel* label, float db)
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%+.1f", static_cast<double>(db));
        label->setText(QString::fromLatin1(buf));
    }

    void refreshCurve()
    {
        float gainsL[NUM_BANDS];
        float gainsR[NUM_BANDS];
        float freqs[NUM_BANDS];
        for (int i = 0; i < NUM_BANDS; ++i) {
            float normL = fx_->paramValue(i);
            float normR = fx_->paramValue(NUM_BANDS + i);
            gainsL[i] = normL * 24.0f - 12.0f;
            gainsR[i] = normR * 24.0f - 12.0f;
            freqs[i]  = kFreqs[i];
        }
        curve_->setDualBands15(gainsL, gainsR, freqs, fx_->sampleRate());
    }

    static QGroupBox* makeGroupBox(const QString& title)
    {
        auto* gb = new QGroupBox(title);
        gb->setStyleSheet(kGroupBoxQss);
        return gb;
    }

    static QString buildSliderQss(const QColor& accent)
    {
        QString hex = accent.name();
        QString darker = accent.darker(130).name();
        return QString(
            "QSlider::groove:vertical {"
            "  background: #1a2240;"
            "  width: 3px;"
            "  border-radius: 1px;"
            "}"
            "QSlider::handle:vertical {"
            "  background: qlineargradient(x1:0,y1:0,x2:1,y2:1,"
            "    stop:0 #c0c0c0, stop:0.5 #909090, stop:1 #707070);"
            "  border: 1px solid #505050;"
            "  width: 16px;"
            "  height: 8px;"
            "  margin: 0 -6px;"
            "  border-radius: 2px;"
            "}"
            "QSlider::handle:vertical:hover {"
            "  background: qlineargradient(x1:0,y1:0,x2:1,y2:1,"
            "    stop:0 #d4d4d4, stop:0.5 #a0a0a0, stop:1 #808080);"
            "  border-color: %1;"
            "}"
            "QSlider::sub-page:vertical {"
            "  background: #1a2240;"
            "}"
            "QSlider::add-page:vertical {"
            "  background: #1a2240;"
            "}").arg(hex);
    }

    /* ── Widgets ──────────────────────────────────────────────────── */

    mc1dsp::DspEffect* fx_ = nullptr;
    EqCurveWidget*     curve_ = nullptr;
    QComboBox*         presetCombo_ = nullptr;
    QLabel*            clipLabel_ = nullptr;
    QTimer*            meterTimer_ = nullptr;
    int                clipFadeCount_ = 0;

    QSlider*           slidersL_[NUM_BANDS] = {};
    QLabel*            freqLabelsL_[NUM_BANDS] = {};
    QLabel*            gainLabelsL_[NUM_BANDS] = {};

    QSlider*           slidersR_[NUM_BANDS] = {};
    QLabel*            freqLabelsR_[NUM_BANDS] = {};
    QLabel*            gainLabelsR_[NUM_BANDS] = {};

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

    /* Left channel: teal accent border */
    static constexpr const char* kGroupBoxQssLeft =
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
        "  color: #00d4aa;"
        "}";

    /* Right channel: orange accent border */
    static constexpr const char* kGroupBoxQssRight =
        "QGroupBox {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 #1a2240, stop:1 #141828);"
        "  border: 1px solid #2a3a50;"
        "  border-left: 3px solid #FF9800;"
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
        "  color: #FF9800;"
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
