// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AppConfig.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QCoreApplication>
#include <QDebug>

namespace dawcast::config {

AppConfig* AppConfig::s_instance = nullptr;

AppConfig::AppConfig(QObject* parent)
    : QObject(parent)
{
}

AppConfig::~AppConfig() = default;

AppConfig* AppConfig::instance()
{
    if (!s_instance) {
        s_instance = new AppConfig(qApp);
        // Auto-bind to ~/.mcaster1/Mcaster1DAWCast/config.json and load
        // whatever's there. Without this, save() silently no-ops because
        // m_path is empty — which is why dock-visibility persistence
        // appeared broken for the entire session.
        const QString cfgPath = appDataDir() + QStringLiteral("/config.json");
        s_instance->m_path = cfgPath;
        if (QFile::exists(cfgPath)) {
            s_instance->load(cfgPath);
        }
    }
    return s_instance;
}

bool AppConfig::load(const QString& path)
{
    m_path = path;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return false;
    }

    m_data = doc.object();
    return true;
}

bool AppConfig::save()
{
    if (m_path.isEmpty()) {
        // Fallback: instance() may not have been used before save is
        // called directly; bind now.
        m_path = appDataDir() + QStringLiteral("/config.json");
    }

    // Ensure the parent directory exists.
    QDir().mkpath(QFileInfo(m_path).absolutePath());

    QFile file(m_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "AppConfig::save: failed to open" << m_path;
        return false;
    }

    QJsonDocument doc(m_data);
    file.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

QVariant AppConfig::value(const QString& key, const QVariant& defaultValue) const
{
    if (m_data.contains(key)) {
        return m_data.value(key).toVariant();
    }
    return defaultValue;
}

void AppConfig::setValue(const QString& key, const QVariant& value)
{
    m_data.insert(key, QJsonValue::fromVariant(value));
    emit configChanged(key);
}

// ── Centralized app data paths ──────────────────────────────────────────────

QString AppConfig::appDataDir()
{
    QString dir = QDir::homePath() + QStringLiteral("/.mcaster1/") + appName();
    QDir().mkpath(dir);
    return dir;
}

QString AppConfig::presetsDir()
{
    QString dir = appDataDir() + QStringLiteral("/presets");
    QDir().mkpath(dir);
    return dir;
}

QString AppConfig::trackPresetsDir()
{
    QString dir = appDataDir() + QStringLiteral("/track_presets");
    QDir().mkpath(dir);
    return dir;
}

QString AppConfig::whisperModelsDir()
{
    QString dir = appDataDir() + QStringLiteral("/whisper-models");
    QDir().mkpath(dir);
    return dir;
}

QString AppConfig::pluginDataDir(const QString& pluginName)
{
    QString dir = appDataDir() + QStringLiteral("/plugins/") + pluginName;
    QDir().mkpath(dir);
    return dir;
}

QString AppConfig::mediaLibraryPath()
{
    return appDataDir() + QStringLiteral("/media_library.json");
}

} // namespace dawcast::config
