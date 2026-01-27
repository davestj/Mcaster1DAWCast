// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "MediaBrowser.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeView>
#include <QDir>
#include <QFileSystemModel>
#include <QPushButton>
#include <QDrag>
#include <QMimeData>
#include <QUrl>

namespace dawcast::widgets {

// ── DraggableTreeView ─────────────────────────────────────────────────────

DraggableTreeView::DraggableTreeView(QWidget* parent)
    : QTreeView(parent)
{
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::DragOnly);
    setDefaultDropAction(Qt::CopyAction);
}

void DraggableTreeView::startDrag(Qt::DropActions supportedActions)
{
    auto* fsModel = qobject_cast<QFileSystemModel*>(model());
    if (!fsModel) {
        QTreeView::startDrag(supportedActions);
        return;
    }

    QModelIndexList selected = selectionModel()->selectedIndexes();
    QList<QUrl> urls;
    for (const QModelIndex& idx : selected) {
        if (idx.column() != 0) continue;
        QString path = fsModel->filePath(idx);
        if (!fsModel->isDir(idx)) {
            urls.append(QUrl::fromLocalFile(path));
        }
    }

    if (urls.isEmpty()) return;

    auto* mimeData = new QMimeData;
    mimeData->setUrls(urls);

    auto* drag = new QDrag(this);
    drag->setMimeData(mimeData);
    drag->exec(Qt::CopyAction);
}

// ── MediaBrowser ──────────────────────────────────────────────────────────

MediaBrowser::MediaBrowser(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);

    m_fileModel = new QFileSystemModel(this);
    m_fileModel->setNameFilters({
        QStringLiteral("*.wav"), QStringLiteral("*.mp3"),
        QStringLiteral("*.flac"), QStringLiteral("*.aac"),
        QStringLiteral("*.ogg"), QStringLiteral("*.opus"),
        QStringLiteral("*.mp4"), QStringLiteral("*.mov"),
        QStringLiteral("*.avi"), QStringLiteral("*.mkv"),
        QStringLiteral("*.webm")
    });
    m_fileModel->setNameFilterDisables(false);

    // Start at home directory, allow browsing full filesystem
    QString homePath = QDir::homePath();
    m_fileModel->setRootPath(homePath);

    m_treeView = new DraggableTreeView(this);
    m_treeView->setModel(m_fileModel);
    m_treeView->setRootIndex(m_fileModel->index(homePath));
    m_treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_treeView->setColumnWidth(0, 250);  // Name column wider
    m_treeView->hideColumn(2);  // Hide Type column
    layout->addWidget(m_treeView);

    // Navigation bar: Home, Root /, custom path
    auto* navLayout = new QHBoxLayout;
    auto* btnHome = new QPushButton(tr("Home"), this);
    auto* btnRoot = new QPushButton(tr("/"), this);
    auto* btnMusic = new QPushButton(tr("Music"), this);
    navLayout->addWidget(btnHome);
    navLayout->addWidget(btnRoot);
    navLayout->addWidget(btnMusic);
    navLayout->addStretch();
    layout->insertLayout(0, navLayout);

    connect(btnHome, &QPushButton::clicked, this, [this] {
        setRootPath(QDir::homePath());
    });
    connect(btnRoot, &QPushButton::clicked, this, [this] {
        setRootPath(QStringLiteral("/"));
    });
    connect(btnMusic, &QPushButton::clicked, this, [this] {
        setRootPath(QDir::homePath() + QStringLiteral("/Music"));
    });

    auto* importBtn = new QPushButton(tr("Import Selected"), this);
    layout->addWidget(importBtn);

    connect(m_treeView, &QTreeView::doubleClicked, this, [this](const QModelIndex& index) {
        QString path = m_fileModel->filePath(index);
        emit fileDoubleClicked(path);
    });

    connect(importBtn, &QPushButton::clicked, this, [this]() {
        emit importRequested(selectedFiles());
    });
}

MediaBrowser::~MediaBrowser() = default;

void MediaBrowser::setRootPath(const QString& path)
{
    m_fileModel->setRootPath(path);
    m_treeView->setRootIndex(m_fileModel->index(path));
}

QStringList MediaBrowser::selectedFiles() const
{
    QStringList files;
    const auto indexes = m_treeView->selectionModel()->selectedIndexes();
    for (const auto& index : indexes) {
        if (index.column() == 0) {
            files.append(m_fileModel->filePath(index));
        }
    }
    return files;
}

} // namespace dawcast::widgets
