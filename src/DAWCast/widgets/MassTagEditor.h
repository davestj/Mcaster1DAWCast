// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QDialog>
#include <QSplitter>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QTextEdit>
#include <QLabel>
#include <QCheckBox>
#include <QToolBar>
#include <QToolButton>
#include <QTableView>
#include <QHeaderView>
#include <QProgressBar>
#include <QGroupBox>
#include <QPushButton>

#include "../codec/TagTransfer.h"

namespace dawcast::widgets {

class TagTableModel;

// ---------------------------------------------------------------------------
// MassTagEditor — professional batch tag editor (Mp3tag / Abander style)
//
// A standalone dialog for managing audio file metadata at scale. Supports:
//   - Loading thousands of files with full tag display in a sortable table
//   - Multi-selection editing with <keep>/<mixed> merge logic
//   - Auto-tag from filename patterns
//   - Rename files from tags
//   - Track numbering wizard
//   - Artwork management (view, add, remove, extract)
//   - Undo all changes before save
//   - Copy/paste tag blocks between files
// ---------------------------------------------------------------------------

class MassTagEditor : public QDialog {
    Q_OBJECT

public:
    explicit MassTagEditor(QWidget* parent = nullptr);
    ~MassTagEditor() override;

    /// Add individual files to the editor.
    void addFiles(const QStringList& paths);

    /// Add all supported audio files from a directory.
    void addFolder(const QString& dirPath, bool recursive = true);

private slots:
    // Toolbar actions
    void onAddFiles();
    void onAddFolder();
    void onSaveAll();
    void onUndoAll();
    void onRemoveTags();
    void onAutoTagFromFilename();
    void onFilenameFromTags();
    void onNumberingWizard();

    // Tag panel
    void onTagPanelSave();
    void onArtworkAdd();
    void onArtworkRemove();
    void onArtworkExtract();

    // Table interaction
    void onSelectionChanged();
    void onTableContextMenu(const QPoint& pos);

    // Status updates
    void updateStatusBar();

private:
    void setupUi();
    void setupToolbar();
    void setupTagPanel();
    void setupFileTable();
    void setupStatusBar();

    /// Populate the tag panel from the currently selected row(s).
    void populateTagPanel(const QList<int>& selectedRows);
    /// Apply tag panel values to the currently selected row(s).
    void applyTagPanel(const QList<int>& selectedRows);
    /// Return the currently selected row indices in the model.
    QList<int> selectedModelRows() const;

    // ── Layout components ──────────────────────────────────────────────
    QToolBar*     m_toolbar       = nullptr;
    QSplitter*    m_splitter      = nullptr;

    // Tag panel (left sidebar)
    QWidget*      m_tagPanel      = nullptr;
    QLineEdit*    m_titleEdit     = nullptr;
    QLineEdit*    m_artistEdit    = nullptr;
    QLineEdit*    m_albumEdit     = nullptr;
    QLineEdit*    m_albumArtistEdit = nullptr;
    QSpinBox*     m_trackNumSpin  = nullptr;
    QSpinBox*     m_trackTotalSpin = nullptr;
    QSpinBox*     m_discNumSpin   = nullptr;
    QSpinBox*     m_discTotalSpin = nullptr;
    QSpinBox*     m_yearSpin      = nullptr;
    QComboBox*    m_genreCombo    = nullptr;
    QLineEdit*    m_composerEdit  = nullptr;
    QTextEdit*    m_commentEdit   = nullptr;
    QLineEdit*    m_copyrightEdit = nullptr;
    QLineEdit*    m_encoderEdit   = nullptr;
    QLineEdit*    m_urlEdit       = nullptr;
    QLabel*       m_artworkLabel  = nullptr;
    QPushButton*  m_artworkAddBtn = nullptr;
    QPushButton*  m_artworkRemBtn = nullptr;
    QPushButton*  m_artworkExtBtn = nullptr;

    // Podcast section (collapsible)
    QGroupBox*    m_podcastGroup  = nullptr;
    QLineEdit*    m_podEpisodeTitle = nullptr;
    QSpinBox*     m_podSeasonNum  = nullptr;
    QSpinBox*     m_podEpisodeNum = nullptr;
    QLineEdit*    m_podCategory   = nullptr;
    QTextEdit*    m_podDescription = nullptr;

    QPushButton*  m_tagPanelSaveBtn = nullptr;

    // File table (center)
    QTableView*   m_tableView     = nullptr;
    TagTableModel* m_model        = nullptr;

    // Filter bar
    QLineEdit*    m_filterEdit    = nullptr;

    // Status bar
    QLabel*       m_statusLabel   = nullptr;
    QProgressBar* m_progressBar   = nullptr;

    // Toolbar actions (stored for enable/disable logic)
    QAction*      m_actSaveAll    = nullptr;
    QAction*      m_actUndo       = nullptr;

    // Sentinel value for fields that differ across selected files
    static const QString kKeepPlaceholder;
};

} // namespace dawcast::widgets
