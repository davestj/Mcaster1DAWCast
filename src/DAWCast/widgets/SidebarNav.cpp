// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "SidebarNav.h"

#include <QLabel>
#include <QFont>
#include <QScrollArea>
#include <QFrame>

namespace dawcast::widgets {

// Stylesheet constants matching the web version dark theme
static const char* kSidebarStyle =
    "QWidget#SidebarNavInner {"
    "  background-color: #151821;"
    "}"
    "QScrollArea {"
    "  background-color: #151821;"
    "  border: none;"
    "}";

static const char* kSectionHeaderStyle =
    "QLabel {"
    "  color: #666666;"
    "  font-size: 10px;"
    "  font-weight: bold;"
    "  letter-spacing: 1px;"
    "  padding: 12px 16px 4px 16px;"
    "}";

static const char* kNavButtonNormal =
    "QPushButton {"
    "  color: #aaaaaa;"
    "  background-color: transparent;"
    "  border: none;"
    "  border-left: 3px solid transparent;"
    "  text-align: left;"
    "  padding: 6px 16px 6px 13px;"
    "  font-size: 12px;"
    "}"
    "QPushButton:hover {"
    "  color: #ffffff;"
    "  background-color: rgba(255, 255, 255, 0.05);"
    "}";

static const char* kNavButtonActive =
    "QPushButton {"
    "  color: #00ccaa;"
    "  background-color: rgba(0, 204, 170, 0.08);"
    "  border: none;"
    "  border-left: 3px solid #00ccaa;"
    "  text-align: left;"
    "  padding: 6px 16px 6px 13px;"
    "  font-size: 12px;"
    "  font-weight: bold;"
    "}"
    "QPushButton:hover {"
    "  color: #00ffdd;"
    "  background-color: rgba(0, 204, 170, 0.12);"
    "}";

SidebarNav::SidebarNav(QWidget* parent)
    : QWidget(parent)
{
    setFixedWidth(200);
    setStyleSheet(QStringLiteral("background-color: #151821;"));

    // Use a scroll area so the sidebar scrolls if the window is short
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet(QString::fromLatin1(kSidebarStyle));

    auto* innerWidget = new QWidget(scrollArea);
    innerWidget->setObjectName(QStringLiteral("SidebarNavInner"));
    innerWidget->setStyleSheet(QString::fromLatin1(kSidebarStyle));

    m_layout = new QVBoxLayout(innerWidget);
    m_layout->setContentsMargins(0, 8, 0, 8);
    m_layout->setSpacing(0);

    // ── MONITOR ─────────────────────────────────────────────────────────
    addSectionHeader(QStringLiteral("MONITOR"));
    addNavItem(QStringLiteral("Dashboard"),   QStringLiteral("\u2302"));     // house
    addNavItem(QStringLiteral("Encoders"),     QStringLiteral("\u2699"));     // gear

    // ── LIBRARY ─────────────────────────────────────────────────────────
    addSectionHeader(QStringLiteral("LIBRARY"));
    addNavItem(QStringLiteral("Media Library"), QStringLiteral("\U0001F4C1")); // folder
    addNavItem(QStringLiteral("Playlists"),     QStringLiteral("\u266B"));     // beamed notes
    addNavItem(QStringLiteral("Media Player"),  QStringLiteral("\u25B6"));     // play
    addNavItem(QStringLiteral("Pro Player"),    QStringLiteral("\u25B6\u25B6"), true); // popup

    // ── DJ ──────────────────────────────────────────────────────────────
    addSectionHeader(QStringLiteral("DJ"));
    addNavItem(QStringLiteral("Crossfader"),   QStringLiteral("\u2194"));     // left-right arrow
    addNavItem(QStringLiteral("Dual Deck"),    QStringLiteral("\u29C9"), true); // popup
    addNavItem(QStringLiteral("Effects Rack"), QStringLiteral("\u2728"));     // sparkles
    addNavItem(QStringLiteral("Mixer"),        QStringLiteral("\u2261"));     // triple bar
    addNavItem(QStringLiteral("JACK Audio"),   QStringLiteral("\u26A1"));     // lightning

    // ── VOICE TOOLS ─────────────────────────────────────────────────────
    addSectionHeader(QStringLiteral("VOICE TOOLS"));
    addNavItem(QStringLiteral("VoicTune"),     QStringLiteral("\U0001F399")); // microphone

    // ── PRODUCER ────────────────────────────────────────────────────────
    addSectionHeader(QStringLiteral("PRODUCER"));
    addNavItem(QStringLiteral("Video Producer"), QStringLiteral("\U0001F3AC")); // clapper board

    m_layout->addStretch();

    scrollArea->setWidget(innerWidget);

    // Outer layout for the sidebar widget itself
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);
    outerLayout->addWidget(scrollArea);

    // Default selection
    setActiveItem(QStringLiteral("Dashboard"));
}

SidebarNav::~SidebarNav() = default;

void SidebarNav::addSectionHeader(const QString& title)
{
    auto* label = new QLabel(title, this);
    label->setStyleSheet(QString::fromLatin1(kSectionHeaderStyle));

    QFont headerFont = label->font();
    headerFont.setCapitalization(QFont::SmallCaps);
    label->setFont(headerFont);

    m_layout->addWidget(label);
}

void SidebarNav::addNavItem(const QString& label, const QString& icon,
                            bool isPopup)
{
    auto* button = new QPushButton(this);

    QString displayText = icon + QStringLiteral("  ") + label;
    if (isPopup) {
        displayText += QStringLiteral("  \u2197");  // north-east arrow for popup
    }
    button->setText(displayText);
    button->setCursor(Qt::PointingHandCursor);
    button->setFlat(true);
    button->setStyleSheet(QString::fromLatin1(kNavButtonNormal));

    m_layout->addWidget(button);

    NavItem item;
    item.button  = button;
    item.isPopup = isPopup;
    m_items.insert(label, item);

    connect(button, &QPushButton::clicked, this, [this, label, isPopup]() {
        if (isPopup) {
            emit popupRequested(label);
        } else {
            setActiveItem(label);
            emit sectionSelected(label);
        }
    });
}

void SidebarNav::setActiveItem(const QString& name)
{
    clearActiveHighlight();
    m_activeItem = name;

    auto it = m_items.find(name);
    if (it != m_items.end()) {
        it->button->setStyleSheet(QString::fromLatin1(kNavButtonActive));
    }
}

void SidebarNav::clearActiveHighlight()
{
    for (auto& item : m_items) {
        item.button->setStyleSheet(QString::fromLatin1(kNavButtonNormal));
    }
}

} // namespace dawcast::widgets
