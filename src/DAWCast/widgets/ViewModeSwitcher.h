// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QWidget>
#include <QList>
#include "../core/ViewModeManager.h"

class QHBoxLayout;

namespace dawcast::widgets {

class BevelButton;

/// Compact horizontal mode switcher bar — a row of exclusive toggle buttons,
/// one per ViewModeManager::Mode.  The active mode gets a teal highlight
/// underline.  Drop this into a toolbar or status area.
class ViewModeSwitcher : public QWidget {
    Q_OBJECT

public:
    explicit ViewModeSwitcher(QWidget* parent = nullptr);
    ~ViewModeSwitcher() override;

    /// Force the visual selection to match the given mode (e.g. when
    /// the mode is changed programmatically or restored from config).
    void setActiveMode(ViewModeManager::Mode mode);

signals:
    /// Emitted when the user clicks a mode button.
    void modeSelected(ViewModeManager::Mode mode);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void buildButtons();
    void updateButtonStates(ViewModeManager::Mode activeMode);

    QHBoxLayout*       m_layout   = nullptr;
    QList<BevelButton*> m_buttons;

    ViewModeManager::Mode m_activeMode = ViewModeManager::Producer;
};

} // namespace dawcast::widgets
