/*
 * Mcaster1DAWCast — MC1 Studios Family
 * fx_ui/SignalHillStudioDialog.h — flagship UI for Signal Hill A
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Top-level studio editor with a custom QPainter-rendered control room
 * view that shows the booth, mic position, console, and outboard rack
 * — visually changes with the user's settings. Plus three knob banks
 * (Booth & Mic / Voice Chain / Output) and an LCD-style status row.
 *
 * Designed to look and feel like the Universal Audio Apollo console
 * plugins — wide, dark, professional, retina-grade.
 */

#pragma once

#include "fx_ui/RackKnob.h"
#include "patchbay/dsp/dsp_effect.h"
#include "patchbay/dsp/fx_mc1_signal_hill_a.h"

#include <QDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QPaintEvent>
#include <QTimer>
#include <QFontMetrics>

/* ─────────────────────────────────────────────────────────────────
 *  SignalHillRoomView — custom QPainter widget that draws an
 *  overhead-isometric view of the studio booth + console + outboard,
 *  with the active mic position highlighted. Updates live as the
 *  user changes Booth and Mic parameters.
 * ───────────────────────────────────────────────────────────────── */
class SignalHillRoomView : public QWidget {
    Q_OBJECT

public:
    explicit SignalHillRoomView(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(510, 135);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void setBoothType(int type)   { m_boothType = type; update(); }
    void setMicType(int type)     { m_micType = type;   update(); }
    void setLoudnessTarget(int t) { m_lufsTarget = t;   update(); }
    void setPhoneLineActive(bool a) { m_phoneLine = a;  update(); }

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);

        const QRect r = rect();
        const int W = r.width();
        const int H = r.height();

        // ── Background gradient (deep night control room) ─────────
        QLinearGradient bgGrad(0, 0, 0, H);
        bgGrad.setColorAt(0.0, QColor(0x0c, 0x14, 0x22));
        bgGrad.setColorAt(0.5, QColor(0x10, 0x1c, 0x30));
        bgGrad.setColorAt(1.0, QColor(0x06, 0x0c, 0x18));
        p.fillRect(r, bgGrad);

        // Subtle horizon line
        p.setPen(QPen(QColor(0x20, 0x40, 0x6a, 80), 1));
        p.drawLine(0, H * 0.55, W, H * 0.55);

        // Soft amber spotlight near the top — feels like control room lighting
        QRadialGradient spot(W * 0.5, -20, W * 0.6);
        spot.setColorAt(0.0, QColor(0xff, 0xb0, 0x20, 36));
        spot.setColorAt(1.0, QColor(0xff, 0xb0, 0x20, 0));
        p.fillRect(r, spot);

        // ── Booth (left side) ─────────────────────────────────────
        const int boothX = 30;
        const int boothY = 30;
        int       boothW = 260;
        int       boothH = H - 60;

        // Booth size grows with the booth type (Tight / Treated / Lounge)
        switch (m_boothType) {
            case 0: boothW = 200; boothH = H - 80; break;  // Tight
            case 1: boothW = 240; boothH = H - 70; break;  // Treated
            case 2: boothW = 280; boothH = H - 60; break;  // Lounge
        }

        // Booth walls (isometric trapezoid)
        QPolygonF booth;
        booth << QPointF(boothX,             boothY)
              << QPointF(boothX + boothW,    boothY + 12)
              << QPointF(boothX + boothW,    boothY + boothH)
              << QPointF(boothX,             boothY + boothH - 12);

        QLinearGradient boothGrad(boothX, boothY, boothX, boothY + boothH);
        boothGrad.setColorAt(0.0, QColor(0x18, 0x28, 0x42));
        boothGrad.setColorAt(0.5, QColor(0x10, 0x1c, 0x30));
        boothGrad.setColorAt(1.0, QColor(0x08, 0x12, 0x20));
        p.setBrush(boothGrad);
        p.setPen(QPen(QColor(0x20, 0x40, 0x6a), 2));
        p.drawPolygon(booth);

        // Acoustic foam pattern (small diamond grid on the back wall)
        p.setPen(QPen(QColor(0x20, 0x40, 0x6a, 100), 1));
        for (int gy = boothY + 18; gy < boothY + boothH - 24; gy += 16) {
            for (int gx = boothX + 16; gx < boothX + boothW - 12; gx += 16) {
                p.drawPoint(gx, gy);
            }
        }

        // Booth label
        p.setPen(QColor(0x60, 0x88, 0xb0));
        QFont small = font();
        small.setPointSize(8);
        small.setBold(true);
        small.setLetterSpacing(QFont::PercentageSpacing, 110);
        p.setFont(small);
        const char* boothNames[3] = { "TIGHT BOOTH", "TREATED ROOM", "INTERVIEW LOUNGE" };
        p.drawText(boothX + 8, boothY + 16, boothNames[m_boothType]);

        // ── Mic stand + capsule (centred inside the booth) ────────
        const int micX = boothX + boothW / 2;
        const int micY = boothY + boothH / 2 - 10;

        // Stand
        p.setPen(QPen(QColor(0x88, 0x90, 0xa8), 2));
        p.drawLine(micX, micY + 4, micX, micY + 36);
        p.setBrush(QColor(0x40, 0x48, 0x58));
        p.drawEllipse(QPointF(micX, micY + 36), 14, 3);

        // Mic body (color-coded by mic type)
        QColor micCol;
        const char* micNames[4] = { "SM7B", "RE20", "MKH 416", "U87" };
        switch (m_micType) {
            case 0: micCol = QColor(0x20, 0x20, 0x24); break;  // SM7B black
            case 1: micCol = QColor(0x90, 0x68, 0x40); break;  // RE20 bronze
            case 2: micCol = QColor(0x60, 0x60, 0x66); break;  // 416 gray
            case 3: micCol = QColor(0xc0, 0xa0, 0x60); break;  // U87 gold
        }
        QLinearGradient micGrad(micX - 12, micY - 12, micX + 12, micY + 12);
        micGrad.setColorAt(0.0, micCol.lighter(140));
        micGrad.setColorAt(0.5, micCol);
        micGrad.setColorAt(1.0, micCol.darker(140));
        p.setBrush(micGrad);
        p.setPen(QPen(QColor(0xff, 0xb0, 0x20, 200), 2));
        p.drawEllipse(QPointF(micX, micY), 12, 14);

        // Mic name plate
        p.setPen(QColor(0xff, 0xb0, 0x20));
        QFont monoSmall("Menlo");
        monoSmall.setPointSize(9);
        monoSmall.setBold(true);
        p.setFont(monoSmall);
        QFontMetrics fm(monoSmall);
        int textW = fm.horizontalAdvance(micNames[m_micType]);
        p.drawText(micX - textW / 2, micY + 56, micNames[m_micType]);

        // ── Pickup pattern indicator (cardioid lobe) ──────────────
        QPainterPath cardioid;
        cardioid.moveTo(micX, micY - 18);
        cardioid.cubicTo(micX + 22, micY - 12,
                         micX + 22, micY + 12,
                         micX, micY + 18);
        cardioid.cubicTo(micX - 22, micY + 12,
                         micX - 22, micY - 12,
                         micX, micY - 18);
        p.setPen(QPen(QColor(0xff, 0xb0, 0x20, 60), 1, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawPath(cardioid);

        // ── Control room (right side) ─────────────────────────────
        const int conX = boothX + boothW + 24;
        const int conY = boothY;
        const int conW = W - conX - 30;
        const int conH = boothH;

        // Frame
        QLinearGradient conGrad(conX, conY, conX, conY + conH);
        conGrad.setColorAt(0.0, QColor(0x14, 0x20, 0x36));
        conGrad.setColorAt(1.0, QColor(0x08, 0x12, 0x22));
        p.setBrush(conGrad);
        p.setPen(QPen(QColor(0x20, 0x40, 0x6a), 2));
        p.drawRoundedRect(QRect(conX, conY, conW, conH), 6, 6);

        // Console panel (top half) — slanted operator surface
        const int conTop = conY + 16;
        const int conSurfH = conH / 2 - 14;
        QPolygonF console;
        console << QPointF(conX + 10,         conTop)
                << QPointF(conX + conW - 10,  conTop)
                << QPointF(conX + conW - 18,  conTop + conSurfH)
                << QPointF(conX + 18,         conTop + conSurfH);
        QLinearGradient consoleGrad(conX, conTop, conX, conTop + conSurfH);
        consoleGrad.setColorAt(0.0, QColor(0x30, 0x38, 0x48));
        consoleGrad.setColorAt(1.0, QColor(0x18, 0x1e, 0x2c));
        p.setBrush(consoleGrad);
        p.setPen(QPen(QColor(0x40, 0x50, 0x68), 1));
        p.drawPolygon(console);

        // Channel strip dots (faders, knobs)
        for (int i = 0; i < 12; ++i) {
            int cx = conX + 22 + i * (conW - 44) / 11;
            int cy = conTop + conSurfH / 2;
            p.setBrush(QColor(0xff, 0xb0, 0x20, 200));
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPointF(cx, cy - 6), 1.5, 1.5);
            p.drawEllipse(QPointF(cx, cy + 2), 1.5, 1.5);
            p.setPen(QPen(QColor(0x88, 0x90, 0xa8), 1));
            p.drawLine(cx, cy + 8, cx, cy + 18);
        }

        // Outboard rack (bottom half)
        const int rackTop  = conTop + conSurfH + 8;
        const int rackH    = conY + conH - rackTop - 12;
        QRect rack(conX + 10, rackTop, conW - 20, rackH);
        QLinearGradient rackGrad(0, rackTop, 0, rackTop + rackH);
        rackGrad.setColorAt(0.0, QColor(0x12, 0x18, 0x24));
        rackGrad.setColorAt(1.0, QColor(0x06, 0x0a, 0x14));
        p.setBrush(rackGrad);
        p.setPen(QPen(QColor(0x30, 0x38, 0x48), 1));
        p.drawRect(rack);

        // Rack units (3 horizontal slots) with tiny status LEDs
        const int unitH = (rackH - 12) / 3;
        const char* unitNames[3] = { "VOICE LIFT", "VOICE STRIP", "LOUDNESS MATCH" };
        for (int u = 0; u < 3; ++u) {
            int uy = rackTop + 4 + u * (unitH + 2);
            QRect unit(rack.x() + 4, uy, rack.width() - 8, unitH - 2);
            QLinearGradient unitGrad(0, uy, 0, uy + unitH);
            unitGrad.setColorAt(0.0, QColor(0x20, 0x28, 0x38));
            unitGrad.setColorAt(1.0, QColor(0x10, 0x16, 0x22));
            p.setBrush(unitGrad);
            p.setPen(QPen(QColor(0x30, 0x40, 0x58), 1));
            p.drawRoundedRect(unit, 2, 2);
            // Status LED
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0x40, 0xff, 0x80));
            p.drawEllipse(QPointF(unit.left() + 8, unit.center().y()), 2.5, 2.5);
            // Unit label
            p.setPen(QColor(0x60, 0x88, 0xb0));
            QFont uf("Menlo");
            uf.setPointSize(7);
            uf.setBold(true);
            p.setFont(uf);
            p.drawText(unit.left() + 16, unit.center().y() + 3, unitNames[u]);
        }

        // ── Phone-line indicator (top right) ──────────────────────
        if (m_phoneLine) {
            int badgeX = conX + conW - 60;
            int badgeY = conY + 4;
            QRect badge(badgeX, badgeY, 56, 14);
            p.setBrush(QColor(0xff, 0x40, 0x40));
            p.setPen(QPen(QColor(0xff, 0xff, 0xff), 1));
            p.drawRoundedRect(badge, 3, 3);
            p.setPen(QColor(0xff, 0xff, 0xff));
            QFont bf("Menlo");
            bf.setPointSize(7);
            bf.setBold(true);
            p.setFont(bf);
            p.drawText(badge, Qt::AlignCenter, "REMOTE");
        }

        // ── LUFS target readout (bottom-right corner) ────────────
        const char* lufsNames[3] = { "-16 LUFS", "-19 LUFS", "-23 LUFS" };
        const char* lufsCtx[3]   = { "SPOTIFY",  "APPLE",    "BROADCAST" };
        QRect lufsBox(W - 130, H - 28, 110, 20);
        p.setBrush(QColor(0x06, 0x0a, 0x14));
        p.setPen(QPen(QColor(0x20, 0x40, 0x6a), 1));
        p.drawRoundedRect(lufsBox, 3, 3);
        p.setPen(QColor(0xff, 0xb0, 0x20));
        QFont lf("Menlo");
        lf.setPointSize(8);
        lf.setBold(true);
        p.setFont(lf);
        p.drawText(lufsBox.adjusted(6, 0, -6, 0), Qt::AlignVCenter | Qt::AlignLeft,
                   QString::fromLatin1(lufsNames[m_lufsTarget]));
        p.setPen(QColor(0x60, 0x88, 0xb0));
        QFont lfs("Menlo");
        lfs.setPointSize(7);
        p.setFont(lfs);
        p.drawText(lufsBox.adjusted(6, 0, -6, 0), Qt::AlignVCenter | Qt::AlignRight,
                   QString::fromLatin1(lufsCtx[m_lufsTarget]));
    }

private:
    int  m_boothType  = 0;
    int  m_micType    = 0;
    int  m_lufsTarget = 0;
    bool m_phoneLine  = false;
};

/* ─────────────────────────────────────────────────────────────────
 *  SignalHillStudioDialog — flagship plugin editor
 * ───────────────────────────────────────────────────────────────── */
class SignalHillStudioDialog : public QDialog {
    Q_OBJECT

public:
    explicit SignalHillStudioDialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : QDialog(parent)
        , m_fx(fx)
    {
        setWindowTitle("MC1 Signal Hill Broadcasting A");
        setMinimumSize(360, 202); resize(720, 405);
        applyTheme();
        buildUi();
        loadFromEffect();

        m_poll = new QTimer(this);
        m_poll->setInterval(150);
        connect(m_poll, &QTimer::timeout, this, &SignalHillStudioDialog::pollDisplay);
        m_poll->start();
    }

private:
    void applyTheme()
    {
        setStyleSheet(
            "QDialog { background: #0a1220; color: #d6e4f0; }"
            "QGroupBox {"
            "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
            "      stop:0 #102036, stop:1 #0a1626);"
            "  border: 1px solid #20406a; border-left: 3px solid #ffb020;"
            "  border-radius: 6px; margin-top: 14px;"
            "  padding: 10px 6px 6px 6px; font-size: 11px; color: #6088b0;"
            "}"
            "QGroupBox::title {"
            "  subcontrol-origin: margin; subcontrol-position: top left;"
            "  padding: 2px 8px; color: #ffb020; font-weight: bold;"
            "  letter-spacing: 1px;"
            "}"
            "QLabel { color: #d6e4f0; }"
            "QPushButton {"
            "  background: #102036; color: #d6e4f0;"
            "  border: 1px solid #20406a; border-radius: 3px;"
            "  padding: 6px 16px; min-width: 70px;"
            "}"
            "QPushButton:hover { background: #1a2c4a; }"
            "QPushButton:pressed { background: #ffb020; color: #0c1422; }"
        );
    }

    void buildUi()
    {
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(14, 10, 14, 10);
        root->setSpacing(8);

        // ── Header bar ───────────────────────────────────────────
        auto* header = new QHBoxLayout;
        auto* title  = new QLabel("MC1   SIGNAL  HILL   BROADCASTING   A");
        title->setStyleSheet(
            "font-size: 17px; font-weight: bold; color: #ffb020; "
            "letter-spacing: 5px;");
        header->addWidget(title);
        header->addStretch();
        auto* tagline = new QLabel("End-to-end podcasting + vodcasting studio");
        tagline->setStyleSheet("color: #6088b0; font-style: italic;");
        header->addWidget(tagline);
        root->addLayout(header);

        // ── Top: rendered control room view ─────────────────────
        m_roomView = new SignalHillRoomView();
        root->addWidget(m_roomView);

        // ── Bottom: knob banks ──────────────────────────────────
        auto* banks = new QHBoxLayout;
        banks->setSpacing(8);

        // BOOTH & MIC
        auto* boothGroup = new QGroupBox("BOOTH  &  MIC");
        auto* boothLayout = new QHBoxLayout(boothGroup);
        m_knobs[mc1dsp::FxSignalHillA::ParamBoothType]    = makeKnob("BOOTH",   mc1dsp::FxSignalHillA::ParamBoothType);
        m_knobs[mc1dsp::FxSignalHillA::ParamMicCharacter] = makeKnob("MIC",     mc1dsp::FxSignalHillA::ParamMicCharacter);
        m_knobs[mc1dsp::FxSignalHillA::ParamVoiceLift]    = makeKnob("LIFT",    mc1dsp::FxSignalHillA::ParamVoiceLift);
        m_knobs[mc1dsp::FxSignalHillA::ParamWarmth]       = makeKnob("WARMTH",  mc1dsp::FxSignalHillA::ParamWarmth);
        boothLayout->addWidget(m_knobs[mc1dsp::FxSignalHillA::ParamBoothType]);
        boothLayout->addWidget(m_knobs[mc1dsp::FxSignalHillA::ParamMicCharacter]);
        boothLayout->addWidget(m_knobs[mc1dsp::FxSignalHillA::ParamVoiceLift]);
        boothLayout->addWidget(m_knobs[mc1dsp::FxSignalHillA::ParamWarmth]);
        banks->addWidget(boothGroup);

        // VOICE CHAIN
        auto* chainGroup = new QGroupBox("VOICE  CHAIN");
        auto* chainLayout = new QHBoxLayout(chainGroup);
        m_knobs[mc1dsp::FxSignalHillA::ParamPlosiveAmount] = makeKnob("PLOSIVE",  mc1dsp::FxSignalHillA::ParamPlosiveAmount);
        m_knobs[mc1dsp::FxSignalHillA::ParamClickRemoval]  = makeKnob("CLICKS",   mc1dsp::FxSignalHillA::ParamClickRemoval);
        m_knobs[mc1dsp::FxSignalHillA::ParamCompression]   = makeKnob("COMP",     mc1dsp::FxSignalHillA::ParamCompression);
        m_knobs[mc1dsp::FxSignalHillA::ParamDeEsser]       = makeKnob("DE-ESS",   mc1dsp::FxSignalHillA::ParamDeEsser);
        m_knobs[mc1dsp::FxSignalHillA::ParamEnhancer]      = makeKnob("ENHANCER", mc1dsp::FxSignalHillA::ParamEnhancer);
        m_knobs[mc1dsp::FxSignalHillA::ParamBleedGate]     = makeKnob("BLEED",    mc1dsp::FxSignalHillA::ParamBleedGate);
        chainLayout->addWidget(m_knobs[mc1dsp::FxSignalHillA::ParamPlosiveAmount]);
        chainLayout->addWidget(m_knobs[mc1dsp::FxSignalHillA::ParamClickRemoval]);
        chainLayout->addWidget(m_knobs[mc1dsp::FxSignalHillA::ParamCompression]);
        chainLayout->addWidget(m_knobs[mc1dsp::FxSignalHillA::ParamDeEsser]);
        chainLayout->addWidget(m_knobs[mc1dsp::FxSignalHillA::ParamEnhancer]);
        chainLayout->addWidget(m_knobs[mc1dsp::FxSignalHillA::ParamBleedGate]);
        banks->addWidget(chainGroup, 1);

        // OUTPUT & DELIVERY
        auto* outGroup = new QGroupBox("DELIVERY");
        auto* outLayout = new QHBoxLayout(outGroup);
        m_knobs[mc1dsp::FxSignalHillA::ParamPhoneLine]      = makeKnob("PHONE",  mc1dsp::FxSignalHillA::ParamPhoneLine);
        m_knobs[mc1dsp::FxSignalHillA::ParamLoudnessTarget] = makeKnob("LUFS",   mc1dsp::FxSignalHillA::ParamLoudnessTarget);
        m_knobs[mc1dsp::FxSignalHillA::ParamOutput]         = makeKnob("OUTPUT", mc1dsp::FxSignalHillA::ParamOutput);
        outLayout->addWidget(m_knobs[mc1dsp::FxSignalHillA::ParamPhoneLine]);
        outLayout->addWidget(m_knobs[mc1dsp::FxSignalHillA::ParamLoudnessTarget]);
        outLayout->addWidget(m_knobs[mc1dsp::FxSignalHillA::ParamOutput]);
        banks->addWidget(outGroup);

        root->addLayout(banks);

        // ── Bottom row: bypass / preset / close ─────────────────
        auto* bottom = new QHBoxLayout;
        auto* bypass = new QPushButton("BYPASS");
        bypass->setCheckable(true);
        connect(bypass, &QPushButton::toggled, this, [this](bool on) {
            if (m_fx) m_fx->setBypassed(on);
        });
        bottom->addWidget(bypass);
        bottom->addStretch();
        auto* statusLabel = new QLabel("READY");
        statusLabel->setStyleSheet(
            "QLabel { color: #ffb020; font-family: 'Menlo'; font-size: 11px; "
            "background: #06101e; padding: 4px 12px; border: 1px solid #20406a; "
            "border-radius: 3px; }");
        bottom->addWidget(statusLabel);
        bottom->addStretch();
        auto* close = new QPushButton("CLOSE");
        connect(close, &QPushButton::clicked, this, &QDialog::close);
        bottom->addWidget(close);
        root->addLayout(bottom);
    }

    RackKnob* makeKnob(const QString& label, int paramIdx)
    {
        auto* k = new RackKnob;
        k->setStyle(RackKnob::SoftLED);
        k->setTitle(label);
        k->setFixedSize(58, 82);
        connect(k, &RackKnob::valueChanged, this, [this, paramIdx](float v) {
            if (m_fx) m_fx->setParamValue(paramIdx, v);
            updateRoomView();
        });
        return k;
    }

    void loadFromEffect()
    {
        if (!m_fx) return;
        for (int i = 0; i < mc1dsp::FxSignalHillA::kParamCount; ++i) {
            if (m_knobs[i]) m_knobs[i]->setValue(m_fx->paramValue(i));
        }
        updateRoomView();
    }

    void updateRoomView()
    {
        if (!m_fx || !m_roomView) return;
        int booth = std::max(0, std::min(2,
            static_cast<int>(m_fx->paramValue(mc1dsp::FxSignalHillA::ParamBoothType) * 2.999f)));
        int mic   = std::max(0, std::min(3,
            static_cast<int>(m_fx->paramValue(mc1dsp::FxSignalHillA::ParamMicCharacter) * 3.999f)));
        int lufs  = std::max(0, std::min(2,
            static_cast<int>(m_fx->paramValue(mc1dsp::FxSignalHillA::ParamLoudnessTarget) * 2.999f)));
        bool phone = m_fx->paramValue(mc1dsp::FxSignalHillA::ParamPhoneLine) > 0.05f;
        m_roomView->setBoothType(booth);
        m_roomView->setMicType(mic);
        m_roomView->setLoudnessTarget(lufs);
        m_roomView->setPhoneLineActive(phone);
    }

    void pollDisplay()
    {
        if (!m_fx) return;
        for (int i = 0; i < mc1dsp::FxSignalHillA::kParamCount; ++i) {
            if (!m_knobs[i]) continue;
            m_knobs[i]->setToolTip(
                QString("%1: %2")
                    .arg(QString::fromLatin1(m_fx->paramName(i)),
                         QString::fromStdString(m_fx->paramDisplayValue(i))));
        }
    }

    mc1dsp::DspEffect*  m_fx = nullptr;
    SignalHillRoomView* m_roomView = nullptr;
    RackKnob*           m_knobs[mc1dsp::FxSignalHillA::kParamCount] = {};
    QTimer*             m_poll = nullptr;
};
