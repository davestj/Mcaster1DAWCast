// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>
#include <QLabel>
#include <QComboBox>
#include <QTableWidget>
#include <cstdint>

namespace dawcast { class Timeline; }

namespace dawcast::widgets {

class BevelButton;

/// Dock-able panel listing all markers with controls for add, delete,
/// sort, inline editing, and navigation.
class MarkerListWidget : public QWidget {
    Q_OBJECT

public:
    explicit MarkerListWidget(QWidget* parent = nullptr);
    ~MarkerListWidget() override;

    void setTimeline(Timeline* timeline);

signals:
    /// Emitted when the user double-clicks a marker row to navigate.
    void markerSelected(int64_t position);

private slots:
    void onAddMarker();
    void onDeleteMarker();
    void onSortChanged(int index);
    void refreshList();
    void onCellDoubleClicked(int row, int column);
    void onCellChanged(int row, int column);
    void onCustomContextMenu(const QPoint& pos);

private:
    /// Populate a single row from a marker at the given index.
    void populateRow(int row, int markerIndex);

    /// Format a sample position as HH:MM:SS.mmm timecode.
    QString formatTimecode(int64_t samples) const;

    Timeline*      m_timeline   = nullptr;
    QTableWidget*  m_table      = nullptr;
    QComboBox*     m_sortCombo  = nullptr;
    QLabel*        m_countLabel = nullptr;
    BevelButton*   m_addBtn     = nullptr;
    BevelButton*   m_deleteBtn  = nullptr;

    bool m_updatingTable = false;  // guard against recursive cell-change signals
};

} // namespace dawcast::widgets
