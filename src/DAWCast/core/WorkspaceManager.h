// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QDateTime>
#include <QHash>

class QMainWindow;

namespace dawcast {

struct WorkspaceProfile {
    QString    name;
    QByteArray windowGeometry;   // from QMainWindow::saveGeometry()
    QByteArray windowState;      // from QMainWindow::saveState()
    QString    viewMode;         // "Producer", "Podcaster", etc.
    bool       isFactory = false;
    QDateTime  lastUsed;
};

class WorkspaceManager : public QObject
{
    Q_OBJECT

public:
    static WorkspaceManager* instance();

    [[nodiscard]] QStringList profileNames() const;
    [[nodiscard]] WorkspaceProfile profile(const QString& name) const;

    void saveProfile(const QString& name, QMainWindow* window);
    void loadProfile(const QString& name, QMainWindow* window);
    void renameProfile(const QString& oldName, const QString& newName);
    void deleteProfile(const QString& name);

    void createFactoryPresets();  // Called on first launch

    /// Returns the current view mode name for saving in profiles
    [[nodiscard]] QString currentViewModeName() const;

signals:
    void profileSaved(const QString& name);
    void profileLoaded(const QString& name);
    void profilesChanged();

private:
    explicit WorkspaceManager(QObject* parent = nullptr);
    ~WorkspaceManager() override;

    QString profileDir() const;
    QString profileFilePath(const QString& name) const;
    void loadAllProfiles();
    void writeProfileToDisk(const WorkspaceProfile& profile);

    QHash<QString, WorkspaceProfile> m_profiles;

    static WorkspaceManager* s_instance;
};

} // namespace dawcast
