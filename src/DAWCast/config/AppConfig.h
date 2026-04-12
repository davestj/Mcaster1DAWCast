// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <QJsonObject>

namespace dawcast::config {

class AppConfig : public QObject {
    Q_OBJECT

public:
    static AppConfig* instance();

    // ── Centralized app data paths (DRY) ────────────────────────────
    /// Application name used for path construction.
    static QString appName() { return QStringLiteral("Mcaster1DAWCast"); }
    /// Base data directory: ~/.mcaster1/Mcaster1DAWCast/
    static QString appDataDir();
    /// DSP effect presets: ~/.mcaster1/Mcaster1DAWCast/presets/
    static QString presetsDir();
    /// Track presets: ~/.mcaster1/Mcaster1DAWCast/track_presets/
    static QString trackPresetsDir();
    /// Whisper AI models: ~/.mcaster1/Mcaster1DAWCast/whisper-models/
    static QString whisperModelsDir();
    /// Per-plugin data: ~/.mcaster1/Mcaster1DAWCast/plugins/<pluginName>/
    static QString pluginDataDir(const QString& pluginName);
    /// Media library DB: ~/.mcaster1/Mcaster1DAWCast/media_library.json
    static QString mediaLibraryPath();

    bool load(const QString& path);
    bool save();

    QVariant value(const QString& key, const QVariant& defaultValue = QVariant()) const;
    void setValue(const QString& key, const QVariant& value);

signals:
    void configChanged(const QString& key);

private:
    explicit AppConfig(QObject* parent = nullptr);
    ~AppConfig() override;

    QString     m_path;
    QJsonObject m_data;

    static AppConfig* s_instance;
};

} // namespace dawcast::config
