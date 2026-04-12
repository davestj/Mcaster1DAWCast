/*
 * Mcaster1DAWCast — MC1 Studios Family
 * fx_ui/BBESonicSweetDialogs.h — flagship UIs for the BBE Sonic Sweet bundle
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Custom QPainter-rendered hero widgets for each BBE plugin:
 *   - D82 Sonic Maximizer   (brushed aluminum + cyan LED meters)
 *   - H82 Harmonic Maximizer (harmonic comb visualization)
 *   - L82 Loudness Maximizer (3-band GR meters + sensitivity arc)
 *   - Mach 3 Bass           (woofer cone + harmonic spectrum)
 *
 * Each dialog uses the shared BBE visual chassis: matte black body,
 * yellow BBE accent, white labels — visually distinct from the MC1
 * studio plugins and the Lexicon series.
 */

#pragma once

#include "fx_ui/RackKnob.h"
#include "patchbay/dsp/dsp_effect.h"
#include "patchbay/dsp/preset_manager.h"

#include <QComboBox>
#include <QInputDialog>
#include <QMessageBox>
#include "patchbay/dsp/fx_bbe_d82.h"
#include "patchbay/dsp/fx_bbe_h82.h"
#include "patchbay/dsp/fx_bbe_l82.h"
#include "patchbay/dsp/fx_bbe_mach3.h"

#include <QDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QPaintEvent>
#include <QTimer>

/* ─────────────────────────────────────────────────────────────────
 *  BBEHeroWidget — base QPainter widget for the BBE visual chassis.
 *  Subclasses override drawFace() to render each plugin's unique
 *  faceplate graphics.
 * ───────────────────────────────────────────────────────────────── */
class BBEHeroWidget : public QWidget {
    Q_OBJECT

public:
    explicit BBEHeroWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(740, 120);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void setParamValues(const float* vals, int count) {
        for (int i = 0; i < count && i < 16; ++i)
            m_vals[i] = vals[i];
        update();
    }

protected:
    float m_vals[16] = {};

    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);

        const int W = width(), H = height();

        // BBE chassis background — matte black with subtle gradient
        QLinearGradient bg(0, 0, 0, H);
        bg.setColorAt(0.0, QColor(0x18, 0x18, 0x18));
        bg.setColorAt(0.5, QColor(0x0a, 0x0a, 0x0a));
        bg.setColorAt(1.0, QColor(0x04, 0x04, 0x04));
        p.fillRect(rect(), bg);

        // Top edge highlight
        p.setPen(QPen(QColor(0x40, 0x40, 0x40), 1));
        p.drawLine(0, 0, W, 0);

        // Bottom edge shadow
        p.setPen(QPen(QColor(0x00, 0x00, 0x00), 1));
        p.drawLine(0, H - 1, W, H - 1);

        // BBE logo badge (top-left)
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0xff, 0xcc, 0x00));
        p.drawRoundedRect(QRect(12, 8, 48, 18), 3, 3);
        QFont logoFont("Helvetica Neue", 10, QFont::Black);
        p.setFont(logoFont);
        p.setPen(QColor(0x0a, 0x0a, 0x0a));
        p.drawText(QRect(12, 8, 48, 18), Qt::AlignCenter, "BBE");

        // Delegate face rendering to subclass
        drawFace(p, W, H);
    }

    virtual void drawFace(QPainter& p, int W, int H) = 0;
};

/* ─────────────────────────────────────────────────────────────────
 *  BBEPluginDialog — shared base dialog for all BBE plugins.
 *  Renders the hero widget + knob banks + status row.
 * ───────────────────────────────────────────────────────────────── */
class BBEPluginDialog : public QDialog {
    Q_OBJECT

public:
    BBEPluginDialog(mc1dsp::DspEffect* fx,
                    BBEHeroWidget* hero,
                    const QString& title,
                    int paramCount,
                    const QStringList& knobSpecs,
                    QWidget* parent = nullptr)
        : QDialog(parent), m_fx(fx), m_hero(hero), m_paramCount(paramCount)
    {
        setWindowTitle(title);
        resize(880, 440);
        setMinimumSize(640, 340);
        setStyleSheet(
            "QDialog { background: #0a0a0a; color: #e0e0e0; }"
            "QGroupBox {"
            "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
            "      stop:0 #1a1a1a, stop:1 #0e0e0e);"
            "  border: 1px solid #333; border-left: 3px solid #ffcc00;"
            "  border-radius: 6px; margin-top: 14px;"
            "  padding: 10px 6px 6px 6px; font-size: 11px; color: #888;"
            "}"
            "QGroupBox::title {"
            "  subcontrol-origin: margin; subcontrol-position: top left;"
            "  padding: 2px 8px; color: #ffcc00; font-weight: bold;"
            "}"
            "QLabel { color: #e0e0e0; }"
            "QComboBox {"
            "  background: #1a1a1a; color: #e0e0e0;"
            "  border: 1px solid #333; border-radius: 3px;"
            "  padding: 4px 8px; min-width: 160px;"
            "}"
            "QComboBox QAbstractItemView { background: #0a0a0a; color: #e0e0e0; }"
            "QPushButton {"
            "  background: #1a1a1a; color: #e0e0e0;"
            "  border: 1px solid #333; border-radius: 3px;"
            "  padding: 6px 16px; min-width: 60px;"
            "}"
            "QPushButton:hover { background: #282828; }"
            "QPushButton:pressed { background: #ffcc00; color: #0a0a0a; }"
        );

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(12, 8, 12, 8);
        root->setSpacing(6);

        // Header: title + preset selector
        auto* header = new QHBoxLayout;
        auto* titleLabel = new QLabel(title.toUpper());
        titleLabel->setStyleSheet(
            "font-size: 15px; font-weight: bold; color: #ffcc00; letter-spacing: 4px;");
        header->addWidget(titleLabel);
        header->addStretch();

        auto* presetLabel = new QLabel("PRESET:");
        presetLabel->setStyleSheet("color: #888; font-size: 10px; font-weight: bold;");
        header->addWidget(presetLabel);
        m_presetCombo = new QComboBox;
        m_presetCombo->setToolTip("Load a preset");
        connect(m_presetCombo, QOverload<int>::of(&QComboBox::activated),
                this, &BBEPluginDialog::onPresetSelected);
        header->addWidget(m_presetCombo);

        auto* saveBtn = new QPushButton("SAVE");
        saveBtn->setFixedWidth(50);
        connect(saveBtn, &QPushButton::clicked, this, &BBEPluginDialog::onSavePreset);
        header->addWidget(saveBtn);

        auto* delBtn = new QPushButton("DEL");
        delBtn->setFixedWidth(40);
        connect(delBtn, &QPushButton::clicked, this, &BBEPluginDialog::onDeletePreset);
        header->addWidget(delBtn);

        root->addLayout(header);

        // Hero widget
        root->addWidget(m_hero, 1);

        // Knob bank
        auto* group = new QGroupBox("CONTROLS");
        auto* knobLayout = new QHBoxLayout(group);
        knobLayout->setSpacing(2);
        for (const QString& spec : knobSpecs) {
            auto parts = spec.split(':');
            if (parts.size() != 2) continue;
            int paramIdx = parts[1].toInt();
            if (paramIdx < 0 || paramIdx >= m_paramCount) continue;
            auto* k = new RackKnob;
            k->setTitle(parts[0]);
            k->setFixedSize(78, 110);
            connect(k, &RackKnob::valueChanged, this, [this, paramIdx](float v) {
                if (m_fx) m_fx->setParamValue(paramIdx, v);
                syncHero();
            });
            m_knobs.append({paramIdx, k});
            knobLayout->addWidget(k);
        }
        root->addWidget(group);

        // Bottom row
        auto* bottom = new QHBoxLayout;
        auto* bypass = new QPushButton("BYPASS");
        bypass->setCheckable(true);
        connect(bypass, &QPushButton::toggled, this, [this](bool on) {
            if (m_fx) m_fx->setBypassed(on);
        });
        bottom->addWidget(bypass);

        auto* resetBtn = new QPushButton("RESET");
        connect(resetBtn, &QPushButton::clicked, this, [this]() {
            if (m_fx) { m_fx->reset(); loadFromEffect(); }
        });
        bottom->addWidget(resetBtn);

        bottom->addStretch();
        m_statusLabel = new QLabel("READY");
        m_statusLabel->setStyleSheet(
            "QLabel { color: #ffcc00; font-family: 'Menlo'; font-size: 11px; "
            "background: #111; padding: 4px 12px; border: 1px solid #333; "
            "border-radius: 3px; }");
        bottom->addWidget(m_statusLabel);
        bottom->addStretch();

        auto* close = new QPushButton("CLOSE");
        connect(close, &QPushButton::clicked, this, &QDialog::close);
        bottom->addWidget(close);
        root->addLayout(bottom);

        loadFromEffect();
        refreshPresetList();

        m_poll = new QTimer(this);
        m_poll->setInterval(120);
        connect(m_poll, &QTimer::timeout, this, &BBEPluginDialog::pollDisplay);
        m_poll->start();
    }

private:
    struct KnobEntry { int paramIdx; RackKnob* knob; };

    void loadFromEffect() {
        if (!m_fx) return;
        for (auto& k : m_knobs)
            k.knob->setValue(m_fx->paramValue(k.paramIdx));
        syncHero();
    }

    void syncHero() {
        if (!m_fx || !m_hero) return;
        float vals[16] = {};
        for (int i = 0; i < m_paramCount && i < 16; ++i)
            vals[i] = m_fx->paramValue(i);
        m_hero->setParamValues(vals, m_paramCount);
    }

    void refreshPresetList() {
        if (!m_fx || !m_presetCombo) return;
        m_presetCombo->clear();
        m_presetCombo->addItem("-- Default --");
        m_presets = mc1dsp::PresetManager::listPresets(
            QString::fromLatin1(m_fx->id()));
        for (const auto& p : m_presets)
            m_presetCombo->addItem(
                (p.isFactory ? QString("[F] ") : QString()) + p.name);
    }

    void onPresetSelected(int index) {
        if (index <= 0 || !m_fx) return;
        int pi = index - 1;
        if (pi >= 0 && pi < m_presets.size()) {
            mc1dsp::PresetManager::applyPreset(m_presets[pi], m_fx);
            loadFromEffect();
            if (m_statusLabel)
                m_statusLabel->setText(QString("Loaded: %1").arg(m_presets[pi].name));
        }
    }

    void onSavePreset() {
        if (!m_fx) return;
        bool ok = false;
        QString name = QInputDialog::getText(
            this, "Save Preset", "Preset name:",
            QLineEdit::Normal, QString(), &ok);
        if (!ok || name.trimmed().isEmpty()) return;
        auto preset = mc1dsp::PresetManager::capturePreset(m_fx, name.trimmed());
        if (mc1dsp::PresetManager::savePreset(preset)) {
            refreshPresetList();
            if (m_statusLabel)
                m_statusLabel->setText(QString("Saved: %1").arg(name));
        }
    }

    void onDeletePreset() {
        if (!m_presetCombo) return;
        int idx = m_presetCombo->currentIndex() - 1;
        if (idx < 0 || idx >= m_presets.size()) return;
        if (m_presets[idx].isFactory) {
            QMessageBox::warning(this, "Cannot Delete",
                "Factory presets cannot be deleted.");
            return;
        }
        mc1dsp::PresetManager::deletePreset(m_presets[idx].filePath);
        refreshPresetList();
        if (m_statusLabel) m_statusLabel->setText("Preset deleted");
    }

    void pollDisplay() {
        if (!m_fx) return;
        for (auto& k : m_knobs)
            k.knob->setToolTip(
                QString("%1: %2")
                    .arg(QString::fromLatin1(m_fx->paramName(k.paramIdx)),
                         QString::fromStdString(m_fx->paramDisplayValue(k.paramIdx))));
    }

    mc1dsp::DspEffect*       m_fx = nullptr;
    BBEHeroWidget*           m_hero = nullptr;
    int                      m_paramCount = 0;
    QList<KnobEntry>         m_knobs;
    QTimer*                  m_poll = nullptr;
    QComboBox*               m_presetCombo = nullptr;
    QLabel*                  m_statusLabel = nullptr;
    QVector<mc1dsp::Preset>  m_presets;
};

/* ═══════════════════════════════════════════════════════════════════
 *  D82 SONIC MAXIMIZER — brushed aluminum face + 3-band response
 * ═══════════════════════════════════════════════════════════════════ */
class D82HeroWidget : public BBEHeroWidget {
    Q_OBJECT
protected:
    void drawFace(QPainter& p, int W, int H) override {
        // Brushed aluminum panel
        QRect panel(70, 6, W - 80, H - 12);
        QLinearGradient alum(panel.topLeft(), panel.bottomLeft());
        alum.setColorAt(0.0, QColor(0x60, 0x68, 0x70));
        alum.setColorAt(0.3, QColor(0x50, 0x58, 0x60));
        alum.setColorAt(0.7, QColor(0x48, 0x50, 0x58));
        alum.setColorAt(1.0, QColor(0x38, 0x40, 0x48));
        p.setBrush(alum);
        p.setPen(QPen(QColor(0x30, 0x38, 0x40), 1));
        p.drawRoundedRect(panel, 4, 4);

        // Brush texture (horizontal lines)
        p.setPen(QPen(QColor(255, 255, 255, 12), 1));
        for (int y = panel.top() + 4; y < panel.bottom() - 2; y += 3)
            p.drawLine(panel.left() + 4, y, panel.right() - 4, y);

        // Model name
        p.setPen(QColor(0xff, 0xcc, 0x00));
        QFont mf("Helvetica Neue", 13, QFont::Bold);
        mf.setLetterSpacing(QFont::PercentageSpacing, 120);
        p.setFont(mf);
        p.drawText(panel.adjusted(16, 8, 0, 0), Qt::AlignTop | Qt::AlignLeft,
                   "D82 SONIC MAXIMIZER");

        // 3-band response visualization
        float loC = m_vals[0];  // Lo Contour
        float proc = m_vals[1]; // Process
        int graphX = panel.left() + 16;
        int graphY = panel.top() + 36;
        int graphW = panel.width() - 32;
        int graphH = H - 56;

        // Grid
        p.setPen(QPen(QColor(0, 200, 255, 30), 1));
        for (int i = 0; i <= 4; ++i) {
            int x = graphX + i * graphW / 4;
            p.drawLine(x, graphY, x, graphY + graphH);
        }
        p.drawLine(graphX, graphY + graphH / 2, graphX + graphW, graphY + graphH / 2);

        // Response curve — boosted lows + flat mid + boosted highs
        QPainterPath curve;
        float midY = graphY + graphH / 2.0f;
        float loBoost = loC * graphH * 0.35f;
        float hiBoost = proc * graphH * 0.35f;
        curve.moveTo(graphX, midY - loBoost);
        curve.cubicTo(graphX + graphW * 0.15f, midY - loBoost * 0.8f,
                      graphX + graphW * 0.25f, midY,
                      graphX + graphW * 0.40f, midY);
        curve.cubicTo(graphX + graphW * 0.55f, midY,
                      graphX + graphW * 0.70f, midY - hiBoost * 0.5f,
                      graphX + graphW, midY - hiBoost);
        p.setPen(QPen(QColor(0, 200, 255, 200), 2.5));
        p.setBrush(Qt::NoBrush);
        p.drawPath(curve);

        // Fill under curve
        QPainterPath fill(curve);
        fill.lineTo(graphX + graphW, graphY + graphH);
        fill.lineTo(graphX, graphY + graphH);
        fill.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 200, 255, 25));
        p.drawPath(fill);

        // Freq labels
        p.setPen(QColor(0, 200, 255, 140));
        QFont sf("Menlo", 7);
        p.setFont(sf);
        p.drawText(graphX, graphY + graphH + 10, "20");
        p.drawText(graphX + graphW / 4, graphY + graphH + 10, "200");
        p.drawText(graphX + graphW / 2, graphY + graphH + 10, "1k");
        p.drawText(graphX + 3 * graphW / 4, graphY + graphH + 10, "5k");
        p.drawText(graphX + graphW - 16, graphY + graphH + 10, "20k");
    }
};

class BbeD82Dialog : public BBEPluginDialog {
    Q_OBJECT
public:
    BbeD82Dialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : BBEPluginDialog(fx, new D82HeroWidget, "MC1 BBE D82 Sonic Maximizer",
            mc1dsp::FxBbeD82::kParamCount,
            QStringList{"LO CONTOUR:0", "PROCESS:1", "DRIVE:2", "STEREO:3",
                        "LO X-OVER:4", "HI X-OVER:5", "MIX:6", "OUTPUT:7"},
            parent) {}
};

/* ═══════════════════════════════════════════════════════════════════
 *  H82 HARMONIC MAXIMIZER — harmonic comb visualization
 * ═══════════════════════════════════════════════════════════════════ */
class H82HeroWidget : public BBEHeroWidget {
    Q_OBJECT
protected:
    void drawFace(QPainter& p, int W, int H) override {
        // Warm dark panel
        QRect panel(70, 6, W - 80, H - 12);
        QLinearGradient warm(panel.topLeft(), panel.bottomLeft());
        warm.setColorAt(0.0, QColor(0x28, 0x1a, 0x10));
        warm.setColorAt(1.0, QColor(0x14, 0x0c, 0x06));
        p.setBrush(warm);
        p.setPen(QPen(QColor(0x40, 0x28, 0x14), 1));
        p.drawRoundedRect(panel, 4, 4);

        // Model name
        p.setPen(QColor(0xff, 0xaa, 0x44));
        QFont mf("Helvetica Neue", 13, QFont::Bold);
        mf.setLetterSpacing(QFont::PercentageSpacing, 120);
        p.setFont(mf);
        p.drawText(panel.adjusted(16, 8, 0, 0), Qt::AlignTop | Qt::AlignLeft,
                   "H82 HARMONIC MAXIMIZER");

        // Harmonic comb bars — vertical bars at fundamental, 2f, 3f, 4f, 5f
        float proc = m_vals[1];     // Process (drive)
        float harm = m_vals[2];     // Harmonics (even/odd)
        int barX = panel.left() + 30;
        int barY = panel.top() + 36;
        int barH = H - 56;
        int barSpacing = (panel.width() - 60) / 6;

        const char* labels[] = {"f", "2f", "3f", "4f", "5f", "6f"};
        for (int i = 0; i < 6; ++i) {
            // Heights: fundamental full, harmonics decay with even/odd weighting
            float amplitude;
            if (i == 0) {
                amplitude = 0.9f;
            } else {
                bool isEven = (i % 2 == 1); // 2f, 4f, 6f
                float evenWeight = 1.0f - harm;
                float oddWeight = harm;
                float weight = isEven ? evenWeight : oddWeight;
                amplitude = proc * weight * (0.7f / (1.0f + i * 0.3f));
            }

            int h = static_cast<int>(amplitude * barH);
            int x = barX + i * barSpacing;

            // Gradient bar
            QLinearGradient bg(x, barY + barH - h, x, barY + barH);
            QColor barCol = (i % 2 == 1)
                ? QColor(0xff, 0x88, 0x22, 200)   // even harmonics: warm orange
                : QColor(0xff, 0xcc, 0x00, 200);   // odd: bright yellow
            bg.setColorAt(0.0, barCol.lighter(140));
            bg.setColorAt(1.0, barCol);
            p.setBrush(bg);
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(QRect(x, barY + barH - h, 16, h), 2, 2);

            // Label
            p.setPen(QColor(0xff, 0xaa, 0x44, 160));
            QFont lf("Menlo", 7);
            p.setFont(lf);
            p.drawText(x, barY + barH + 10, labels[i]);
        }

        // Tube glow indicator (top right)
        QPointF glowCenter(panel.right() - 30, panel.top() + 24);
        QRadialGradient glow(glowCenter, 12);
        float glowIntensity = 0.3f + proc * 0.7f;
        glow.setColorAt(0.0, QColor(255, 140, 40, static_cast<int>(200 * glowIntensity)));
        glow.setColorAt(0.5, QColor(255, 80, 20, static_cast<int>(80 * glowIntensity)));
        glow.setColorAt(1.0, QColor(0, 0, 0, 0));
        p.setBrush(glow);
        p.setPen(Qt::NoPen);
        p.drawEllipse(glowCenter, 14, 14);
    }
};

class BbeH82Dialog : public BBEPluginDialog {
    Q_OBJECT
public:
    BbeH82Dialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : BBEPluginDialog(fx, new H82HeroWidget, "MC1 BBE H82 Harmonic Maximizer",
            mc1dsp::FxBbeH82::kParamCount,
            QStringList{"LO CONTOUR:0", "PROCESS:1", "HARMONICS:2",
                        "LO RESTORE:3", "MIX:4", "OUTPUT:5"},
            parent) {}
};

/* ═══════════════════════════════════════════════════════════════════
 *  L82 LOUDNESS MAXIMIZER — 3 GR meters + sensitivity arc
 * ═══════════════════════════════════════════════════════════════════ */
class L82HeroWidget : public BBEHeroWidget {
    Q_OBJECT
protected:
    void drawFace(QPainter& p, int W, int H) override {
        QRect panel(70, 6, W - 80, H - 12);
        QLinearGradient dark(panel.topLeft(), panel.bottomLeft());
        dark.setColorAt(0.0, QColor(0x14, 0x14, 0x1a));
        dark.setColorAt(1.0, QColor(0x06, 0x06, 0x0a));
        p.setBrush(dark);
        p.setPen(QPen(QColor(0x30, 0x30, 0x40), 1));
        p.drawRoundedRect(panel, 4, 4);

        // Model name
        p.setPen(QColor(0xff, 0xcc, 0x00));
        QFont mf("Helvetica Neue", 13, QFont::Bold);
        mf.setLetterSpacing(QFont::PercentageSpacing, 120);
        p.setFont(mf);
        p.drawText(panel.adjusted(16, 8, 0, 0), Qt::AlignTop | Qt::AlignLeft,
                   "L82 LOUDNESS MAXIMIZER");

        // 3-band GR meters (vertical LED ladders)
        float sensitivity = m_vals[0];
        float loT = m_vals[1], midT = m_vals[2], hiT = m_vals[3];
        const char* bandLabels[] = {"LO", "MID", "HI"};
        float thresholds[] = {loT, midT, hiT};
        int meterX = panel.left() + 40;
        int meterY = panel.top() + 36;
        int meterH = H - 60;
        int meterSpacing = 80;

        for (int b = 0; b < 3; ++b) {
            int x = meterX + b * meterSpacing;
            int segments = 12;
            int segH = (meterH - segments) / segments;

            for (int s = 0; s < segments; ++s) {
                int sy = meterY + s * (segH + 1);
                // GR simulation: more segments lit = more reduction
                float grLevel = sensitivity * (1.0f - thresholds[b]) * 0.8f;
                float segFrac = static_cast<float>(segments - s) / static_cast<float>(segments);
                bool lit = segFrac <= grLevel;

                QColor c;
                if (s < 3)
                    c = lit ? QColor(0xff, 0x30, 0x30) : QColor(0x40, 0x10, 0x10);
                else if (s < 6)
                    c = lit ? QColor(0xff, 0xcc, 0x00) : QColor(0x40, 0x38, 0x10);
                else
                    c = lit ? QColor(0x00, 0xcc, 0x44) : QColor(0x10, 0x38, 0x10);

                p.setPen(Qt::NoPen);
                p.setBrush(c);
                p.drawRect(x, sy, 24, segH);
            }

            // Band label
            p.setPen(QColor(0xff, 0xcc, 0x00, 180));
            QFont lf("Menlo", 8, QFont::Bold);
            p.setFont(lf);
            p.drawText(x, meterY + meterH + 12, bandLabels[b]);
        }

        // Sensitivity arc (right side)
        int arcCx = panel.right() - 100;
        int arcCy = panel.top() + H / 2;
        int arcR = 36;
        p.setPen(QPen(QColor(0x30, 0x30, 0x40), 3));
        p.setBrush(Qt::NoBrush);
        p.drawArc(QRect(arcCx - arcR, arcCy - arcR, arcR * 2, arcR * 2),
                  210 * 16, -240 * 16);

        // Filled portion
        int sweepAngle = static_cast<int>(-240 * sensitivity * 16);
        p.setPen(QPen(QColor(0xff, 0xcc, 0x00), 3));
        p.drawArc(QRect(arcCx - arcR, arcCy - arcR, arcR * 2, arcR * 2),
                  210 * 16, sweepAngle);

        // Sensitivity label
        p.setPen(QColor(0xff, 0xcc, 0x00));
        QFont sf("Menlo", 7, QFont::Bold);
        p.setFont(sf);
        QFontMetrics fm(sf);
        QString sensText = QString::number(static_cast<int>(sensitivity * 10));
        p.drawText(arcCx - fm.horizontalAdvance(sensText) / 2, arcCy + 4, sensText);
        p.setPen(QColor(0x88, 0x88, 0x88));
        p.drawText(arcCx - fm.horizontalAdvance("SENS") / 2, arcCy + 16, "SENS");
    }
};

class BbeL82Dialog : public BBEPluginDialog {
    Q_OBJECT
public:
    BbeL82Dialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : BBEPluginDialog(fx, new L82HeroWidget, "MC1 BBE L82 Loudness Maximizer",
            mc1dsp::FxBbeL82::kParamCount,
            QStringList{"SENSITIVITY:0", "LO:1", "MID:2", "HI:3",
                        "RELEASE:4", "CEILING:5", "MIX:6", "OUTPUT:7"},
            parent) {}
};

/* ═══════════════════════════════════════════════════════════════════
 *  MACH 3 BASS — woofer cone + harmonic spectrum
 * ═══════════════════════════════════════════════════════════════════ */
class Mach3HeroWidget : public BBEHeroWidget {
    Q_OBJECT
protected:
    void drawFace(QPainter& p, int W, int H) override {
        QRect panel(70, 6, W - 80, H - 12);
        QLinearGradient dark(panel.topLeft(), panel.bottomLeft());
        dark.setColorAt(0.0, QColor(0x10, 0x14, 0x20));
        dark.setColorAt(1.0, QColor(0x06, 0x08, 0x10));
        p.setBrush(dark);
        p.setPen(QPen(QColor(0x20, 0x30, 0x50), 1));
        p.drawRoundedRect(panel, 4, 4);

        // Model name
        p.setPen(QColor(0xff, 0xcc, 0x00));
        QFont mf("Helvetica Neue", 13, QFont::Bold);
        mf.setLetterSpacing(QFont::PercentageSpacing, 120);
        p.setFont(mf);
        p.drawText(panel.adjusted(16, 8, 0, 0), Qt::AlignTop | Qt::AlignLeft,
                   "MACH 3 BASS");

        // Woofer cone (left side)
        float freq = m_vals[0];  // Frequency
        float drive = m_vals[2]; // Drive
        int cx = panel.left() + 80;
        int cy = panel.top() + H / 2 + 4;
        int outerR = 38;
        int innerR = 14;

        // Surround ring
        QRadialGradient surround(cx, cy, outerR + 4);
        surround.setColorAt(0.0, QColor(0x20, 0x28, 0x38));
        surround.setColorAt(0.8, QColor(0x14, 0x1a, 0x28));
        surround.setColorAt(1.0, QColor(0x0a, 0x0e, 0x18));
        p.setBrush(surround);
        p.setPen(QPen(QColor(0x30, 0x40, 0x60), 1));
        p.drawEllipse(QPointF(cx, cy), outerR, outerR);

        // Cone
        QRadialGradient cone(cx - 4, cy - 4, outerR - 6);
        cone.setColorAt(0.0, QColor(0x44, 0x3c, 0x30));
        cone.setColorAt(0.6, QColor(0x30, 0x28, 0x20));
        cone.setColorAt(1.0, QColor(0x20, 0x1a, 0x14));
        p.setBrush(cone);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(cx, cy), outerR - 6, outerR - 6);

        // Dust cap
        QRadialGradient cap(cx, cy, innerR);
        float capBright = 0.5f + drive * 0.5f;
        cap.setColorAt(0.0, QColor(static_cast<int>(80 * capBright),
                                    static_cast<int>(70 * capBright),
                                    static_cast<int>(50 * capBright)));
        cap.setColorAt(1.0, QColor(0x20, 0x1a, 0x14));
        p.setBrush(cap);
        p.drawEllipse(QPointF(cx, cy), innerR, innerR);

        // Frequency selector indicator below woofer
        p.setPen(QColor(0xff, 0xcc, 0x00));
        QFont ff("Menlo", 9, QFont::Bold);
        p.setFont(ff);
        int hz = static_cast<int>(40.0f + freq * 160.0f);
        p.drawText(QRect(cx - 30, cy + outerR + 6, 60, 14), Qt::AlignCenter,
                   QString("%1 Hz").arg(hz));

        // Harmonic spectrum (right side)
        int specX = panel.left() + 180;
        int specY = panel.top() + 32;
        int specW = panel.width() - 200;
        int specH = H - 52;

        // Spectrum background
        p.setPen(QPen(QColor(0x20, 0x30, 0x50), 1));
        p.setBrush(QColor(0x06, 0x0a, 0x14));
        p.drawRect(specX, specY, specW, specH);

        // Grid lines
        p.setPen(QPen(QColor(0x18, 0x24, 0x38), 1));
        for (int i = 1; i < 4; ++i)
            p.drawLine(specX + i * specW / 4, specY, specX + i * specW / 4, specY + specH);
        p.drawLine(specX, specY + specH / 2, specX + specW, specY + specH / 2);

        // Harmonic bars — f, 2f, 3f, 4f at positions proportional to frequency
        float bassBoost = m_vals[1];
        for (int h = 1; h <= 5; ++h) {
            float hzVal = (40.0f + freq * 160.0f) * h;
            // Map frequency to x position (log scale approximation)
            float logPos = std::log2(hzVal / 20.0f) / std::log2(20000.0f / 20.0f);
            int bx = specX + static_cast<int>(logPos * specW);
            if (bx < specX || bx > specX + specW - 8) continue;

            float amp = (h == 1) ? (0.6f + bassBoost * 0.4f)
                                 : drive * (0.6f / h);
            int bh = static_cast<int>(amp * specH * 0.8f);

            QLinearGradient bg(bx, specY + specH - bh, bx, specY + specH);
            QColor barCol = (h == 1) ? QColor(0x00, 0x88, 0xff, 220)
                                     : QColor(0xff, 0xcc, 0x00, 180);
            bg.setColorAt(0.0, barCol.lighter(130));
            bg.setColorAt(1.0, barCol);
            p.setBrush(bg);
            p.setPen(Qt::NoPen);
            p.drawRect(bx, specY + specH - bh, 6, bh);
        }

        // Freq labels
        p.setPen(QColor(0x40, 0x60, 0x90));
        QFont lf("Menlo", 6);
        p.setFont(lf);
        p.drawText(specX, specY + specH + 9, "20");
        p.drawText(specX + specW / 2 - 6, specY + specH + 9, "600");
        p.drawText(specX + specW - 18, specY + specH + 9, "20k");
    }
};

class BbeMach3Dialog : public BBEPluginDialog {
    Q_OBJECT
public:
    BbeMach3Dialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : BBEPluginDialog(fx, new Mach3HeroWidget, "MC1 BBE Mach 3 Bass",
            mc1dsp::FxBbeMach3::kParamCount,
            QStringList{"FREQUENCY:0", "BASS BOOST:1", "DRIVE:2",
                        "TIGHTNESS:3", "MIX:4", "OUTPUT:5"},
            parent) {}
};
