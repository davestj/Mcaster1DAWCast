// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QMap>

namespace dawcast::widgets {

/// Left sidebar navigation panel matching the web version layout.
/// Emits sectionSelected(name) when a menu item is clicked.
/// Items tagged as "(popup)" emit popupRequested(name) instead.
class SidebarNav : public QWidget {
    Q_OBJECT

public:
    explicit SidebarNav(QWidget* parent = nullptr);
    ~SidebarNav() override;

    /// Programmatically select an item (highlights it, emits signal).
    void setActiveItem(const QString& name);

signals:
    /// Emitted when a non-popup item is clicked.
    void sectionSelected(const QString& section);

    /// Emitted when a popup item is clicked (Pro Player, Dual Deck).
    void popupRequested(const QString& section);

private:
    struct NavItem {
        QPushButton* button = nullptr;
        bool         isPopup = false;
    };

    void addSectionHeader(const QString& title);
    void addNavItem(const QString& label, const QString& icon,
                    bool isPopup = false);
    void clearActiveHighlight();

    QVBoxLayout*           m_layout      = nullptr;
    QMap<QString, NavItem> m_items;
    QString                m_activeItem;
};

} // namespace dawcast::widgets
