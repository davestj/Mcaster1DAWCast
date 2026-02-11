// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QApplication>
#include <QDir>
#include <QSettings>
#include <QStyleFactory>
#include <QThread>

#include "app.h"
#include "widgets/MainWindow.h"
#include "widgets/SplashScreen.h"
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

    // Show the splash screen immediately
    auto* splash = new dawcast::widgets::SplashScreen();
    splash->show();
    splash->showMessage(QStringLiteral("Initializing..."));
    app.processEvents();
    QThread::msleep(400);

    // Initialize debug logger — write to <appDir>/logs/mcaster1dawcast.log
    splash->showMessage(QStringLiteral("Starting debug logger..."));
    app.processEvents();
    QThread::msleep(300);
    QString appDir = QCoreApplication::applicationDirPath();
    QDir logDir(appDir + QStringLiteral("/logs"));
    if (!logDir.exists()) {
        logDir.mkpath(QStringLiteral("."));
    }
    dawcast::config::DebugLogger::init(
        logDir.filePath(QStringLiteral("mcaster1dawcast.log")));
    dawcast::config::DebugLogger::instance()->info(
        QStringLiteral("Mcaster1DAWCast 1.0.0-alpha starting"));

    // Load theme — honour user preference, default to DarkStudio on first launch
    splash->showMessage(QStringLiteral("Loading theme..."));
    app.processEvents();
    QThread::msleep(300);
    {
        QSettings settings;
        QString themeName = settings.value(
            QStringLiteral("appearance/theme"),
            QStringLiteral("DarkStudio")).toString();
        if (!dawcast::widgets::ThemeEngine::instance()->loadTheme(themeName)) {
            // Fallback: try DarkStudio, then Default
            if (themeName != QStringLiteral("DarkStudio")) {
                dawcast::widgets::ThemeEngine::instance()->loadTheme(
                    QStringLiteral("DarkStudio"));
            } else {
                dawcast::widgets::ThemeEngine::instance()->loadTheme(
                    QStringLiteral("Default"));
            }
        }
    }

    // Create the main window (this initializes audio engine, timeline, etc.)
    splash->showMessage(QStringLiteral("Loading audio engine..."));
    app.processEvents();
    QThread::msleep(400);
    auto* mainWindow = new dawcast::widgets::MainWindow();

    splash->showMessage(QStringLiteral("Preparing workspace..."));
    app.processEvents();
    QThread::msleep(500);

    splash->showMessage(QStringLiteral("Ready"));
    app.processEvents();
    QThread::msleep(600);

    // Show the main window and fade out the splash
    mainWindow->showMaximized();
    splash->finish(mainWindow);

    dawcast::config::DebugLogger::instance()->info(
        QStringLiteral("Main window displayed — entering event loop"));

    return app.exec();
}
