/*
 * Mcaster1AudioPipe — Virtual Audio Routing Application
 * fx_ui/TubePreampDialog.h — Vintage Tube Mic Preamp editor dialog
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Warm vintage aesthetic with:
 *   - Dark walnut/mahogany background
 *   - Amber/orange accent color (tube glow)
 *   - Custom VU meter with warm needle
 *   - Glowing vacuum tube indicator
 *   - Brushed brass knob faces
 */

#pragma once

#include "fx_ui/RackKnob.h"
#include "fx_ui/RackMeter.h"
#include "patchbay/dsp/dsp_effect.h"
#include "patchbay/dsp/preset_manager.h"

#include <QDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QLinearGradient>
#include <QTimer>
#include <QInputDialog>

#include <cmath>
#include <string>

/* ── TubeGlowWidget — Custom-painted vacuum tube indicator ─────────── */

class TubeGlowWidget : public QWidget {
    Q_OBJECT

public:
    explicit TubeGlowWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(300, 200); resize(27, 39);
        setAttribute(Qt::WA_TranslucentBackground, true);
    }

    void setActive(bool on)
    {
        active_ = on;
        update();
    }

    bool isActive() const { return active_; }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const qreal w = width();
        const qreal h = height();
        const QPointF center(w / 2.0, h / 2.0);
        const qreal rx = w * 0.38;
        const qreal ry = h * 0.42;
        const QRectF tubeRect(center.x() - rx, center.y() - ry, rx * 2, ry * 2);

        /* Glass envelope (dark when off, warm amber when on) */
        if (active_) {
            /* Outer glow halo */
            QRadialGradient halo(center, std::max(rx, ry) * 1.8);
            halo.setColorAt(0.0, QColor(212, 160, 74, 50));
            halo.setColorAt(0.5, QColor(180, 100, 30, 20));
            halo.setColorAt(1.0, QColor(100, 50, 15, 0));
            p.setPen(Qt::NoPen);
            p.setBrush(halo);
            p.drawEllipse(center, std::max(rx, ry) * 1.8, std::max(rx, ry) * 1.8);

            /* Tube body — warm amber radial gradient */
            QRadialGradient tubeGrad(center, std::max(rx, ry));
            tubeGrad.setColorAt(0.0, QColor(212, 160, 74, 200));   // bright amber center
            tubeGrad.setColorAt(0.3, QColor(232, 150, 58, 150));    // warm orange
            tubeGrad.setColorAt(0.7, QColor(180, 100, 30, 80));     // dim
            tubeGrad.setColorAt(1.0, QColor(100, 50, 15, 0));       // transparent edge
            p.setBrush(tubeGrad);
            p.setPen(Qt::NoPen);
            p.drawEllipse(tubeRect);

            /* Filament — tiny bright core */
            QRadialGradient filament(center, rx * 0.25);
            filament.setColorAt(0.0, QColor(255, 220, 140, 240));
            filament.setColorAt(0.6, QColor(232, 160, 58, 120));
            filament.setColorAt(1.0, QColor(200, 120, 40, 0));
            p.setBrush(filament);
            p.drawEllipse(center, rx * 0.25, ry * 0.2);
        } else {
            /* Cold tube — dark glass */
            QRadialGradient coldGrad(center, std::max(rx, ry));
            coldGrad.setColorAt(0.0, QColor(60, 45, 30, 120));
            coldGrad.setColorAt(0.6, QColor(40, 28, 18, 80));
            coldGrad.setColorAt(1.0, QColor(26, 18, 16, 0));
            p.setBrush(coldGrad);
            p.setPen(Qt::NoPen);
            p.drawEllipse(tubeRect);
        }

        /* Glass rim highlight (always visible) */
        p.setPen(QPen(QColor(180, 140, 80, active_ ? 90 : 40), 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(tubeRect.adjusted(1, 1, -1, -1));

        /* Pin base at bottom */
        QRectF pinBase(center.x() - rx * 0.6, h - 6, rx * 1.2, 5);
        QLinearGradient pinGrad(pinBase.left(), pinBase.top(),
                                pinBase.left(), pinBase.bottom());
        pinGrad.setColorAt(0.0, QColor(90, 70, 40));
        pinGrad.setColorAt(0.5, QColor(140, 110, 60));
        pinGrad.setColorAt(1.0, QColor(80, 60, 35));
        p.setPen(Qt::NoPen);
        p.setBrush(pinGrad);
        p.drawRoundedRect(pinBase, 2, 2);
    }

private:
    bool active_ = true;
};

/* ── TubePreampDialog ──────────────────────────────────────────────── */

class TubePreampDialog : public QDialog {
    Q_OBJECT

public:
    explicit TubePreampDialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : QDialog(parent)
        , fx_(fx)
    {
        setWindowTitle("Tube Preamp");
        setMinimumSize(319, 200); resize(638, 360);
        setAttribute(Qt::WA_DeleteOnClose, false);
        setStyleSheet(kVintageStyle);
        buildUi();
        loadFromEffect();
        startMetering();
    }

private:
    /* ── UI construction ───────────────────────────────────────────── */

    void buildUi()
    {
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(14, 10, 14, 10);
        root->setSpacing(6);

        /* ── Header ────────────────────────────────────────────────── */

        auto* headerRow = new QHBoxLayout;
        headerRow->setSpacing(10);

        /* Title block */
        auto* titleBlock = new QVBoxLayout;
        titleBlock->setSpacing(0);

        auto* titleLabel = new QLabel("TUBE PREAMP");
        titleLabel->setStyleSheet(
            "font-size: 26px; font-weight: bold; color: #D4A04A;"
            " font-family: 'Georgia', 'Times New Roman', serif;"
            " letter-spacing: 4px; background: transparent;");
        titleBlock->addWidget(titleLabel);

        auto* subtitleLabel = new QLabel("Vacuum Tube Mic Preamplifier");
        subtitleLabel->setStyleSheet(
            "font-size: 11px; color: #A08060; font-style: italic;"
            " letter-spacing: 1px; background: transparent;");
        titleBlock->addWidget(subtitleLabel);

        headerRow->addLayout(titleBlock);

        /* Tube glow indicator */
        tubeGlow_ = new TubeGlowWidget;
        tubeGlow_->setActive(fx_ ? fx_->isEnabled() : true);
        headerRow->addWidget(tubeGlow_, 0, Qt::AlignVCenter);

        headerRow->addStretch();

        /* Preset controls in header */
        auto* presetLabel = new QLabel("Preset:");
        presetLabel->setStyleSheet(
            "font-size: 11px; color: #A08060; background: transparent;");
        headerRow->addWidget(presetLabel, 0, Qt::AlignVCenter);

        presetCombo_ = new QComboBox;
        presetCombo_->setFixedWidth(120);
        headerRow->addWidget(presetCombo_, 0, Qt::AlignVCenter);

        auto* loadBtn = new QPushButton("Load");
        loadBtn->setFixedWidth(38);
        connect(loadBtn, &QPushButton::clicked, this, &TubePreampDialog::applyPreset);
        headerRow->addWidget(loadBtn, 0, Qt::AlignVCenter);

        auto* saveBtn = new QPushButton("Save");
        saveBtn->setFixedWidth(38);
        connect(saveBtn, &QPushButton::clicked, this, &TubePreampDialog::savePreset);
        headerRow->addWidget(saveBtn, 0, Qt::AlignVCenter);

        root->addLayout(headerRow);

        /* ── Decorative divider line ───────────────────────────────── */

        auto* divider = new QFrame;
        divider->setFrameShape(QFrame::HLine);
        divider->setStyleSheet(
            "color: #3a2a1e; background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
            " stop:0 transparent, stop:0.2 #D4A04A, stop:0.8 #D4A04A,"
            " stop:1 transparent);");
        divider->setFixedHeight(2);
        root->addWidget(divider);

        /* ── Main area: knobs + meters ─────────────────────────────── */

        auto* mainRow = new QHBoxLayout;
        mainRow->setSpacing(8);

        /* --- Knob panel (left, takes most space) --- */
        auto* knobPanel = new QVBoxLayout;
        knobPanel->setSpacing(6);

        /* Row 1: Input Stage */
        auto* inputGroup = createSection("INPUT STAGE");
        auto* inputRow = new QHBoxLayout;
        inputRow->setSpacing(4);

        knobs_[0] = createKnob("Input Gain", kAmber);
        knobs_[1] = createKnob("Drive",      kDriveOrange);
        knobs_[2] = createKnob("Warmth",     kAmber);
        knobs_[6] = createKnob("Bias",       kBronze);
        knobs_[7] = createKnob("Sag",        kBronze);

        /* Make Drive knob slightly larger to stand out */
        knobs_[1]->setFixedSize(66, 88);
        knobs_[1]->setNotches(13);

        inputRow->addWidget(knobs_[0], 0, Qt::AlignCenter);
        inputRow->addWidget(knobs_[1], 0, Qt::AlignCenter);
        inputRow->addWidget(knobs_[2], 0, Qt::AlignCenter);
        inputRow->addWidget(knobs_[6], 0, Qt::AlignCenter);
        inputRow->addWidget(knobs_[7], 0, Qt::AlignCenter);

        static_cast<QVBoxLayout*>(inputGroup->layout())->addLayout(inputRow);
        knobPanel->addWidget(inputGroup);

        /* Row 2: Tone + Output */
        auto* toneGroup = createSection("TONE + OUTPUT");
        auto* toneRow = new QHBoxLayout;
        toneRow->setSpacing(4);

        knobs_[4] = createKnob("Low Cut",     kAmber);
        knobs_[3] = createKnob("Presence",    kAmber);
        knobs_[5] = createKnob("Transformer", kAmber);
        knobs_[8] = createKnob("Air",         kLightAmber);
        knobs_[9] = createKnob("Output",      kAmber);

        toneRow->addWidget(knobs_[4], 0, Qt::AlignCenter);
        toneRow->addWidget(knobs_[3], 0, Qt::AlignCenter);
        toneRow->addWidget(knobs_[5], 0, Qt::AlignCenter);
        toneRow->addWidget(knobs_[8], 0, Qt::AlignCenter);
        toneRow->addWidget(knobs_[9], 0, Qt::AlignCenter);

        static_cast<QVBoxLayout*>(toneGroup->layout())->addLayout(toneRow);
        knobPanel->addWidget(toneGroup);

        mainRow->addLayout(knobPanel, 1);

        /* --- Meter panel (right side) --- */
        auto* meterGroup = createSection("METERS");
        auto* meterLayout = new QHBoxLayout;
        meterLayout->setSpacing(6);

        auto addMeterCol = [&](RackMeter* m, const QString& label) {
            auto* col = new QVBoxLayout;
            col->setSpacing(2);
            auto* lbl = new QLabel(label);
            lbl->setStyleSheet(
                "font-size: 9px; font-weight: bold; color: #D4A04A;"
                " background: transparent;");
            lbl->setAlignment(Qt::AlignCenter);
            col->addWidget(lbl);
            col->addWidget(m, 1);
            meterLayout->addLayout(col);
        };

        inputMeter_  = new RackMeter(RackMeter::INPUT_METER);
        grMeter_     = new RackMeter(RackMeter::GR_METER);
        outputMeter_ = new RackMeter(RackMeter::OUTPUT_METER);

        inputMeter_->setFixedWidth(21);
        grMeter_->setFixedWidth(21);
        outputMeter_->setFixedWidth(21);

        addMeterCol(inputMeter_,  "IN");
        addMeterCol(grMeter_,     "SAG");
        addMeterCol(outputMeter_, "OUT");

        static_cast<QVBoxLayout*>(meterGroup->layout())->addLayout(meterLayout);
        meterGroup->setFixedWidth(98);
        mainRow->addWidget(meterGroup);
        root->addLayout(mainRow, 0);

        root->addStretch(1);

        /* ── Bottom bar: Apply / Reset / Close ─────────────────────── */

        auto* bottomRow = new QHBoxLayout;
        bottomRow->setSpacing(8);
        bottomRow->addStretch();

        auto* applyBtn = new QPushButton("Apply");
        auto* resetBtn = new QPushButton("Reset");
        auto* closeBtn = new QPushButton("Close");

        connect(applyBtn, &QPushButton::clicked, this, &TubePreampDialog::applyPreset);
        connect(resetBtn, &QPushButton::clicked, this, &TubePreampDialog::resetParams);
        connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);

        bottomRow->addWidget(applyBtn);
        bottomRow->addWidget(resetBtn);
        bottomRow->addWidget(closeBtn);

        root->addLayout(bottomRow);
    }

    /* ── Section factory ───────────────────────────────────────────── */

    QGroupBox* createSection(const QString& title)
    {
        auto* group = new QGroupBox(title);
        auto* layout = new QVBoxLayout(group);
        layout->setSpacing(4);
        layout->setContentsMargins(6, 20, 6, 6);
        return group;
    }

    /* ── Knob factory ──────────────────────────────────────────────── */

    RackKnob* createKnob(const QString& title, const QColor& accent,
                          int w = 48, int h = 62)
    {
        auto* knob = new RackKnob;
        knob->setStyle(RackKnob::Chicken);
        knob->setTitle(title);
        knob->setAccentColor(accent);
        knob->setNotches(11);
        knob->setFixedSize(w, h);
        return knob;
    }

    /* ── Load state from effect ────────────────────────────────────── */

    void loadFromEffect()
    {
        if (!fx_) return;

        /* Wire up knobs to params and read initial values */
        for (int i = 0; i < kParamCount; ++i) {
            if (!knobs_[i]) continue;

            /* Connect knob -> effect param */
            connect(knobs_[i], &RackKnob::valueChanged,
                    this, [this, i](float val) {
                if (fx_) fx_->setParamValue(i, val);
                updateKnobText(i);
            });

            /* Read current value */
            knobs_[i]->blockSignals(true);
            knobs_[i]->setValue(fx_->paramValue(i));
            knobs_[i]->blockSignals(false);
            updateKnobText(i);
        }

        /* Populate preset list */
        populatePresets();

        /* Sync tube glow to enabled state */
        tubeGlow_->setActive(fx_->isEnabled());
    }

    void updateKnobText(int index)
    {
        if (!fx_ || index < 0 || index >= kParamCount || !knobs_[index])
            return;
        knobs_[index]->setValueText(
            QString::fromStdString(fx_->paramDisplayValue(index)));
    }

    /* ── Metering ──────────────────────────────────────────────────── */

    void startMetering()
    {
        meterTimer_ = new QTimer(this);
        connect(meterTimer_, &QTimer::timeout, this, [this]() {
            if (!fx_) return;

            inputMeter_->setLevel(fx_->meterInputPeak());
            grMeter_->setLevel(fx_->meterGainReduction());
            outputMeter_->setLevel(fx_->meterOutputPeak());

            /* Refresh knob display text */
            for (int i = 0; i < kParamCount; ++i)
                updateKnobText(i);

            /* Sync tube glow with enabled state */
            tubeGlow_->setActive(fx_->isEnabled());
        });
        meterTimer_->start(50);  /* 20 FPS */
    }

    /* ── Presets ────────────────────────────────────────────────────── */

    void populatePresets()
    {
        presetCombo_->clear();
        if (!fx_) return;
        presets_ = mc1dsp::PresetManager::listPresets(
            QString::fromUtf8(fx_->id()));
        for (const auto& p : presets_) {
            QString label = p.isFactory ? p.name + "  [F]" : p.name;
            presetCombo_->addItem(label, p.filePath);
        }
    }

    void applyPreset()
    {
        if (!fx_ || presetCombo_->currentIndex() < 0) return;
        QString path = presetCombo_->currentData().toString();
        if (path.isEmpty()) return;
        auto preset = mc1dsp::PresetManager::loadPreset(path);
        mc1dsp::PresetManager::applyPreset(preset, fx_);
        reloadKnobs();
    }

    void savePreset()
    {
        bool ok = false;
        QString name = QInputDialog::getText(this, "Save Preset",
            "Preset name:", QLineEdit::Normal, "", &ok);
        if (!ok || name.trimmed().isEmpty()) return;
        auto preset = mc1dsp::PresetManager::capturePreset(fx_, name.trimmed());
        mc1dsp::PresetManager::savePreset(preset);
        populatePresets();
    }

    void resetParams()
    {
        if (!fx_) return;
        fx_->reset();
        reloadKnobs();
    }

    void reloadKnobs()
    {
        if (!fx_) return;
        for (int i = 0; i < kParamCount; ++i) {
            if (!knobs_[i]) continue;
            knobs_[i]->blockSignals(true);
            knobs_[i]->setValue(fx_->paramValue(i));
            knobs_[i]->blockSignals(false);
            updateKnobText(i);
        }
    }

    /* ── Vintage style constants ───────────────────────────────────── */

    static constexpr const char* kVintageStyle =
        /* Dialog background: dark walnut gradient */
        "QDialog {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 #2a1e18, stop:1 #1a1210);"
        "  color: #E8D8C0;"
        "}"
        /* Group boxes: walnut panels with amber left accent */
        "QGroupBox {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 rgba(42,30,24,0.9), stop:1 rgba(26,18,16,0.9));"
        "  border: 1px solid #3a2a1e;"
        "  border-left: 3px solid #D4A04A;"
        "  border-radius: 4px;"
        "  margin-top: 14px;"
        "  padding: 10px 6px 6px 6px;"
        "  font-size: 11px;"
        "  color: #A08060;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  subcontrol-position: top left;"
        "  padding: 2px 10px;"
        "  color: #D4A04A;"
        "  background: #1a1210;"
        "  border: 1px solid #3a2a1e;"
        "  border-radius: 3px;"
        "  font-weight: bold;"
        "  letter-spacing: 2px;"
        "}"
        /* Labels */
        "QLabel { color: #E8D8C0; background: transparent; }"
        /* Buttons: dark bronze with amber text */
        "QPushButton {"
        "  background: #3a2a1e;"
        "  color: #D4A04A;"
        "  padding: 5px 14px;"
        "  border: 1px solid #4a3a2e;"
        "  border-radius: 4px;"
        "  font-size: 11px;"
        "  min-width: 60px;"
        "}"
        "QPushButton:hover {"
        "  background: #4a3a2e;"
        "  border-color: #D4A04A;"
        "}"
        "QPushButton:pressed {"
        "  background: #D4A04A;"
        "  color: #1a1210;"
        "}"
        /* Combo box: walnut with cream text */
        "QComboBox {"
        "  background: #2a1e18;"
        "  color: #E8D8C0;"
        "  border: 1px solid #3a2a1e;"
        "  border-radius: 4px;"
        "  padding: 4px 8px;"
        "  font-size: 11px;"
        "}"
        "QComboBox::drop-down { border: none; width: 20px; }"
        "QComboBox QAbstractItemView {"
        "  background: #2a1e18;"
        "  border: 1px solid #3a2a1e;"
        "  color: #E8D8C0;"
        "  selection-background-color: #D4A04A;"
        "  selection-color: #1a1210;"
        "}"
        /* Frame divider */
        "QFrame { background: transparent; }";

    /* ── Color constants ───────────────────────────────────────────── */

    static inline const QColor kAmber      = QColor("#D4A04A");
    static inline const QColor kDriveOrange= QColor("#FF8C00");
    static inline const QColor kBronze     = QColor("#9A7B4F");
    static inline const QColor kLightAmber = QColor("#E8B84A");

    /* ── Data ──────────────────────────────────────────────────────── */

    static constexpr int kParamCount = 10;

    mc1dsp::DspEffect* fx_ = nullptr;
    QTimer*            meterTimer_  = nullptr;
    QComboBox*         presetCombo_ = nullptr;
    TubeGlowWidget*    tubeGlow_    = nullptr;
    QVector<mc1dsp::Preset> presets_;

    RackKnob*  knobs_[kParamCount] = {};
    RackMeter* inputMeter_  = nullptr;
    RackMeter* grMeter_     = nullptr;
    RackMeter* outputMeter_ = nullptr;
};
