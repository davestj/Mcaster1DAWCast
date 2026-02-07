// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ViewModeSwitcher.h"
#include "BevelButton.h"
#include "../core/ViewModeManager.h"

#include <QHBoxLayout>
#include <QPainter>

namespace dawcast::widgets {

// ── Style Constants ────────────────────────────────────────────────────────

static const QColor kTealAccent(0, 188, 180);         // Active indicator
static const QColor kActiveFace(38, 50, 56);          // Darker face for active
static const QColor kInactiveFace(48, 56, 62);        // Default face
static const QColor kActiveText(0, 220, 210);         // Teal text
static const QColor kInactiveText(160, 170, 175);     // Muted text

// Short icon-style labels for the compact bar
static const char* kShortLabels[] = {
    "Podcast",    // Podcaster
    "Producer",   // Producer
    "DJ / Live",  // DJLive
    "Studio",     // StudioArtist
    "Voice",      // VoiceOver
    "Guitar FX"   // GuitarFX
};

// Unicode fallback symbols when icon theme is missing
static const char* kFallbackGlyphs[] = {
    "\xF0\x9F\x8E\x99",   // microphone (Podcaster)
    "\xF0\x9F\x8E\x9B",   // control knobs (Producer)
    "\xF0\x9F\x93\xA1",   // satellite antenna (DJ/Live)
    "\xF0\x9F\x8E\xB5",   // music note (Studio)
    "\xF0\x9F\x92\xAC",   // speech balloon (Voice Over)
    "\xF0\x9F\x8E\xB8"    // guitar (Guitar FX)
};

// ── Construction ───────────────────────────────────────────────────────────

ViewModeSwitcher::ViewModeSwitcher(QWidget* parent)
    : QWidget(parent)
{
    setAutoFillBackground(false);
    setAttribute(Qt::WA_StyledBackground, true);

    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(4, 2, 4, 2);
    m_layout->setSpacing(2);

    buildButtons();

    // Respond to external mode changes
    auto* mgr = ViewModeManager::instance();
    connect(mgr, &ViewModeManager::modeChanged,
            this, &ViewModeSwitcher::setActiveMode);

    // Initialize to current mode
    setActiveMode(mgr->currentMode());
}

ViewModeSwitcher::~ViewModeSwitcher() = default;

// ── Build Buttons ──────────────────────────────────────────────────────────

void ViewModeSwitcher::buildButtons()
{
    auto* mgr = ViewModeManager::instance();

    for (int i = 0; i < ViewModeManager::modeCount(); ++i) {
        auto mode = static_cast<ViewModeManager::Mode>(i);

        // Try to get an icon; fall back to a Unicode glyph + label
        QIcon icon = mgr->modeIcon(mode);
        QString label = QString::fromUtf8(kShortLabels[i]);

        BevelButton* btn;
        if (icon.isNull()) {
            QString text = QStringLiteral("%1 %2")
                .arg(QString::fromUtf8(kFallbackGlyphs[i]), label);
            btn = new BevelButton(text, this);
        } else {
            btn = new BevelButton(icon, label, this);
            btn->setIconSize(QSize(16, 16));
        }

        btn->setCheckable(true);
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setMinimumHeight(28);
        btn->setMaximumHeight(32);
        btn->setFaceColor(kInactiveFace);
        btn->setCheckedFaceColor(kActiveFace);
        btn->setBevelDepth(2);
        btn->setToolTip(mgr->modeDescription(mode));

        btn->setStyleSheet(QStringLiteral(
            "BevelButton { font-size: 11px; color: %1; padding: 2px 8px; }"
            "BevelButton:checked { color: %2; }")
            .arg(kInactiveText.name(), kActiveText.name()));

        // Wire click -> mode selection
        connect(btn, &BevelButton::clicked, this, [this, mode]() {
            emit modeSelected(mode);
        });

        m_layout->addWidget(btn);
        m_buttons.append(btn);
    }

    m_layout->addStretch();
}

// ── Mode Update ────────────────────────────────────────────────────────────

void ViewModeSwitcher::setActiveMode(ViewModeManager::Mode mode)
{
    m_activeMode = mode;
    updateButtonStates(mode);
    update();  // repaint underline indicator
}

void ViewModeSwitcher::updateButtonStates(ViewModeManager::Mode activeMode)
{
    for (int i = 0; i < m_buttons.size(); ++i) {
        bool active = (static_cast<ViewModeManager::Mode>(i) == activeMode);
        m_buttons[i]->setChecked(active);
    }
}

// ── Paint (teal underline indicator) ───────────────────────────────────────

void ViewModeSwitcher::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);

    // Draw a teal underline beneath the active button
    int idx = static_cast<int>(m_activeMode);
    if (idx < 0 || idx >= m_buttons.size())
        return;

    BevelButton* activeBtn = m_buttons[idx];
    QRect r = activeBtn->geometry();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(kTealAccent);
    p.drawRoundedRect(r.left() + 4, r.bottom() + 1,
                      r.width() - 8, 3,
                      1.5, 1.5);
}

} // namespace dawcast::widgets
