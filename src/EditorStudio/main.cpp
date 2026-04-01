// DAWCast Editor Studio — Forensic-Grade Single-File Audio/Video Editor
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QStyleFactory>

#include "EditorStudioWindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("DAWCast Editor Studio"));
    app.setApplicationVersion(QStringLiteral("1.0.0-alpha"));
    app.setOrganizationName(QStringLiteral("Mcaster1"));
    app.setOrganizationDomain(QStringLiteral("mcaster1.com"));

    // Set app icon — same icon as the main DAW
    QString iconPath = QCoreApplication::applicationDirPath()
                     + QStringLiteral("/../../image_resources/app-icon-1024.png");
    if (QFile::exists(iconPath)) {
        app.setWindowIcon(QIcon(iconPath));
    } else {
        iconPath = QStringLiteral("image_resources/app-icon-1024.png");
        if (QFile::exists(iconPath))
            app.setWindowIcon(QIcon(iconPath));
    }

    // Fusion style with system default (light) theme
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    auto* window = new dawcast::editor::EditorStudioWindow();

    // Accept command-line file argument: dawcast-editor-studio /path/to/file.wav
    QStringList args = app.arguments();
    if (args.size() > 1) {
        QString filePath = args.at(1);
        if (QFile::exists(filePath)) {
            window->openFile(filePath);
        }
    }

    window->show();
    return app.exec();
}
