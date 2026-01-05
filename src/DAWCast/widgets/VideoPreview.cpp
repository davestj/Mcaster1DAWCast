// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoPreview.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QFontMetrics>

namespace dawcast::widgets {

namespace {
constexpr int kControlBarHeight = 32;
constexpr int kPlayBtnSize      = 24;
constexpr int kControlPadding   = 6;

const QColor kBarBg(0, 0, 0, 140);
const QColor kPlayBtnBg(255, 255, 255, 50);
const QColor kPlayBtnIcon(220, 220, 220);
const QColor kNoVideoText(120, 120, 130);
} // anonymous namespace

VideoPreview::VideoPreview(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(320, 180);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
}

VideoPreview::~VideoPreview() = default;

void VideoPreview::setFrame(const QImage& frame)
{
    m_frame = frame;
    update();
}

void VideoPreview::clear()
{
    m_frame = QImage();
    update();
}

void VideoPreview::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    const int w = width();
    const int h = height();

    // Dark background (letterbox/pillarbox bars)
    p.fillRect(rect(), Qt::black);

    if (m_frame.isNull()) {
        // "No Video" message centered
        QFont noVideoFont = font();
        noVideoFont.setPointSize(16);
        noVideoFont.setWeight(QFont::Light);
        p.setFont(noVideoFont);
        p.setPen(kNoVideoText);
        p.drawText(rect(), Qt::AlignCenter, tr("No Video"));

        // Decorative frame icon (simple clapperboard silhouette)
        int iconSize = 48;
        int cx = w / 2;
        int cy = h / 2 - 30;
        p.setPen(QPen(QColor(80, 80, 90), 2));
        p.setBrush(QColor(40, 40, 48));
        p.drawRoundedRect(cx - iconSize / 2, cy - iconSize / 2, iconSize, iconSize, 4, 4);
        // Play triangle inside
        QPainterPath tri;
        tri.moveTo(cx - 8, cy - 12);
        tri.lineTo(cx - 8, cy + 12);
        tri.lineTo(cx + 12, cy);
        tri.closeSubpath();
        p.setBrush(QColor(80, 80, 90));
        p.setPen(Qt::NoPen);
        p.drawPath(tri);

        return;
    }

    // Compute scaled frame rect preserving aspect ratio
    QSize frameSize = m_frame.size();
    QSize displaySize;

    switch (m_scaleMode) {
    case ScaleMode::Fit:
        displaySize = frameSize.scaled(size(), Qt::KeepAspectRatio);
        break;
    case ScaleMode::ActualSize:
        displaySize = frameSize;
        break;
    case ScaleMode::Half:
        displaySize = frameSize / 2;
        break;
    case ScaleMode::Double:
        displaySize = frameSize * 2;
        break;
    }

    int x = (w - displaySize.width()) / 2;
    int y = (h - displaySize.height()) / 2;

    p.drawImage(QRect(x, y, displaySize.width(), displaySize.height()), m_frame);

    // --- Overlay transport control bar at bottom ---
    QRect barRect(0, h - kControlBarHeight, w, kControlBarHeight);
    p.fillRect(barRect, kBarBg);

    // Play/Pause button
    int btnX = kControlPadding;
    int btnY = h - kControlBarHeight + (kControlBarHeight - kPlayBtnSize) / 2;
    QRect btnRect(btnX, btnY, kPlayBtnSize, kPlayBtnSize);

    p.setBrush(kPlayBtnBg);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(btnRect, 4, 4);

    if (m_playing) {
        // Pause icon (two bars)
        p.setBrush(kPlayBtnIcon);
        int bw = 4, bh = 12;
        int by = btnY + (kPlayBtnSize - bh) / 2;
        p.drawRect(btnX + 6, by, bw, bh);
        p.drawRect(btnX + 14, by, bw, bh);
    } else {
        // Play triangle
        QPainterPath playTri;
        int cx = btnX + kPlayBtnSize / 2 + 2;
        int cy = btnY + kPlayBtnSize / 2;
        playTri.moveTo(cx - 6, cy - 7);
        playTri.lineTo(cx - 6, cy + 7);
        playTri.lineTo(cx + 6, cy);
        playTri.closeSubpath();
        p.setBrush(kPlayBtnIcon);
        p.drawPath(playTri);
    }

    // Frame info text
    QFont infoFont = font();
    infoFont.setPointSize(9);
    p.setFont(infoFont);
    p.setPen(QColor(180, 180, 180));
    QString info = QString("%1x%2").arg(m_frame.width()).arg(m_frame.height());
    p.drawText(btnX + kPlayBtnSize + 10, h - kControlBarHeight / 2 + 4, info);
}

void VideoPreview::mousePressEvent(QMouseEvent* event)
{
    // Check if click is on the play/pause button
    int btnX = kControlPadding;
    int btnY = height() - kControlBarHeight + (kControlBarHeight - kPlayBtnSize) / 2;
    QRect btnRect(btnX, btnY, kPlayBtnSize, kPlayBtnSize);

    if (btnRect.contains(event->pos())) {
        m_playing = !m_playing;
        update();
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void VideoPreview::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);

    auto* fitAction = menu.addAction(tr("Fit to Window"));
    auto* actualAction = menu.addAction(tr("Actual Size"));
    auto* halfAction = menu.addAction(tr("50%"));
    auto* doubleAction = menu.addAction(tr("200%"));

    // Checkmarks for current mode
    fitAction->setCheckable(true);
    actualAction->setCheckable(true);
    halfAction->setCheckable(true);
    doubleAction->setCheckable(true);
    fitAction->setChecked(m_scaleMode == ScaleMode::Fit);
    actualAction->setChecked(m_scaleMode == ScaleMode::ActualSize);
    halfAction->setChecked(m_scaleMode == ScaleMode::Half);
    doubleAction->setChecked(m_scaleMode == ScaleMode::Double);

    connect(fitAction, &QAction::triggered, this, [this] {
        m_scaleMode = ScaleMode::Fit; update();
    });
    connect(actualAction, &QAction::triggered, this, [this] {
        m_scaleMode = ScaleMode::ActualSize; update();
    });
    connect(halfAction, &QAction::triggered, this, [this] {
        m_scaleMode = ScaleMode::Half; update();
    });
    connect(doubleAction, &QAction::triggered, this, [this] {
        m_scaleMode = ScaleMode::Double; update();
    });

    menu.exec(event->globalPos());
}

} // namespace dawcast::widgets
