// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ActionBar.h"

#include <QHBoxLayout>
#include <QPushButton>

namespace dawcast::widgets {

ActionBar::ActionBar(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(44);
    setObjectName(QStringLiteral("ActionBar"));

    // Dark background with subtle top border
    setStyleSheet(QStringLiteral(
        "QWidget#ActionBar {"
        "  background-color: #1a1e2e;"
        "  border-top: 1px solid #2a2f42;"
        "}"
    ));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(6);

    // ── Shared button stylesheet fragments ─────────────────────────────
    const QString outlineStyle = QStringLiteral(
        "QPushButton {"
        "  background-color: transparent;"
        "  color: #d0d4e0;"
        "  border: 1px solid #3ea8a0;"
        "  border-radius: 4px;"
        "  padding: 4px 12px;"
        "  font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "  background-color: rgba(62, 168, 160, 0.15);"
        "  color: #ffffff;"
        "}"
        "QPushButton:pressed {"
        "  background-color: rgba(62, 168, 160, 0.30);"
        "}"
    );

    const QString secondaryStyle = QStringLiteral(
        "QPushButton {"
        "  background-color: transparent;"
        "  color: #9098b0;"
        "  border: 1px solid #3a3f52;"
        "  border-radius: 4px;"
        "  padding: 4px 12px;"
        "  font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "  background-color: rgba(255, 255, 255, 0.06);"
        "  color: #d0d4e0;"
        "}"
        "QPushButton:pressed {"
        "  background-color: rgba(255, 255, 255, 0.10);"
        "}"
    );

    const QString primaryStyle = QStringLiteral(
        "QPushButton {"
        "  background-color: #3ea8a0;"
        "  color: #ffffff;"
        "  border: none;"
        "  border-radius: 4px;"
        "  padding: 4px 14px;"
        "  font-size: 12px;"
        "  font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "  background-color: #4bbab2;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #34918a;"
        "}"
    );

    // ── Left-side buttons ──────────────────────────────────────────────

    m_addTrackBtn = new QPushButton(QStringLiteral("+ Add Track"), this);
    m_addTrackBtn->setStyleSheet(outlineStyle);
    m_addTrackBtn->setToolTip(tr("Add a new audio track"));

    m_loadLibraryBtn = new QPushButton(tr("Load from Library"), this);
    m_loadLibraryBtn->setStyleSheet(secondaryStyle);
    m_loadLibraryBtn->setToolTip(tr("Load media from the library"));

    m_exportMixdownBtn = new QPushButton(tr("Export Mixdown"), this);
    m_exportMixdownBtn->setStyleSheet(secondaryStyle);
    m_exportMixdownBtn->setToolTip(tr("Export the final mixdown"));

    layout->addWidget(m_addTrackBtn);
    layout->addWidget(m_loadLibraryBtn);
    layout->addWidget(m_exportMixdownBtn);

    // ── Spacer ─────────────────────────────────────────────────────────

    layout->addStretch(1);

    // ── Right-side buttons ─────────────────────────────────────────────

    m_projectsBtn = new QPushButton(tr("Projects"), this);
    m_projectsBtn->setStyleSheet(secondaryStyle);
    m_projectsBtn->setToolTip(tr("Open project browser"));

    m_saveProjectBtn = new QPushButton(QStringLiteral("Save Project"), this);
    m_saveProjectBtn->setStyleSheet(primaryStyle);
    m_saveProjectBtn->setToolTip(tr("Save the current project"));

    layout->addWidget(m_projectsBtn);
    layout->addWidget(m_saveProjectBtn);

    // ── Connections ────────────────────────────────────────────────────

    connect(m_addTrackBtn,      &QPushButton::clicked, this, &ActionBar::addTrackClicked);
    connect(m_loadLibraryBtn,   &QPushButton::clicked, this, &ActionBar::loadFromLibraryClicked);
    connect(m_exportMixdownBtn, &QPushButton::clicked, this, &ActionBar::exportMixdownClicked);
    connect(m_projectsBtn,      &QPushButton::clicked, this, &ActionBar::projectsClicked);
    connect(m_saveProjectBtn,   &QPushButton::clicked, this, &ActionBar::saveProjectClicked);
}

} // namespace dawcast::widgets
