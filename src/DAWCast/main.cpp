// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QApplication>
#include <QDir>
#include <QStyleFactory>

#include "app.h"
#include "widgets/MainWindow.h"
#include "config/DebugLogger.h"
#include "widgets/ThemeEngine.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Mcaster1DAWCast"));
    app.setApplicationVersion(QStringLiteral("1.0.0-alpha"));
    app.setOrganizationName(QStringLiteral("Mcaster1"));
    app.setOrganizationDomain(QStringLiteral("mcaster1.com"));

    // Use Fusion as the base style for consistent cross-platform appearance
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    // Initialize debug logger — write to <appDir>/logs/mcaster1dawcast.log
    QString appDir = QCoreApplication::applicationDirPath();
    QDir logDir(appDir + QStringLiteral("/logs"));
    if (!logDir.exists()) {
        logDir.mkpath(QStringLiteral("."));
    }
    dawcast::config::DebugLogger::init(
        logDir.filePath(QStringLiteral("mcaster1dawcast.log")));
    dawcast::config::DebugLogger::instance()->info(
        QStringLiteral("Mcaster1DAWCast 1.0.0-alpha starting"));

    // Load default theme if available
    dawcast::widgets::ThemeEngine::instance()->loadTheme(QStringLiteral("default"));

    // Create and show main window
    auto* mainWindow = new dawcast::widgets::MainWindow();
    mainWindow->showMaximized();

    dawcast::config::DebugLogger::instance()->info(
        QStringLiteral("Main window displayed — entering event loop"));

    return app.exec();
}
