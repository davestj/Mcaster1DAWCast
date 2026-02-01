// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QWidget>

class QPushButton;

namespace dawcast::widgets {

/// Bottom action bar matching the DAWCast web UI.
///
/// Left side:  "+ Add Track", "Load from Library", "Export Mixdown"
/// Right side: "Projects", "Save Project" (teal highlight)
class ActionBar : public QWidget {
    Q_OBJECT

public:
    explicit ActionBar(QWidget* parent = nullptr);
    ~ActionBar() override = default;

signals:
    void addTrackClicked();
    void loadFromLibraryClicked();
    void exportMixdownClicked();
    void projectsClicked();
    void saveProjectClicked();

private:
    QPushButton* m_addTrackBtn       = nullptr;
    QPushButton* m_loadLibraryBtn    = nullptr;
    QPushButton* m_exportMixdownBtn  = nullptr;
    QPushButton* m_projectsBtn       = nullptr;
    QPushButton* m_saveProjectBtn    = nullptr;
};

} // namespace dawcast::widgets
