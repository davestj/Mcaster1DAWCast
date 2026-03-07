// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ViewModeSwitcher.h"
#include "../core/ViewModeManager.h"

#include <QHBoxLayout>
#include <QComboBox>
#include <QLabel>

namespace dawcast::widgets {

ViewModeSwitcher::ViewModeSwitcher(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(6);

    m_label = new QLabel(tr("Workspace:"), this);
    m_label->setStyleSheet(QStringLiteral(
        "QLabel { color: #8892a4; font-size: 11px; font-weight: 600; }"));
    layout->addWidget(m_label);

    m_combo = new QComboBox(this);
    m_combo->setMinimumWidth(140);
    m_combo->setFixedHeight(24);
    m_combo->setStyleSheet(QStringLiteral(
        "QComboBox { background: #252a3a; color: #f0f0f0; border: 1px solid #3a3f52; "
        "border-radius: 4px; padding: 2px 8px; font-size: 12px; font-weight: 500; }"
        "QComboBox:hover { border-color: #2dd4bf; }"
        "QComboBox::drop-down { border: none; width: 20px; }"
        "QComboBox QAbstractItemView { background: #1e2235; color: #f0f0f0; "
        "border: 1px solid #3a3f52; selection-background-color: #252a3a; "
        "selection-color: #2dd4bf; padding: 4px; }"));

    // Populate from ViewModeManager
    auto* mgr = ViewModeManager::instance();
    for (int i = 0; i < ViewModeManager::modeCount(); ++i) {
        auto mode = static_cast<ViewModeManager::Mode>(i);
        m_combo->addItem(mgr->modeName(mode), i);
    }

    layout->addWidget(m_combo);
    layout->addStretch();

    // Connect combo change to mode selection
    connect(m_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        if (m_updating) return;
        auto mode = static_cast<ViewModeManager::Mode>(index);
        emit modeSelected(mode);
    });

    // Respond to external mode changes
    connect(mgr, &ViewModeManager::modeChanged,
            this, &ViewModeSwitcher::setActiveMode);

    // Initialize to current mode
    setActiveMode(mgr->currentMode());
}

ViewModeSwitcher::~ViewModeSwitcher() = default;

void ViewModeSwitcher::setActiveMode(ViewModeManager::Mode mode)
{
    m_updating = true;
    m_combo->setCurrentIndex(static_cast<int>(mode));
    m_updating = false;
}

} // namespace dawcast::widgets
