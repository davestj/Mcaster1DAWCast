// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "MassTagEditor.h"
#include "TagTableModel.h"
#include "FilenameToTagDialog.h"
#include "TagToFilenameDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QScrollArea>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>
#include <QInputDialog>
#include <QPixmap>
#include <QImage>
#include <QBuffer>
#include <QApplication>
#include <QSortFilterProxyModel>
#include <QDesktopServices>
#include <QUrl>
#include <QProcess>
#include <QDialogButtonBox>

namespace dawcast::widgets {

const QString MassTagEditor::kKeepPlaceholder = QStringLiteral("<keep>");

// ── Common genre list (ID3v1 standard + modern additions) ──────────────────
static const QStringList kGenres = {
    QString(),  // empty = unset
    QStringLiteral("Blues"), QStringLiteral("Classic Rock"), QStringLiteral("Country"),
    QStringLiteral("Dance"), QStringLiteral("Disco"), QStringLiteral("Funk"),
    QStringLiteral("Grunge"), QStringLiteral("Hip-Hop"), QStringLiteral("Jazz"),
    QStringLiteral("Metal"), QStringLiteral("New Age"), QStringLiteral("Oldies"),
    QStringLiteral("Other"), QStringLiteral("Pop"), QStringLiteral("R&B"),
    QStringLiteral("Rap"), QStringLiteral("Reggae"), QStringLiteral("Rock"),
    QStringLiteral("Techno"), QStringLiteral("Industrial"), QStringLiteral("Alternative"),
    QStringLiteral("Ska"), QStringLiteral("Death Metal"), QStringLiteral("Punk"),
    QStringLiteral("Space"), QStringLiteral("Ambient"), QStringLiteral("Trip-Hop"),
    QStringLiteral("Vocal"), QStringLiteral("Jazz+Funk"), QStringLiteral("Fusion"),
    QStringLiteral("Trance"), QStringLiteral("Classical"), QStringLiteral("Instrumental"),
    QStringLiteral("Acid"), QStringLiteral("House"), QStringLiteral("Game"),
    QStringLiteral("Sound Clip"), QStringLiteral("Gospel"), QStringLiteral("Noise"),
    QStringLiteral("Electronic"), QStringLiteral("Folk"), QStringLiteral("Soundtrack"),
    QStringLiteral("Podcast"), QStringLiteral("Audiobook"), QStringLiteral("Speech"),
    QStringLiteral("Lo-Fi"), QStringLiteral("Drum & Bass"), QStringLiteral("Dubstep"),
    QStringLiteral("EDM"), QStringLiteral("Indie"), QStringLiteral("Latin"),
    QStringLiteral("Reggaeton"), QStringLiteral("K-Pop"), QStringLiteral("J-Pop"),
    QStringLiteral("World"), QStringLiteral("Singer-Songwriter")
};

// ── Constructor / Destructor ───────────────────────────────────────────────

MassTagEditor::MassTagEditor(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Mass Tag Editor"));
    setMinimumSize(1100, 700);
    resize(1300, 800);
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint);

    m_model = new TagTableModel(this);

    setupUi();

    // Connections
    connect(m_model, &TagTableModel::modifiedCountChanged,
            this, &MassTagEditor::updateStatusBar);
    connect(m_model, &TagTableModel::fileLoadProgress,
            this, [this](int cur, int total) {
        m_progressBar->setMaximum(total);
        m_progressBar->setValue(cur);
    });
    connect(m_model, &TagTableModel::saveProgress,
            this, [this](int cur, int total) {
        m_progressBar->setMaximum(total);
        m_progressBar->setValue(cur);
    });

    updateStatusBar();
}

MassTagEditor::~MassTagEditor() = default;

// ── Public API ─────────────────────────────────────────────────────────────

void MassTagEditor::addFiles(const QStringList& paths)
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_model->addFiles(paths);
    QApplication::restoreOverrideCursor();
    updateStatusBar();
}

void MassTagEditor::addFolder(const QString& dirPath, bool recursive)
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_model->addFolder(dirPath, recursive);
    QApplication::restoreOverrideCursor();
    updateStatusBar();
}

// ── UI Setup ───────────────────────────────────────────────────────────────

void MassTagEditor::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(2);

    setupToolbar();
    mainLayout->addWidget(m_toolbar);

    // Filter bar
    auto* filterLayout = new QHBoxLayout();
    auto* filterLabel = new QLabel(tr("Filter:"), this);
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(tr("Search by any field..."));
    m_filterEdit->setClearButtonEnabled(true);
    filterLayout->addWidget(filterLabel);
    filterLayout->addWidget(m_filterEdit, 1);
    mainLayout->addLayout(filterLayout);

    // Splitter: tag panel (left) | file table (center)
    m_splitter = new QSplitter(Qt::Horizontal, this);

    setupTagPanel();
    setupFileTable();

    m_splitter->addWidget(m_tagPanel);
    m_splitter->addWidget(m_tableView);
    m_splitter->setStretchFactor(0, 0);  // tag panel fixed-ish
    m_splitter->setStretchFactor(1, 1);  // table stretches
    m_splitter->setSizes({260, 1000});

    mainLayout->addWidget(m_splitter, 1);

    setupStatusBar();
    // Status bar is a horizontal layout at the bottom
    auto* statusLayout = new QHBoxLayout();
    statusLayout->addWidget(m_statusLabel, 1);
    statusLayout->addWidget(m_progressBar, 0);
    mainLayout->addLayout(statusLayout);
}

void MassTagEditor::setupToolbar()
{
    m_toolbar = new QToolBar(tr("Tag Editor"), this);
    m_toolbar->setMovable(false);
    m_toolbar->setFloatable(false);
    m_toolbar->setIconSize(QSize(20, 20));

    // Add Files
    auto* actAddFiles = m_toolbar->addAction(tr("Add Files..."));
    connect(actAddFiles, &QAction::triggered, this, &MassTagEditor::onAddFiles);

    // Add Folder
    auto* actAddFolder = m_toolbar->addAction(tr("Add Folder..."));
    connect(actAddFolder, &QAction::triggered, this, &MassTagEditor::onAddFolder);

    m_toolbar->addSeparator();

    // Save All
    m_actSaveAll = m_toolbar->addAction(tr("Save All"));
    m_actSaveAll->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));
    connect(m_actSaveAll, &QAction::triggered, this, &MassTagEditor::onSaveAll);

    // Undo All
    m_actUndo = m_toolbar->addAction(tr("Undo All"));
    m_actUndo->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Z));
    connect(m_actUndo, &QAction::triggered, this, &MassTagEditor::onUndoAll);

    m_toolbar->addSeparator();

    // Auto Tag menu
    auto* autoTagBtn = new QToolButton(this);
    autoTagBtn->setText(tr("Auto Tag"));
    autoTagBtn->setPopupMode(QToolButton::InstantPopup);
    auto* autoTagMenu = new QMenu(autoTagBtn);
    autoTagMenu->addAction(tr("From Filename..."), this, &MassTagEditor::onAutoTagFromFilename);
    autoTagMenu->addAction(tr("From Online (placeholder)"), this, [this]() {
        QMessageBox::information(this, tr("Online Lookup"),
            tr("Online tag lookup via MusicBrainz / AcoustID is planned for a future release."));
    });
    autoTagMenu->addAction(tr("Numbering Wizard..."), this, &MassTagEditor::onNumberingWizard);
    autoTagBtn->setMenu(autoTagMenu);
    m_toolbar->addWidget(autoTagBtn);

    // Filename from Tags
    auto* actFilenameFromTags = m_toolbar->addAction(tr("Filename from Tags..."));
    connect(actFilenameFromTags, &QAction::triggered, this, &MassTagEditor::onFilenameFromTags);

    m_toolbar->addSeparator();

    // Remove Tags
    auto* actRemoveTags = m_toolbar->addAction(tr("Remove Tags"));
    connect(actRemoveTags, &QAction::triggered, this, &MassTagEditor::onRemoveTags);
}

void MassTagEditor::setupTagPanel()
{
    // Scrollable tag panel
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setMinimumWidth(250);
    scrollArea->setMaximumWidth(350);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* panelWidget = new QWidget(scrollArea);
    auto* layout = new QFormLayout(panelWidget);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(4);

    // Standard fields
    m_titleEdit = new QLineEdit(panelWidget);
    m_titleEdit->setPlaceholderText(kKeepPlaceholder);
    layout->addRow(tr("Title:"), m_titleEdit);

    m_artistEdit = new QLineEdit(panelWidget);
    m_artistEdit->setPlaceholderText(kKeepPlaceholder);
    layout->addRow(tr("Artist:"), m_artistEdit);

    m_albumEdit = new QLineEdit(panelWidget);
    m_albumEdit->setPlaceholderText(kKeepPlaceholder);
    layout->addRow(tr("Album:"), m_albumEdit);

    m_albumArtistEdit = new QLineEdit(panelWidget);
    m_albumArtistEdit->setPlaceholderText(kKeepPlaceholder);
    layout->addRow(tr("Album Artist:"), m_albumArtistEdit);

    // Track # / Total
    auto* trackLayout = new QHBoxLayout();
    m_trackNumSpin = new QSpinBox(panelWidget);
    m_trackNumSpin->setRange(0, 9999);
    m_trackNumSpin->setSpecialValueText(kKeepPlaceholder);
    trackLayout->addWidget(m_trackNumSpin);
    trackLayout->addWidget(new QLabel(QStringLiteral("/"), panelWidget));
    m_trackTotalSpin = new QSpinBox(panelWidget);
    m_trackTotalSpin->setRange(0, 9999);
    m_trackTotalSpin->setSpecialValueText(kKeepPlaceholder);
    trackLayout->addWidget(m_trackTotalSpin);
    layout->addRow(tr("Track:"), trackLayout);

    // Disc # / Total
    auto* discLayout = new QHBoxLayout();
    m_discNumSpin = new QSpinBox(panelWidget);
    m_discNumSpin->setRange(0, 999);
    m_discNumSpin->setSpecialValueText(kKeepPlaceholder);
    discLayout->addWidget(m_discNumSpin);
    discLayout->addWidget(new QLabel(QStringLiteral("/"), panelWidget));
    m_discTotalSpin = new QSpinBox(panelWidget);
    m_discTotalSpin->setRange(0, 999);
    m_discTotalSpin->setSpecialValueText(kKeepPlaceholder);
    discLayout->addWidget(m_discTotalSpin);
    layout->addRow(tr("Disc:"), discLayout);

    m_yearSpin = new QSpinBox(panelWidget);
    m_yearSpin->setRange(0, 2099);
    m_yearSpin->setSpecialValueText(kKeepPlaceholder);
    layout->addRow(tr("Year:"), m_yearSpin);

    m_genreCombo = new QComboBox(panelWidget);
    m_genreCombo->setEditable(true);
    m_genreCombo->addItems(kGenres);
    m_genreCombo->setCurrentIndex(-1);
    layout->addRow(tr("Genre:"), m_genreCombo);

    m_composerEdit = new QLineEdit(panelWidget);
    m_composerEdit->setPlaceholderText(kKeepPlaceholder);
    layout->addRow(tr("Composer:"), m_composerEdit);

    m_commentEdit = new QTextEdit(panelWidget);
    m_commentEdit->setMaximumHeight(60);
    m_commentEdit->setPlaceholderText(kKeepPlaceholder);
    layout->addRow(tr("Comment:"), m_commentEdit);

    m_copyrightEdit = new QLineEdit(panelWidget);
    m_copyrightEdit->setPlaceholderText(kKeepPlaceholder);
    layout->addRow(tr("Copyright:"), m_copyrightEdit);

    m_encoderEdit = new QLineEdit(panelWidget);
    m_encoderEdit->setPlaceholderText(kKeepPlaceholder);
    layout->addRow(tr("Encoder:"), m_encoderEdit);

    m_urlEdit = new QLineEdit(panelWidget);
    m_urlEdit->setPlaceholderText(kKeepPlaceholder);
    layout->addRow(tr("URL:"), m_urlEdit);

    // ── Artwork ────────────────────────────────────────────────────────
    layout->addRow(new QLabel(tr("Artwork:"), panelWidget));
    m_artworkLabel = new QLabel(panelWidget);
    m_artworkLabel->setFixedSize(200, 200);
    m_artworkLabel->setAlignment(Qt::AlignCenter);
    m_artworkLabel->setFrameShape(QFrame::StyledPanel);
    m_artworkLabel->setStyleSheet(QStringLiteral("background: #222;"));
    m_artworkLabel->setText(tr("No Artwork"));
    layout->addRow(m_artworkLabel);

    auto* artBtnLayout = new QHBoxLayout();
    m_artworkAddBtn = new QPushButton(tr("Add"), panelWidget);
    m_artworkRemBtn = new QPushButton(tr("Remove"), panelWidget);
    m_artworkExtBtn = new QPushButton(tr("Extract"), panelWidget);
    artBtnLayout->addWidget(m_artworkAddBtn);
    artBtnLayout->addWidget(m_artworkRemBtn);
    artBtnLayout->addWidget(m_artworkExtBtn);
    layout->addRow(artBtnLayout);

    connect(m_artworkAddBtn, &QPushButton::clicked, this, &MassTagEditor::onArtworkAdd);
    connect(m_artworkRemBtn, &QPushButton::clicked, this, &MassTagEditor::onArtworkRemove);
    connect(m_artworkExtBtn, &QPushButton::clicked, this, &MassTagEditor::onArtworkExtract);

    // ── Podcast section (collapsible) ──────────────────────────────────
    m_podcastGroup = new QGroupBox(tr("Podcast"), panelWidget);
    m_podcastGroup->setCheckable(true);
    m_podcastGroup->setChecked(false);
    auto* podLayout = new QFormLayout(m_podcastGroup);

    m_podEpisodeTitle = new QLineEdit(m_podcastGroup);
    m_podEpisodeTitle->setPlaceholderText(kKeepPlaceholder);
    podLayout->addRow(tr("Episode Title:"), m_podEpisodeTitle);

    m_podSeasonNum = new QSpinBox(m_podcastGroup);
    m_podSeasonNum->setRange(0, 999);
    m_podSeasonNum->setSpecialValueText(kKeepPlaceholder);
    podLayout->addRow(tr("Season #:"), m_podSeasonNum);

    m_podEpisodeNum = new QSpinBox(m_podcastGroup);
    m_podEpisodeNum->setRange(0, 9999);
    m_podEpisodeNum->setSpecialValueText(kKeepPlaceholder);
    podLayout->addRow(tr("Episode #:"), m_podEpisodeNum);

    m_podCategory = new QLineEdit(m_podcastGroup);
    m_podCategory->setPlaceholderText(kKeepPlaceholder);
    podLayout->addRow(tr("Category:"), m_podCategory);

    m_podDescription = new QTextEdit(m_podcastGroup);
    m_podDescription->setMaximumHeight(60);
    m_podDescription->setPlaceholderText(kKeepPlaceholder);
    podLayout->addRow(tr("Description:"), m_podDescription);

    layout->addRow(m_podcastGroup);

    // ── Save button ────────────────────────────────────────────────────
    m_tagPanelSaveBtn = new QPushButton(tr("Apply to Selected"), panelWidget);
    m_tagPanelSaveBtn->setStyleSheet(
        QStringLiteral("QPushButton { background: #2d7d46; color: white; "
                        "padding: 6px; font-weight: bold; } "
                        "QPushButton:hover { background: #3a9d5a; }"));
    layout->addRow(m_tagPanelSaveBtn);
    connect(m_tagPanelSaveBtn, &QPushButton::clicked, this, &MassTagEditor::onTagPanelSave);

    scrollArea->setWidget(panelWidget);
    m_tagPanel = scrollArea;
}

void MassTagEditor::setupFileTable()
{
    m_tableView = new QTableView(this);
    m_tableView->setModel(m_model);
    m_tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSortingEnabled(true);
    m_tableView->setAlternatingRowColors(true);
    m_tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tableView->setEditTriggers(QAbstractItemView::DoubleClicked
                                 | QAbstractItemView::EditKeyPressed);
    m_tableView->verticalHeader()->setVisible(false);
    m_tableView->verticalHeader()->setDefaultSectionSize(24);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
    m_tableView->horizontalHeader()->setSectionsMovable(true);
    m_tableView->horizontalHeader()->setSortIndicatorShown(true);

    // Default column widths
    m_tableView->setColumnWidth(TagTableModel::ColRow, 40);
    m_tableView->setColumnWidth(TagTableModel::ColFilename, 180);
    m_tableView->setColumnWidth(TagTableModel::ColTitle, 180);
    m_tableView->setColumnWidth(TagTableModel::ColArtist, 150);
    m_tableView->setColumnWidth(TagTableModel::ColAlbum, 150);
    m_tableView->setColumnWidth(TagTableModel::ColAlbumArtist, 120);
    m_tableView->setColumnWidth(TagTableModel::ColTrack, 60);
    m_tableView->setColumnWidth(TagTableModel::ColYear, 50);
    m_tableView->setColumnWidth(TagTableModel::ColGenre, 100);
    m_tableView->setColumnWidth(TagTableModel::ColComposer, 120);
    m_tableView->setColumnWidth(TagTableModel::ColDuration, 60);
    m_tableView->setColumnWidth(TagTableModel::ColBitrate, 70);
    m_tableView->setColumnWidth(TagTableModel::ColSampleRate, 80);
    m_tableView->setColumnWidth(TagTableModel::ColFormat, 55);
    m_tableView->setColumnWidth(TagTableModel::ColSize, 70);

    // Selection -> tag panel update
    connect(m_tableView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MassTagEditor::onSelectionChanged);

    // Context menu
    connect(m_tableView, &QTableView::customContextMenuRequested,
            this, &MassTagEditor::onTableContextMenu);

    // Filter: apply simple text filter across all columns
    connect(m_filterEdit, &QLineEdit::textChanged,
            this, [this](const QString& text) {
        // Simple approach: hide rows that don't match.
        // For very large lists a QSortFilterProxyModel would be better,
        // but this avoids the complexity of proxy index mapping.
        const QString filter = text.trimmed().toLower();
        for (int r = 0; r < m_model->rowCount(); ++r) {
            bool match = filter.isEmpty();
            if (!match) {
                for (int c = 0; c < m_model->columnCount(); ++c) {
                    QVariant val = m_model->data(m_model->index(r, c));
                    if (val.toString().toLower().contains(filter)) {
                        match = true;
                        break;
                    }
                }
            }
            m_tableView->setRowHidden(r, !match);
        }
    });
}

void MassTagEditor::setupStatusBar()
{
    m_statusLabel = new QLabel(this);
    m_progressBar = new QProgressBar(this);
    m_progressBar->setMaximumWidth(200);
    m_progressBar->setMaximumHeight(16);
    m_progressBar->setTextVisible(false);
    m_progressBar->setVisible(false);
}

// ── Toolbar Slots ──────────────────────────────────────────────────────────

void MassTagEditor::onAddFiles()
{
    QStringList paths = QFileDialog::getOpenFileNames(
        this, tr("Add Audio Files"), QString(),
        tr("Audio Files (*.mp3 *.flac *.ogg *.opus *.m4a *.aac *.wav *.aiff *.aif "
           "*.wma *.ape *.wv *.mpc *.mp4 *.oga);;"
           "All Files (*)"));

    if (!paths.isEmpty())
        addFiles(paths);
}

void MassTagEditor::onAddFolder()
{
    QString dir = QFileDialog::getExistingDirectory(
        this, tr("Add Folder"), QString(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (!dir.isEmpty()) {
        // Ask about recursive scanning
        auto reply = QMessageBox::question(this, tr("Recursive?"),
            tr("Scan subfolders recursively?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        addFolder(dir, reply == QMessageBox::Yes);
    }
}

void MassTagEditor::onSaveAll()
{
    if (m_model->modifiedCount() == 0) {
        QMessageBox::information(this, tr("Save"), tr("No modified files to save."));
        return;
    }

    m_progressBar->setVisible(true);
    QApplication::setOverrideCursor(Qt::WaitCursor);
    int saved = m_model->saveAll();
    QApplication::restoreOverrideCursor();
    m_progressBar->setVisible(false);

    QMessageBox::information(this, tr("Save Complete"),
        tr("Successfully saved tags for %1 file(s).").arg(saved));
    updateStatusBar();
}

void MassTagEditor::onUndoAll()
{
    if (m_model->modifiedCount() == 0) return;

    auto reply = QMessageBox::question(this, tr("Undo All"),
        tr("Revert all %1 modified file(s) to their original tags?")
            .arg(m_model->modifiedCount()),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        m_model->revertAll();
        onSelectionChanged();  // refresh tag panel
    }
}

void MassTagEditor::onRemoveTags()
{
    QList<int> rows = selectedModelRows();
    if (rows.isEmpty()) {
        QMessageBox::information(this, tr("Remove Tags"),
            tr("Select one or more files first."));
        return;
    }

    auto reply = QMessageBox::warning(this, tr("Remove Tags"),
        tr("Strip ALL metadata from %1 selected file(s)?\n"
           "This will clear title, artist, album, artwork, etc.\n"
           "Save to write changes to disk.").arg(rows.size()),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    for (int r : rows) {
        AudioTags empty;
        m_model->setTagsAt(r, empty);
    }
    onSelectionChanged();
}

void MassTagEditor::onAutoTagFromFilename()
{
    QList<int> rows = selectedModelRows();
    if (rows.isEmpty()) {
        // If nothing selected, use all files
        for (int i = 0; i < m_model->entryCount(); ++i)
            rows.append(i);
    }
    if (rows.isEmpty()) return;

    QStringList paths;
    paths.reserve(rows.size());
    for (int r : rows)
        paths.append(m_model->entryAt(r).path);

    FilenameToTagDialog dlg(paths, this);
    if (dlg.exec() != QDialog::Accepted) return;

    auto results = dlg.results();
    for (int r : rows) {
        const QString& path = m_model->entryAt(r).path;
        auto it = results.constFind(path);
        if (it == results.constEnd()) continue;

        // Merge: only overwrite non-empty parsed fields
        AudioTags merged = m_model->entryAt(r).tags;
        const AudioTags& parsed = it.value();
        if (!parsed.title.isEmpty())       merged.title = parsed.title;
        if (!parsed.artist.isEmpty())      merged.artist = parsed.artist;
        if (!parsed.album.isEmpty())       merged.album = parsed.album;
        if (!parsed.albumArtist.isEmpty()) merged.albumArtist = parsed.albumArtist;
        if (!parsed.genre.isEmpty())       merged.genre = parsed.genre;
        if (!parsed.composer.isEmpty())    merged.composer = parsed.composer;
        if (parsed.trackNumber > 0)        merged.trackNumber = parsed.trackNumber;
        if (parsed.discNumber > 0)         merged.discNumber = parsed.discNumber;
        if (parsed.year > 0)               merged.year = parsed.year;

        m_model->setTagsAt(r, merged);
    }
    onSelectionChanged();
}

void MassTagEditor::onFilenameFromTags()
{
    QList<int> rows = selectedModelRows();
    if (rows.isEmpty()) {
        QMessageBox::information(this, tr("Filename from Tags"),
            tr("Select one or more files first."));
        return;
    }

    QStringList paths;
    QMap<QString, AudioTags> tagMap;
    for (int r : rows) {
        const auto& e = m_model->entryAt(r);
        paths.append(e.path);
        tagMap.insert(e.path, e.tags);
    }

    TagToFilenameDialog dlg(paths, tagMap, this);
    if (dlg.exec() != QDialog::Accepted) return;

    auto renames = dlg.renames();
    if (renames.isEmpty()) return;

    int renamed = 0;
    int failed = 0;
    for (auto it = renames.constBegin(); it != renames.constEnd(); ++it) {
        const QString& oldPath = it.key();
        const QString& newPath = it.value();

        // Create directory if needed
        QFileInfo nfi(newPath);
        QDir().mkpath(nfi.absolutePath());

        if (QFile::rename(oldPath, newPath)) {
            ++renamed;
            // Update the model entry path
            for (int r = 0; r < m_model->entryCount(); ++r) {
                if (m_model->entryAt(r).path == oldPath) {
                    m_model->entryAt(r).path = newPath;
                    break;
                }
            }
        } else {
            ++failed;
        }
    }

    QString msg = tr("Renamed %1 file(s).").arg(renamed);
    if (failed > 0)
        msg += tr("\n%1 file(s) could not be renamed.").arg(failed);
    QMessageBox::information(this, tr("Rename Complete"), msg);
}

void MassTagEditor::onNumberingWizard()
{
    QList<int> rows = selectedModelRows();
    if (rows.isEmpty()) {
        // If nothing selected, use all visible rows in current sort order
        for (int i = 0; i < m_model->entryCount(); ++i) {
            if (!m_tableView->isRowHidden(i))
                rows.append(i);
        }
    }
    if (rows.isEmpty()) return;

    // Simple numbering dialog
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Track Numbering Wizard"));
    dlg.setMinimumWidth(300);

    auto* layout = new QFormLayout(&dlg);

    auto* startSpin = new QSpinBox(&dlg);
    startSpin->setRange(1, 9999);
    startSpin->setValue(1);
    layout->addRow(tr("Starting number:"), startSpin);

    auto* incSpin = new QSpinBox(&dlg);
    incSpin->setRange(1, 100);
    incSpin->setValue(1);
    layout->addRow(tr("Increment:"), incSpin);

    auto* paddingCombo = new QComboBox(&dlg);
    paddingCombo->addItem(tr("None (1, 2, 3)"), 0);
    paddingCombo->addItem(tr("2 digits (01, 02)"), 2);
    paddingCombo->addItem(tr("3 digits (001, 002)"), 3);
    paddingCombo->setCurrentIndex(1);
    layout->addRow(tr("Padding:"), paddingCombo);

    auto* totalCheck = new QCheckBox(tr("Set track total to number of files"), &dlg);
    totalCheck->setChecked(true);
    layout->addRow(totalCheck);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;

    int num = startSpin->value();
    const int inc = incSpin->value();
    const int total = totalCheck->isChecked() ? rows.size() : 0;

    for (int r : rows) {
        auto tags = m_model->entryAt(r).tags;
        tags.trackNumber = num;
        if (total > 0) tags.trackTotal = total;
        m_model->setTagsAt(r, tags);
        num += inc;
    }
    onSelectionChanged();
}

// ── Tag Panel ──────────────────────────────────────────────────────────────

void MassTagEditor::onTagPanelSave()
{
    QList<int> rows = selectedModelRows();
    if (rows.isEmpty()) return;
    applyTagPanel(rows);
}

void MassTagEditor::onArtworkAdd()
{
    QList<int> rows = selectedModelRows();
    if (rows.isEmpty()) return;

    QString imgPath = QFileDialog::getOpenFileName(
        this, tr("Add Artwork"), QString(),
        tr("Images (*.jpg *.jpeg *.png *.bmp *.gif);;All Files (*)"));
    if (imgPath.isEmpty()) return;

    QFile f(imgPath);
    if (!f.open(QIODevice::ReadOnly)) return;
    QByteArray data = f.readAll();
    f.close();

    QString mime = QStringLiteral("image/jpeg");
    if (imgPath.endsWith(QLatin1String(".png"), Qt::CaseInsensitive))
        mime = QStringLiteral("image/png");

    for (int r : rows) {
        auto tags = m_model->entryAt(r).tags;
        tags.artworkData = data;
        tags.artworkMimeType = mime;
        m_model->setTagsAt(r, tags);
    }

    // Update artwork preview
    QPixmap pix;
    pix.loadFromData(data);
    m_artworkLabel->setPixmap(pix.scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void MassTagEditor::onArtworkRemove()
{
    QList<int> rows = selectedModelRows();
    if (rows.isEmpty()) return;

    for (int r : rows) {
        auto tags = m_model->entryAt(r).tags;
        tags.artworkData.clear();
        tags.artworkMimeType.clear();
        m_model->setTagsAt(r, tags);
    }
    m_artworkLabel->clear();
    m_artworkLabel->setText(tr("No Artwork"));
}

void MassTagEditor::onArtworkExtract()
{
    QList<int> rows = selectedModelRows();
    if (rows.isEmpty()) return;

    const auto& tags = m_model->entryAt(rows.first()).tags;
    if (tags.artworkData.isEmpty()) {
        QMessageBox::information(this, tr("Extract Artwork"),
            tr("Selected file has no embedded artwork."));
        return;
    }

    QString ext = QStringLiteral("jpg");
    if (tags.artworkMimeType.contains(QLatin1String("png")))
        ext = QStringLiteral("png");

    QString savePath = QFileDialog::getSaveFileName(
        this, tr("Save Artwork"),
        QStringLiteral("artwork.%1").arg(ext),
        tr("Images (*.jpg *.png);;All Files (*)"));

    if (savePath.isEmpty()) return;

    QFile f(savePath);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(tags.artworkData);
        f.close();
        QMessageBox::information(this, tr("Artwork Extracted"),
            tr("Artwork saved to:\n%1").arg(savePath));
    }
}

// ── Table Interaction ──────────────────────────────────────────────────────

void MassTagEditor::onSelectionChanged()
{
    QList<int> rows = selectedModelRows();
    populateTagPanel(rows);
}

void MassTagEditor::onTableContextMenu(const QPoint& pos)
{
    QModelIndex idx = m_tableView->indexAt(pos);
    QList<int> rows = selectedModelRows();

    QMenu menu(this);

    if (!rows.isEmpty()) {
        menu.addAction(tr("Edit in Tag Panel"), this, [this]() {
            // Just focus the tag panel — selection already populates it
            m_titleEdit->setFocus();
        });

        menu.addSeparator();

        menu.addAction(tr("Copy Tags"), this, [this, &rows]() {
            if (!rows.isEmpty())
                m_model->copyTagsFrom(rows.first());
        });

        menu.addAction(tr("Paste Tags"), this, [this, &rows]() {
            m_model->pasteTags(rows);
            onSelectionChanged();
        });

        menu.addSeparator();

        menu.addAction(tr("Remove from List"), this, [this, rows]() {
            m_model->removeRows(rows);
            updateStatusBar();
        });

        menu.addSeparator();

#if defined(Q_OS_MAC)
        menu.addAction(tr("Show in Finder"), this, [this, &rows]() {
            if (!rows.isEmpty()) {
                const QString& path = m_model->entryAt(rows.first()).path;
                QProcess::startDetached(QStringLiteral("open"),
                    {QStringLiteral("-R"), path});
            }
        });
#else
        menu.addAction(tr("Open Containing Folder"), this, [this, &rows]() {
            if (!rows.isEmpty()) {
                QFileInfo fi(m_model->entryAt(rows.first()).path);
                QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absolutePath()));
            }
        });
#endif
    }

    if (!menu.isEmpty())
        menu.exec(m_tableView->viewport()->mapToGlobal(pos));
}

// ── Tag Panel Populate / Apply ─────────────────────────────────────────────

void MassTagEditor::populateTagPanel(const QList<int>& selectedRows)
{
    if (selectedRows.isEmpty()) {
        // Clear all fields
        m_titleEdit->clear();
        m_artistEdit->clear();
        m_albumEdit->clear();
        m_albumArtistEdit->clear();
        m_trackNumSpin->setValue(0);
        m_trackTotalSpin->setValue(0);
        m_discNumSpin->setValue(0);
        m_discTotalSpin->setValue(0);
        m_yearSpin->setValue(0);
        m_genreCombo->setCurrentIndex(-1);
        m_composerEdit->clear();
        m_commentEdit->clear();
        m_copyrightEdit->clear();
        m_encoderEdit->clear();
        m_urlEdit->clear();
        m_artworkLabel->clear();
        m_artworkLabel->setText(tr("No Artwork"));
        m_podEpisodeTitle->clear();
        m_podSeasonNum->setValue(0);
        m_podEpisodeNum->setValue(0);
        m_podCategory->clear();
        m_podDescription->clear();
        return;
    }

    if (selectedRows.size() == 1) {
        // Single selection: show exact values
        const auto& tags = m_model->entryAt(selectedRows.first()).tags;
        m_titleEdit->setText(tags.title);
        m_artistEdit->setText(tags.artist);
        m_albumEdit->setText(tags.album);
        m_albumArtistEdit->setText(tags.albumArtist);
        m_trackNumSpin->setValue(tags.trackNumber);
        m_trackTotalSpin->setValue(tags.trackTotal);
        m_discNumSpin->setValue(tags.discNumber);
        m_discTotalSpin->setValue(tags.discTotal);
        m_yearSpin->setValue(tags.year);
        m_genreCombo->setCurrentText(tags.genre);
        m_composerEdit->setText(tags.composer);
        m_commentEdit->setPlainText(tags.comment);
        m_copyrightEdit->setText(tags.copyright);
        m_encoderEdit->setText(tags.encoder);
        m_urlEdit->setText(tags.url);

        // Artwork
        if (!tags.artworkData.isEmpty()) {
            QPixmap pix;
            pix.loadFromData(tags.artworkData);
            m_artworkLabel->setPixmap(
                pix.scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            m_artworkLabel->clear();
            m_artworkLabel->setText(tr("No Artwork"));
        }

        // Podcast
        m_podEpisodeTitle->setText(tags.podcastTitle);
        m_podSeasonNum->setValue(tags.podcastSeason.toInt());
        m_podEpisodeNum->setValue(tags.podcastEpisode.toInt());
        m_podCategory->setText(tags.podcastCategory);
        m_podDescription->setPlainText(tags.podcastDescription);
        return;
    }

    // Multi-selection: show shared values, <keep> for differing
    const auto& first = m_model->entryAt(selectedRows.first()).tags;

    auto fieldMatches = [&](auto getter) -> bool {
        const auto& ref = getter(first);
        for (int i = 1; i < selectedRows.size(); ++i) {
            if (getter(m_model->entryAt(selectedRows.at(i)).tags) != ref)
                return false;
        }
        return true;
    };

    auto setText = [&](QLineEdit* edit, const QString& val, bool matches) {
        if (matches) {
            edit->setText(val);
        } else {
            edit->clear();
            edit->setPlaceholderText(kKeepPlaceholder);
        }
    };

    auto setSpinVal = [&](QSpinBox* spin, int val, bool matches) {
        spin->setValue(matches ? val : 0);
    };

    setText(m_titleEdit,       first.title,
            fieldMatches([](const AudioTags& t) -> const QString& { return t.title; }));
    setText(m_artistEdit,      first.artist,
            fieldMatches([](const AudioTags& t) -> const QString& { return t.artist; }));
    setText(m_albumEdit,       first.album,
            fieldMatches([](const AudioTags& t) -> const QString& { return t.album; }));
    setText(m_albumArtistEdit, first.albumArtist,
            fieldMatches([](const AudioTags& t) -> const QString& { return t.albumArtist; }));
    setText(m_composerEdit,    first.composer,
            fieldMatches([](const AudioTags& t) -> const QString& { return t.composer; }));
    setText(m_copyrightEdit,   first.copyright,
            fieldMatches([](const AudioTags& t) -> const QString& { return t.copyright; }));
    setText(m_encoderEdit,     first.encoder,
            fieldMatches([](const AudioTags& t) -> const QString& { return t.encoder; }));
    setText(m_urlEdit,         first.url,
            fieldMatches([](const AudioTags& t) -> const QString& { return t.url; }));

    bool trackMatch = fieldMatches([](const AudioTags& t) -> const int& { return t.trackNumber; });
    setSpinVal(m_trackNumSpin,   first.trackNumber, trackMatch);
    bool trackTotMatch = fieldMatches([](const AudioTags& t) -> const int& { return t.trackTotal; });
    setSpinVal(m_trackTotalSpin, first.trackTotal, trackTotMatch);
    bool discMatch = fieldMatches([](const AudioTags& t) -> const int& { return t.discNumber; });
    setSpinVal(m_discNumSpin,    first.discNumber, discMatch);
    bool discTotMatch = fieldMatches([](const AudioTags& t) -> const int& { return t.discTotal; });
    setSpinVal(m_discTotalSpin,  first.discTotal, discTotMatch);

    bool yearMatch = fieldMatches([](const AudioTags& t) -> const int& { return t.year; });
    setSpinVal(m_yearSpin, first.year, yearMatch);

    bool genreMatch = fieldMatches([](const AudioTags& t) -> const QString& { return t.genre; });
    if (genreMatch) {
        m_genreCombo->setCurrentText(first.genre);
    } else {
        m_genreCombo->setCurrentIndex(-1);
        m_genreCombo->lineEdit()->setPlaceholderText(kKeepPlaceholder);
    }

    // Comment
    bool commentMatch = fieldMatches([](const AudioTags& t) -> const QString& { return t.comment; });
    if (commentMatch) {
        m_commentEdit->setPlainText(first.comment);
    } else {
        m_commentEdit->clear();
        m_commentEdit->setPlaceholderText(kKeepPlaceholder);
    }

    // Artwork: check if all have identical artwork
    bool artworkMatch = true;
    for (int i = 1; i < selectedRows.size(); ++i) {
        if (m_model->entryAt(selectedRows.at(i)).tags.artworkData != first.artworkData) {
            artworkMatch = false;
            break;
        }
    }
    if (artworkMatch && !first.artworkData.isEmpty()) {
        QPixmap pix;
        pix.loadFromData(first.artworkData);
        m_artworkLabel->setPixmap(
            pix.scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else if (!artworkMatch) {
        m_artworkLabel->clear();
        m_artworkLabel->setText(tr("<mixed>"));
    } else {
        m_artworkLabel->clear();
        m_artworkLabel->setText(tr("No Artwork"));
    }

    // Podcast fields
    setText(m_podEpisodeTitle, first.podcastTitle,
            fieldMatches([](const AudioTags& t) -> const QString& { return t.podcastTitle; }));
    setText(m_podCategory,     first.podcastCategory,
            fieldMatches([](const AudioTags& t) -> const QString& { return t.podcastCategory; }));

    bool podDescMatch = fieldMatches(
        [](const AudioTags& t) -> const QString& { return t.podcastDescription; });
    if (podDescMatch) {
        m_podDescription->setPlainText(first.podcastDescription);
    } else {
        m_podDescription->clear();
        m_podDescription->setPlaceholderText(kKeepPlaceholder);
    }

    bool seasonMatch = true;
    int firstSeason = first.podcastSeason.toInt();
    for (int i = 1; i < selectedRows.size(); ++i) {
        if (m_model->entryAt(selectedRows.at(i)).tags.podcastSeason.toInt() != firstSeason) {
            seasonMatch = false;
            break;
        }
    }
    m_podSeasonNum->setValue(seasonMatch ? firstSeason : 0);

    bool episodeMatch = true;
    int firstEp = first.podcastEpisode.toInt();
    for (int i = 1; i < selectedRows.size(); ++i) {
        if (m_model->entryAt(selectedRows.at(i)).tags.podcastEpisode.toInt() != firstEp) {
            episodeMatch = false;
            break;
        }
    }
    m_podEpisodeNum->setValue(episodeMatch ? firstEp : 0);
}

void MassTagEditor::applyTagPanel(const QList<int>& selectedRows)
{
    if (selectedRows.isEmpty()) return;

    // For multi-select, only apply fields that the user has actually changed
    // from the <keep> state. If a field shows a value (not placeholder), apply it.

    for (int r : selectedRows) {
        AudioTags tags = m_model->entryAt(r).tags;

        // Text fields: apply if non-empty text or if the user cleared it
        // (empty + no placeholder = intentional clear)
        auto applyText = [](QLineEdit* edit, QString& field) {
            const QString text = edit->text();
            // If placeholder is showing and text is empty, skip (keep original)
            if (text.isEmpty() && edit->placeholderText() == kKeepPlaceholder)
                return;
            field = text;
        };

        applyText(m_titleEdit,       tags.title);
        applyText(m_artistEdit,      tags.artist);
        applyText(m_albumEdit,       tags.album);
        applyText(m_albumArtistEdit, tags.albumArtist);
        applyText(m_composerEdit,    tags.composer);
        applyText(m_copyrightEdit,   tags.copyright);
        applyText(m_encoderEdit,     tags.encoder);
        applyText(m_urlEdit,         tags.url);

        // Spin boxes: apply if value > 0 (0 means <keep>)
        auto applySpin = [](QSpinBox* spin, int& field) {
            if (spin->value() > 0)
                field = spin->value();
        };

        applySpin(m_trackNumSpin,   tags.trackNumber);
        applySpin(m_trackTotalSpin, tags.trackTotal);
        applySpin(m_discNumSpin,    tags.discNumber);
        applySpin(m_discTotalSpin,  tags.discTotal);
        applySpin(m_yearSpin,       tags.year);

        // Genre: apply if there's text
        if (!m_genreCombo->currentText().isEmpty())
            tags.genre = m_genreCombo->currentText();

        // Comment
        if (!m_commentEdit->toPlainText().isEmpty() ||
            m_commentEdit->placeholderText() != kKeepPlaceholder) {
            tags.comment = m_commentEdit->toPlainText();
        }

        // Podcast fields
        if (m_podcastGroup->isChecked()) {
            applyText(m_podEpisodeTitle, tags.podcastTitle);
            applyText(m_podCategory,     tags.podcastCategory);

            if (m_podSeasonNum->value() > 0)
                tags.podcastSeason = QString::number(m_podSeasonNum->value());
            if (m_podEpisodeNum->value() > 0)
                tags.podcastEpisode = QString::number(m_podEpisodeNum->value());

            if (!m_podDescription->toPlainText().isEmpty() ||
                m_podDescription->placeholderText() != kKeepPlaceholder) {
                tags.podcastDescription = m_podDescription->toPlainText();
            }
        }

        m_model->setTagsAt(r, tags);
    }
}

// ── Helpers ────────────────────────────────────────────────────────────────

QList<int> MassTagEditor::selectedModelRows() const
{
    QList<int> rows;
    const auto indexes = m_tableView->selectionModel()->selectedRows();
    rows.reserve(indexes.size());
    for (const QModelIndex& idx : indexes)
        rows.append(idx.row());
    std::sort(rows.begin(), rows.end());
    return rows;
}

void MassTagEditor::updateStatusBar()
{
    const int total = m_model->entryCount();
    const int modified = m_model->modifiedCount();
    m_statusLabel->setText(
        tr("%1 files loaded, %2 modified").arg(total).arg(modified));

    m_actSaveAll->setEnabled(modified > 0);
    m_actUndo->setEnabled(modified > 0);
}

} // namespace dawcast::widgets
