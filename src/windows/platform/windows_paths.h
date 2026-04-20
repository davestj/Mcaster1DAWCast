// Mcaster1DAWCast — Windows known-folder path resolution
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Cross-platform code that expects ~/Library/Application Support or
// $XDG_CONFIG_HOME uses these accessors so the Windows build lands in
// %APPDATA%\Mcaster1\DAWCast (roaming) and %LOCALAPPDATA%\Mcaster1\DAWCast
// (caches, waveforms, crash dumps) instead of random working directories.
#pragma once

#include <QString>

namespace dawcast {

class WindowsPaths {
public:
    // %APPDATA%\Mcaster1\DAWCast — for user preferences, project templates,
    // exported preset YAML. Roams with the user's Windows profile.
    static QString appDataRoot();

    // %LOCALAPPDATA%\Mcaster1\DAWCast — for waveform caches, media library
    // SQLite index, crash reports. Machine-local, not roamed.
    static QString localAppDataRoot();

    // %USERPROFILE%\Documents\Mcaster1\DAWCast — default Projects folder.
    static QString documentsRoot();

    // Creates the directory if missing. Returns true on success or if already
    // present. Logs via DebugLogger on failure.
    static bool ensureExists(const QString& path);
};

} // namespace dawcast
