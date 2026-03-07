// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QWidget>
#include "../core/ViewModeManager.h"

class QComboBox;
class QLabel;

namespace dawcast::widgets {

/// Compact workspace mode dropdown — shows current mode with a combo box.
class ViewModeSwitcher : public QWidget {
    Q_OBJECT

public:
    explicit ViewModeSwitcher(QWidget* parent = nullptr);
    ~ViewModeSwitcher() override;

    void setActiveMode(ViewModeManager::Mode mode);

signals:
    void modeSelected(ViewModeManager::Mode mode);

private:
    QLabel*    m_label = nullptr;
    QComboBox* m_combo = nullptr;
    bool       m_updating = false;  // prevent signal feedback
};

} // namespace dawcast::widgets
