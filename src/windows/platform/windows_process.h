// Mcaster1DAWCast — Windows subprocess helpers
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Thin wrapper around CreateProcess for invoking yt-dlp, ffmpeg CLI, or any
// bundled helper binary without the console window flashing on screen.
// QProcess works for most cases, but it doesn't expose CREATE_NO_WINDOW
// cleanly and it inherits handles we don't want leaking.
#pragma once

#include <QString>
#include <QStringList>

namespace dawcast {

struct ProcessResult {
    int     exitCode   = -1;
    QString stdoutText;
    QString stderrText;
    bool    timedOut   = false;
};

class WindowsProcess {
public:
    // Run `exe args...`, capture stdout/stderr, block up to timeoutMs.
    // CREATE_NO_WINDOW is always set — these are background helpers.
    static ProcessResult run(const QString&     exe,
                             const QStringList& args,
                             int                timeoutMs = 30000);

    // Resolve a bundled helper binary shipped alongside Mcaster1DAWCast.exe.
    // Checks <exeDir>/tools/<name>.exe first, then PATH.
    static QString resolveHelper(const QString& name);
};

} // namespace dawcast
