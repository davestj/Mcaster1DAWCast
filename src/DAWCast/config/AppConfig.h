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
