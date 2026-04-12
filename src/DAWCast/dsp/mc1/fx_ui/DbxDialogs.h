/*
 * Mcaster1DAWCast — MC1 Studios Family
 * fx_ui/DbxDialogs.h — flagship UIs for the dbx 500/600 series
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Custom QPainter-rendered hero widgets for each dbx plugin.
 * Visual identity: matte black chassis, yellow dbx logo (#ffcc00),
 * white labels, LED segment meters.
 *
 * Reuses BBEPluginDialog base from BBESonicSweetDialogs.h for the
 * shared layout (hero + knob bank + status row). Each dbx plugin
 * subclasses BBEHeroWidget with its own drawFace() rendering.
 */

#pragma once

#include "fx_ui/BBESonicSweetDialogs.h"
#include "patchbay/dsp/fx_dbx_676.h"
#include "patchbay/dsp/fx_dbx_580.h"
#include "patchbay/dsp/fx_dbx_266xs.h"
#include "patchbay/dsp/fx_dbx_560a.h"
#include "patchbay/dsp/fx_dbx_520.h"
#include "patchbay/dsp/fx_dbx_510.h"
#include "patchbay/dsp/fx_dbx_530.h"

/* ═══════════════════════════════════════════════════════════════════
 *  Helper: draw the dbx badge (replaces the BBE badge)
 * ═══════════════════════════════════════════════════════════════════ */
namespace dbx_ui {
inline void drawDbxBadge(QPainter& p, int x, int y) {
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0xff, 0xcc, 0x00));
    p.drawRoundedRect(QRect(x, y, 40, 18), 3, 3);
    QFont f("Helvetica Neue", 9, QFont::Black);
    p.setFont(f);
    p.setPen(QColor(0x0a, 0x0a, 0x0a));
    p.drawText(QRect(x, y, 40, 18), Qt::AlignCenter, "dbx");
}

inline void drawLedLadder(QPainter& p, int x, int y, int w, int h,
                           int segments, float level, bool vertical = true) {
    int segGap = 1;
    if (vertical) {
        int segH = (h - (segments - 1) * segGap) / segments;
        for (int s = 0; s < segments; ++s) {
            int sy = y + h - (s + 1) * (segH + segGap);
            float frac = static_cast<float>(s) / static_cast<float>(segments);
            bool lit = frac < level;
            QColor c;
            if (s >= segments - 2)
                c = lit ? QColor(0xff, 0x30, 0x30) : QColor(0x40, 0x10, 0x10);
            else if (s >= segments - 5)
                c = lit ? QColor(0xff, 0xcc, 0x00) : QColor(0x40, 0x38, 0x10);
            else
                c = lit ? QColor(0x00, 0xcc, 0x44) : QColor(0x10, 0x30, 0x10);
            p.setPen(Qt::NoPen);
            p.setBrush(c);
            p.drawRect(x, sy, w, segH);
        }
    } else {
        int segW = (w - (segments - 1) * segGap) / segments;
        for (int s = 0; s < segments; ++s) {
            int sx = x + s * (segW + segGap);
            float frac = static_cast<float>(s) / static_cast<float>(segments);
            bool lit = frac < level;
            QColor c = lit ? QColor(0xff, 0xcc, 0x00) : QColor(0x28, 0x24, 0x10);
            p.setPen(Qt::NoPen);
            p.setBrush(c);
            p.drawRect(sx, y, segW, h);
        }
    }
}
} // namespace dbx_ui

/* ═══════════════════════════════════════════════════════════════════
 *  676 TUBE MIC PREAMP — silver face + blue VU + tube glow
 * ═══════════════════════════════════════════════════════════════════ */
class Dbx676HeroWidget : public BBEHeroWidget {
    Q_OBJECT
protected:
    void drawFace(QPainter& p, int W, int H) override {
        QRect panel(70, 6, W - 80, H - 12);
        // Silver brushed face
        QLinearGradient silver(panel.topLeft(), panel.bottomLeft());
        silver.setColorAt(0.0, QColor(0x88, 0x90, 0x98));
        silver.setColorAt(0.5, QColor(0x70, 0x78, 0x82));
        silver.setColorAt(1.0, QColor(0x58, 0x60, 0x6a));
        p.setBrush(silver);
        p.setPen(QPen(QColor(0x40, 0x48, 0x50), 1));
        p.drawRoundedRect(panel, 4, 4);

        dbx_ui::drawDbxBadge(p, panel.left() + 8, panel.top() + 6);

        p.setPen(QColor(0x20, 0x20, 0x30));
        QFont mf("Helvetica Neue", 11, QFont::Bold);
        mf.setLetterSpacing(QFont::PercentageSpacing, 115);
        p.setFont(mf);
        p.drawText(panel.adjusted(56, 8, 0, 0), Qt::AlignTop | Qt::AlignLeft,
                   "676 TUBE MIC PREAMP");

        // Blue VU meter
        int vuX = panel.left() + 20, vuY = panel.top() + 34;
        int vuW = panel.width() / 3, vuH = H - 56;
        p.setBrush(QColor(0x08, 0x10, 0x28));
        p.setPen(QPen(QColor(0x30, 0x40, 0x60), 1));
        p.drawRoundedRect(vuX, vuY, vuW, vuH, 3, 3);
        float gain = m_vals[0]; // Gain param
        dbx_ui::drawLedLadder(p, vuX + 6, vuY + 4, 12, vuH - 8, 16, gain);
        p.setPen(QColor(0x40, 0x60, 0xb0));
        QFont lf("Menlo", 7);
        p.setFont(lf);
        p.drawText(vuX + 24, vuY + vuH / 2, "INPUT");

        // Tube glow
        float drive = m_vals[1];
        QPointF glowC(panel.right() - 40, panel.top() + 30);
        QRadialGradient glow(glowC, 16);
        float gi = 0.3f + drive * 0.7f;
        glow.setColorAt(0.0, QColor(255, 140, 40, static_cast<int>(200 * gi)));
        glow.setColorAt(0.6, QColor(255, 80, 20, static_cast<int>(60 * gi)));
        glow.setColorAt(1.0, QColor(0, 0, 0, 0));
        p.setBrush(glow);
        p.setPen(Qt::NoPen);
        p.drawEllipse(glowC, 18, 18);

        // EQ curve hint
        int eqX = vuX + vuW + 16, eqY = vuY, eqW = panel.width() - vuW - 80, eqH = vuH;
        p.setPen(QPen(QColor(0x40, 0x60, 0xb0, 80), 1));
        p.drawLine(eqX, eqY + eqH / 2, eqX + eqW, eqY + eqH / 2);
        float loG = (m_vals[3] - 0.5f) * eqH * 0.4f;
        float midG = (m_vals[5] - 0.5f) * eqH * 0.4f;
        float hiG = (m_vals[6] - 0.5f) * eqH * 0.4f;
        QPainterPath eq;
        float midY = eqY + eqH / 2.0f;
        eq.moveTo(eqX, midY - loG);
        eq.cubicTo(eqX + eqW * 0.25f, midY - loG * 0.5f,
                   eqX + eqW * 0.35f, midY - midG,
                   eqX + eqW * 0.5f, midY - midG);
        eq.cubicTo(eqX + eqW * 0.65f, midY - midG,
                   eqX + eqW * 0.8f, midY - hiG * 0.5f,
                   eqX + eqW, midY - hiG);
        p.setPen(QPen(QColor(0x40, 0x80, 0xff, 160), 2));
        p.setBrush(Qt::NoBrush);
        p.drawPath(eq);
    }
};

class Dbx676Dialog : public BBEPluginDialog {
    Q_OBJECT
public:
    Dbx676Dialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : BBEPluginDialog(fx, new Dbx676HeroWidget, "MC1 dbx 676 Tube Mic Preamp",
            mc1dsp::FxDbx676::kParamCount,
            QStringList{"GAIN:0", "DRIVE:1", "HPF:2", "LO:3", "MID F:4",
                        "MID:5", "HI:6", "THRESH:7", "RATIO:8", "TUBE:9",
                        "MIX:10", "OUTPUT:11"},
            parent) {}
};

/* ═══════════════════════════════════════════════════════════════════
 *  580 MIC PREAMP — yellow dbx logo + LED gain ladder
 * ═══════════════════════════════════════════════════════════════════ */
class Dbx580HeroWidget : public BBEHeroWidget {
    Q_OBJECT
protected:
    void drawFace(QPainter& p, int W, int H) override {
        QRect panel(70, 6, W - 80, H - 12);
        QLinearGradient dark(panel.topLeft(), panel.bottomLeft());
        dark.setColorAt(0.0, QColor(0x1a, 0x1a, 0x1e));
        dark.setColorAt(1.0, QColor(0x0a, 0x0a, 0x0e));
        p.setBrush(dark);
        p.setPen(QPen(QColor(0x30, 0x30, 0x38), 1));
        p.drawRoundedRect(panel, 4, 4);
        dbx_ui::drawDbxBadge(p, panel.left() + 8, panel.top() + 6);
        p.setPen(QColor(0xff, 0xcc, 0x00));
        QFont mf("Helvetica Neue", 11, QFont::Bold);
        mf.setLetterSpacing(QFont::PercentageSpacing, 115);
        p.setFont(mf);
        p.drawText(panel.adjusted(56, 8, 0, 0), Qt::AlignTop | Qt::AlignLeft, "580 MIC PREAMP");

        // Large gain ladder (center)
        int ladX = panel.center().x() - 60, ladY = panel.top() + 32;
        int ladW = 120, ladH = H - 52;
        dbx_ui::drawLedLadder(p, ladX, ladY, ladW, ladH, 20, m_vals[0], false);
        p.setPen(QColor(0xff, 0xcc, 0x00, 140));
        QFont lf("Menlo", 8, QFont::Bold);
        p.setFont(lf);
        p.drawText(ladX, ladY + ladH + 12, "GAIN");

        // Phantom/Pad/Phase indicators
        const char* indicators[] = {"+48V", "PAD", "PHASE"};
        float vals[] = {m_vals[3], m_vals[1], m_vals[2]};
        for (int i = 0; i < 3; ++i) {
            int ix = panel.right() - 80, iy = panel.top() + 30 + i * 22;
            bool on = vals[i] > 0.5f;
            p.setBrush(on ? QColor(0xff, 0xcc, 0x00) : QColor(0x28, 0x28, 0x28));
            p.setPen(QPen(QColor(0x40, 0x40, 0x40), 1));
            p.drawRoundedRect(ix, iy, 50, 16, 3, 3);
            p.setPen(on ? QColor(0x0a, 0x0a, 0x0a) : QColor(0x60, 0x60, 0x60));
            QFont sf("Menlo", 7, QFont::Bold);
            p.setFont(sf);
            p.drawText(QRect(ix, iy, 50, 16), Qt::AlignCenter, indicators[i]);
        }
    }
};

class Dbx580Dialog : public BBEPluginDialog {
    Q_OBJECT
public:
    Dbx580Dialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : BBEPluginDialog(fx, new Dbx580HeroWidget, "MC1 dbx 580 Mic Preamp",
            mc1dsp::FxDbx580::kParamCount,
            QStringList{"GAIN:0", "PAD:1", "PHASE:2", "48V:3", "HPF:4", "OUTPUT:5"},
            parent) {}
};

/* ═══════════════════════════════════════════════════════════════════
 *  266xs COMP/GATE — amber GR meter + OverEasy LED arc
 * ═══════════════════════════════════════════════════════════════════ */
class Dbx266xsHeroWidget : public BBEHeroWidget {
    Q_OBJECT
protected:
    void drawFace(QPainter& p, int W, int H) override {
        QRect panel(70, 6, W - 80, H - 12);
        QLinearGradient dark(panel.topLeft(), panel.bottomLeft());
        dark.setColorAt(0.0, QColor(0x18, 0x14, 0x10));
        dark.setColorAt(1.0, QColor(0x0a, 0x08, 0x06));
        p.setBrush(dark);
        p.setPen(QPen(QColor(0x38, 0x30, 0x28), 1));
        p.drawRoundedRect(panel, 4, 4);
        dbx_ui::drawDbxBadge(p, panel.left() + 8, panel.top() + 6);
        p.setPen(QColor(0xff, 0xaa, 0x44));
        QFont mf("Helvetica Neue", 11, QFont::Bold);
        p.setFont(mf);
        p.drawText(panel.adjusted(56, 8, 0, 0), Qt::AlignTop | Qt::AlignLeft, "266xs COMP/GATE");

        // GR meter
        int mX = panel.left() + 30, mY = panel.top() + 34, mH = H - 56;
        dbx_ui::drawLedLadder(p, mX, mY, 16, mH, 14, m_vals[0] * (1.0f - m_vals[1]));
        p.setPen(QColor(0xff, 0xaa, 0x44, 140));
        QFont lf("Menlo", 7);
        p.setFont(lf);
        p.drawText(mX, mY + mH + 10, "GR");

        // OverEasy arc
        float knee = m_vals[4];
        int arcCx = panel.center().x(), arcCy = panel.top() + H / 2;
        int arcR = 30;
        p.setPen(QPen(QColor(0x38, 0x30, 0x28), 3));
        p.setBrush(Qt::NoBrush);
        p.drawArc(QRect(arcCx - arcR, arcCy - arcR, arcR * 2, arcR * 2), 210 * 16, -240 * 16);
        int sweep = static_cast<int>(-240 * knee * 16);
        p.setPen(QPen(QColor(0xff, 0xaa, 0x44), 3));
        p.drawArc(QRect(arcCx - arcR, arcCy - arcR, arcR * 2, arcR * 2), 210 * 16, sweep);
        p.setPen(QColor(0xff, 0xaa, 0x44));
        QFont sf("Menlo", 7, QFont::Bold);
        p.setFont(sf);
        p.drawText(arcCx - 16, arcCy + 4, knee > 0.5f ? "AUTO" : "HARD");

        // Gate indicator
        float gateT = m_vals[5];
        bool gateOn = gateT > 0.05f;
        int gx = panel.right() - 70, gy = panel.top() + 30;
        p.setBrush(gateOn ? QColor(0xff, 0x44, 0x22) : QColor(0x28, 0x18, 0x10));
        p.setPen(QPen(QColor(0x40, 0x30, 0x20), 1));
        p.drawRoundedRect(gx, gy, 50, 16, 3, 3);
        p.setPen(gateOn ? QColor(0xff, 0xff, 0xff) : QColor(0x60, 0x50, 0x40));
        p.setFont(sf);
        p.drawText(QRect(gx, gy, 50, 16), Qt::AlignCenter, "GATE");
    }
};

class Dbx266xsDialog : public BBEPluginDialog {
    Q_OBJECT
public:
    Dbx266xsDialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : BBEPluginDialog(fx, new Dbx266xsHeroWidget, "MC1 dbx 266xs Compressor/Gate",
            mc1dsp::FxDbx266xs::kParamCount,
            QStringList{"THRESH:0", "RATIO:1", "ATTACK:2", "RELEASE:3",
                        "KNEE:4", "GATE T:5", "GATE R:6", "OUTPUT:7", "MIX:8"},
            parent) {}
};

/* ═══════════════════════════════════════════════════════════════════
 *  560A COMP/LIMITER — GR LED + threshold dual-stack
 * ═══════════════════════════════════════════════════════════════════ */
class Dbx560AHeroWidget : public BBEHeroWidget {
    Q_OBJECT
protected:
    void drawFace(QPainter& p, int W, int H) override {
        QRect panel(70, 6, W - 80, H - 12);
        QLinearGradient dark(panel.topLeft(), panel.bottomLeft());
        dark.setColorAt(0.0, QColor(0x14, 0x14, 0x18));
        dark.setColorAt(1.0, QColor(0x06, 0x06, 0x0a));
        p.setBrush(dark);
        p.setPen(QPen(QColor(0x30, 0x30, 0x38), 1));
        p.drawRoundedRect(panel, 4, 4);
        dbx_ui::drawDbxBadge(p, panel.left() + 8, panel.top() + 6);
        p.setPen(QColor(0xff, 0xcc, 0x00));
        QFont mf("Helvetica Neue", 11, QFont::Bold);
        p.setFont(mf);
        p.drawText(panel.adjusted(56, 8, 0, 0), Qt::AlignTop | Qt::AlignLeft, "560A COMP/LIMITER");

        int mX = panel.left() + 30, mY = panel.top() + 34, mH = H - 56;
        float thresh = m_vals[0];
        dbx_ui::drawLedLadder(p, mX, mY, 18, mH, 16, thresh * 0.8f);
        p.setPen(QColor(0xff, 0xcc, 0x00, 140));
        QFont lf("Menlo", 7);
        p.setFont(lf);
        p.drawText(mX, mY + mH + 10, "GR");

        // AUTO indicator
        bool autoOn = m_vals[6] > 0.5f;
        int ax = panel.right() - 60, ay = panel.top() + 30;
        p.setBrush(autoOn ? QColor(0xff, 0xcc, 0x00) : QColor(0x28, 0x28, 0x28));
        p.setPen(QPen(QColor(0x40, 0x40, 0x40), 1));
        p.drawRoundedRect(ax, ay, 44, 16, 3, 3);
        p.setPen(autoOn ? QColor(0x0a, 0x0a, 0x0a) : QColor(0x60, 0x60, 0x60));
        QFont sf("Menlo", 7, QFont::Bold);
        p.setFont(sf);
        p.drawText(QRect(ax, ay, 44, 16), Qt::AlignCenter, "AUTO");
    }
};

class Dbx560ADialog : public BBEPluginDialog {
    Q_OBJECT
public:
    Dbx560ADialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : BBEPluginDialog(fx, new Dbx560AHeroWidget, "MC1 dbx 560A Compressor/Limiter",
            mc1dsp::FxDbx560A::kParamCount,
            QStringList{"THRESH:0", "RATIO:1", "ATTACK:2", "RELEASE:3",
                        "KNEE:4", "SC HPF:5", "AUTO:6", "OUTPUT:7", "MIX:8"},
            parent) {}
};

/* ═══════════════════════════════════════════════════════════════════
 *  520 DE-ESSER — frequency LCD + sibilance histogram
 * ═══════════════════════════════════════════════════════════════════ */
class Dbx520HeroWidget : public BBEHeroWidget {
    Q_OBJECT
protected:
    void drawFace(QPainter& p, int W, int H) override {
        QRect panel(70, 6, W - 80, H - 12);
        QLinearGradient dark(panel.topLeft(), panel.bottomLeft());
        dark.setColorAt(0.0, QColor(0x14, 0x18, 0x20));
        dark.setColorAt(1.0, QColor(0x06, 0x08, 0x10));
        p.setBrush(dark);
        p.setPen(QPen(QColor(0x24, 0x30, 0x40), 1));
        p.drawRoundedRect(panel, 4, 4);
        dbx_ui::drawDbxBadge(p, panel.left() + 8, panel.top() + 6);
        p.setPen(QColor(0x00, 0xcc, 0xff));
        QFont mf("Helvetica Neue", 11, QFont::Bold);
        p.setFont(mf);
        p.drawText(panel.adjusted(56, 8, 0, 0), Qt::AlignTop | Qt::AlignLeft, "520 DE-ESSER");

        // Frequency LCD
        float freq = m_vals[0];
        int hz = static_cast<int>(1000.0f + freq * 11000.0f);
        int lcdX = panel.left() + 20, lcdY = panel.top() + 36;
        QRect lcd(lcdX, lcdY, 100, 28);
        p.setBrush(QColor(0x04, 0x0a, 0x14));
        p.setPen(QPen(QColor(0x20, 0x40, 0x60), 1));
        p.drawRoundedRect(lcd, 3, 3);
        p.setPen(QColor(0x00, 0xcc, 0xff));
        QFont df("Menlo", 14, QFont::Bold);
        p.setFont(df);
        QString freqStr = (hz >= 10000) ? QString("%1.%2k").arg(hz / 1000).arg((hz % 1000) / 100)
                                        : QString("%1 Hz").arg(hz);
        p.drawText(lcd, Qt::AlignCenter, freqStr);

        // Sibilance spectrum (center)
        int specX = lcdX + 120, specY = panel.top() + 32;
        int specW = panel.width() - 200, specH = H - 52;
        p.setBrush(QColor(0x04, 0x08, 0x12));
        p.setPen(QPen(QColor(0x18, 0x28, 0x3a), 1));
        p.drawRect(specX, specY, specW, specH);

        // Frequency marker line
        float normFreq = freq;
        int fxLine = specX + static_cast<int>(normFreq * specW);
        p.setPen(QPen(QColor(0x00, 0xcc, 0xff, 160), 2, Qt::DashLine));
        p.drawLine(fxLine, specY, fxLine, specY + specH);

        // Threshold line
        float thresh = m_vals[2];
        int thY = specY + static_cast<int>((1.0f - thresh) * specH);
        p.setPen(QPen(QColor(0xff, 0x44, 0x22, 140), 1, Qt::DashLine));
        p.drawLine(specX, thY, specX + specW, thY);

        // Listen indicator
        bool listen = m_vals[4] > 0.5f;
        if (listen) {
            p.setPen(QColor(0x00, 0xcc, 0xff));
            QFont lf("Menlo", 8, QFont::Bold);
            p.setFont(lf);
            p.drawText(panel.right() - 70, panel.top() + 30, "LISTEN");
        }
    }
};

class Dbx520Dialog : public BBEPluginDialog {
    Q_OBJECT
public:
    Dbx520Dialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : BBEPluginDialog(fx, new Dbx520HeroWidget, "MC1 dbx 520 De-Esser",
            mc1dsp::FxDbx520::kParamCount,
            QStringList{"FREQ:0", "RANGE:1", "THRESH:2", "WIDTH:3",
                        "LISTEN:4", "MIX:5", "OUTPUT:6"},
            parent) {}
};

/* ═══════════════════════════════════════════════════════════════════
 *  510 SUBHARMONIC SYNTHESIZER — energy bars + sine wave
 * ═══════════════════════════════════════════════════════════════════ */
class Dbx510HeroWidget : public BBEHeroWidget {
    Q_OBJECT
protected:
    void drawFace(QPainter& p, int W, int H) override {
        QRect panel(70, 6, W - 80, H - 12);
        QLinearGradient dark(panel.topLeft(), panel.bottomLeft());
        dark.setColorAt(0.0, QColor(0x10, 0x10, 0x1a));
        dark.setColorAt(1.0, QColor(0x04, 0x04, 0x0a));
        p.setBrush(dark);
        p.setPen(QPen(QColor(0x20, 0x20, 0x34), 1));
        p.drawRoundedRect(panel, 4, 4);
        dbx_ui::drawDbxBadge(p, panel.left() + 8, panel.top() + 6);
        p.setPen(QColor(0x88, 0x44, 0xff));
        QFont mf("Helvetica Neue", 11, QFont::Bold);
        p.setFont(mf);
        p.drawText(panel.adjusted(56, 8, 0, 0), Qt::AlignTop | Qt::AlignLeft, "510 SUBHARMONIC SYNTH");

        // Two band energy bars
        float b1 = m_vals[0], b2 = m_vals[1], synth = m_vals[2];
        int barX = panel.left() + 40, barY = panel.top() + 38, barH = H - 62;

        // Band 1 (24-36 Hz)
        int h1 = static_cast<int>(b1 * synth * barH);
        QLinearGradient g1(barX, barY + barH - h1, barX, barY + barH);
        g1.setColorAt(0.0, QColor(0x88, 0x44, 0xff, 220));
        g1.setColorAt(1.0, QColor(0x44, 0x22, 0xaa, 220));
        p.setBrush(g1);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(barX, barY + barH - h1, 30, h1, 2, 2);
        p.setPen(QColor(0x88, 0x44, 0xff, 160));
        QFont lf("Menlo", 7);
        p.setFont(lf);
        p.drawText(barX, barY + barH + 10, "24-36");

        // Band 2 (36-56 Hz)
        int h2 = static_cast<int>(b2 * synth * barH);
        QLinearGradient g2(barX + 50, barY + barH - h2, barX + 50, barY + barH);
        g2.setColorAt(0.0, QColor(0x44, 0x88, 0xff, 220));
        g2.setColorAt(1.0, QColor(0x22, 0x44, 0xaa, 220));
        p.setBrush(g2);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(barX + 50, barY + barH - h2, 30, h2, 2, 2);
        p.setPen(QColor(0x44, 0x88, 0xff, 160));
        p.setFont(lf);
        p.drawText(barX + 50, barY + barH + 10, "36-56");

        // Sine wave visualization
        int waveX = barX + 110, waveY = panel.top() + 36;
        int waveW = panel.width() - 180, waveH = H - 52;
        p.setBrush(QColor(0x04, 0x04, 0x0a));
        p.setPen(QPen(QColor(0x14, 0x14, 0x24), 1));
        p.drawRect(waveX, waveY, waveW, waveH);
        float midY = waveY + waveH / 2.0f;
        float amp = synth * waveH * 0.35f;
        QPainterPath wave;
        wave.moveTo(waveX, midY);
        for (int x = 0; x < waveW; ++x) {
            float t = static_cast<float>(x) / static_cast<float>(waveW);
            float y = midY - amp * std::sin(t * 6.28318f * 2.0f);
            if (x == 0) wave.moveTo(waveX + x, y);
            else wave.lineTo(waveX + x, y);
        }
        p.setPen(QPen(QColor(0x88, 0x44, 0xff, 180), 2));
        p.setBrush(Qt::NoBrush);
        p.drawPath(wave);
    }
};

class Dbx510Dialog : public BBEPluginDialog {
    Q_OBJECT
public:
    Dbx510Dialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : BBEPluginDialog(fx, new Dbx510HeroWidget, "MC1 dbx 510 Subharmonic Synthesizer",
            mc1dsp::FxDbx510::kParamCount,
            QStringList{"24-36 Hz:0", "36-56 Hz:1", "SYNTH:2",
                        "X-OVER:3", "TIGHT:4", "MIX:5", "OUTPUT:6"},
            parent) {}
};

/* ═══════════════════════════════════════════════════════════════════
 *  530 PARAMETRIC EQ — 4-band EQ curve
 * ═══════════════════════════════════════════════════════════════════ */
class Dbx530HeroWidget : public BBEHeroWidget {
    Q_OBJECT
protected:
    void drawFace(QPainter& p, int W, int H) override {
        QRect panel(70, 6, W - 80, H - 12);
        QLinearGradient dark(panel.topLeft(), panel.bottomLeft());
        dark.setColorAt(0.0, QColor(0x14, 0x18, 0x14));
        dark.setColorAt(1.0, QColor(0x06, 0x0a, 0x06));
        p.setBrush(dark);
        p.setPen(QPen(QColor(0x28, 0x38, 0x28), 1));
        p.drawRoundedRect(panel, 4, 4);
        dbx_ui::drawDbxBadge(p, panel.left() + 8, panel.top() + 6);
        p.setPen(QColor(0x44, 0xcc, 0x44));
        QFont mf("Helvetica Neue", 11, QFont::Bold);
        p.setFont(mf);
        p.drawText(panel.adjusted(56, 8, 0, 0), Qt::AlignTop | Qt::AlignLeft, "530 PARAMETRIC EQ");

        // 4-band EQ response curve
        int gX = panel.left() + 20, gY = panel.top() + 34;
        int gW = panel.width() - 40, gH = H - 54;
        p.setBrush(QColor(0x04, 0x08, 0x04));
        p.setPen(QPen(QColor(0x18, 0x28, 0x18), 1));
        p.drawRect(gX, gY, gW, gH);

        // Grid
        p.setPen(QPen(QColor(0x18, 0x28, 0x18), 1));
        for (int i = 1; i < 4; ++i)
            p.drawLine(gX + i * gW / 4, gY, gX + i * gW / 4, gY + gH);
        p.drawLine(gX, gY + gH / 2, gX + gW, gY + gH / 2);

        // EQ curve from the 4 band gains
        float lfG = (m_vals[2] - 0.5f) * 2.0f;   // LF gain (-1..+1)
        float lmfG = (m_vals[4] - 0.5f) * 2.0f;  // LMF gain
        float hmfG = (m_vals[7] - 0.5f) * 2.0f;  // HMF gain
        float hfG = (m_vals[10] - 0.5f) * 2.0f;  // HF gain
        float midY = gY + gH / 2.0f;
        float scale = gH * 0.35f;

        QPainterPath curve;
        curve.moveTo(gX, midY - lfG * scale);
        curve.cubicTo(gX + gW * 0.15f, midY - lfG * scale * 0.6f,
                      gX + gW * 0.20f, midY - lmfG * scale * 0.4f,
                      gX + gW * 0.30f, midY - lmfG * scale);
        curve.cubicTo(gX + gW * 0.40f, midY - lmfG * scale * 0.6f,
                      gX + gW * 0.50f, midY - hmfG * scale * 0.4f,
                      gX + gW * 0.65f, midY - hmfG * scale);
        curve.cubicTo(gX + gW * 0.75f, midY - hmfG * scale * 0.5f,
                      gX + gW * 0.85f, midY - hfG * scale * 0.6f,
                      gX + gW, midY - hfG * scale);

        p.setPen(QPen(QColor(0x44, 0xcc, 0x44, 200), 2.5));
        p.setBrush(Qt::NoBrush);
        p.drawPath(curve);

        // Fill
        QPainterPath fill(curve);
        fill.lineTo(gX + gW, gY + gH);
        fill.lineTo(gX, gY + gH);
        fill.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x44, 0xcc, 0x44, 20));
        p.drawPath(fill);

        // Freq labels
        p.setPen(QColor(0x44, 0xcc, 0x44, 120));
        QFont lf("Menlo", 6);
        p.setFont(lf);
        p.drawText(gX, gY + gH + 9, "20");
        p.drawText(gX + gW / 4, gY + gH + 9, "200");
        p.drawText(gX + gW / 2, gY + gH + 9, "1k");
        p.drawText(gX + 3 * gW / 4, gY + gH + 9, "5k");
        p.drawText(gX + gW - 18, gY + gH + 9, "20k");
    }
};

class Dbx530Dialog : public BBEPluginDialog {
    Q_OBJECT
public:
    Dbx530Dialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : BBEPluginDialog(fx, new Dbx530HeroWidget, "MC1 dbx 530 Parametric EQ",
            mc1dsp::FxDbx530::kParamCount,
            QStringList{"HPF:0", "LF F:1", "LF:2", "LMF F:3", "LMF:4", "LMF Q:5",
                        "HMF F:6", "HMF:7", "HMF Q:8", "HF F:9", "HF:10",
                        "LPF:11", "MIX:12", "OUTPUT:13"},
            parent) {}
};
