// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QWidget>
#include <QStringList>

class QComboBox;
class QLabel;
class QLineEdit;
class QMenu;
class QPushButton;
class QSortFilterProxyModel;
class QTableView;
class QToolButton;

namespace dawcast::widgets {

class LibraryTableModel;
class MediaBrowser;

// ---------------------------------------------------------------------------
// MediaLibraryWidget — organized file manager for audio/video assets
//
// Layout (top to bottom):
//   1. Title bar: icon + "Media Library" + category dropdown + clear button
//   2. Search bar: text input + magnifying glass button
//   3. Tab area: "Library" tab (table view) / "File Browser" tab (old tree)
//   4. File table: sortable, draggable, multi-select with context menu
//   5. Bottom bar: item count + Import Files + Import Folder buttons
//
// Categories: Recording, Podcast, Vocal, Voice Over, Music Bed, SFX, Video
// ---------------------------------------------------------------------------

class MediaLibraryWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MediaLibraryWidget(QWidget* parent = nullptr);
    ~MediaLibraryWidget() override;

    /// Import files into the library and refresh the table.
    void importFiles(const QStringList& paths);

    /// Import all supported files from a folder.
    void importFolder(const QString& dirPath);

    /// Access the embedded file browser (for backward compatibility).
    MediaBrowser* fileBrowser() const { return m_fileBrowser; }

signals:
    /// Emitted when the user double-clicks a file in the library table.
    void fileDoubleClicked(const QString& path);

    /// Emitted when files are requested for import (from the file browser).
    void importRequested(const QStringList& paths);

private slots:
    void onSearchTextChanged(const QString& text);
    void onCategoryChanged(int index);
    void onClearFilter();
    void onImportFilesClicked();
    void onImportFolderClicked();
    void onTableDoubleClicked(const QModelIndex& index);
    void onTableContextMenu(const QPoint& pos);

private:
    void setupUi();
    void refreshItemCount();

    // Top bar
    QComboBox*   m_categoryCombo = nullptr;
    QToolButton* m_clearFilterBtn = nullptr;

    // Search bar
    QLineEdit*   m_searchEdit = nullptr;

    // Table
    QTableView*           m_tableView  = nullptr;
    LibraryTableModel*    m_model      = nullptr;
    QSortFilterProxyModel* m_proxyModel = nullptr;

    // File browser (legacy, as a tab)
    MediaBrowser* m_fileBrowser = nullptr;

    // Bottom bar
    QLabel* m_itemCountLabel = nullptr;

    // Context menu
    QMenu* m_contextMenu = nullptr;
};

} // namespace dawcast::widgets
