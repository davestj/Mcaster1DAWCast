// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "WorkspaceManager.h"
#include "ViewModeManager.h"

#include <QMainWindow>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>

namespace dawcast {

WorkspaceManager* WorkspaceManager::s_instance = nullptr;

WorkspaceManager* WorkspaceManager::instance()
{
    if (!s_instance)
        s_instance = new WorkspaceManager(nullptr);
    return s_instance;
}

WorkspaceManager::WorkspaceManager(QObject* parent)
    : QObject(parent)
{
    // Create the profiles directory if it does not exist
    QDir dir(profileDir());
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
        createFactoryPresets();
    }

    loadAllProfiles();
}

WorkspaceManager::~WorkspaceManager() = default;

QString WorkspaceManager::profileDir() const
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return base + QStringLiteral("/mcaster1/workspaces");
}

QString WorkspaceManager::profileFilePath(const QString& name) const
{
    // Sanitize name for filesystem: replace non-alphanumeric with underscores
    QString safe = name;
    safe.replace(QRegularExpression(QStringLiteral("[^a-zA-Z0-9_\\- ]")),
                 QStringLiteral("_"));
    return profileDir() + QStringLiteral("/") + safe + QStringLiteral(".json");
}

QStringList WorkspaceManager::profileNames() const
{
    // Return factory presets first, then user profiles sorted alphabetically
    QStringList factory;
    QStringList user;

    for (auto it = m_profiles.constBegin(); it != m_profiles.constEnd(); ++it) {
        if (it.value().isFactory)
            factory.append(it.key());
        else
            user.append(it.key());
    }

    factory.sort();
    user.sort();
    return factory + user;
}

WorkspaceProfile WorkspaceManager::profile(const QString& name) const
{
    return m_profiles.value(name);
}

QString WorkspaceManager::currentViewModeName() const
{
    auto* mgr = ViewModeManager::instance();
    return mgr->modeName(mgr->currentMode());
}

void WorkspaceManager::saveProfile(const QString& name, QMainWindow* window)
{
    if (!window || name.isEmpty())
        return;

    WorkspaceProfile p;
    p.name = name;
    p.windowGeometry = window->saveGeometry();
    p.windowState = window->saveState();
    p.viewMode = currentViewModeName();
    p.lastUsed = QDateTime::currentDateTime();

    // Preserve factory flag if overwriting a factory preset
    if (m_profiles.contains(name))
        p.isFactory = m_profiles[name].isFactory;

    m_profiles[name] = p;
    writeProfileToDisk(p);

    emit profileSaved(name);
    emit profilesChanged();
}

void WorkspaceManager::loadProfile(const QString& name, QMainWindow* window)
{
    if (!window || !m_profiles.contains(name))
        return;

    const WorkspaceProfile& p = m_profiles[name];

    // Restore the view mode first so dock widgets are configured correctly
    auto* mgr = ViewModeManager::instance();
    for (int i = 0; i < ViewModeManager::modeCount(); ++i) {
        auto mode = static_cast<ViewModeManager::Mode>(i);
        if (mgr->modeName(mode) == p.viewMode) {
            mgr->setMode(mode);
            break;
        }
    }

    // Restore window geometry and dock state
    if (!p.windowGeometry.isEmpty())
        window->restoreGeometry(p.windowGeometry);
    if (!p.windowState.isEmpty())
        window->restoreState(p.windowState);

    // Update last-used timestamp
    m_profiles[name].lastUsed = QDateTime::currentDateTime();
    writeProfileToDisk(m_profiles[name]);

    emit profileLoaded(name);
}

void WorkspaceManager::renameProfile(const QString& oldName, const QString& newName)
{
    if (!m_profiles.contains(oldName) || newName.isEmpty())
        return;

    // Factory presets cannot be renamed
    if (m_profiles[oldName].isFactory)
        return;

    WorkspaceProfile p = m_profiles.take(oldName);

    // Remove old file from disk
    QFile::remove(profileFilePath(oldName));

    p.name = newName;
    m_profiles[newName] = p;
    writeProfileToDisk(p);

    emit profilesChanged();
}

void WorkspaceManager::deleteProfile(const QString& name)
{
    if (!m_profiles.contains(name))
        return;

    // Factory presets cannot be deleted
    if (m_profiles[name].isFactory)
        return;

    m_profiles.remove(name);
    QFile::remove(profileFilePath(name));

    emit profilesChanged();
}

void WorkspaceManager::createFactoryPresets()
{
    // Create one factory profile per view mode
    static const char* modeNames[] = {
        "Podcaster", "Producer", "DJ/Live",
        "Studio Artist", "Voice Over", "Guitar FX"
    };

    for (const char* modeName : modeNames) {
        WorkspaceProfile p;
        p.name = QString::fromLatin1(modeName);
        p.viewMode = p.name;
        p.isFactory = true;
        p.lastUsed = QDateTime::currentDateTime();
        // Geometry and state are left empty -- loadProfile will just switch the
        // view mode, and the mode's default layout will be applied by MainWindow.

        m_profiles[p.name] = p;
        writeProfileToDisk(p);
    }
}

void WorkspaceManager::loadAllProfiles()
{
    m_profiles.clear();

    QDir dir(profileDir());
    if (!dir.exists())
        return;

    const QStringList jsonFiles = dir.entryList(
        QStringList() << QStringLiteral("*.json"), QDir::Files);

    for (const QString& fileName : jsonFiles) {
        QFile file(dir.filePath(fileName));
        if (!file.open(QIODevice::ReadOnly))
            continue;

        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();

        if (!doc.isObject())
            continue;

        QJsonObject obj = doc.object();
        WorkspaceProfile p;
        p.name = obj.value(QStringLiteral("name")).toString();
        p.windowGeometry = QByteArray::fromBase64(
            obj.value(QStringLiteral("geometry")).toString().toLatin1());
        p.windowState = QByteArray::fromBase64(
            obj.value(QStringLiteral("state")).toString().toLatin1());
        p.viewMode = obj.value(QStringLiteral("viewMode")).toString();
        p.isFactory = obj.value(QStringLiteral("isFactory")).toBool(false);
        p.lastUsed = QDateTime::fromString(
            obj.value(QStringLiteral("lastUsed")).toString(), Qt::ISODate);

        if (!p.name.isEmpty())
            m_profiles[p.name] = p;
    }
}

void WorkspaceManager::writeProfileToDisk(const WorkspaceProfile& profile)
{
    QDir dir(profileDir());
    if (!dir.exists())
        dir.mkpath(QStringLiteral("."));

    QJsonObject obj;
    obj[QStringLiteral("name")]      = profile.name;
    obj[QStringLiteral("geometry")]   = QString::fromLatin1(profile.windowGeometry.toBase64());
    obj[QStringLiteral("state")]      = QString::fromLatin1(profile.windowState.toBase64());
    obj[QStringLiteral("viewMode")]   = profile.viewMode;
    obj[QStringLiteral("isFactory")]  = profile.isFactory;
    obj[QStringLiteral("lastUsed")]   = profile.lastUsed.toString(Qt::ISODate);

    QJsonDocument doc(obj);

    QFile file(profileFilePath(profile.name));
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
    }
}

} // namespace dawcast
