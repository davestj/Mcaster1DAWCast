/*
 * Mcaster1AudioPipe — Virtual Audio Routing Application
 * fx_ui/CasterTubeDialog.h — CasterTube Vocal Tone Shaper editor
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Boutique studio aesthetic with:
 *   - Deep indigo/midnight blue background
 *   - Electric blue accent color (#4A9BD9)
 *   - Warm amber accents for tube-related controls
 *   - Vocal Range selector with frequency display
 *   - Glowing tube indicator (blue-tinted)
 *   - Custom VU-style meters
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
#include <QFrame>

#include <cmath>
#include <string>

/* ── BlueTubeGlowWidget — Blue-tinted vacuum tube indicator ──────── */

class BlueTubeGlowWidget : public QWidget {
    Q_OBJECT

public:
    explicit BlueTubeGlowWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setFixedSize(36, 52);
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

        if (active_) {
            /* Outer glow halo — electric blue */
            QRadialGradient halo(center, std::max(rx, ry) * 1.8);
            halo.setColorAt(0.0, QColor(74, 155, 217, 60));
            halo.setColorAt(0.3, QColor(60, 130, 200, 35));
            halo.setColorAt(0.6, QColor(40, 80, 160, 15));
            halo.setColorAt(1.0, QColor(20, 40, 100, 0));
            p.setPen(Qt::NoPen);
            p.setBrush(halo);
            p.drawEllipse(center, std::max(rx, ry) * 1.8, std::max(rx, ry) * 1.8);

            /* Tube body — blue radial gradient */
            QRadialGradient tubeGrad(center, std::max(rx, ry));
            tubeGrad.setColorAt(0.0, QColor(74, 155, 217, 200));
            tubeGrad.setColorAt(0.3, QColor(60, 130, 200, 150));
            tubeGrad.setColorAt(0.7, QColor(30, 70, 140, 80));
            tubeGrad.setColorAt(1.0, QColor(14, 30, 80, 0));
            p.setBrush(tubeGrad);
            p.setPen(Qt::NoPen);
            p.drawEllipse(tubeRect);

            /* Filament — bright blue-white core */
            QRadialGradient filament(center, rx * 0.25);
            filament.setColorAt(0.0, QColor(180, 220, 255, 240));
            filament.setColorAt(0.4, QColor(100, 170, 230, 160));
            filament.setColorAt(0.7, QColor(74, 155, 217, 80));
            filament.setColorAt(1.0, QColor(40, 80, 160, 0));
            p.setBrush(filament);
            p.drawEllipse(center, rx * 0.25, ry * 0.2);
        } else {
            /* Cold tube — dark glass */
            QRadialGradient coldGrad(center, std::max(rx, ry));
            coldGrad.setColorAt(0.0, QColor(30, 40, 60, 120));
            coldGrad.setColorAt(0.6, QColor(18, 25, 45, 80));
            coldGrad.setColorAt(1.0, QColor(14, 20, 40, 0));
            p.setBrush(coldGrad);
            p.setPen(Qt::NoPen);
            p.drawEllipse(tubeRect);
        }

        /* Glass rim highlight */
        p.setPen(QPen(QColor(74, 155, 217, active_ ? 100 : 35), 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(tubeRect.adjusted(1, 1, -1, -1));

        /* Pin base at bottom — brushed steel tint */
        QRectF pinBase(center.x() - rx * 0.6, h - 6, rx * 1.2, 5);
        QLinearGradient pinGrad(pinBase.left(), pinBase.top(),
                                pinBase.left(), pinBase.bottom());
        pinGrad.setColorAt(0.0, QColor(60, 70, 90));
        pinGrad.setColorAt(0.5, QColor(90, 105, 130));
        pinGrad.setColorAt(1.0, QColor(50, 60, 80));
        p.setPen(Qt::NoPen);
        p.setBrush(pinGrad);
        p.drawRoundedRect(pinBase, 2, 2);
    }

private:
    bool active_ = true;
};

/* ── CasterTubeDialog ─────────────────────────────────────────────── */

class CasterTubeDialog : public QDialog {
    Q_OBJECT

public:
    explicit CasterTubeDialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : QDialog(parent)
        , fx_(fx)
    {
        setWindowTitle("CasterTube Vocal Tone Shaper");
        setFixedSize(850, 550);
        setAttribute(Qt::WA_DeleteOnClose, false);
        setStyleSheet(kCasterTubeStyle);
        buildUi();
        loadFromEffect();
        startMetering();
    }

private:
    /* ── UI construction ──────────────────────────────────────────── */

    void buildUi()
    {
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(14, 10, 14, 10);
        root->setSpacing(6);

        /* ── Header Row ───────────────────────────────────────────── */

        auto* headerRow = new QHBoxLayout;
        headerRow->setSpacing(10);

        /* Title block */
        auto* titleBlock = new QVBoxLayout;
        titleBlock->setSpacing(0);

        auto* titleLabel = new QLabel("CASTERTUBE");
        titleLabel->setStyleSheet(
            "font-size: 20px; font-weight: bold; color: #4A9BD9;"
            " font-family: 'Helvetica Neue', 'Arial Black', sans-serif;"
            " letter-spacing: 4px; background: transparent;");
        titleBlock->addWidget(titleLabel);

        auto* subtitleLabel = new QLabel("Vocal Tone Shaper");
        subtitleLabel->setStyleSheet(
            "font-size: 11px; color: #8899BB; font-style: italic;"
            " letter-spacing: 1px; background: transparent;");
        titleBlock->addWidget(subtitleLabel);

        headerRow->addLayout(titleBlock);

        /* Blue tube glow indicator */
        tubeGlow_ = new BlueTubeGlowWidget;
        tubeGlow_->setActive(fx_ ? fx_->isEnabled() : true);
        headerRow->addWidget(tubeGlow_, 0, Qt::AlignVCenter);

        headerRow->addSpacing(10);

        /* Vocal Range selector */
        auto* rangeBlock = new QVBoxLayout;
        rangeBlock->setSpacing(2);

        auto* rangeLabel = new QLabel("VOCAL RANGE");
        rangeLabel->setStyleSheet(
            "font-size: 9px; font-weight: bold; color: #C8A86E;"
            " letter-spacing: 2px; background: transparent;");
        rangeBlock->addWidget(rangeLabel);

        rangeCombo_ = new QComboBox;
        rangeCombo_->addItems({"Bass (E2-E4)", "Baritone (A2-A4)", "Tenor (C3-C5)",
                               "Alto (F3-F5)", "Soprano (C4-C6)"});
        rangeCombo_->setStyleSheet(
            "QComboBox { background: #1a2848; color: #C8A86E; border: 2px solid #4A9BD9;"
            "  border-radius: 4px; padding: 6px 12px; font-size: 13px; font-weight: bold; }"
            "QComboBox:hover { border-color: #6AB5E9; }"
            "QComboBox::drop-down { border: none; width: 22px; }"
            "QComboBox QAbstractItemView { background: #141e38; color: #D8E0F0;"
            "  selection-background-color: #4A9BD9; selection-color: #0e1428;"
            "  border: 1px solid #1a2848; }");
        rangeCombo_->setFixedWidth(200);
        connect(rangeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int index) {
            if (fx_ && index >= 0 && index <= 4)
                fx_->setParamValue(4, static_cast<float>(index) / 4.0f);
        });
        rangeBlock->addWidget(rangeCombo_);

        rangeFreqLabel_ = new QLabel("82 - 330 Hz");
        rangeFreqLabel_->setStyleSheet(
            "font-size: 9px; color: #6680AA; font-style: italic;"
            " background: transparent;");
        rangeBlock->addWidget(rangeFreqLabel_);

        headerRow->addLayout(rangeBlock);

        headerRow->addStretch();

        /* Preset controls */
        auto* presetLabel = new QLabel("Preset:");
        presetLabel->setStyleSheet(
            "font-size: 11px; color: #8899BB; background: transparent;");
        headerRow->addWidget(presetLabel, 0, Qt::AlignVCenter);

        presetCombo_ = new QComboBox;
        presetCombo_->setFixedWidth(160);
        headerRow->addWidget(presetCombo_, 0, Qt::AlignVCenter);

        auto* loadBtn = new QPushButton("Load");
        loadBtn->setFixedWidth(50);
        connect(loadBtn, &QPushButton::clicked, this, &CasterTubeDialog::applyPreset);
        headerRow->addWidget(loadBtn, 0, Qt::AlignVCenter);

        auto* saveBtn = new QPushButton("Save");
        saveBtn->setFixedWidth(50);
        connect(saveBtn, &QPushButton::clicked, this, &CasterTubeDialog::savePreset);
        headerRow->addWidget(saveBtn, 0, Qt::AlignVCenter);

        root->addLayout(headerRow);

        /* ── Decorative divider ───────────────────────────────────── */

        auto* divider = new QFrame;
        divider->setFrameShape(QFrame::HLine);
        divider->setStyleSheet(
            "color: #1a2848; background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
            " stop:0 transparent, stop:0.15 #4A9BD9, stop:0.5 #6AB5E9,"
            " stop:0.85 #4A9BD9, stop:1 transparent);");
        divider->setFixedHeight(2);
        root->addWidget(divider);

        /* ── Main area: 3 groups + meters ─────────────────────────── */

        auto* mainRow = new QHBoxLayout;
        mainRow->setSpacing(8);

        /* ── LEFT: TUBE STAGE (amber accent) ──────────────────────── */

        auto* tubeGroup = createSection("TUBE STAGE", kAmber);
        auto* tubeGrid = new QGridLayout;
        tubeGrid->setSpacing(4);
        tubeGrid->setContentsMargins(6, 4, 6, 6);

        knobs_[0]  = createKnob("Input Gain",  kAmber);
        knobs_[1]  = createKnob("Tube Drive",  kDriveAmber, 88, 118);
        knobs_[2]  = createKnob("Character",   kAmber);
        knobs_[12] = createKnob("Transformer", kAmber);

        /* Make Drive the hero knob */
        knobs_[1]->setNotches(13);

        /* 2x2 grid: Drive top-left (larger), rest fill around it */
        tubeGrid->addWidget(knobs_[0],  0, 0, Qt::AlignCenter);
        tubeGrid->addWidget(knobs_[1],  0, 1, Qt::AlignCenter);
        tubeGrid->addWidget(knobs_[2],  1, 0, Qt::AlignCenter);
        tubeGrid->addWidget(knobs_[12], 1, 1, Qt::AlignCenter);

        static_cast<QVBoxLayout*>(tubeGroup->layout())->addLayout(tubeGrid);
        mainRow->addWidget(tubeGroup);

        /* ── CENTER: VOCAL SHAPE (gold accent) ────────────────────── */

        auto* vocalGroup = createSection("VOCAL SHAPE", kGold);
        auto* vocalGrid = new QGridLayout;
        vocalGrid->setSpacing(4);
        vocalGrid->setContentsMargins(6, 4, 6, 6);

        knobs_[6] = createKnob("Warmth",   kGold);
        knobs_[7] = createKnob("Presence", kGold);
        knobs_[5] = createKnob("Depth",    kGold);
        knobs_[3] = createKnob("Sustain",  kGold);

        vocalGrid->addWidget(knobs_[6], 0, 0, Qt::AlignCenter);
        vocalGrid->addWidget(knobs_[7], 0, 1, Qt::AlignCenter);
        vocalGrid->addWidget(knobs_[5], 1, 0, Qt::AlignCenter);
        vocalGrid->addWidget(knobs_[3], 1, 1, Qt::AlignCenter);

        static_cast<QVBoxLayout*>(vocalGroup->layout())->addLayout(vocalGrid);
        mainRow->addWidget(vocalGroup);

        /* ── RIGHT: TONE + OUTPUT (blue accent) ───────────────────── */

        auto* toneGroup = createSection("TONE + OUTPUT", kElectricBlue);
        auto* toneLayout = new QVBoxLayout;
        toneLayout->setSpacing(4);
        toneLayout->setContentsMargins(6, 4, 6, 6);

        /* Top row: De-Ess + Silk (cool silver) */
        auto* toneTopRow = new QHBoxLayout;
        toneTopRow->setSpacing(4);
        knobs_[9] = createKnob("De-Ess", kCoolSilver);
        knobs_[8] = createKnob("Silk",   kCoolSilver);
        toneTopRow->addWidget(knobs_[9], 0, Qt::AlignCenter);
        toneTopRow->addWidget(knobs_[8], 0, Qt::AlignCenter);
        toneLayout->addLayout(toneTopRow);

        /* Bottom row: Air + Low Cut + Output (blue) */
        auto* toneBotRow = new QHBoxLayout;
        toneBotRow->setSpacing(4);
        knobs_[10] = createKnob("Air",     kElectricBlue);
        knobs_[11] = createKnob("Low Cut", kElectricBlue);
        knobs_[13] = createKnob("Output",  kElectricBlue);
        toneBotRow->addWidget(knobs_[10], 0, Qt::AlignCenter);
        toneBotRow->addWidget(knobs_[11], 0, Qt::AlignCenter);
        toneBotRow->addWidget(knobs_[13], 0, Qt::AlignCenter);
        toneLayout->addLayout(toneBotRow);

        static_cast<QVBoxLayout*>(toneGroup->layout())->addLayout(toneLayout);
        mainRow->addWidget(toneGroup);

        /* ── FAR RIGHT: Meters ────────────────────────────────────── */

        auto* meterGroup = createSection("METERS", kElectricBlue);
        auto* meterLayout = new QHBoxLayout;
        meterLayout->setSpacing(6);

        auto addMeterCol = [&](RackMeter* m, const QString& label,
                               const QColor& labelColor) {
            auto* col = new QVBoxLayout;
            col->setSpacing(2);
            auto* lbl = new QLabel(label);
            lbl->setStyleSheet(
                QString("font-size: 9px; font-weight: bold; color: %1;"
                        " background: transparent;")
                    .arg(labelColor.name()));
            lbl->setAlignment(Qt::AlignCenter);
            col->addWidget(lbl);
            col->addWidget(m, 1);
            meterLayout->addLayout(col);
        };

        inputMeter_   = new RackMeter(RackMeter::INPUT_METER);
        sustainMeter_ = new RackMeter(RackMeter::GR_METER);
        outputMeter_  = new RackMeter(RackMeter::OUTPUT_METER);

        inputMeter_->setFixedWidth(28);
        sustainMeter_->setFixedWidth(28);
        outputMeter_->setFixedWidth(28);

        addMeterCol(inputMeter_,   "IN",      kElectricBlue);
        addMeterCol(sustainMeter_, "SUSTAIN",  kGold);
        addMeterCol(outputMeter_,  "OUT",      kElectricBlue);

        static_cast<QVBoxLayout*>(meterGroup->layout())->addLayout(meterLayout);
        meterGroup->setFixedWidth(140);
        mainRow->addWidget(meterGroup);

        root->addLayout(mainRow, 1);

        /* ── Bottom bar ───────────────────────────────────────────── */

        auto* bottomRow = new QHBoxLayout;
        bottomRow->setSpacing(8);
        bottomRow->addStretch();

        auto* applyBtn = new QPushButton("Apply");
        auto* resetBtn = new QPushButton("Reset");
        auto* closeBtn = new QPushButton("Close");

        connect(applyBtn, &QPushButton::clicked, this, &CasterTubeDialog::applyPreset);
        connect(resetBtn, &QPushButton::clicked, this, &CasterTubeDialog::resetParams);
        connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);

        bottomRow->addWidget(applyBtn);
        bottomRow->addWidget(resetBtn);
        bottomRow->addWidget(closeBtn);

        root->addLayout(bottomRow);
    }

    /* ── Section factory (with colored left accent border) ────────── */

    QGroupBox* createSection(const QString& title, const QColor& accent)
    {
        auto* group = new QGroupBox(title);
        group->setStyleSheet(
            QString(
                "QGroupBox {"
                "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
                "    stop:0 rgba(20,30,56,0.9), stop:1 rgba(14,20,40,0.9));"
                "  border: 1px solid #1a2848;"
                "  border-left: 3px solid %1;"
                "  border-radius: 4px;"
                "  margin-top: 14px;"
                "  padding: 10px 6px 6px 6px;"
                "  font-size: 11px;"
                "  color: %2;"
                "}"
                "QGroupBox::title {"
                "  subcontrol-origin: margin;"
                "  subcontrol-position: top left;"
                "  padding: 2px 10px;"
                "  color: %1;"
                "  background: #0e1428;"
                "  border: 1px solid #1a2848;"
                "  border-radius: 3px;"
                "  font-weight: bold;"
                "  letter-spacing: 2px;"
                "}")
                .arg(accent.name(), accent.name()));
        auto* layout = new QVBoxLayout(group);
        layout->setSpacing(4);
        layout->setContentsMargins(6, 20, 6, 6);
        return group;
    }

    /* ── Knob factory ─────────────────────────────────────────────── */

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

    /* ── Load state from effect ───────────────────────────────────── */

    void loadFromEffect()
    {
        if (!fx_) return;

        /* Wire knobs to params and read initial values */
        for (int i = 0; i < kParamCount; ++i) {
            if (i == 4) continue;   /* Vocal Range handled by combo */
            if (!knobs_[i]) continue;

            connect(knobs_[i], &RackKnob::valueChanged,
                    this, [this, i](float val) {
                if (fx_) fx_->setParamValue(i, val);
                updateKnobText(i);
            });

            knobs_[i]->blockSignals(true);
            knobs_[i]->setValue(fx_->paramValue(i));
            knobs_[i]->blockSignals(false);
            updateKnobText(i);
        }

        /* Sync Vocal Range combo to param 4 */
        float rangeVal = fx_->paramValue(4);
        int rangeIndex = static_cast<int>(std::round(rangeVal * 4.0f));
        rangeIndex = (rangeIndex < 0 ? 0 : (rangeIndex > 4 ? 4 : rangeIndex));
        rangeCombo_->blockSignals(true);
        rangeCombo_->setCurrentIndex(rangeIndex);
        rangeCombo_->blockSignals(false);
        updateRangeFreqLabel(rangeIndex);

        /* Connect combo index change -> freq label update */
        connect(rangeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int idx) {
            updateRangeFreqLabel(idx);
        });

        /* Populate preset list */
        populatePresets();

        /* Sync tube glow */
        tubeGlow_->setActive(fx_->isEnabled());
    }

    void updateKnobText(int index)
    {
        if (!fx_ || index < 0 || index >= kParamCount || !knobs_[index])
            return;
        knobs_[index]->setValueText(
            QString::fromStdString(fx_->paramDisplayValue(index)));
    }

    void updateRangeFreqLabel(int index)
    {
        static const char* kRangeFreqs[] = {
            "82 - 330 Hz",      /* Bass: E2-E4 */
            "110 - 440 Hz",     /* Baritone: A2-A4 */
            "131 - 523 Hz",     /* Tenor: C3-C5 */
            "175 - 698 Hz",     /* Alto: F3-F5 */
            "262 - 1047 Hz"     /* Soprano: C4-C6 */
        };
        if (index >= 0 && index <= 4)
            rangeFreqLabel_->setText(kRangeFreqs[index]);
    }

    /* ── Metering ─────────────────────────────────────────────────── */

    void startMetering()
    {
        meterTimer_ = new QTimer(this);
        connect(meterTimer_, &QTimer::timeout, this, [this]() {
            if (!fx_) return;

            inputMeter_->setLevel(fx_->meterInputPeak());
            sustainMeter_->setLevel(fx_->meterGainReduction());
            outputMeter_->setLevel(fx_->meterOutputPeak());

            /* Refresh knob display text */
            for (int i = 0; i < kParamCount; ++i) {
                if (i == 4) continue;
                updateKnobText(i);
            }

            /* Sync vocal range combo */
            float rangeVal = fx_->paramValue(4);
            int rangeIndex = static_cast<int>(std::round(rangeVal * 4.0f));
            rangeIndex = (rangeIndex < 0 ? 0 : (rangeIndex > 4 ? 4 : rangeIndex));
            if (rangeCombo_->currentIndex() != rangeIndex) {
                rangeCombo_->blockSignals(true);
                rangeCombo_->setCurrentIndex(rangeIndex);
                rangeCombo_->blockSignals(false);
                updateRangeFreqLabel(rangeIndex);
            }

            /* Sync tube glow with enabled state */
            tubeGlow_->setActive(fx_->isEnabled());
        });
        meterTimer_->start(50);  /* 20 FPS */
    }

    /* ── Presets ───────────────────────────────────────────────────── */

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
            if (i == 4) continue;
            if (!knobs_[i]) continue;
            knobs_[i]->blockSignals(true);
            knobs_[i]->setValue(fx_->paramValue(i));
            knobs_[i]->blockSignals(false);
            updateKnobText(i);
        }

        /* Reload vocal range combo */
        float rangeVal = fx_->paramValue(4);
        int rangeIndex = static_cast<int>(std::round(rangeVal * 4.0f));
        rangeIndex = (rangeIndex < 0 ? 0 : (rangeIndex > 4 ? 4 : rangeIndex));
        rangeCombo_->blockSignals(true);
        rangeCombo_->setCurrentIndex(rangeIndex);
        rangeCombo_->blockSignals(false);
        updateRangeFreqLabel(rangeIndex);
    }

    /* ── Midnight blue theme ──────────────────────────────────────── */

    static constexpr const char* kCasterTubeStyle =
        /* Dialog background: deep midnight indigo gradient */
        "QDialog {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 #141e38, stop:1 #0e1428);"
        "  color: #D8E0F0;"
        "}"
        /* Group boxes: indigo panels with accent left border */
        "QGroupBox {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 rgba(20,30,56,0.9), stop:1 rgba(14,20,40,0.9));"
        "  border: 1px solid #1a2848;"
        "  border-left: 3px solid #4A9BD9;"
        "  border-radius: 4px;"
        "  margin-top: 14px;"
        "  padding: 10px 6px 6px 6px;"
        "  font-size: 11px;"
        "  color: #8899BB;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  subcontrol-position: top left;"
        "  padding: 2px 10px;"
        "  color: #4A9BD9;"
        "  background: #0e1428;"
        "  border: 1px solid #1a2848;"
        "  border-radius: 3px;"
        "  font-weight: bold;"
        "  letter-spacing: 2px;"
        "}"
        /* Labels */
        "QLabel { color: #D8E0F0; background: transparent; }"
        /* Buttons: midnight steel with blue accents */
        "QPushButton {"
        "  background: #1a2848;"
        "  color: #4A9BD9;"
        "  padding: 5px 14px;"
        "  border: 1px solid #243458;"
        "  border-radius: 4px;"
        "  font-size: 11px;"
        "  min-width: 60px;"
        "}"
        "QPushButton:hover {"
        "  background: #243458;"
        "  border-color: #4A9BD9;"
        "}"
        "QPushButton:pressed {"
        "  background: #4A9BD9;"
        "  color: #0e1428;"
        "}"
        /* Combo box: midnight with silver text */
        "QComboBox {"
        "  background: #1a2848;"
        "  color: #D8E0F0;"
        "  border: 1px solid #243458;"
        "  border-radius: 4px;"
        "  padding: 4px 8px;"
        "  font-size: 11px;"
        "}"
        "QComboBox::drop-down { border: none; width: 20px; }"
        "QComboBox QAbstractItemView {"
        "  background: #141e38;"
        "  border: 1px solid #1a2848;"
        "  color: #D8E0F0;"
        "  selection-background-color: #4A9BD9;"
        "  selection-color: #0e1428;"
        "}"
        /* Frame divider */
        "QFrame { background: transparent; }";

    /* ── Color constants (Boutique Blue palette) ──────────────────── */

    static inline const QColor kElectricBlue = QColor("#4A9BD9");
    static inline const QColor kAmber        = QColor("#D4A04A");
    static inline const QColor kDriveAmber   = QColor("#E8963A");
    static inline const QColor kGold         = QColor("#C8A86E");
    static inline const QColor kCoolSilver   = QColor("#8899BB");

    /* ── Data ─────────────────────────────────────────────────────── */

    static constexpr int kParamCount = 14;

    mc1dsp::DspEffect*      fx_            = nullptr;
    QTimer*                 meterTimer_    = nullptr;
    QComboBox*              presetCombo_   = nullptr;
    QComboBox*              rangeCombo_    = nullptr;
    QLabel*                 rangeFreqLabel_= nullptr;
    BlueTubeGlowWidget*     tubeGlow_      = nullptr;
    QVector<mc1dsp::Preset> presets_;

    RackKnob*  knobs_[kParamCount] = {};
    RackMeter* inputMeter_   = nullptr;
    RackMeter* sustainMeter_ = nullptr;
    RackMeter* outputMeter_  = nullptr;
};
