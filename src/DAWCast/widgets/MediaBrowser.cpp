// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "MediaBrowser.h"

#include <QVBoxLayout>
#include <QTreeView>
#include <QFileSystemModel>
#include <QPushButton>

namespace dawcast::widgets {

MediaBrowser::MediaBrowser(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);

    m_fileModel = new QFileSystemModel(this);
    m_fileModel->setNameFilters({
        QStringLiteral("*.wav"), QStringLiteral("*.mp3"),
        QStringLiteral("*.flac"), QStringLiteral("*.aac"),
        QStringLiteral("*.ogg"), QStringLiteral("*.mp4"),
        QStringLiteral("*.mov"), QStringLiteral("*.avi"),
        QStringLiteral("*.mkv")
    });
    m_fileModel->setNameFilterDisables(false);

    m_treeView = new QTreeView(this);
    m_treeView->setModel(m_fileModel);
    m_treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    layout->addWidget(m_treeView);

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
