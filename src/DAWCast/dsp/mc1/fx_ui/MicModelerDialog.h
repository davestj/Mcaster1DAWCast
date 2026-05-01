/*
 * Mcaster1AudioPipe — Virtual Audio Routing Application
 * fx_ui/MicModelerDialog.h — Microphone Modeler editor with 3D graphics
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Premium studio mic modeling interface featuring:
 *   - Custom-painted 3D mic visualizations (12 unique models)
 *   - Mic selection grid with category tabs
 *   - Per-mic configuration knobs
 *   - Polar pattern display widget
 *   - Frequency response curve preview
 *   - Studio control room aesthetic
 */

#pragma once

#include <QDialog>
#include <QWidget>
#include <QPainter>
#include <QPainterPath>
#include <QListWidget>
#include <QListWidgetItem>
#include <QTabWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFrame>
#include <QRadialGradient>
#include <QLinearGradient>
#include <QConicalGradient>
#include <QTimer>
#include <QInputDialog>
#include <QtMath>

#include <algorithm>
#include <cmath>

#include "fx_ui/RackKnob.h"
#include "fx_ui/RackMeter.h"
#include "patchbay/dsp/dsp_effect.h"
#include "patchbay/dsp/preset_manager.h"

/* ════════════════════════════════════════════════════════════════════
 *  MicGraphic — custom-painted 3D microphone visualization
 * ════════════════════════════════════════════════════════════════════ */

class MicGraphic : public QWidget {
    Q_OBJECT
public:
    enum Category { TubeCondenser, Dynamic, Ribbon };

    explicit MicGraphic(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(165, 270);
        setAttribute(Qt::WA_OpaquePaintEvent, false);
    }

    void setMicModel(int modelIndex)
    {
        modelIndex = std::clamp(modelIndex, 0, 11);
        if (modelIndex_ == modelIndex) return;
        modelIndex_ = modelIndex;
        update();
    }

    int micModel() const { return modelIndex_; }

    Category category() const
    {
        if (modelIndex_ <= 3) return TubeCondenser;
        if (modelIndex_ <= 7) return Dynamic;
        return Ribbon;
    }

    QSize minimumSizeHint() const override { return {220, 360}; }
    QSize sizeHint() const override { return {260, 420}; }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);

        const QRectF r = rect().adjusted(10, 10, -10, -10);

        /* Background: deep radial vignette */
        {
            QRadialGradient bg(r.center(), r.width() * 0.8);
            bg.setColorAt(0.0, QColor("#1a2535"));
            bg.setColorAt(0.6, QColor("#0d141e"));
            bg.setColorAt(1.0, QColor("#05090f"));
            p.setBrush(bg);
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(r, 8, 8);
        }

        /* Floor reflection ellipse */
        {
            QRectF floor(r.center().x() - r.width() * 0.35,
                         r.bottom() - 40,
                         r.width() * 0.70,
                         30);
            QRadialGradient fg(floor.center(), floor.width() * 0.5);
            fg.setColorAt(0.0, QColor(255, 255, 255, 30));
            fg.setColorAt(1.0, QColor(0, 0, 0, 0));
            p.setBrush(fg);
            p.setPen(Qt::NoPen);
            p.drawEllipse(floor);
        }

        /* Shadow under mic */
        {
            QRectF shadow(r.center().x() - 60,
                          r.bottom() - 32,
                          120, 18);
            QRadialGradient sg(shadow.center(), 60);
            sg.setColorAt(0.0, QColor(0, 0, 0, 180));
            sg.setColorAt(1.0, QColor(0, 0, 0, 0));
            p.setBrush(sg);
            p.setPen(Qt::NoPen);
            p.drawEllipse(shadow);
        }

        /* Mic body area (inset from rect) */
        QRectF micRect = r.adjusted(20, 18, -20, -60);

        const Category cat = category();
        if (cat == TubeCondenser)      paintTubeCondenser(p, micRect, modelIndex_);
        else if (cat == Dynamic)       paintDynamic(p, micRect, modelIndex_);
        else                           paintRibbon(p, micRect, modelIndex_);

        /* Model name label at bottom */
        {
            const char* names[12] = {
                "BOCK 167", "U47 TUBE", "C12 TUBE", "U67 TUBE",
                "DN-7", "DN-20", "DN-88", "DN-441",
                "RB-77DX", "RB-160", "ROYER 121", "DN-421"
            };
            const char* cats[12] = {
                "Tube Condenser", "Tube Condenser", "Tube Condenser", "Tube Condenser",
                "Dynamic", "Dynamic", "Dynamic", "Dynamic",
                "Ribbon", "Ribbon", "Ribbon", "Dynamic"
            };

            QFont nf = font();
            nf.setPixelSize(14);
            nf.setWeight(QFont::Bold);
            nf.setLetterSpacing(QFont::AbsoluteSpacing, 2.0);
            p.setFont(nf);
            p.setPen(QColor("#e8e8e8"));
            QRectF nameR(r.left(), r.bottom() - 30, r.width(), 18);
            p.drawText(nameR, Qt::AlignCenter, names[modelIndex_]);

            QFont cf = font();
            cf.setPixelSize(10);
            cf.setItalic(true);
            cf.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
            p.setFont(cf);
            QColor catColor = categoryColor(cat);
            p.setPen(catColor);
            QRectF catR(r.left(), r.bottom() - 14, r.width(), 12);
            p.drawText(catR, Qt::AlignCenter, cats[modelIndex_]);
        }
    }

private:
    static QColor categoryColor(Category c)
    {
        switch (c) {
            case TubeCondenser: return QColor("#D4AF37");
            case Dynamic:       return QColor("#4A9BD9");
            case Ribbon:        return QColor("#B87333");
        }
        return QColor("#c0c0c0");
    }

    /* ── Tube Condenser painting ─────────────────────────────────── */
    void paintTubeCondenser(QPainter& p, const QRectF& r, int modelIndex)
    {
        /* Each tube condenser has distinct styling */
        // Color variations
        QColor baseHi, baseMid, baseLo;
        switch (modelIndex) {
            case 0: // Bock 167 — champagne gold
                baseHi  = QColor("#F4D98A");
                baseMid = QColor("#D4AF37");
                baseLo  = QColor("#6B5014");
                break;
            case 1: // U47 — cream/ivory
                baseHi  = QColor("#F8EED0");
                baseMid = QColor("#D9C89A");
                baseLo  = QColor("#5C4E28");
                break;
            case 2: // C12 — brushed steel gold
                baseHi  = QColor("#E6D89C");
                baseMid = QColor("#B8A055");
                baseLo  = QColor("#4A3D18");
                break;
            case 3: // U67 — warm nickel
                baseHi  = QColor("#E8DBA8");
                baseMid = QColor("#C5A858");
                baseLo  = QColor("#523F16");
                break;
            default:
                baseHi = QColor("#F4D98A");
                baseMid = QColor("#D4AF37");
                baseLo = QColor("#6B5014");
                break;
        }

        /* Grille dimensions — large for tube condenser */
        const qreal grilleW = r.width() * 0.78;
        const qreal grilleH = r.height() * 0.48;
        const qreal grilleX = r.center().x() - grilleW / 2.0;
        const qreal grilleY = r.top() + 10;
        QRectF grilleRect(grilleX, grilleY, grilleW, grilleH);

        /* Body dimensions */
        const qreal bodyW = r.width() * 0.42;
        const qreal bodyH = r.height() * 0.40;
        const qreal bodyX = r.center().x() - bodyW / 2.0;
        const qreal bodyY = grilleY + grilleH - 8;
        QRectF bodyRect(bodyX, bodyY, bodyW, bodyH);

        /* ── Body (cylindrical, drawn first so grille overlaps) ── */
        {
            QLinearGradient bgrad(bodyRect.topLeft(), bodyRect.topRight());
            bgrad.setColorAt(0.0, baseLo.darker(130));
            bgrad.setColorAt(0.15, baseMid.darker(120));
            bgrad.setColorAt(0.5, baseHi);
            bgrad.setColorAt(0.85, baseMid.darker(120));
            bgrad.setColorAt(1.0, baseLo.darker(130));

            p.setBrush(bgrad);
            p.setPen(QPen(baseLo.darker(150), 1.0));
            p.drawRoundedRect(bodyRect, 6, 6);

            // Top bevel highlight
            QLinearGradient topBevel(bodyRect.topLeft(),
                                      QPointF(bodyRect.left(), bodyRect.top() + 10));
            topBevel.setColorAt(0.0, QColor(255, 255, 255, 60));
            topBevel.setColorAt(1.0, QColor(255, 255, 255, 0));
            p.setBrush(topBevel);
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(QRectF(bodyRect.left() + 2, bodyRect.top() + 2,
                                     bodyRect.width() - 4, 10), 4, 4);

            // Bottom shadow
            QLinearGradient botShadow(QPointF(bodyRect.left(), bodyRect.bottom() - 12),
                                       bodyRect.bottomLeft());
            botShadow.setColorAt(0.0, QColor(0, 0, 0, 0));
            botShadow.setColorAt(1.0, QColor(0, 0, 0, 120));
            p.setBrush(botShadow);
            p.drawRoundedRect(QRectF(bodyRect.left() + 2,
                                     bodyRect.bottom() - 12,
                                     bodyRect.width() - 4, 10), 4, 4);

            // Brand label
            QRectF label(bodyRect.center().x() - bodyRect.width() * 0.35,
                         bodyRect.center().y() - 10,
                         bodyRect.width() * 0.70,
                         20);
            p.setBrush(QColor(0, 0, 0, 120));
            p.setPen(QPen(baseHi, 0.5));
            p.drawRoundedRect(label, 2, 2);

            QFont lf = p.font();
            lf.setPixelSize(8);
            lf.setWeight(QFont::Bold);
            lf.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
            p.setFont(lf);
            p.setPen(baseHi);
            const char* brands[4] = {"BOCK", "NEUMANN", "AKG", "NEUMANN"};
            p.drawText(label, Qt::AlignCenter, brands[modelIndex]);
        }

        /* ── Grille (large oval, metallic frame) ── */
        {
            // Outer frame ring
            QRectF outerRing = grilleRect.adjusted(-4, -4, 4, 4);
            QRadialGradient ringGrad(outerRing.center(), outerRing.width() / 2.0);
            ringGrad.setColorAt(0.7, baseMid);
            ringGrad.setColorAt(0.92, baseHi);
            ringGrad.setColorAt(1.0, baseLo);
            p.setBrush(ringGrad);
            p.setPen(QPen(baseLo.darker(150), 1.0));
            p.drawEllipse(outerRing);

            // Inner grille mesh area
            QRadialGradient meshBg(grilleRect.center(),
                                    grilleRect.width() / 2.0,
                                    QPointF(grilleRect.center().x() - grilleRect.width() * 0.15,
                                            grilleRect.center().y() - grilleRect.height() * 0.15));
            meshBg.setColorAt(0.0, baseMid.lighter(115));
            meshBg.setColorAt(0.6, baseMid.darker(110));
            meshBg.setColorAt(1.0, baseLo);
            p.setBrush(meshBg);
            p.setPen(Qt::NoPen);
            p.drawEllipse(grilleRect);

            // Mesh pattern — diagonal grid
            p.save();
            p.setClipRegion(QRegion(grilleRect.toRect(), QRegion::Ellipse));
            p.setPen(QPen(baseLo.darker(140), 0.6));
            const qreal step = 5.0;
            for (qreal x = grilleRect.left(); x < grilleRect.right() + grilleRect.height();
                 x += step) {
                p.drawLine(QPointF(x, grilleRect.top()),
                           QPointF(x - grilleRect.height(), grilleRect.bottom()));
            }
            for (qreal x = grilleRect.left() - grilleRect.height();
                 x < grilleRect.right(); x += step) {
                p.drawLine(QPointF(x, grilleRect.top()),
                           QPointF(x + grilleRect.height(), grilleRect.bottom()));
            }
            p.restore();

            // Highlight on grille
            QRadialGradient hl(QPointF(grilleRect.center().x() - grilleRect.width() * 0.25,
                                        grilleRect.center().y() - grilleRect.height() * 0.30),
                                grilleRect.width() * 0.30);
            hl.setColorAt(0.0, QColor(255, 255, 255, 90));
            hl.setColorAt(1.0, QColor(255, 255, 255, 0));
            p.setBrush(hl);
            p.setPen(Qt::NoPen);
            p.drawEllipse(grilleRect);

            // Capsule dot (center)
            QRectF capsule(grilleRect.center().x() - 4,
                           grilleRect.center().y() - 4,
                           8, 8);
            p.setBrush(QColor(0, 0, 0, 180));
            p.drawEllipse(capsule);
        }

        /* Signal indicator LED on body */
        {
            QRectF led(bodyRect.center().x() - 3,
                       bodyRect.bottom() - 10,
                       6, 6);
            QRadialGradient ledGrad(led.center(), 4);
            ledGrad.setColorAt(0.0, QColor("#ff6060"));
            ledGrad.setColorAt(1.0, QColor("#601010"));
            p.setBrush(ledGrad);
            p.setPen(Qt::NoPen);
            p.drawEllipse(led);
        }
    }

    /* ── Dynamic microphone painting ─────────────────────────────── */
    void paintDynamic(QPainter& p, const QRectF& r, int modelIndex)
    {
        /* Dynamics are long cylindrical (SM7B, RE20 style) */
        QColor bodyHi  = QColor("#606060");
        QColor bodyMid = QColor("#2A2A2A");
        QColor bodyLo  = QColor("#0C0C0C");

        switch (modelIndex) {
            case 4: // DN-7 (SM7B) — matte black cylinder, windscreen at top
                bodyHi  = QColor("#4A4A4A");
                bodyMid = QColor("#1E1E1E");
                bodyLo  = QColor("#050505");
                break;
            case 5: // DN-20 (RE20) — dark gunmetal
                bodyHi  = QColor("#505560");
                bodyMid = QColor("#252A32");
                bodyLo  = QColor("#0A0C10");
                break;
            case 6: // DN-88 (RE320) — black/silver
                bodyHi  = QColor("#5A5F68");
                bodyMid = QColor("#22262C");
                bodyLo  = QColor("#080A10");
                break;
            case 7:  // DN-441 (MD441)
            case 11: // DN-421 (MD421)
                bodyHi  = QColor("#484848");
                bodyMid = QColor("#1A1A1A");
                bodyLo  = QColor("#050505");
                break;
        }

        /* Long body */
        const qreal bodyW = r.width() * 0.32;
        const qreal bodyH = r.height() * 0.70;
        const qreal bodyX = r.center().x() - bodyW / 2.0;
        const qreal bodyY = r.top() + r.height() * 0.22;
        QRectF bodyRect(bodyX, bodyY, bodyW, bodyH);

        /* Windscreen/grille at top */
        const qreal grilleW = bodyW * 1.25;
        const qreal grilleH = r.height() * 0.25;
        const qreal grilleX = r.center().x() - grilleW / 2.0;
        const qreal grilleY = r.top() + 6;
        QRectF grilleRect(grilleX, grilleY, grilleW, grilleH);

        /* ── Body cylinder ── */
        {
            QLinearGradient bgrad(bodyRect.topLeft(), bodyRect.topRight());
            bgrad.setColorAt(0.0, bodyLo);
            bgrad.setColorAt(0.12, bodyMid);
            bgrad.setColorAt(0.5, bodyHi);
            bgrad.setColorAt(0.88, bodyMid);
            bgrad.setColorAt(1.0, bodyLo);

            p.setBrush(bgrad);
            p.setPen(QPen(QColor(0, 0, 0, 200), 1.0));
            p.drawRoundedRect(bodyRect, 8, 8);

            // Top cap (where grille meets body)
            QRectF cap(bodyRect.left() - 2, bodyRect.top() - 2,
                       bodyRect.width() + 4, 6);
            QLinearGradient capGrad(cap.topLeft(), cap.bottomLeft());
            capGrad.setColorAt(0.0, bodyHi.lighter(130));
            capGrad.setColorAt(1.0, bodyLo);
            p.setBrush(capGrad);
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(cap, 2, 2);

            // Specular highlight strip (brushed metal)
            QLinearGradient spec(bodyRect.topLeft(),
                                  QPointF(bodyRect.right(), bodyRect.top()));
            spec.setColorAt(0.0, QColor(255, 255, 255, 0));
            spec.setColorAt(0.45, QColor(255, 255, 255, 35));
            spec.setColorAt(0.55, QColor(255, 255, 255, 35));
            spec.setColorAt(1.0, QColor(255, 255, 255, 0));
            p.setBrush(spec);
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(bodyRect.adjusted(2, 4, -2, -4), 6, 6);

            // Horizontal separation line (typical on SM7B/RE20)
            p.setPen(QPen(QColor(0, 0, 0, 180), 1.0));
            p.drawLine(QPointF(bodyRect.left() + 2, bodyRect.center().y()),
                       QPointF(bodyRect.right() - 2, bodyRect.center().y()));
            p.setPen(QPen(QColor(255, 255, 255, 30), 1.0));
            p.drawLine(QPointF(bodyRect.left() + 2, bodyRect.center().y() + 1),
                       QPointF(bodyRect.right() - 2, bodyRect.center().y() + 1));

            // Model code label
            QRectF label(bodyRect.center().x() - bodyRect.width() * 0.40,
                         bodyRect.bottom() - bodyRect.height() * 0.22,
                         bodyRect.width() * 0.80,
                         14);
            p.setBrush(QColor(0, 0, 0, 140));
            p.setPen(QPen(QColor("#4A9BD9"), 0.5));
            p.drawRoundedRect(label, 2, 2);

            QFont lf = p.font();
            lf.setPixelSize(7);
            lf.setWeight(QFont::Bold);
            lf.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
            p.setFont(lf);
            p.setPen(QColor("#4A9BD9"));
            const char* codes[12] = {
                "", "", "", "",
                "DN-7", "DN-20", "DN-88", "DN-441",
                "", "", "", "DN-421"
            };
            p.drawText(label, Qt::AlignCenter, codes[modelIndex]);
        }

        /* ── Grille / windscreen ── */
        {
            QRadialGradient gGrad(grilleRect.center(),
                                   grilleRect.width() * 0.6,
                                   QPointF(grilleRect.center().x() - grilleRect.width() * 0.15,
                                           grilleRect.center().y() - grilleRect.height() * 0.25));
            gGrad.setColorAt(0.0, bodyHi.lighter(130));
            gGrad.setColorAt(0.5, bodyMid);
            gGrad.setColorAt(1.0, bodyLo);
            p.setBrush(gGrad);
            p.setPen(QPen(bodyLo, 1.0));
            p.drawRoundedRect(grilleRect, grilleRect.height() / 2.0,
                              grilleRect.height() / 2.0);

            // Mesh dots pattern
            p.save();
            QPainterPath clip;
            clip.addRoundedRect(grilleRect,
                                grilleRect.height() / 2.0,
                                grilleRect.height() / 2.0);
            p.setClipPath(clip);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 0, 0, 140));
            const qreal dotStep = 3.5;
            for (qreal y = grilleRect.top() + 2; y < grilleRect.bottom(); y += dotStep) {
                qreal offset = (static_cast<int>((y - grilleRect.top()) / dotStep) % 2)
                               ? dotStep / 2.0 : 0.0;
                for (qreal x = grilleRect.left() + 2 + offset;
                     x < grilleRect.right(); x += dotStep) {
                    p.drawEllipse(QPointF(x, y), 0.7, 0.7);
                }
            }
            p.restore();

            // Top highlight
            QLinearGradient topHl(grilleRect.topLeft(),
                                   QPointF(grilleRect.left(), grilleRect.top() + grilleRect.height() * 0.4));
            topHl.setColorAt(0.0, QColor(255, 255, 255, 60));
            topHl.setColorAt(1.0, QColor(255, 255, 255, 0));
            p.setBrush(topHl);
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(grilleRect.adjusted(3, 2, -3, -grilleRect.height() * 0.5),
                              grilleRect.height() * 0.3, grilleRect.height() * 0.3);
        }
    }

    /* ── Ribbon microphone painting ─────────────────────────────── */
    void paintRibbon(QPainter& p, const QRectF& r, int modelIndex)
    {
        QColor chromeHi = QColor("#E0E0E0");
        QColor chromeMid = QColor("#808080");
        QColor chromeLo = QColor("#1C1C1C");
        QColor ribbonCol = QColor("#B87333");

        switch (modelIndex) {
            case 8: // RB-77DX (RCA 77) — classic chrome, rectangular
                chromeHi  = QColor("#EAEAEA");
                chromeMid = QColor("#888888");
                chromeLo  = QColor("#1A1A1A");
                ribbonCol = QColor("#C07545");
                break;
            case 9: // RB-160 (Coles) — darker gunmetal
                chromeHi  = QColor("#B8B8B8");
                chromeMid = QColor("#5A5F68");
                chromeLo  = QColor("#101418");
                ribbonCol = QColor("#A0633D");
                break;
            case 10: // Royer 121 — modern polished chrome
                chromeHi  = QColor("#F0F0F0");
                chromeMid = QColor("#909090");
                chromeLo  = QColor("#181818");
                ribbonCol = QColor("#C88060");
                break;
        }

        /* Rectangular body */
        const qreal bodyW = r.width() * 0.58;
        const qreal bodyH = r.height() * 0.68;
        const qreal bodyX = r.center().x() - bodyW / 2.0;
        const qreal bodyY = r.top() + r.height() * 0.12;
        QRectF bodyRect(bodyX, bodyY, bodyW, bodyH);

        /* ── Main body ── */
        {
            QLinearGradient bgrad(bodyRect.topLeft(), bodyRect.topRight());
            bgrad.setColorAt(0.0, chromeLo);
            bgrad.setColorAt(0.1, chromeMid.darker(120));
            bgrad.setColorAt(0.4, chromeHi);
            bgrad.setColorAt(0.55, chromeMid);
            bgrad.setColorAt(0.9, chromeMid.darker(130));
            bgrad.setColorAt(1.0, chromeLo);

            p.setBrush(bgrad);
            p.setPen(QPen(chromeLo.darker(150), 1.2));
            p.drawRoundedRect(bodyRect, 6, 6);

            // Angled sheen
            QLinearGradient sheen(bodyRect.topLeft(), bodyRect.bottomRight());
            sheen.setColorAt(0.0, QColor(255, 255, 255, 0));
            sheen.setColorAt(0.35, QColor(255, 255, 255, 30));
            sheen.setColorAt(0.5, QColor(255, 255, 255, 60));
            sheen.setColorAt(0.65, QColor(255, 255, 255, 30));
            sheen.setColorAt(1.0, QColor(255, 255, 255, 0));
            p.setBrush(sheen);
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(bodyRect.adjusted(2, 2, -2, -2), 4, 4);
        }

        /* ── Ribbon window (visible ribbon element) ── */
        {
            const qreal winW = bodyRect.width() * 0.55;
            const qreal winH = bodyRect.height() * 0.55;
            QRectF window(bodyRect.center().x() - winW / 2.0,
                          bodyRect.top() + bodyRect.height() * 0.12,
                          winW, winH);

            // Window frame (bezel)
            QRectF bezel = window.adjusted(-3, -3, 3, 3);
            QLinearGradient bezelGrad(bezel.topLeft(), bezel.bottomRight());
            bezelGrad.setColorAt(0.0, chromeHi);
            bezelGrad.setColorAt(0.5, chromeLo);
            bezelGrad.setColorAt(1.0, chromeMid);
            p.setBrush(bezelGrad);
            p.setPen(QPen(chromeLo, 0.8));
            p.drawRoundedRect(bezel, 3, 3);

            // Dark cavity background
            QRadialGradient cavity(window.center(), window.width() * 0.6);
            cavity.setColorAt(0.0, QColor("#1C1410"));
            cavity.setColorAt(1.0, QColor("#000000"));
            p.setBrush(cavity);
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(window, 2, 2);

            // Horizontal grille bars over window
            p.setPen(QPen(chromeMid.darker(130), 0.8));
            const qreal barStep = 2.5;
            for (qreal y = window.top() + 2; y < window.bottom(); y += barStep) {
                p.drawLine(QPointF(window.left() + 1, y),
                           QPointF(window.right() - 1, y));
            }

            // The ribbon itself — vertical strip in center
            QRectF ribbon(window.center().x() - 3,
                          window.top() + 4,
                          6,
                          window.height() - 8);
            QLinearGradient ribGrad(ribbon.topLeft(), ribbon.topRight());
            ribGrad.setColorAt(0.0, ribbonCol.darker(130));
            ribGrad.setColorAt(0.5, ribbonCol.lighter(120));
            ribGrad.setColorAt(1.0, ribbonCol.darker(140));
            p.setBrush(ribGrad);
            p.setPen(Qt::NoPen);
            p.drawRect(ribbon);

            // Ribbon glow
            QLinearGradient glow(ribbon.topLeft(), ribbon.topRight());
            glow.setColorAt(0.0, QColor(ribbonCol.red(), ribbonCol.green(), ribbonCol.blue(), 0));
            glow.setColorAt(0.5, QColor(ribbonCol.red(), ribbonCol.green(), ribbonCol.blue(), 100));
            glow.setColorAt(1.0, QColor(ribbonCol.red(), ribbonCol.green(), ribbonCol.blue(), 0));
            p.setBrush(glow);
            p.drawRect(ribbon.adjusted(-4, 0, 4, 0));
        }

        /* ── Brand plate at top ── */
        {
            QRectF plate(bodyRect.center().x() - bodyRect.width() * 0.3,
                         bodyRect.top() + 4,
                         bodyRect.width() * 0.6,
                         12);
            QLinearGradient plateGrad(plate.topLeft(), plate.bottomLeft());
            plateGrad.setColorAt(0.0, chromeHi);
            plateGrad.setColorAt(0.5, chromeMid);
            plateGrad.setColorAt(1.0, chromeLo);
            p.setBrush(plateGrad);
            p.setPen(QPen(chromeLo, 0.5));
            p.drawRoundedRect(plate, 1, 1);

            QFont lf = p.font();
            lf.setPixelSize(7);
            lf.setWeight(QFont::Bold);
            lf.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
            p.setFont(lf);
            p.setPen(chromeLo);
            const char* brands[12] = {
                "", "", "", "",
                "", "", "", "",
                "RCA", "COLES", "ROYER", ""
            };
            p.drawText(plate, Qt::AlignCenter, brands[modelIndex]);
        }

        /* Bottom shock mount stub */
        {
            QRectF stub(bodyRect.center().x() - 8,
                        bodyRect.bottom() - 2,
                        16, 10);
            QLinearGradient sg(stub.topLeft(), stub.bottomLeft());
            sg.setColorAt(0.0, chromeMid);
            sg.setColorAt(1.0, chromeLo);
            p.setBrush(sg);
            p.setPen(QPen(chromeLo, 0.8));
            p.drawRoundedRect(stub, 2, 2);
        }
    }

    int modelIndex_ = 0;
};

/* ════════════════════════════════════════════════════════════════════
 *  PolarPatternWidget — polar response visualization
 * ════════════════════════════════════════════════════════════════════ */

class PolarPatternWidget : public QWidget {
    Q_OBJECT
public:
    enum Pattern { Omni, Cardioid, Hypercardioid, FigureEight };

    explicit PolarPatternWidget(QWidget* parent = nullptr)
        : QWidget(parent) {}

    void setPattern(Pattern p)
    {
        if (pattern_ == p) return;
        pattern_ = p;
        update();
    }

    void setAxisAmount(float axis)
    {
        axis = std::clamp(axis, 0.0f, 1.0f);
        if (std::abs(axis - axis_) < 1e-4f) return;
        axis_ = axis;
        update();
    }

    void setPatternForMic(int modelIndex)
    {
        /* Map mic index to polar pattern */
        switch (modelIndex) {
            case 0: case 1: case 2: case 3: // Tube condensers — cardioid
                setPattern(Cardioid); break;
            case 4: case 5: case 6: // SM7B, RE20, RE320 — cardioid
                setPattern(Cardioid); break;
            case 7: case 11: // MD441, MD421 — hyper / super
                setPattern(Hypercardioid); break;
            case 8: // RCA 77 — figure 8
                setPattern(FigureEight); break;
            case 9: case 10: // Coles, Royer — figure 8
                setPattern(FigureEight); break;
            default: setPattern(Cardioid); break;
        }
    }

    QSize sizeHint() const override { return {140, 140}; }
    QSize minimumSizeHint() const override { return {120, 120}; }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const QRectF r = rect();
        const QPointF c = r.center();
        const qreal R = std::min(r.width(), r.height()) * 0.42;

        /* Background circle (dark) */
        {
            QRadialGradient bg(c, R + 6);
            bg.setColorAt(0.0, QColor("#0a1018"));
            bg.setColorAt(1.0, QColor("#05080c"));
            p.setBrush(bg);
            p.setPen(QPen(QColor("#2a3a4c"), 1.0));
            p.drawEllipse(c, R + 6, R + 6);
        }

        /* Grid rings */
        p.setPen(QPen(QColor(100, 140, 180, 40), 0.6, Qt::DashLine));
        for (int i = 1; i <= 4; ++i) {
            qreal rr = R * i / 4.0;
            p.drawEllipse(c, rr, rr);
        }

        /* Crosshair */
        p.setPen(QPen(QColor(100, 140, 180, 50), 0.6));
        p.drawLine(QPointF(c.x() - R - 4, c.y()),
                   QPointF(c.x() + R + 4, c.y()));
        p.drawLine(QPointF(c.x(), c.y() - R - 4),
                   QPointF(c.x(), c.y() + R + 4));

        /* Angle labels */
        QFont lf = p.font();
        lf.setPixelSize(7);
        p.setFont(lf);
        p.setPen(QColor(120, 150, 180, 150));
        p.drawText(QRectF(c.x() - 10, c.y() - R - 12, 20, 10),
                   Qt::AlignCenter, "0");
        p.drawText(QRectF(c.x() + R - 6, c.y() - 5, 20, 10),
                   Qt::AlignLeft, "90");
        p.drawText(QRectF(c.x() - 12, c.y() + R + 2, 24, 10),
                   Qt::AlignCenter, "180");
        p.drawText(QRectF(c.x() - R - 22, c.y() - 5, 20, 10),
                   Qt::AlignRight, "270");

        /* Polar pattern shape */
        QPainterPath path;
        const int steps = 180;
        for (int i = 0; i <= steps; ++i) {
            qreal angle = (static_cast<qreal>(i) / steps) * 2.0 * M_PI;
            qreal val = patternValue(angle);
            // Apply axis modulation (0 = on-axis only, 1 = full pattern)
            val = val * (0.4f + 0.6f * (1.0f - axis_ * 0.3f));
            qreal rr = R * val;
            // Rotate so 0 deg is at top (north)
            qreal x = c.x() + rr * std::sin(angle);
            qreal y = c.y() - rr * std::cos(angle);
            if (i == 0) path.moveTo(x, y);
            else path.lineTo(x, y);
        }
        path.closeSubpath();

        // Glow
        QPen glowPen(QColor(0, 200, 180, 60), 6.0);
        p.setPen(glowPen);
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);

        // Fill
        QRadialGradient fg(c, R);
        fg.setColorAt(0.0, QColor(0, 220, 200, 100));
        fg.setColorAt(1.0, QColor(0, 180, 170, 20));
        p.setBrush(fg);
        p.setPen(QPen(QColor("#00e0c8"), 1.5));
        p.drawPath(path);

        /* Pattern name */
        const char* names[4] = {"OMNI", "CARDIOID", "HYPER", "FIG-8"};
        QFont nf = p.font();
        nf.setPixelSize(8);
        nf.setWeight(QFont::Bold);
        nf.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
        p.setFont(nf);
        p.setPen(QColor("#00e0c8"));
        p.drawText(QRectF(r.left(), r.bottom() - 14, r.width(), 12),
                   Qt::AlignCenter, names[static_cast<int>(pattern_)]);
    }

private:
    qreal patternValue(qreal angle) const
    {
        /* Standard polar equations — angle in radians, 0=front */
        switch (pattern_) {
            case Omni:
                return 0.95;
            case Cardioid:
                return 0.5 + 0.5 * std::cos(angle);
            case Hypercardioid:
                return std::abs(0.25 + 0.75 * std::cos(angle));
            case FigureEight:
                return std::abs(std::cos(angle));
        }
        return 0.5;
    }

    Pattern pattern_ = Cardioid;
    float   axis_    = 0.0f;
};

/* ════════════════════════════════════════════════════════════════════
 *  MicResponseCurve — frequency response preview
 * ════════════════════════════════════════════════════════════════════ */

class MicResponseCurve : public QWidget {
    Q_OBJECT
public:
    explicit MicResponseCurve(QWidget* parent = nullptr)
        : QWidget(parent) {}

    void setMicModel(int modelIndex)
    {
        modelIndex = std::clamp(modelIndex, 0, 11);
        if (modelIndex_ == modelIndex) return;
        modelIndex_ = modelIndex;
        update();
    }

    QSize sizeHint() const override { return {320, 110}; }
    QSize minimumSizeHint() const override { return {280, 90}; }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const QRectF r = rect().adjusted(4, 4, -4, -4);

        /* Background */
        QLinearGradient bg(r.topLeft(), r.bottomLeft());
        bg.setColorAt(0.0, QColor("#0a1018"));
        bg.setColorAt(1.0, QColor("#05080c"));
        p.setBrush(bg);
        p.setPen(QPen(QColor("#2a3a4c"), 1.0));
        p.drawRoundedRect(r, 3, 3);

        /* Grid */
        p.setPen(QPen(QColor(100, 140, 180, 35), 0.5));
        for (int i = 1; i < 5; ++i) {
            qreal y = r.top() + r.height() * i / 5.0;
            p.drawLine(QPointF(r.left() + 2, y), QPointF(r.right() - 2, y));
        }
        // Octave grid (log scale approximation)
        const int bands = 8;
        for (int i = 1; i < bands; ++i) {
            qreal x = r.left() + r.width() * i / bands;
            p.drawLine(QPointF(x, r.top() + 2), QPointF(x, r.bottom() - 2));
        }

        /* 0 dB reference line */
        qreal zeroY = r.center().y();
        p.setPen(QPen(QColor(120, 150, 180, 80), 0.8, Qt::DashLine));
        p.drawLine(QPointF(r.left() + 2, zeroY), QPointF(r.right() - 2, zeroY));

        /* Frequency response curve (computed from simple band table) */
        QPainterPath path;
        const int samples = 200;
        QColor curveColor = micAccentColor(modelIndex_);

        for (int i = 0; i <= samples; ++i) {
            qreal t = static_cast<qreal>(i) / samples;
            // Log frequency from 20 Hz to 20 kHz
            qreal freq = 20.0 * std::pow(1000.0, t);
            qreal gainDb = micResponseAt(modelIndex_, freq);

            qreal x = r.left() + r.width() * t;
            // Map -12..+8 dB to widget vertical
            qreal y = zeroY - (gainDb / 12.0) * (r.height() * 0.40);
            y = std::clamp(y, r.top() + 2, r.bottom() - 2);

            if (i == 0) path.moveTo(x, y);
            else path.lineTo(x, y);
        }

        // Glow
        p.setPen(QPen(QColor(curveColor.red(), curveColor.green(),
                              curveColor.blue(), 70), 4.5));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);

        // Main curve
        p.setPen(QPen(curveColor, 1.8));
        p.drawPath(path);

        /* Frequency labels */
        QFont lf = p.font();
        lf.setPixelSize(7);
        p.setFont(lf);
        p.setPen(QColor(120, 150, 180, 160));
        const char* freqs[4] = {"100", "1k", "10k", "20k"};
        const qreal positions[4] = {0.24, 0.50, 0.76, 0.98};
        for (int i = 0; i < 4; ++i) {
            qreal x = r.left() + r.width() * positions[i];
            p.drawText(QRectF(x - 15, r.bottom() - 10, 30, 9),
                       Qt::AlignCenter, freqs[i]);
        }

        /* dB labels */
        p.drawText(QRectF(r.left() + 2, r.top() + 1, 20, 9),
                   Qt::AlignLeft, "+8");
        p.drawText(QRectF(r.left() + 2, zeroY - 4, 20, 9),
                   Qt::AlignLeft, "0");
        p.drawText(QRectF(r.left() + 2, r.bottom() - 18, 20, 9),
                   Qt::AlignLeft, "-8");
    }

private:
    static QColor micAccentColor(int modelIndex)
    {
        if (modelIndex <= 3)  return QColor("#D4AF37");
        if (modelIndex <= 7)  return QColor("#4A9BD9");
        if (modelIndex == 11) return QColor("#4A9BD9");
        return QColor("#B87333");
    }

    /* Approximated response curves — sum of band gains */
    qreal micResponseAt(int modelIndex, qreal freq) const
    {
        /* Model-specific response signature: array of
         * {center_freq, gain_db, Q} peaking bands */
        struct Band { qreal f; qreal g; qreal q; };
        static const Band kBands[12][6] = {
            // 0: Bock 167
            {{80,2,0.7},{200,1,1},{3000,3,1.2},{5000,1.5,1},{10000,1,0.8},{14000,-1,0.7}},
            // 1: U47
            {{60,3,0.6},{240,1.5,0.9},{2800,4,1.5},{5000,2,1},{8000,-0.5,0.8},{12000,-2,0.7}},
            // 2: C12
            {{40,1,0.7},{300,0.5,0.8},{3500,3.5,1.3},{8000,3,1},{12000,2.5,0.8},{16000,1,0.7}},
            // 3: U67
            {{80,1.5,0.7},{250,1,1},{2000,2.5,1.2},{4000,1,1},{8000,-1,0.8},{12000,-2.5,0.6}},
            // 4: DN-7 (SM7B)
            {{80,0.5,0.8},{400,0,1},{2000,1,1.2},{5000,2.5,1.2},{9000,1,0.9},{15000,-1,0.7}},
            // 5: DN-20 (RE20)
            {{100,0.2,1},{500,0.5,1},{2500,2,1.3},{5000,3,1.2},{10000,0.5,0.8},{16000,-2,0.7}},
            // 6: DN-88 (RE320)
            {{80,1,0.8},{350,0.5,1},{2500,2.5,1.3},{6000,2,1},{10000,1,0.9},{14000,-0.5,0.7}},
            // 7: DN-441 (MD441)
            {{100,0.5,0.9},{500,1,1},{3000,3,1.3},{6000,2.5,1.1},{10000,2,0.9},{14000,1,0.7}},
            // 8: RB-77DX
            {{60,2,0.6},{200,1,0.9},{1500,-0.5,1},{3000,-1,1},{8000,-3,0.7},{12000,-5,0.6}},
            // 9: RB-160 (Coles)
            {{80,1.5,0.7},{300,1,0.9},{2000,1.5,1.2},{4000,0.5,1},{8000,-4,0.7},{12000,-6,0.6}},
            // 10: Royer 121
            {{80,1,0.7},{250,0.5,0.9},{2500,1,1.2},{5000,-0.5,1},{10000,-2,0.8},{14000,-3,0.7}},
            // 11: DN-421 (MD421)
            {{120,1,0.8},{500,0.5,1},{3000,3,1.3},{5000,3.5,1.1},{10000,2,0.9},{14000,1,0.7}}
        };

        qreal total = 0.0;
        for (int i = 0; i < 6; ++i) {
            const Band& b = kBands[modelIndex][i];
            // Simple RBJ-ish peak shape: log-gaussian around center
            qreal ratio = std::log2(freq / b.f);
            qreal width = 1.0 / b.q;
            qreal g = b.g * std::exp(-(ratio * ratio) / (2.0 * width * width));
            total += g;
        }
        return total;
    }

    int modelIndex_ = 0;
};

/* ════════════════════════════════════════════════════════════════════
 *  MicTile — a single mic entry in the selection list
 * ════════════════════════════════════════════════════════════════════ */

class MicTile : public QWidget {
    Q_OBJECT
public:
    explicit MicTile(int modelIndex, const QString& name,
                      const QString& subtitle,
                      QWidget* parent = nullptr)
        : QWidget(parent), modelIndex_(modelIndex),
          name_(name), subtitle_(subtitle)
    {
        setFixedHeight(36);
        setMinimumWidth(150);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover, true);
    }

    void setSelected(bool s)
    {
        if (selected_ == s) return;
        selected_ = s;
        update();
    }

    bool isSelected() const { return selected_; }
    int  modelIndex() const { return modelIndex_; }

signals:
    void clicked(int modelIndex);

protected:
    void mousePressEvent(QMouseEvent* ev) override
    {
        if (ev->button() == Qt::LeftButton) {
            emit clicked(modelIndex_);
            ev->accept();
        }
    }

    void enterEvent(QEnterEvent*) override { hover_ = true; update(); }
    void leaveEvent(QEvent*) override       { hover_ = false; update(); }

    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const QRectF r = rect().adjusted(4, 3, -4, -3);

        /* Accent color based on category */
        QColor accent;
        if (modelIndex_ <= 3)      accent = QColor("#D4AF37");
        else if (modelIndex_ <= 7) accent = QColor("#4A9BD9");
        else if (modelIndex_ == 11) accent = QColor("#4A9BD9");
        else                        accent = QColor("#B87333");

        /* Background */
        QLinearGradient bg(r.topLeft(), r.bottomLeft());
        if (selected_) {
            bg.setColorAt(0.0, QColor(accent.red(), accent.green(), accent.blue(), 80));
            bg.setColorAt(1.0, QColor(accent.red(), accent.green(), accent.blue(), 30));
        } else if (hover_) {
            bg.setColorAt(0.0, QColor("#1e2a3a"));
            bg.setColorAt(1.0, QColor("#141c28"));
        } else {
            bg.setColorAt(0.0, QColor("#141c28"));
            bg.setColorAt(1.0, QColor("#0d141e"));
        }
        p.setBrush(bg);
        p.setPen(QPen(selected_ ? accent : QColor("#2a3a4c"),
                      selected_ ? 1.5 : 0.8));
        p.drawRoundedRect(r, 3, 3);

        /* Left accent stripe */
        if (selected_) {
            QRectF stripe(r.left() + 1, r.top() + 1, 3, r.height() - 2);
            p.setBrush(accent);
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(stripe, 1, 1);
        }

        /* Mic icon (small silhouette) */
        QRectF iconR(r.left() + 10, r.top() + 6, 30, r.height() - 12);
        drawMicIcon(p, iconR, accent);

        /* Text */
        QFont nf = p.font();
        nf.setPixelSize(11);
        nf.setWeight(QFont::Bold);
        p.setFont(nf);
        p.setPen(selected_ ? QColor("#ffffff") : QColor("#c0d0e0"));
        p.drawText(QRectF(r.left() + 48, r.top() + 5,
                          r.width() - 56, 16),
                   Qt::AlignLeft | Qt::AlignVCenter, name_);

        QFont sf = p.font();
        sf.setPixelSize(9);
        sf.setWeight(QFont::Normal);
        sf.setItalic(true);
        p.setFont(sf);
        p.setPen(selected_ ? QColor(accent.red(), accent.green(),
                                     accent.blue(), 220)
                            : QColor("#708090"));
        p.drawText(QRectF(r.left() + 48, r.top() + 22,
                          r.width() - 56, 14),
                   Qt::AlignLeft | Qt::AlignVCenter, subtitle_);
    }

private:
    void drawMicIcon(QPainter& p, const QRectF& r, const QColor& accent)
    {
        /* Small stylized mic icon */
        QRectF grille(r.center().x() - 8, r.top() + 2, 16, 16);
        QRadialGradient gg(grille.center(), 10);
        gg.setColorAt(0.0, accent.lighter(130));
        gg.setColorAt(1.0, accent.darker(180));
        p.setBrush(gg);
        p.setPen(QPen(accent.darker(200), 0.8));
        p.drawEllipse(grille);

        // mesh
        p.setPen(QPen(accent.darker(150), 0.5));
        for (int i = -6; i <= 6; i += 2) {
            p.drawLine(QPointF(grille.center().x() + i, grille.top() + 2),
                       QPointF(grille.center().x() + i, grille.bottom() - 2));
        }

        // stand
        QRectF stand(r.center().x() - 2, grille.bottom(), 4, r.bottom() - grille.bottom());
        QLinearGradient sg(stand.topLeft(), stand.topRight());
        sg.setColorAt(0.0, QColor("#404040"));
        sg.setColorAt(0.5, QColor("#808080"));
        sg.setColorAt(1.0, QColor("#404040"));
        p.setBrush(sg);
        p.setPen(Qt::NoPen);
        p.drawRect(stand);
    }

    int     modelIndex_;
    QString name_;
    QString subtitle_;
    bool    selected_ = false;
    bool    hover_    = false;
};

/* ════════════════════════════════════════════════════════════════════
 *  MicModelerDialog — the main dialog
 * ════════════════════════════════════════════════════════════════════ */

class MicModelerDialog : public QDialog {
    Q_OBJECT

public:
    explicit MicModelerDialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : QDialog(parent), fx_(fx)
    {
        setWindowTitle("Mic Modeler");
        setMinimumSize(825, 488);
        resize(825, 488);
        setAttribute(Qt::WA_DeleteOnClose, false);
        setStyleSheet(kDialogStyle);

        auto* root = new QVBoxLayout(this);
        root->setSpacing(8);
        root->setContentsMargins(14, 12, 14, 12);

        /* ── Header ────────────────────────────────────────────── */
        root->addLayout(buildHeader());

        /* ── Main body: 3-column layout ────────────────────────── */
        auto* bodyLayout = new QHBoxLayout;
        bodyLayout->setSpacing(10);

        bodyLayout->addLayout(buildMicSelector(), 2);
        bodyLayout->addWidget(buildCenterPanel(), 3);
        bodyLayout->addWidget(buildSettingsPanel(), 3);

        root->addLayout(bodyLayout, 0);
        root->addStretch(1);;

        /* ── Footer buttons ───────────────────────────────────── */
        root->addLayout(buildFooter());

        /* Connect knobs to effect params */
        connectKnob(knobInputGain_,    3);
        connectKnob(knobProximity_,    1);
        connectKnob(knobAxis_,         2);
        connectKnob(knobTubeColor_,    7);
        connectKnob(knobBodyRes_,      8);
        connectKnob(knobLowCut_,       6);
        connectKnob(knobOutput_,       9);

        /* Fat switch & HF contour via combos */
        connect(fatSwitch_, &QPushButton::toggled, this, [this](bool on) {
            if (fx_) fx_->setParamValue(4, on ? 1.0f : 0.0f);
        });
        connect(hfContourCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int idx) {
            if (fx_) fx_->setParamValue(5, idx / 3.0f);
        });

        /* Initial read from effect */
        readAllParams();
        refreshPresetList();

        /* Display refresh timer */
        displayTimer_ = new QTimer(this);
        connect(displayTimer_, &QTimer::timeout, this,
                &MicModelerDialog::updateDisplays);
        displayTimer_->start(50);
    }

private slots:
    void updateDisplays()
    {
        if (!fx_) return;
        updateKnobText(knobInputGain_, 3);
        updateKnobText(knobProximity_, 1);
        updateKnobText(knobAxis_,      2);
        updateKnobText(knobTubeColor_, 7);
        updateKnobText(knobBodyRes_,   8);
        updateKnobText(knobLowCut_,    6);
        updateKnobText(knobOutput_,    9);

        /* Update axis on polar pattern */
        polarWidget_->setAxisAmount(fx_->paramValue(2));
    }

    void onMicSelected(int modelIndex)
    {
        if (!fx_) return;
        fx_->setParamValue(0, modelIndex / 11.0f);
        applyMicSelection(modelIndex);
    }

    void onApply()
    {
        /* Force a re-read for any linked params */
        readAllParams();
    }

    void onReset()
    {
        if (!fx_) return;
        fx_->setParamValue(1, 0.30f); // proximity
        fx_->setParamValue(2, 0.00f); // axis
        fx_->setParamValue(3, 0.25f); // input gain
        fx_->setParamValue(4, 0.00f); // fat
        fx_->setParamValue(5, 2.0f / 3.0f); // hf contour = flat
        fx_->setParamValue(6, 0.00f); // low cut
        fx_->setParamValue(7, 0.40f); // tube color
        fx_->setParamValue(8, 0.50f); // body resonance
        fx_->setParamValue(9, 0.50f); // output
        readAllParams();
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
    /* ── Layout builders ─────────────────────────────────────── */

    QHBoxLayout* buildHeader()
    {
        auto* headerLayout = new QHBoxLayout;

        auto* titleBlock = new QVBoxLayout;
        auto* titleLabel = new QLabel("MIC MODELER");
        titleLabel->setStyleSheet(
            "font-size: 24px; font-weight: bold; color: #e8e8e8;"
            "font-family: 'Helvetica Neue', 'Arial Black', sans-serif;"
            "letter-spacing: 3px;");
        titleBlock->addWidget(titleLabel);

        auto* subtitleLabel = new QLabel("Studio Microphone Emulation");
        subtitleLabel->setStyleSheet(
            "font-size: 11px; color: #708090; font-style: italic;"
            "letter-spacing: 1px;");
        titleBlock->addWidget(subtitleLabel);
        titleBlock->setSpacing(0);

        headerLayout->addLayout(titleBlock);
        headerLayout->addStretch();

        auto* presetLabel = new QLabel("Preset:");
        presetLabel->setStyleSheet("font-size: 11px; color: #90a0b0;");
        headerLayout->addWidget(presetLabel);

        presetCombo_ = new QComboBox;
        presetCombo_->setFixedWidth(135);
        presetCombo_->setStyleSheet(kComboStyle);
        headerLayout->addWidget(presetCombo_);

        auto* loadBtn = new QPushButton("Load");
        loadBtn->setFixedWidth(42);
        loadBtn->setStyleSheet(kButtonStyle);
        connect(loadBtn, &QPushButton::clicked, this, &MicModelerDialog::loadPreset);
        headerLayout->addWidget(loadBtn);

        auto* saveBtn = new QPushButton("Save");
        saveBtn->setFixedWidth(42);
        saveBtn->setStyleSheet(kButtonStyle);
        connect(saveBtn, &QPushButton::clicked, this, &MicModelerDialog::savePreset);
        headerLayout->addWidget(saveBtn);

        return headerLayout;
    }

    QVBoxLayout* buildMicSelector()
    {
        auto* col = new QVBoxLayout;
        col->setSpacing(6);

        auto* label = new QLabel("MIC LIBRARY");
        label->setStyleSheet(
            "font-size: 10px; font-weight: bold; color: #00d4aa;"
            "letter-spacing: 2px; padding: 2px;");
        label->setAlignment(Qt::AlignCenter);
        col->addWidget(label);

        micTabs_ = new QTabWidget;
        micTabs_->setStyleSheet(kTabStyle);

        /* Tab 1: Tube Condensers */
        auto* tubeTab = new QWidget;
        auto* tubeLay = new QVBoxLayout(tubeTab);
        tubeLay->setContentsMargins(4, 6, 4, 4);
        tubeLay->setSpacing(4);
        addMicTile(tubeLay, 0, "Bock 167",  "K67 capsule, EF732 tube");
        addMicTile(tubeLay, 1, "U47 Tube",  "VF14 pentode, warm");
        addMicTile(tubeLay, 2, "C12 Tube",  "6072 tube, brilliant");
        addMicTile(tubeLay, 3, "U67 Tube",  "EF86, smooth mids");
        tubeLay->addStretch();
        micTabs_->addTab(tubeTab, "Tubes");

        /* Tab 2: Dynamics */
        auto* dynTab = new QWidget;
        auto* dynLay = new QVBoxLayout(dynTab);
        dynLay->setContentsMargins(4, 6, 4, 4);
        dynLay->setSpacing(4);
        addMicTile(dynLay, 4,  "DN-7",   "Broadcast cylindrical");
        addMicTile(dynLay, 5,  "DN-20",  "Variable-D broadcast");
        addMicTile(dynLay, 6,  "DN-88",  "Smooth, flattering");
        addMicTile(dynLay, 7,  "DN-441", "Supercardioid");
        addMicTile(dynLay, 11, "DN-421", "Hypercardioid");
        dynLay->addStretch();
        micTabs_->addTab(dynTab, "Dynamics");

        /* Tab 3: Ribbons */
        auto* ribTab = new QWidget;
        auto* ribLay = new QVBoxLayout(ribTab);
        ribLay->setContentsMargins(4, 6, 4, 4);
        ribLay->setSpacing(4);
        addMicTile(ribLay, 8,  "RB-77DX",   "Vintage broadcast");
        addMicTile(ribLay, 9,  "RB-160",    "Mid-forward rock");
        addMicTile(ribLay, 10, "Royer 121", "Modern detailed");
        ribLay->addStretch();
        micTabs_->addTab(ribTab, "Ribbons");

        col->addWidget(micTabs_, 1);
        return col;
    }

    QWidget* buildCenterPanel()
    {
        auto* frame = new QFrame;
        frame->setStyleSheet(
            "QFrame {"
            "  background: qlineargradient(y1:0, y2:1,"
            "    stop:0 #141c28, stop:1 #0a0e14);"
            "  border: 1px solid #2a3a4c;"
            "  border-radius: 4px;"
            "}");

        auto* col = new QVBoxLayout(frame);
        col->setContentsMargins(10, 10, 10, 10);
        col->setSpacing(8);

        micGraphic_ = new MicGraphic;
        col->addWidget(micGraphic_, 1, Qt::AlignCenter);

        /* Polar + response row */
        auto* lowerRow = new QHBoxLayout;
        lowerRow->setSpacing(8);

        polarWidget_ = new PolarPatternWidget;
        polarWidget_->setFixedSize(105, 105);
        lowerRow->addWidget(polarWidget_);

        responseCurve_ = new MicResponseCurve;
        responseCurve_->setMinimumHeight(105);
        lowerRow->addWidget(responseCurve_, 1);

        col->addLayout(lowerRow);

        return frame;
    }

    QWidget* buildSettingsPanel()
    {
        auto* frame = new QFrame;
        frame->setStyleSheet(
            "QFrame {"
            "  background: qlineargradient(y1:0, y2:1,"
            "    stop:0 #141c28, stop:1 #0a0e14);"
            "  border: 1px solid #2a3a4c;"
            "  border-radius: 4px;"
            "}");

        auto* col = new QVBoxLayout(frame);
        col->setContentsMargins(10, 10, 10, 10);
        col->setSpacing(6);

        auto* title = new QLabel("SETTINGS");
        title->setStyleSheet(
            "font-size: 10px; font-weight: bold; color: #00d4aa;"
            "letter-spacing: 2px; padding: 2px;");
        title->setAlignment(Qt::AlignCenter);
        col->addWidget(title);

        /* Section 1: Position */
        auto* posGroup = createSection("POSITION");
        auto* posLay = new QHBoxLayout;
        posLay->setSpacing(8);
        posLay->setContentsMargins(6, 4, 6, 6);
        knobProximity_ = createKnob("Proximity", QColor("#00d4aa"));
        knobAxis_      = createKnob("Axis",      QColor("#00d4aa"));
        posLay->addWidget(knobProximity_);
        posLay->addWidget(knobAxis_);
        posLay->addStretch();
        static_cast<QVBoxLayout*>(posGroup->layout())->addLayout(posLay);
        col->addWidget(posGroup);

        /* Section 2: Tone */
        auto* toneGroup = createSection("TONE");
        auto* toneLay = new QHBoxLayout;
        toneLay->setSpacing(8);
        toneLay->setContentsMargins(6, 4, 6, 6);
        knobTubeColor_ = createKnob("Color",    QColor("#D4AF37"));
        knobBodyRes_   = createKnob("Body",     QColor("#D4AF37"));
        knobLowCut_    = createKnob("Low Cut",  QColor("#D4AF37"));
        toneLay->addWidget(knobTubeColor_);
        toneLay->addWidget(knobBodyRes_);
        toneLay->addWidget(knobLowCut_);
        static_cast<QVBoxLayout*>(toneGroup->layout())->addLayout(toneLay);
        col->addWidget(toneGroup);

        /* Section 3: Switches */
        auto* swGroup = createSection("CHARACTER");
        auto* swLay = new QHBoxLayout;
        swLay->setSpacing(8);
        swLay->setContentsMargins(6, 4, 6, 6);

        fatSwitch_ = new QPushButton("FAT");
        fatSwitch_->setCheckable(true);
        fatSwitch_->setFixedSize(45, 30);
        fatSwitch_->setStyleSheet(kToggleStyle);
        swLay->addWidget(fatSwitch_);

        auto* hfBox = new QVBoxLayout;
        hfBox->setSpacing(2);
        auto* hfLbl = new QLabel("HF Contour");
        hfLbl->setStyleSheet("font-size: 9px; color: #90a0b0;");
        hfLbl->setAlignment(Qt::AlignCenter);
        hfBox->addWidget(hfLbl);
        hfContourCombo_ = new QComboBox;
        hfContourCombo_->addItem("Cut 5kHz");
        hfContourCombo_->addItem("Cut 10kHz");
        hfContourCombo_->addItem("Flat");
        hfContourCombo_->addItem("Boost 10kHz");
        hfContourCombo_->setStyleSheet(kComboStyle);
        hfContourCombo_->setFixedWidth(90);
        hfBox->addWidget(hfContourCombo_);
        swLay->addLayout(hfBox);
        swLay->addStretch();
        static_cast<QVBoxLayout*>(swGroup->layout())->addLayout(swLay);
        col->addWidget(swGroup);

        /* Section 4: Gain */
        auto* gainGroup = createSection("GAIN");
        auto* gainLay = new QHBoxLayout;
        gainLay->setSpacing(8);
        gainLay->setContentsMargins(6, 4, 6, 6);
        knobInputGain_ = createKnob("Input",  QColor("#00d4aa"));
        knobOutput_    = createKnob("Output", QColor("#00d4aa"));
        gainLay->addWidget(knobInputGain_);
        gainLay->addWidget(knobOutput_);
        gainLay->addStretch();
        static_cast<QVBoxLayout*>(gainGroup->layout())->addLayout(gainLay);
        col->addWidget(gainGroup);

        col->addStretch();
        return frame;
    }

    QHBoxLayout* buildFooter()
    {
        auto* footer = new QHBoxLayout;
        footer->addStretch();

        auto* applyBtn = new QPushButton("Apply");
        applyBtn->setFixedWidth(68);
        applyBtn->setFixedHeight(21);
        applyBtn->setStyleSheet(kPrimaryButtonStyle);
        connect(applyBtn, &QPushButton::clicked, this, &MicModelerDialog::onApply);
        footer->addWidget(applyBtn);

        auto* resetBtn = new QPushButton("Reset");
        resetBtn->setFixedWidth(68);
        resetBtn->setFixedHeight(21);
        resetBtn->setStyleSheet(kButtonStyle);
        connect(resetBtn, &QPushButton::clicked, this, &MicModelerDialog::onReset);
        footer->addWidget(resetBtn);

        auto* closeBtn = new QPushButton("Close");
        closeBtn->setFixedWidth(68);
        closeBtn->setFixedHeight(21);
        closeBtn->setStyleSheet(kButtonStyle);
        connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
        footer->addWidget(closeBtn);

        return footer;
    }

    /* ── Helpers ─────────────────────────────────────────────── */

    void addMicTile(QVBoxLayout* lay, int modelIndex, const QString& name,
                     const QString& subtitle)
    {
        auto* tile = new MicTile(modelIndex, name, subtitle);
        connect(tile, &MicTile::clicked, this, &MicModelerDialog::onMicSelected);
        micTiles_.append(tile);
        lay->addWidget(tile);
    }

    QGroupBox* createSection(const QString& title)
    {
        auto* group = new QGroupBox(title);
        group->setStyleSheet(kGroupStyle);
        auto* layout = new QVBoxLayout(group);
        layout->setSpacing(3);
        layout->setContentsMargins(4, 16, 4, 4);
        return group;
    }

    RackKnob* createKnob(const QString& title, const QColor& accent)
    {
        auto* knob = new RackKnob;
        knob->setTitle(title);
        knob->setAccentColor(accent);
        knob->setNotches(11);
        knob->setFixedSize(60, 82);
        return knob;
    }

    void connectKnob(RackKnob* knob, int paramIndex)
    {
        connect(knob, &RackKnob::valueChanged, this,
                [this, paramIndex](float v) {
            if (fx_) fx_->setParamValue(paramIndex, v);
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

        /* Determine current mic model from effect */
        float modelNorm = fx_->paramValue(0);
        int modelIdx = static_cast<int>(std::round(modelNorm * 11.0f));
        modelIdx = std::clamp(modelIdx, 0, 11);
        applyMicSelection(modelIdx);

        auto readKnob = [this](RackKnob* knob, int idx) {
            knob->blockSignals(true);
            knob->setValue(fx_->paramValue(idx));
            knob->blockSignals(false);
            updateKnobText(knob, idx);
        };

        readKnob(knobProximity_, 1);
        readKnob(knobAxis_,      2);
        readKnob(knobInputGain_, 3);
        readKnob(knobLowCut_,    6);
        readKnob(knobTubeColor_, 7);
        readKnob(knobBodyRes_,   8);
        readKnob(knobOutput_,    9);

        /* Fat switch (param 4) */
        bool fat = fx_->paramValue(4) > 0.5f;
        fatSwitch_->blockSignals(true);
        fatSwitch_->setChecked(fat);
        fatSwitch_->blockSignals(false);

        /* HF contour (param 5, 0..1 -> 0..3) */
        int hfIdx = static_cast<int>(std::round(fx_->paramValue(5) * 3.0f));
        hfIdx = std::clamp(hfIdx, 0, 3);
        hfContourCombo_->blockSignals(true);
        hfContourCombo_->setCurrentIndex(hfIdx);
        hfContourCombo_->blockSignals(false);
    }

    void applyMicSelection(int modelIndex)
    {
        modelIndex = std::clamp(modelIndex, 0, 11);

        /* Update highlight in tiles */
        for (auto* tile : micTiles_) {
            tile->setSelected(tile->modelIndex() == modelIndex);
        }

        /* Switch to appropriate tab */
        int targetTab = 0;
        if (modelIndex <= 3)        targetTab = 0;
        else if (modelIndex <= 7)   targetTab = 1;
        else if (modelIndex == 11)  targetTab = 1;
        else                        targetTab = 2;

        if (micTabs_->currentIndex() != targetTab) {
            micTabs_->blockSignals(true);
            micTabs_->setCurrentIndex(targetTab);
            micTabs_->blockSignals(false);
        }

        /* Update visualizations */
        micGraphic_->setMicModel(modelIndex);
        polarWidget_->setPatternForMic(modelIndex);
        responseCurve_->setMicModel(modelIndex);

        /* Update knob accent colors based on category */
        QColor accent;
        if (modelIndex <= 3)        accent = QColor("#D4AF37");
        else if (modelIndex <= 7)   accent = QColor("#4A9BD9");
        else if (modelIndex == 11)  accent = QColor("#4A9BD9");
        else                        accent = QColor("#B87333");

        knobTubeColor_->setAccentColor(accent);
        knobBodyRes_->setAccentColor(accent);
        knobLowCut_->setAccentColor(accent);
    }

    void refreshPresetList()
    {
        presetCombo_->clear();
        if (!fx_) return;
        presets_ = mc1dsp::PresetManager::listPresets(
            QString::fromLatin1(fx_->id()));
        for (const auto& p : presets_) {
            QString label = p.isFactory ? p.name + "  [F]" : p.name;
            presetCombo_->addItem(label);
        }
    }

    /* ── Style constants ─────────────────────────────────────── */

    static constexpr const char* kDialogStyle =
        "QDialog {"
        "  background: qlineargradient(y1:0, y2:1,"
        "    stop:0 #0a0e14, stop:0.5 #0d1420, stop:1 #141a24);"
        "}"
        "QLabel { color: #c0d0e0; }";

    static constexpr const char* kGroupStyle =
        "QGroupBox {"
        "  background: qlineargradient(y1:0, y2:1,"
        "    stop:0 #1a2233, stop:1 #121a26);"
        "  border: 1px solid #2a3a4c;"
        "  border-radius: 4px;"
        "  font-size: 9px;"
        "  font-weight: bold;"
        "  color: #00d4aa;"
        "  padding-top: 14px;"
        "  margin-top: 4px;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  subcontrol-position: top center;"
        "  padding: 2px 10px;"
        "  background: #0d1420;"
        "  border: 1px solid #2a3a4c;"
        "  border-radius: 3px;"
        "  letter-spacing: 2px;"
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
        "  padding: 4px 10px;"
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

    static constexpr const char* kPrimaryButtonStyle =
        "QPushButton {"
        "  background: qlineargradient(y1:0, y2:1,"
        "    stop:0 #00e0b8, stop:1 #00a088);"
        "  border: 1px solid #00d4aa;"
        "  border-radius: 3px;"
        "  color: #0a0e14;"
        "  padding: 4px 10px;"
        "  font-size: 11px;"
        "  font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "  background: qlineargradient(y1:0, y2:1,"
        "    stop:0 #00f0c0, stop:1 #00b090);"
        "}"
        "QPushButton:pressed {"
        "  background: #008870;"
        "}";

    static constexpr const char* kToggleStyle =
        "QPushButton {"
        "  background: qlineargradient(y1:0, y2:1,"
        "    stop:0 #1e2a3a, stop:1 #141c28);"
        "  border: 1px solid #2a3a4c;"
        "  border-radius: 4px;"
        "  color: #708090;"
        "  font-size: 10px;"
        "  font-weight: bold;"
        "  letter-spacing: 1px;"
        "}"
        "QPushButton:hover {"
        "  border-color: #00d4aa;"
        "}"
        "QPushButton:checked {"
        "  background: qlineargradient(y1:0, y2:1,"
        "    stop:0 #00e0b8, stop:1 #00a088);"
        "  color: #0a0e14;"
        "  border-color: #00d4aa;"
        "}";

    static constexpr const char* kTabStyle =
        "QTabWidget::pane {"
        "  background: qlineargradient(y1:0, y2:1,"
        "    stop:0 #141c28, stop:1 #0a0e14);"
        "  border: 1px solid #2a3a4c;"
        "  border-radius: 3px;"
        "  top: -1px;"
        "}"
        "QTabBar::tab {"
        "  background: #141c28;"
        "  border: 1px solid #2a3a4c;"
        "  border-bottom: none;"
        "  border-top-left-radius: 3px;"
        "  border-top-right-radius: 3px;"
        "  color: #708090;"
        "  padding: 5px 14px;"
        "  font-size: 10px;"
        "  font-weight: bold;"
        "  letter-spacing: 1px;"
        "}"
        "QTabBar::tab:selected {"
        "  background: qlineargradient(y1:0, y2:1,"
        "    stop:0 #1e2a3a, stop:1 #141c28);"
        "  color: #00d4aa;"
        "  border-color: #00d4aa;"
        "}"
        "QTabBar::tab:hover:!selected {"
        "  background: #1a2233;"
        "  color: #c0d0e0;"
        "}";

    /* ── Members ─────────────────────────────────────────────── */

    mc1dsp::DspEffect* fx_ = nullptr;
    QTimer* displayTimer_ = nullptr;

    /* Header */
    QComboBox* presetCombo_ = nullptr;
    QVector<mc1dsp::Preset> presets_;

    /* Mic selector */
    QTabWidget* micTabs_ = nullptr;
    QVector<MicTile*> micTiles_;

    /* Center visuals */
    MicGraphic*          micGraphic_    = nullptr;
    PolarPatternWidget*  polarWidget_   = nullptr;
    MicResponseCurve*    responseCurve_ = nullptr;

    /* Settings knobs */
    RackKnob* knobInputGain_ = nullptr;
    RackKnob* knobProximity_ = nullptr;
    RackKnob* knobAxis_      = nullptr;
    RackKnob* knobTubeColor_ = nullptr;
    RackKnob* knobBodyRes_   = nullptr;
    RackKnob* knobLowCut_    = nullptr;
    RackKnob* knobOutput_    = nullptr;

    /* Character switches */
    QPushButton* fatSwitch_      = nullptr;
    QComboBox*   hfContourCombo_ = nullptr;
};
