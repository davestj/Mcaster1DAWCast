// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QWidget>
#include <QString>
#include <QTimer>

namespace dawcast::widgets {

/// Custom splash screen displayed during application startup.
/// Shows branding, version information, and loading status messages
/// with a polished dark gradient appearance matching the app theme.
class SplashScreen : public QWidget
{
    Q_OBJECT

public:
    explicit SplashScreen(QWidget* parent = nullptr);
    ~SplashScreen() override;

    /// Update the status message shown near the bottom of the splash.
    void showMessage(const QString& message);

    /// Fade out, then close and delete this widget.
    /// Call after the main window is visible.
    void finish(QWidget* mainWindow);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void startFadeOut();

    QString m_message;
    QTimer* m_fadeTimer   = nullptr;
    qreal   m_opacity     = 1.0;
    bool    m_fadingOut    = false;
    int     m_dotAnimation = 0;      // loading dots counter
    QTimer* m_dotTimer     = nullptr;
};

} // namespace dawcast::widgets
