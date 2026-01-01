// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QWidget>
#include <QStringList>
#include <QTreeView>
#include <QFileSystemModel>

namespace dawcast::widgets {

class MediaBrowser : public QWidget {
    Q_OBJECT

public:
    explicit MediaBrowser(QWidget* parent = nullptr);
    ~MediaBrowser() override;

    void setRootPath(const QString& path);
    QStringList selectedFiles() const;

signals:
    void fileDoubleClicked(const QString& path);
    void importRequested(const QStringList& paths);

private:
    QTreeView*        m_treeView  = nullptr;
    QFileSystemModel* m_fileModel = nullptr;
};

} // namespace dawcast::widgets
