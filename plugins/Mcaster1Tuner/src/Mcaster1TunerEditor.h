/*
 * Mcaster1Tuner — VST3 Chromatic Tuner Plugin
 * src/Mcaster1TunerEditor.h — Custom VSTGUI editor view
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "Mcaster1Tuner.h"
#include "TuningTables.h"

#include "vstgui/vstgui.h"
#include "vstgui/lib/cvstguitimer.h"
#include "public.sdk/source/vst/vsteditcontroller.h"

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace Mcaster1 {

// ===================================================================
//  Brand colors
// ===================================================================

namespace Colors {
    static const VSTGUI::CColor Navy   (0x0d, 0x23, 0x42);
    static const VSTGUI::CColor NavyLt (0x14, 0x30, 0x58);
    static const VSTGUI::CColor Gold   (0xff, 0xc4, 0x2e);
    static const VSTGUI::CColor GoldDim(0xb0, 0x88, 0x1e);
    static const VSTGUI::CColor Mint   (0x1d, 0xe9, 0xb6);
    static const VSTGUI::CColor Blue   (0x0e, 0xa5, 0xe9);
    static const VSTGUI::CColor White  (0xff, 0xff, 0xff);
    static const VSTGUI::CColor WhiteDim(0x88, 0x99, 0xaa);
    static const VSTGUI::CColor Black  (0x00, 0x00, 0x00);
    static const VSTGUI::CColor Green  (0x22, 0xcc, 0x44);
    static const VSTGUI::CColor Yellow (0xee, 0xbb, 0x22);
    static const VSTGUI::CColor Red    (0xdd, 0x33, 0x33);
    static const VSTGUI::CColor LCDBg  (0x08, 0x14, 0x28);
    static const VSTGUI::CColor LCDBorder(0x1a, 0x3a, 0x5a);
    static const VSTGUI::CColor ConfBar (0x0e, 0xa5, 0xe9, 0xcc);
    static const VSTGUI::CColor ConfBg  (0x08, 0x14, 0x28, 0x80);
}

// ===================================================================
//  Editor dimensions
// ===================================================================

static constexpr int kEditorWidth  = 460;
static constexpr int kEditorHeight = 600;

// ===================================================================
//  TunerView — custom CView with full QPainter-style drawing
// ===================================================================

class TunerView : public VSTGUI::CView
{
public:
    TunerView(const VSTGUI::CRect& size,
              Steinberg::Vst::EditController* controller)
        : CView(size)
        , m_controller(controller)
    {
        m_pitch.store(0.0f, std::memory_order_relaxed);
        m_confidence.store(0.0f, std::memory_order_relaxed);
        m_cents.store(0.0f, std::memory_order_relaxed);
        m_noteName[0] = '-';
        m_noteName[1] = '\0';
        m_octave.store(0, std::memory_order_relaxed);
    }

    ~TunerView() override
    {
        if (m_timer) {
            m_timer->stop();
            m_timer->forget();
            m_timer = nullptr;
        }
    }

    // ---------------------------------------------------------------
    //  Pitch data setters (called from controller message handler)
    // ---------------------------------------------------------------

    void setPitchData(float pitch, float confidence, float cents,
                      const char* noteName, int octave)
    {
        m_pitch.store(pitch, std::memory_order_relaxed);
        m_confidence.store(confidence, std::memory_order_relaxed);
        m_cents.store(cents, std::memory_order_relaxed);
        m_octave.store(octave, std::memory_order_relaxed);

        // Copy note name (up to 3 chars + null)
        size_t len = std::strlen(noteName);
        if (len > 3) len = 3;
        std::memcpy(m_noteName, noteName, len);
        m_noteName[len] = '\0';
    }

    // ---------------------------------------------------------------
    //  Timer management
    // ---------------------------------------------------------------

    void startTimer()
    {
        if (!m_timer) {
            m_timer = new VSTGUI::CVSTGUITimer(
                [this](VSTGUI::CVSTGUITimer*) {
                    invalid();
                },
                33,    // ~30 fps
                true);
        }
    }

    void stopTimer()
    {
        if (m_timer) {
            m_timer->stop();
            m_timer->forget();
            m_timer = nullptr;
        }
    }

    // ---------------------------------------------------------------
    //  Draw
    // ---------------------------------------------------------------

    void draw(VSTGUI::CDrawContext* ctx) override
    {
        const auto& r = getViewSize();
        const auto cx = r.left + r.getWidth() * 0.5;
        const auto cy = r.top + r.getHeight() * 0.5;

        drawBackground(ctx, r);
        drawTuningArc(ctx, r, cx);
        drawNeedle(ctx, r, cx);
        drawNoteLCD(ctx, r, cx);
        drawCentReadout(ctx, r, cx);
        drawInTuneIndicator(ctx, r, cx);
        drawPresetName(ctx, r, cx);
        drawConfidenceBar(ctx, r);
        drawBrandFooter(ctx, r, cx);

        setDirty(false);
    }

private:
    // ---------------------------------------------------------------
    //  Background with radial gradient
    // ---------------------------------------------------------------

    void drawBackground(VSTGUI::CDrawContext* ctx, const VSTGUI::CRect& r)
    {
        // Solid fill first
        ctx->setFillColor(Colors::Navy);
        ctx->drawRect(r, VSTGUI::kDrawFilled);

        // Radial gradient overlay
        auto* path = ctx->createGraphicsPath();
        if (path) {
            path->addEllipse(r);
            auto* grad = path->createGradient(0.0, 1.0,
                Colors::NavyLt, Colors::Navy);
            if (grad) {
                VSTGUI::CPoint center(
                    r.left + r.getWidth() * 0.5,
                    r.top + r.getHeight() * 0.35);
                ctx->fillRadialGradient(path, *grad, center,
                    r.getWidth() * 0.7);
                grad->forget();
            }
            path->forget();
        }

        // Subtle outer border
        ctx->setFrameColor(Colors::GoldDim);
        ctx->setLineWidth(1.5);
        VSTGUI::CRect inner(r);
        inner.inset(1, 1);
        ctx->drawRect(inner, VSTGUI::kDrawStroked);
    }

    // ---------------------------------------------------------------
    //  Tuning arc — semicircular gauge
    // ---------------------------------------------------------------

    void drawTuningArc(VSTGUI::CDrawContext* ctx, const VSTGUI::CRect& r,
                       double cx)
    {
        const double arcCenterY = r.top + 220;
        const double arcRadius  = 150;

        // Arc bounding rect
        VSTGUI::CRect arcRect(
            cx - arcRadius, arcCenterY - arcRadius,
            cx + arcRadius, arcCenterY + arcRadius);

        // The arc sweeps from 210 deg (left) to 330 deg (right)
        // VSTGUI angles: 0=right, going clockwise

        // Draw colored zone arcs (background track)
        const double startAngle = 210.0;
        const double endAngle   = 330.0;
        const double totalSweep = endAngle - startAngle; // 120 degrees

        // Red zone (full arc, drawn first as backdrop)
        ctx->setLineWidth(12);
        ctx->setFrameColor(VSTGUI::CColor(0xdd, 0x33, 0x33, 0x50));
        drawArcSegment(ctx, arcRect, startAngle, endAngle);

        // Yellow zone: -15..+15 cents = center 30%
        double yellowStart = startAngle + totalSweep * 0.2917;  // -15c
        double yellowEnd   = startAngle + totalSweep * 0.7083;  // +15c
        ctx->setFrameColor(VSTGUI::CColor(0xee, 0xbb, 0x22, 0x60));
        drawArcSegment(ctx, arcRect, yellowStart, yellowEnd);

        // Green zone: -5..+5 cents = center 10%
        double greenStart = startAngle + totalSweep * 0.4167;   // -5c
        double greenEnd   = startAngle + totalSweep * 0.5833;   // +5c
        ctx->setFrameColor(VSTGUI::CColor(0x22, 0xcc, 0x44, 0x80));
        drawArcSegment(ctx, arcRect, greenStart, greenEnd);

        // Bright gold arc outline
        ctx->setLineWidth(2);
        ctx->setFrameColor(Colors::Gold);
        drawArcSegment(ctx, arcRect, startAngle, endAngle);

        // Tick marks
        drawTickMarks(ctx, cx, arcCenterY, arcRadius, startAngle,
                      totalSweep);

        // Center dot
        ctx->setFillColor(Colors::Gold);
        VSTGUI::CRect dot(cx - 4, arcCenterY - 4, cx + 4, arcCenterY + 4);
        ctx->drawEllipse(dot, VSTGUI::kDrawFilled);
    }

    void drawArcSegment(VSTGUI::CDrawContext* ctx,
                        const VSTGUI::CRect& arcRect,
                        double startDeg, double endDeg)
    {
        auto* path = ctx->createGraphicsPath();
        if (path) {
            path->addArc(arcRect, startDeg, endDeg, true);
            ctx->drawGraphicsPath(path,
                VSTGUI::CDrawContext::kPathStroked);
            path->forget();
        }
    }

    void drawTickMarks(VSTGUI::CDrawContext* ctx, double cx, double cy,
                       double radius, double startAngle, double totalSweep)
    {
        ctx->setLineWidth(1);
        // Major ticks at -50, -25, 0, +25, +50
        for (int i = 0; i <= 4; ++i) {
            double frac = static_cast<double>(i) / 4.0;
            double angle = (startAngle + totalSweep * frac) * M_PI / 180.0;
            double innerR = radius - 20;
            double outerR = radius + 8;

            VSTGUI::CPoint inner(
                cx + innerR * std::cos(angle),
                cy + innerR * std::sin(angle));
            VSTGUI::CPoint outer(
                cx + outerR * std::cos(angle),
                cy + outerR * std::sin(angle));

            ctx->setFrameColor((i == 2) ? Colors::Mint : Colors::GoldDim);
            ctx->setLineWidth((i == 2) ? 2.5 : 1.5);
            ctx->drawLine(inner, outer);
        }

        // Minor ticks every 5 cents (20 segments)
        ctx->setFrameColor(VSTGUI::CColor(0x66, 0x77, 0x88));
        ctx->setLineWidth(0.8);
        for (int i = 0; i <= 20; ++i) {
            if (i % 5 == 0) continue; // skip majors
            double frac = static_cast<double>(i) / 20.0;
            double angle = (startAngle + totalSweep * frac) * M_PI / 180.0;
            double innerR = radius - 10;
            double outerR = radius + 4;

            VSTGUI::CPoint inner(
                cx + innerR * std::cos(angle),
                cy + innerR * std::sin(angle));
            VSTGUI::CPoint outer(
                cx + outerR * std::cos(angle),
                cy + outerR * std::sin(angle));

            ctx->drawLine(inner, outer);
        }
    }

    // ---------------------------------------------------------------
    //  Needle
    // ---------------------------------------------------------------

    void drawNeedle(VSTGUI::CDrawContext* ctx, const VSTGUI::CRect& r,
                    double cx)
    {
        const double arcCenterY = r.top + 220;
        const double needleLen  = 130;
        const float  cents = m_cents.load(std::memory_order_relaxed);
        const float  conf  = m_confidence.load(std::memory_order_relaxed);

        // Map cents (-50..+50) to angle (210..330 degrees)
        double clampedCents = cents;
        if (clampedCents < -50.0) clampedCents = -50.0;
        if (clampedCents > 50.0)  clampedCents = 50.0;

        double frac = (clampedCents + 50.0) / 100.0;
        double angleDeg = 210.0 + frac * 120.0;
        double angleRad = angleDeg * M_PI / 180.0;

        VSTGUI::CPoint tip(
            cx + needleLen * std::cos(angleRad),
            arcCenterY + needleLen * std::sin(angleRad));

        // Needle color: green if in tune, gold otherwise
        bool inTune = (std::fabs(cents) <= 5.0f && conf > 0.3f);
        VSTGUI::CColor needleColor = inTune ? Colors::Green : Colors::Gold;

        // If no signal, dim the needle
        if (conf < 0.1f) {
            needleColor = VSTGUI::CColor(0x44, 0x55, 0x66);
        }

        // Draw needle shadow
        ctx->setFrameColor(VSTGUI::CColor(0x00, 0x00, 0x00, 0x60));
        ctx->setLineWidth(4);
        ctx->drawLine(
            VSTGUI::CPoint(cx + 1, arcCenterY + 1),
            VSTGUI::CPoint(tip.x + 1, tip.y + 1));

        // Draw needle
        ctx->setFrameColor(needleColor);
        ctx->setLineWidth(2.5);
        ctx->drawLine(VSTGUI::CPoint(cx, arcCenterY), tip);

        // Needle tip dot
        ctx->setFillColor(needleColor);
        VSTGUI::CRect tipDot(tip.x - 3, tip.y - 3, tip.x + 3, tip.y + 3);
        ctx->drawEllipse(tipDot, VSTGUI::kDrawFilled);
    }

    // ---------------------------------------------------------------
    //  Note name LCD
    // ---------------------------------------------------------------

    void drawNoteLCD(VSTGUI::CDrawContext* ctx, const VSTGUI::CRect& r,
                     double cx)
    {
        const double lcdY = r.top + 340;
        const double lcdW = 160;
        const double lcdH = 70;

        VSTGUI::CRect lcdRect(
            cx - lcdW * 0.5, lcdY,
            cx + lcdW * 0.5, lcdY + lcdH);

        // LCD background
        ctx->setFillColor(Colors::LCDBg);
        auto* path = ctx->createRoundRectGraphicsPath(lcdRect, 8);
        if (path) {
            ctx->drawGraphicsPath(path,
                VSTGUI::CDrawContext::kPathFilled);
            path->forget();
        }

        // LCD border
        ctx->setFrameColor(Colors::LCDBorder);
        ctx->setLineWidth(1.5);
        path = ctx->createRoundRectGraphicsPath(lcdRect, 8);
        if (path) {
            ctx->drawGraphicsPath(path,
                VSTGUI::CDrawContext::kPathStroked);
            path->forget();
        }

        // Note name text
        float conf = m_confidence.load(std::memory_order_relaxed);
        char noteStr[16];
        if (conf > 0.1f) {
            int oct = m_octave.load(std::memory_order_relaxed);
            std::snprintf(noteStr, sizeof(noteStr), "%s%d",
                          m_noteName, oct);
        } else {
            std::snprintf(noteStr, sizeof(noteStr), "--");
        }

        auto* font = new VSTGUI::CFontDesc("Helvetica Neue", 44,
            VSTGUI::kBoldFace);
        ctx->setFont(font);
        ctx->setFontColor(Colors::Gold);
        ctx->drawString(noteStr, lcdRect, VSTGUI::kCenterText);
        font->forget();
    }

    // ---------------------------------------------------------------
    //  Cent readout
    // ---------------------------------------------------------------

    void drawCentReadout(VSTGUI::CDrawContext* ctx, const VSTGUI::CRect& r,
                         double cx)
    {
        const double readoutY = r.top + 418;
        float cents = m_cents.load(std::memory_order_relaxed);
        float conf  = m_confidence.load(std::memory_order_relaxed);

        char centStr[16];
        if (conf > 0.1f) {
            std::snprintf(centStr, sizeof(centStr), "%+.1fc", cents);
        } else {
            std::snprintf(centStr, sizeof(centStr), "0.0c");
        }

        VSTGUI::CRect textRect(cx - 80, readoutY, cx + 80, readoutY + 28);

        auto* font = new VSTGUI::CFontDesc("Helvetica Neue", 22,
            VSTGUI::kBoldFace);
        ctx->setFont(font);

        // Color by deviation
        VSTGUI::CColor col = Colors::WhiteDim;
        if (conf > 0.1f) {
            float absCents = std::fabs(cents);
            if (absCents <= 5.0f)       col = Colors::Green;
            else if (absCents <= 15.0f) col = Colors::Yellow;
            else                        col = Colors::Red;
        }

        ctx->setFontColor(col);
        ctx->drawString(centStr, textRect, VSTGUI::kCenterText);
        font->forget();
    }

    // ---------------------------------------------------------------
    //  In-tune indicator
    // ---------------------------------------------------------------

    void drawInTuneIndicator(VSTGUI::CDrawContext* ctx,
                             const VSTGUI::CRect& r, double cx)
    {
        const double indY = r.top + 450;
        float cents = m_cents.load(std::memory_order_relaxed);
        float conf  = m_confidence.load(std::memory_order_relaxed);
        bool  inTune = (std::fabs(cents) <= 5.0f && conf > 0.3f);

        // Glow circle
        VSTGUI::CRect glowRect(cx - 14, indY, cx + 14, indY + 28);

        if (inTune) {
            // Outer glow
            ctx->setFillColor(VSTGUI::CColor(0x22, 0xcc, 0x44, 0x40));
            VSTGUI::CRect outerGlow(cx - 20, indY - 6, cx + 20, indY + 34);
            ctx->drawEllipse(outerGlow, VSTGUI::kDrawFilled);

            ctx->setFillColor(Colors::Green);
        } else {
            ctx->setFillColor(VSTGUI::CColor(0x33, 0x44, 0x33));
        }
        ctx->drawEllipse(glowRect, VSTGUI::kDrawFilled);

        // Label
        VSTGUI::CRect labelRect(cx - 60, indY + 30, cx + 60, indY + 48);
        auto* font = new VSTGUI::CFontDesc("Helvetica Neue", 12,
            VSTGUI::kNormalFace);
        ctx->setFont(font);
        ctx->setFontColor(inTune ? Colors::Green : Colors::WhiteDim);
        ctx->drawString("IN TUNE", labelRect, VSTGUI::kCenterText);
        font->forget();
    }

    // ---------------------------------------------------------------
    //  Preset name display
    // ---------------------------------------------------------------

    void drawPresetName(VSTGUI::CDrawContext* ctx, const VSTGUI::CRect& r,
                        double cx)
    {
        const double presetY = r.top + 500;

        // Read preset index from controller parameter
        int presetIdx = 0;
        if (m_controller) {
            double norm = m_controller->getParamNormalized(
                kParamPresetIndex);
            presetIdx = static_cast<int>(
                norm * (TuningTables::presetCount() - 1) + 0.5);
            if (presetIdx < 0) presetIdx = 0;
            if (presetIdx >= TuningTables::presetCount())
                presetIdx = TuningTables::presetCount() - 1;
        }

        const auto& preset = TuningTables::preset(presetIdx);

        // Category label
        VSTGUI::CRect catRect(cx - 180, presetY, cx + 180, presetY + 16);
        auto* catFont = new VSTGUI::CFontDesc("Helvetica Neue", 11,
            VSTGUI::kNormalFace);
        ctx->setFont(catFont);
        ctx->setFontColor(Colors::WhiteDim);
        ctx->drawString(preset.category, catRect, VSTGUI::kCenterText);
        catFont->forget();

        // Preset name
        VSTGUI::CRect nameRect(cx - 180, presetY + 18, cx + 180,
                                presetY + 40);
        auto* nameFont = new VSTGUI::CFontDesc("Helvetica Neue", 16,
            VSTGUI::kBoldFace);
        ctx->setFont(nameFont);
        ctx->setFontColor(Colors::Gold);
        ctx->drawString(preset.name, nameRect, VSTGUI::kCenterText);
        nameFont->forget();

        // Concert pitch readout
        double pitchNorm = 0.5;
        if (m_controller) {
            pitchNorm = m_controller->getParamNormalized(
                kParamConcertPitch);
        }
        float concertHz = 430.0f + static_cast<float>(pitchNorm) * 20.0f;
        char hzStr[32];
        std::snprintf(hzStr, sizeof(hzStr), "A4 = %.1f Hz", concertHz);

        VSTGUI::CRect hzRect(cx - 100, presetY + 42, cx + 100,
                              presetY + 56);
        auto* hzFont = new VSTGUI::CFontDesc("Helvetica Neue", 11,
            VSTGUI::kNormalFace);
        ctx->setFont(hzFont);
        ctx->setFontColor(Colors::Blue);
        ctx->drawString(hzStr, hzRect, VSTGUI::kCenterText);
        hzFont->forget();
    }

    // ---------------------------------------------------------------
    //  Confidence bar
    // ---------------------------------------------------------------

    void drawConfidenceBar(VSTGUI::CDrawContext* ctx,
                           const VSTGUI::CRect& r)
    {
        const double barX = r.left + 30;
        const double barY = r.top + 565;
        const double barW = r.getWidth() - 60;
        const double barH = 10;

        float conf = m_confidence.load(std::memory_order_relaxed);

        // Background track
        VSTGUI::CRect bgRect(barX, barY, barX + barW, barY + barH);
        auto* bgPath = ctx->createRoundRectGraphicsPath(bgRect, 5);
        if (bgPath) {
            ctx->setFillColor(Colors::ConfBg);
            ctx->drawGraphicsPath(bgPath,
                VSTGUI::CDrawContext::kPathFilled);
            bgPath->forget();
        }

        // Filled portion
        double fillW = barW * static_cast<double>(conf);
        if (fillW > 1.0) {
            VSTGUI::CRect fillRect(barX, barY, barX + fillW, barY + barH);
            auto* fillPath = ctx->createRoundRectGraphicsPath(fillRect, 5);
            if (fillPath) {
                ctx->setFillColor(Colors::ConfBar);
                ctx->drawGraphicsPath(fillPath,
                    VSTGUI::CDrawContext::kPathFilled);
                fillPath->forget();
            }
        }

        // Label
        VSTGUI::CRect labelRect(barX, barY - 16, barX + barW, barY - 2);
        auto* font = new VSTGUI::CFontDesc("Helvetica Neue", 10,
            VSTGUI::kNormalFace);
        ctx->setFont(font);
        ctx->setFontColor(Colors::WhiteDim);
        char confStr[32];
        std::snprintf(confStr, sizeof(confStr), "CONFIDENCE  %.0f%%",
                      conf * 100.0f);
        ctx->drawString(confStr, labelRect, VSTGUI::kLeftText);
        font->forget();
    }

    // ---------------------------------------------------------------
    //  Brand footer
    // ---------------------------------------------------------------

    void drawBrandFooter(VSTGUI::CDrawContext* ctx, const VSTGUI::CRect& r,
                         double cx)
    {
        // Title at top
        VSTGUI::CRect titleRect(cx - 150, r.top + 12, cx + 150,
                                 r.top + 36);
        auto* titleFont = new VSTGUI::CFontDesc("Helvetica Neue", 16,
            VSTGUI::kBoldFace);
        ctx->setFont(titleFont);
        ctx->setFontColor(Colors::Gold);
        ctx->drawString("MC1 TUNER", titleRect, VSTGUI::kCenterText);
        titleFont->forget();

        // Subtitle
        VSTGUI::CRect subRect(cx - 150, r.top + 36, cx + 150,
                                r.top + 50);
        auto* subFont = new VSTGUI::CFontDesc("Helvetica Neue", 10,
            VSTGUI::kNormalFace);
        ctx->setFont(subFont);
        ctx->setFontColor(Colors::WhiteDim);
        ctx->drawString("CHROMATIC PRECISION TUNER", subRect,
                        VSTGUI::kCenterText);
        subFont->forget();

        // Bottom brand
        VSTGUI::CRect brandRect(cx - 100, r.bottom - 18, cx + 100,
                                 r.bottom - 4);
        auto* brandFont = new VSTGUI::CFontDesc("Helvetica Neue", 9,
            VSTGUI::kNormalFace);
        ctx->setFont(brandFont);
        ctx->setFontColor(VSTGUI::CColor(0x55, 0x66, 0x77));
        ctx->drawString("MC1 Studios", brandRect, VSTGUI::kCenterText);
        brandFont->forget();
    }

    // ---------------------------------------------------------------
    //  Data members
    // ---------------------------------------------------------------

    Steinberg::Vst::EditController* m_controller = nullptr;
    VSTGUI::CVSTGUITimer*           m_timer      = nullptr;

    std::atomic<float> m_pitch;
    std::atomic<float> m_confidence;
    std::atomic<float> m_cents;
    char               m_noteName[4] = {'-', '\0', '\0', '\0'};
    std::atomic<int>   m_octave;
};

} // namespace Mcaster1
