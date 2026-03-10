// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "MediaLibraryWidget.h"
#include "LibraryTableModel.h"
#include "MediaBrowser.h"
#include "../core/MediaLibrary.h"

#include <QAction>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QTabWidget>
#include <QTableView>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

namespace dawcast::widgets {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

MediaLibraryWidget::MediaLibraryWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();

    // Connect to MediaLibrary change signals to refresh the view
    auto* lib = dawcast::MediaLibrary::instance();
    connect(lib, &dawcast::MediaLibrary::libraryChanged,
            this, [this]() {
        m_model->refresh();
        refreshItemCount();
    });

    // Initial data load
    m_model->refresh();
    refreshItemCount();
}

MediaLibraryWidget::~MediaLibraryWidget() = default;

// ---------------------------------------------------------------------------
// UI Setup
// ---------------------------------------------------------------------------

void MediaLibraryWidget::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // ── Top bar: title + category filter ─────────────────────────────────
    auto* topBar = new QHBoxLayout;
    topBar->setSpacing(6);

    auto* titleLabel = new QLabel(tr("Media Library"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 1);
    titleLabel->setFont(titleFont);
    topBar->addWidget(titleLabel);

    topBar->addStretch();

    m_categoryCombo = new QComboBox(this);
    m_categoryCombo->addItem(tr("All Files"),     QString());
    m_categoryCombo->addItem(tr("Recordings"),    QStringLiteral("Recording"));
    m_categoryCombo->addItem(tr("Podcasts"),       QStringLiteral("Podcast"));
    m_categoryCombo->addItem(tr("Vocals"),         QStringLiteral("Vocal"));
    m_categoryCombo->addItem(tr("Voice Over"),     QStringLiteral("Voice Over"));
    m_categoryCombo->addItem(tr("Music Beds"),     QStringLiteral("Music Bed"));
    m_categoryCombo->addItem(tr("Sound Effects"),  QStringLiteral("SFX"));
    m_categoryCombo->addItem(tr("Video"),          QStringLiteral("Video"));
    m_categoryCombo->setToolTip(tr("Filter by category"));
    m_categoryCombo->setMinimumWidth(120);
    topBar->addWidget(m_categoryCombo);

    m_clearFilterBtn = new QToolButton(this);
    m_clearFilterBtn->setText(QStringLiteral("X"));
    m_clearFilterBtn->setToolTip(tr("Clear filter"));
    m_clearFilterBtn->setAutoRaise(true);
    m_clearFilterBtn->setFixedSize(24, 24);
    topBar->addWidget(m_clearFilterBtn);

    mainLayout->addLayout(topBar);

    // ── Search bar ───────────────────────────────────────────────────────
    auto* searchLayout = new QHBoxLayout;
    searchLayout->setSpacing(4);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Search tracks by title, artist, album..."));
    m_searchEdit->setClearButtonEnabled(true);
    searchLayout->addWidget(m_searchEdit);

    auto* searchBtn = new QToolButton(this);
    searchBtn->setText(tr("Search"));
    searchBtn->setToolTip(tr("Search"));
    searchBtn->setAutoRaise(true);
    searchLayout->addWidget(searchBtn);

    mainLayout->addLayout(searchLayout);

    // ── Tab widget: Library table + File Browser ─────────────────────────
    auto* tabWidget = new QTabWidget(this);

    // -- Library tab --
    auto* libraryPage = new QWidget(tabWidget);
    auto* libraryLayout = new QVBoxLayout(libraryPage);
    libraryLayout->setContentsMargins(0, 0, 0, 0);

    m_model = new LibraryTableModel(this);

    m_proxyModel = new QSortFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_model);
    m_proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);

    m_tableView = new QTableView(libraryPage);
    m_tableView->setModel(m_proxyModel);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tableView->setSortingEnabled(true);
    m_tableView->setAlternatingRowColors(true);
    m_tableView->setShowGrid(false);
    m_tableView->setWordWrap(false);
    m_tableView->setDragEnabled(true);
    m_tableView->setDragDropMode(QAbstractItemView::DragOnly);
    m_tableView->setDefaultDropAction(Qt::CopyAction);
    m_tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tableView->verticalHeader()->setVisible(false);
    m_tableView->verticalHeader()->setDefaultSectionSize(24);

    // Column widths
    auto* header = m_tableView->horizontalHeader();
    header->setStretchLastSection(true);
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->resizeSection(LibraryTableModel::ColTitle,     200);
    header->resizeSection(LibraryTableModel::ColArtist,    140);
    header->resizeSection(LibraryTableModel::ColDuration,   70);
    header->resizeSection(LibraryTableModel::ColBPM,        55);
    header->resizeSection(LibraryTableModel::ColFormat,     60);
    header->resizeSection(LibraryTableModel::ColCategory,   90);
    header->resizeSection(LibraryTableModel::ColDateAdded,  90);

    // Sort by Date Added descending by default
    m_tableView->sortByColumn(LibraryTableModel::ColDateAdded, Qt::DescendingOrder);

    libraryLayout->addWidget(m_tableView);
    tabWidget->addTab(libraryPage, tr("Library"));

    // -- File Browser tab --
    m_fileBrowser = new MediaBrowser(tabWidget);
    tabWidget->addTab(m_fileBrowser, tr("File Browser"));

    mainLayout->addWidget(tabWidget, 1);

    // ── Bottom bar: item count + import buttons ──────────────────────────
    auto* bottomBar = new QHBoxLayout;
    bottomBar->setSpacing(8);

    m_itemCountLabel = new QLabel(this);
    m_itemCountLabel->setStyleSheet(QStringLiteral("color: #888;"));
    bottomBar->addWidget(m_itemCountLabel);

    bottomBar->addStretch();

    auto* importFilesBtn = new QPushButton(tr("Import Files"), this);
    importFilesBtn->setToolTip(tr("Import individual audio/video files"));
    bottomBar->addWidget(importFilesBtn);

    auto* importFolderBtn = new QPushButton(tr("Import Folder"), this);
    importFolderBtn->setToolTip(tr("Import all supported files from a folder"));
    bottomBar->addWidget(importFolderBtn);

    mainLayout->addLayout(bottomBar);

    // ── Context menu ─────────────────────────────────────────────────────
    m_contextMenu = new QMenu(this);
    m_contextMenu->addAction(tr("Play"), this, [this]() {
        QModelIndex idx = m_tableView->currentIndex();
        if (idx.isValid()) {
            QModelIndex srcIdx = m_proxyModel->mapToSource(idx);
            emit fileDoubleClicked(m_model->pathAtRow(srcIdx.row()));
        }
    });
    m_contextMenu->addAction(tr("Add to Timeline"), this, [this]() {
        QModelIndexList sel = m_tableView->selectionModel()->selectedRows();
        QStringList paths;
        for (const QModelIndex& idx : sel) {
            QModelIndex srcIdx = m_proxyModel->mapToSource(idx);
            paths.append(m_model->pathAtRow(srcIdx.row()));
        }
        if (!paths.isEmpty())
            emit importRequested(paths);
    });
    m_contextMenu->addSeparator();

    // Set Category submenu
    auto* categoryMenu = m_contextMenu->addMenu(tr("Set Category"));
    static const QStringList categories = {
        QStringLiteral("Recording"), QStringLiteral("Podcast"),
        QStringLiteral("Vocal"),     QStringLiteral("Voice Over"),
        QStringLiteral("Music Bed"), QStringLiteral("SFX"),
        QStringLiteral("Video")
    };
    for (const QString& cat : categories) {
        categoryMenu->addAction(cat, this, [this, cat]() {
            auto* lib = dawcast::MediaLibrary::instance();
            QModelIndexList sel = m_tableView->selectionModel()->selectedRows();
            for (const QModelIndex& idx : sel) {
                QModelIndex srcIdx = m_proxyModel->mapToSource(idx);
                int id = m_model->itemIdAtRow(srcIdx.row());
                if (id > 0) lib->setCategory(id, cat);
            }
            lib->saveDatabase();
        });
    }

    m_contextMenu->addSeparator();
    m_contextMenu->addAction(tr("Remove from Library"), this, [this]() {
        auto* lib = dawcast::MediaLibrary::instance();
        QModelIndexList sel = m_tableView->selectionModel()->selectedRows();

        // Collect ids before modifying the model
        QList<int> ids;
        for (const QModelIndex& idx : sel) {
            QModelIndex srcIdx = m_proxyModel->mapToSource(idx);
            int id = m_model->itemIdAtRow(srcIdx.row());
            if (id > 0) ids.append(id);
        }

        for (int id : ids) {
            lib->removeItem(id);
        }
        lib->saveDatabase();
    });

    m_contextMenu->addSeparator();
    m_contextMenu->addAction(tr("Show in Finder"), this, [this]() {
        QModelIndex idx = m_tableView->currentIndex();
        if (idx.isValid()) {
            QModelIndex srcIdx = m_proxyModel->mapToSource(idx);
            QString path = m_model->pathAtRow(srcIdx.row());
            if (!path.isEmpty()) {
                QDesktopServices::openUrl(
                    QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
            }
        }
    });

    // ── Signal connections ────────────────────────────────────────────────
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &MediaLibraryWidget::onSearchTextChanged);
    connect(m_categoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MediaLibraryWidget::onCategoryChanged);
    connect(m_clearFilterBtn, &QToolButton::clicked,
            this, &MediaLibraryWidget::onClearFilter);
    connect(m_tableView, &QTableView::doubleClicked,
            this, &MediaLibraryWidget::onTableDoubleClicked);
    connect(m_tableView, &QTableView::customContextMenuRequested,
            this, &MediaLibraryWidget::onTableContextMenu);
    connect(importFilesBtn, &QPushButton::clicked,
            this, &MediaLibraryWidget::onImportFilesClicked);
    connect(importFolderBtn, &QPushButton::clicked,
            this, &MediaLibraryWidget::onImportFolderClicked);

    // File browser signals: forward file operations
    connect(m_fileBrowser, &MediaBrowser::fileDoubleClicked,
            this, &MediaLibraryWidget::fileDoubleClicked);
    connect(m_fileBrowser, &MediaBrowser::importRequested,
            this, [this](const QStringList& paths) {
        importFiles(paths);
    });
}

// ---------------------------------------------------------------------------
// Public methods
// ---------------------------------------------------------------------------

void MediaLibraryWidget::importFiles(const QStringList& paths)
{
    auto* lib = dawcast::MediaLibrary::instance();
    for (const QString& path : paths) {
        lib->importFile(path);
    }
    lib->saveDatabase();
    // libraryChanged signal triggers refresh
}

void MediaLibraryWidget::importFolder(const QString& dirPath)
{
    dawcast::MediaLibrary::instance()->importFolder(dirPath);
    // libraryChanged signal triggers refresh
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void MediaLibraryWidget::onSearchTextChanged(const QString& text)
{
    m_model->setSearchQuery(text);
    refreshItemCount();
}

void MediaLibraryWidget::onCategoryChanged(int index)
{
    QString category = m_categoryCombo->itemData(index).toString();
    m_model->setCategoryFilter(category);
    refreshItemCount();
}

void MediaLibraryWidget::onClearFilter()
{
    m_searchEdit->clear();
    m_categoryCombo->setCurrentIndex(0);  // "All Files"
}

void MediaLibraryWidget::onImportFilesClicked()
{
    QStringList paths = QFileDialog::getOpenFileNames(
        this, tr("Import Media Files"),
        QDir::homePath(),
        tr("Audio/Video Files (*.wav *.mp3 *.flac *.aac *.ogg *.opus *.m4a "
           "*.aiff *.aif *.wma *.mp4 *.mov *.avi *.mkv *.webm);;"
           "All Files (*)"));

    if (!paths.isEmpty())
        importFiles(paths);
}

void MediaLibraryWidget::onImportFolderClicked()
{
    QString dir = QFileDialog::getExistingDirectory(
        this, tr("Import Folder"),
        QDir::homePath(),
        QFileDialog::ShowDirsOnly);

    if (!dir.isEmpty())
        importFolder(dir);
}

void MediaLibraryWidget::onTableDoubleClicked(const QModelIndex& index)
{
    if (!index.isValid()) return;
    QModelIndex srcIdx = m_proxyModel->mapToSource(index);
    QString path = m_model->pathAtRow(srcIdx.row());
    if (!path.isEmpty())
        emit fileDoubleClicked(path);
}

void MediaLibraryWidget::onTableContextMenu(const QPoint& pos)
{
    QModelIndex idx = m_tableView->indexAt(pos);
    if (!idx.isValid()) return;

    m_contextMenu->exec(m_tableView->viewport()->mapToGlobal(pos));
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void MediaLibraryWidget::refreshItemCount()
{
    int displayed = m_model->rowCount();
    int total     = dawcast::MediaLibrary::instance()->itemCount();

    if (displayed == total) {
        m_itemCountLabel->setText(tr("%1 items").arg(total));
    } else {
        m_itemCountLabel->setText(tr("%1 of %2 items").arg(displayed).arg(total));
    }
}

} // namespace dawcast::widgets
