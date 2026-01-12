// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "SplashScreen.h"

#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QScreen>
#include <QGuiApplication>
#include <QApplication>

#ifdef HAVE_QT6SVG
#include <QSvgRenderer>
#endif

namespace dawcast::widgets {

static constexpr int kSplashWidth  = 520;
static constexpr int kSplashHeight = 320;
static constexpr int kCornerRadius = 12;
static constexpr int kFadeStepMs   = 16;   // ~60 fps fade
static constexpr qreal kFadeStep   = 0.05;

SplashScreen::SplashScreen(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::SplashScreen | Qt::FramelessWindowHint |
                   Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setFixedSize(kSplashWidth, kSplashHeight);

    // Center on primary screen
    if (QScreen* screen = QGuiApplication::primaryScreen()) {
        QRect geo = screen->availableGeometry();
        move(geo.center() - QPoint(kSplashWidth / 2, kSplashHeight / 2));
    }

    // Fade-out timer (not started until finish() is called)
    m_fadeTimer = new QTimer(this);
    m_fadeTimer->setInterval(kFadeStepMs);
    connect(m_fadeTimer, &QTimer::timeout, this, [this]() {
        m_opacity -= kFadeStep;
        if (m_opacity <= 0.0) {
            m_opacity = 0.0;
            m_fadeTimer->stop();
            close();
            deleteLater();
            return;
        }
        setWindowOpacity(m_opacity);
        update();
    });

    // Animated loading dots
    m_dotTimer = new QTimer(this);
    m_dotTimer->setInterval(400);
    connect(m_dotTimer, &QTimer::timeout, this, [this]() {
        m_dotAnimation = (m_dotAnimation + 1) % 4;
        update();
    });
    m_dotTimer->start();
}

SplashScreen::~SplashScreen() = default;

void SplashScreen::showMessage(const QString& message)
{
    m_message = message;
    update();
    QApplication::processEvents();
}

void SplashScreen::finish(QWidget* mainWindow)
{
    Q_UNUSED(mainWindow)
    m_dotTimer->stop();
    startFadeOut();
}

void SplashScreen::startFadeOut()
{
    m_fadingOut = true;
    m_opacity = 1.0;
    m_fadeTimer->start();
}

void SplashScreen::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    QRectF r(0.5, 0.5, width() - 1.0, height() - 1.0);

    // -- Clipped rounded rect path --
    QPainterPath clipPath;
    clipPath.addRoundedRect(r, kCornerRadius, kCornerRadius);
    p.setClipPath(clipPath);

    // -- Dark gradient background --
    QLinearGradient bg(0, 0, 0, height());
    bg.setColorAt(0.0, QColor(0x0f, 0x17, 0x2a));       // #0f172a
    bg.setColorAt(1.0, QColor(0x1a, 0x1a, 0x2e));       // #1a1a2e
    p.fillRect(rect(), bg);

    // -- Subtle radial glow at center-top --
    QRadialGradient glow(width() / 2.0, height() * 0.35, width() * 0.45);
    glow.setColorAt(0.0, QColor(78, 205, 196, 18));       // teal glow, very faint
    glow.setColorAt(1.0, QColor(78, 205, 196, 0));
    p.fillRect(rect(), glow);

    // -- Border --
    p.setClipping(false);
    QPen borderPen(QColor(78, 205, 196, 80), 1.0);        // subtle teal border
    p.setPen(borderPen);
    p.drawRoundedRect(r, kCornerRadius, kCornerRadius);
    p.setClipPath(clipPath);

    // -- App icon (from embedded resources) --
    QRectF iconRect(width() / 2.0 - 32, 40, 64, 64);
    bool iconRendered = false;

#ifdef HAVE_QT6SVG
    QSvgRenderer svgIcon(QStringLiteral(":/icons/icons/podcast.svg"));
    if (svgIcon.isValid()) {
        svgIcon.render(&p, iconRect);
        iconRendered = true;
    }
#endif

    if (!iconRendered) {
        // Fallback: draw a simple broadcast-style icon
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(78, 205, 196));
        p.drawEllipse(iconRect.adjusted(12, 12, -12, -12));
    }

    // -- Title --
    QFont titleFont;
    titleFont.setFamily(QStringLiteral("Helvetica Neue"));
    titleFont.setPixelSize(28);
    titleFont.setBold(true);
    p.setFont(titleFont);
    p.setPen(QColor(255, 255, 255));
    p.drawText(QRectF(0, 114, width(), 36), Qt::AlignCenter,
               QStringLiteral("Mcaster1DAWCast"));

    // -- Subtitle --
    QFont subtitleFont;
    subtitleFont.setFamily(QStringLiteral("Helvetica Neue"));
    subtitleFont.setPixelSize(14);
    p.setFont(subtitleFont);
    p.setPen(QColor(0x4e, 0xcd, 0xc4));                  // #4ecdc4 teal
    p.drawText(QRectF(0, 152, width(), 24), Qt::AlignCenter,
               QStringLiteral("Multi-Channel DAW for Broadcasting"));

    // -- Loading bar (thin, animated) --
    int barY = 210;
    int barH = 3;
    int barMargin = 60;
    QRectF barBg(barMargin, barY, width() - barMargin * 2, barH);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, 20));
    p.drawRoundedRect(barBg, 1.5, 1.5);

    // Animated fill segment
    double phase = m_dotAnimation / 3.0;
    double segW = barBg.width() * 0.3;
    double segX = barBg.x() + phase * (barBg.width() - segW);
    QLinearGradient barGrad(segX, 0, segX + segW, 0);
    barGrad.setColorAt(0.0, QColor(78, 205, 196, 0));
    barGrad.setColorAt(0.3, QColor(78, 205, 196, 200));
    barGrad.setColorAt(0.7, QColor(78, 205, 196, 200));
    barGrad.setColorAt(1.0, QColor(78, 205, 196, 0));
    p.setBrush(barGrad);
    p.drawRoundedRect(QRectF(segX, barY, segW, barH), 1.5, 1.5);

    // -- Status message --
    if (!m_message.isEmpty()) {
        QFont msgFont;
        msgFont.setFamily(QStringLiteral("Helvetica Neue"));
        msgFont.setPixelSize(12);
        p.setFont(msgFont);
        p.setPen(QColor(0x4e, 0xcd, 0xc4, 200));

        QString dots;
        for (int i = 0; i < m_dotAnimation; ++i) dots += QChar('.');
        QString displayMsg = m_message + dots;

        p.drawText(QRectF(0, 228, width(), 20), Qt::AlignCenter, displayMsg);
    }

    // -- Version (bottom right) --
    QFont smallFont;
    smallFont.setFamily(QStringLiteral("Helvetica Neue"));
    smallFont.setPixelSize(11);
    p.setFont(smallFont);
    p.setPen(QColor(160, 160, 180));
    p.drawText(QRectF(0, height() - 30, width() - 16, 20),
               Qt::AlignRight | Qt::AlignVCenter,
               QStringLiteral("v1.0.0-alpha"));

    // -- Copyright (bottom left) --
    p.drawText(QRectF(16, height() - 30, width(), 20),
               Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("\u00A9 2026 David St. John"));
}

} // namespace dawcast::widgets
