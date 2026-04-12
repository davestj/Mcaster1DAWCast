/*
 * Mcaster1DAWCast — MC1 Studios Family
 * fx_ui/FlagshipDialogs.h — Vocal Producer Pro + Topline Key Finder UIs
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "fx_ui/BBESonicSweetDialogs.h"   // reuse BBEPluginDialog base + BBEHeroWidget
#include "patchbay/dsp/fx_mc1_vocal_producer.h"
#include "patchbay/dsp/fx_mc1_key_finder.h"

/* ═══════════════════════════════════════════════════════════════════
 *  VOCAL PRODUCER PRO — pitch correction arc + channel strip meters
 * ═══════════════════════════════════════════════════════════════════ */
class VocalProducerHeroWidget : public BBEHeroWidget {
    Q_OBJECT
protected:
    void drawFace(QPainter& p, int W, int H) override {
        QRect panel(70, 6, W - 80, H - 12);
        // Deep navy gradient (mc1 brand)
        QLinearGradient bg(panel.topLeft(), panel.bottomLeft());
        bg.setColorAt(0.0, QColor(0x0d, 0x23, 0x42));
        bg.setColorAt(1.0, QColor(0x06, 0x10, 0x22));
        p.setBrush(bg);
        p.setPen(QPen(QColor(0x15, 0x65, 0xc0), 1));
        p.drawRoundedRect(panel, 6, 6);

        // Model name
        p.setPen(QColor(0xff, 0xc4, 0x2e));  // mc1-gold
        QFont mf("Helvetica Neue", 14, QFont::Bold);
        mf.setLetterSpacing(QFont::PercentageSpacing, 130);
        p.setFont(mf);
        p.drawText(panel.adjusted(16, 10, 0, 0), Qt::AlignTop | Qt::AlignLeft,
                   "VOCAL PRODUCER PRO");

        // Pitch correction arc (center-left)
        float pitchAmt = m_vals[0];  // PitchCorrect
        float pitchSpd = m_vals[1];  // PitchSpeed
        int arcCx = panel.left() + 90, arcCy = panel.top() + H / 2 + 8;
        int arcR = 34;

        // Background arc
        p.setPen(QPen(QColor(0x15, 0x30, 0x55), 3));
        p.setBrush(Qt::NoBrush);
        p.drawArc(QRect(arcCx - arcR, arcCy - arcR, arcR * 2, arcR * 2),
                  210 * 16, -240 * 16);
        // Filled arc (gold)
        int sweep = static_cast<int>(-240 * pitchAmt * 16);
        p.setPen(QPen(QColor(0xff, 0xc4, 0x2e), 3));
        p.drawArc(QRect(arcCx - arcR, arcCy - arcR, arcR * 2, arcR * 2),
                  210 * 16, sweep);
        // Center label
        p.setPen(QColor(0xff, 0xc4, 0x2e));
        QFont af("Menlo", 8, QFont::Bold);
        p.setFont(af);
        QString pitchLabel = pitchSpd < 0.15f ? "HARD" : pitchSpd < 0.5f ? "MED" : "NAT";
        QFontMetrics fm(af);
        p.drawText(arcCx - fm.horizontalAdvance(pitchLabel) / 2, arcCy + 4, pitchLabel);
        p.setPen(QColor(0x60, 0x88, 0xb0));
        QFont sf("Menlo", 6);
        p.setFont(sf);
        p.drawText(arcCx - 14, arcCy + 16, "PITCH");

        // Channel strip meters (right side)
        int mX = panel.left() + 200, mY = panel.top() + 36;
        int mW = panel.width() - 220, mH = H - 56;

        // 4 vertical strip indicators: DRIVE, COMP, EQ, VERB
        const char* labels[] = {"DRIVE", "COMP", "EQ", "VERB"};
        float vals[] = {m_vals[3], m_vals[4], 0.5f, m_vals[10]};
        int stripW = mW / 5;
        for (int i = 0; i < 4; ++i) {
            int sx = mX + i * stripW + stripW / 4;
            int barH = static_cast<int>(vals[i] * mH * 0.8f);
            int barY = mY + mH - barH;

            QLinearGradient g(sx, barY, sx, mY + mH);
            g.setColorAt(0.0, QColor(0x1d, 0xe9, 0xb6, 200)); // mc1-mint
            g.setColorAt(1.0, QColor(0x0e, 0xa5, 0xe9, 200));  // mc1-blue
            p.setBrush(g);
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(sx, barY, 18, barH, 2, 2);

            p.setPen(QColor(0x60, 0x88, 0xb0));
            p.setFont(sf);
            p.drawText(sx - 2, mY + mH + 10, labels[i]);
        }

        // Delay indicator
        float delayMix = m_vals[8];
        if (delayMix > 0.05f) {
            int dx = panel.right() - 60, dy = panel.top() + 30;
            p.setBrush(QColor(0x1d, 0xe9, 0xb6));
            p.setPen(QPen(QColor(0xff, 0xff, 0xff), 1));
            p.drawRoundedRect(dx, dy, 44, 14, 3, 3);
            p.setPen(QColor(0x0d, 0x23, 0x42));
            QFont df("Menlo", 7, QFont::Bold);
            p.setFont(df);
            p.drawText(QRect(dx, dy, 44, 14), Qt::AlignCenter, "DELAY");
        }
    }
};

class VocalProducerDialog : public BBEPluginDialog {
    Q_OBJECT
public:
    VocalProducerDialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : BBEPluginDialog(fx, new VocalProducerHeroWidget,
            "MC1 Vocal Producer Pro",
            mc1dsp::FxVocalProducer::kParamCount,
            QStringList{"PITCH:0", "SPEED:1", "KEY:2", "DRIVE:3", "COMP:4",
                        "LO EQ:5", "MID EQ:6", "HI EQ:7", "DELAY:8",
                        "D.TIME:9", "REVERB:10", "DECAY:11", "MIX:12", "OUTPUT:13"},
            parent) {}
};

/* ═══════════════════════════════════════════════════════════════════
 *  TOPLINE KEY FINDER — chromagram display + detected key readout
 * ═══════════════════════════════════════════════════════════════════ */
class KeyFinderHeroWidget : public BBEHeroWidget {
    Q_OBJECT
public:
    void setKeyInfo(int keyIdx, bool isMinor, float confidence, const float* chroma) {
        m_keyIdx = keyIdx;
        m_isMinor = isMinor;
        m_confidence = confidence;
        if (chroma) {
            for (int i = 0; i < 12; ++i) m_chroma[i] = chroma[i];
        }
        update();
    }

protected:
    void drawFace(QPainter& p, int W, int H) override {
        QRect panel(70, 6, W - 80, H - 12);
        QLinearGradient bg(panel.topLeft(), panel.bottomLeft());
        bg.setColorAt(0.0, QColor(0x0d, 0x23, 0x42));
        bg.setColorAt(1.0, QColor(0x06, 0x10, 0x22));
        p.setBrush(bg);
        p.setPen(QPen(QColor(0x15, 0x65, 0xc0), 1));
        p.drawRoundedRect(panel, 6, 6);

        p.setPen(QColor(0xff, 0xc4, 0x2e));
        QFont mf("Helvetica Neue", 14, QFont::Bold);
        mf.setLetterSpacing(QFont::PercentageSpacing, 130);
        p.setFont(mf);
        p.drawText(panel.adjusted(16, 10, 0, 0), Qt::AlignTop | Qt::AlignLeft,
                   "TOPLINE KEY FINDER");

        // Detected key LCD (large, center-left)
        static const char* noteNames[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        int lcdX = panel.left() + 16, lcdY = panel.top() + 40;
        QRect lcd(lcdX, lcdY, 120, 50);
        p.setBrush(QColor(0x04, 0x0a, 0x18));
        p.setPen(QPen(QColor(0x15, 0x65, 0xc0), 1));
        p.drawRoundedRect(lcd, 4, 4);

        if (m_keyIdx >= 0 && m_keyIdx < 12) {
            QString keyStr = QString("%1 %2")
                .arg(noteNames[m_keyIdx])
                .arg(m_isMinor ? "minor" : "Major");
            p.setPen(QColor(0x1d, 0xe9, 0xb6));  // mc1-mint
            QFont kf("Menlo", 18, QFont::Bold);
            p.setFont(kf);
            p.drawText(lcd, Qt::AlignCenter, keyStr);
        } else {
            p.setPen(QColor(0x40, 0x60, 0x80));
            QFont kf("Menlo", 14);
            p.setFont(kf);
            p.drawText(lcd, Qt::AlignCenter, "Listening...");
        }

        // Confidence bar
        int cbX = lcdX, cbY = lcdY + 56;
        int cbW = 120, cbH = 8;
        p.setBrush(QColor(0x10, 0x18, 0x28));
        p.setPen(Qt::NoPen);
        p.drawRect(cbX, cbY, cbW, cbH);
        int fillW = static_cast<int>(m_confidence * cbW);
        QColor confCol = m_confidence > 0.7f ? QColor(0x1d, 0xe9, 0xb6)
                       : m_confidence > 0.4f ? QColor(0xff, 0xc4, 0x2e)
                       : QColor(0xff, 0x44, 0x22);
        p.setBrush(confCol);
        p.drawRect(cbX, cbY, fillW, cbH);

        // Chromagram bars (right side)
        int chromaX = panel.left() + 160, chromaY = panel.top() + 36;
        int chromaW = panel.width() - 180, chromaH = H - 52;
        int barW = chromaW / 13;

        for (int i = 0; i < 12; ++i) {
            int bx = chromaX + i * barW + 2;
            int bh = static_cast<int>(m_chroma[i] * chromaH * 0.9f);
            int by = chromaY + chromaH - bh;

            bool isBlack = (i == 1 || i == 3 || i == 6 || i == 8 || i == 10);
            bool isKey = (i == m_keyIdx);
            QColor barCol = isKey ? QColor(0x1d, 0xe9, 0xb6, 220)
                          : isBlack ? QColor(0x40, 0x60, 0x90, 160)
                          : QColor(0x0e, 0xa5, 0xe9, 180);
            p.setBrush(barCol);
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(bx, by, barW - 4, bh, 1, 1);

            // Note label
            p.setPen(isKey ? QColor(0x1d, 0xe9, 0xb6) : QColor(0x40, 0x60, 0x90));
            QFont lf("Menlo", 6);
            p.setFont(lf);
            p.drawText(bx, chromaY + chromaH + 9, noteNames[i]);
        }
    }

private:
    int   m_keyIdx = -1;
    bool  m_isMinor = false;
    float m_confidence = 0.0f;
    float m_chroma[12] = {};
};

class KeyFinderDialog : public BBEPluginDialog {
    Q_OBJECT
public:
    KeyFinderDialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : BBEPluginDialog(fx, new KeyFinderHeroWidget,
            "MC1 Topline Key Finder",
            mc1dsp::FxKeyFinder::kParamCount,
            QStringList{"SENSITIVITY:0", "SMOOTHING:1", "A=Hz:2", "OUTPUT:3"},
            parent)
    {
        // Poll the key finder's atomic state and update the hero widget
        auto* keyPoll = new QTimer(this);
        keyPoll->setInterval(100);
        connect(keyPoll, &QTimer::timeout, this, [this, fx]() {
            auto* kf = dynamic_cast<mc1dsp::FxKeyFinder*>(fx);
            if (!kf) return;
            auto* hero = dynamic_cast<KeyFinderHeroWidget*>(findChild<KeyFinderHeroWidget*>());
            if (!hero) return;
            hero->setKeyInfo(kf->detectedKeyIndex(), kf->detectedIsMinor(),
                            kf->detectedConfidence(), kf->chromagram());
        });
        keyPoll->start();
    }
};
