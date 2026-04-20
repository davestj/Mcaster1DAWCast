// Mcaster1DAWCast — Windows known-folder path resolution
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "windows_paths.h"
#include "compat_windows.h"

#include <shlobj.h>
#include <knownfolders.h>

#include <QDir>

namespace {

QString known_folder(REFKNOWNFOLDERID id) {
    PWSTR path = nullptr;
    if (FAILED(SHGetKnownFolderPath(id, KF_FLAG_CREATE, nullptr, &path)) || !path) {
        if (path) CoTaskMemFree(path);
        return {};
    }
    QString out = QString::fromWCharArray(path);
    CoTaskMemFree(path);
    return QDir::fromNativeSeparators(out);
}

} // namespace

namespace dawcast {

QString WindowsPaths::appDataRoot() {
    const QString base = known_folder(FOLDERID_RoamingAppData);
    return base.isEmpty() ? QString()
                           : base + QStringLiteral("/Mcaster1/DAWCast");
}

QString WindowsPaths::localAppDataRoot() {
    const QString base = known_folder(FOLDERID_LocalAppData);
    return base.isEmpty() ? QString()
                           : base + QStringLiteral("/Mcaster1/DAWCast");
}

QString WindowsPaths::documentsRoot() {
    const QString base = known_folder(FOLDERID_Documents);
    return base.isEmpty() ? QString()
                           : base + QStringLiteral("/Mcaster1/DAWCast");
}

bool WindowsPaths::ensureExists(const QString& path) {
    if (path.isEmpty()) return false;
    QDir d(path);
    if (d.exists()) return true;
    return d.mkpath(QStringLiteral("."));
}

} // namespace dawcast
