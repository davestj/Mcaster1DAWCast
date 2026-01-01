// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace dawcast::config {

class YamlPresets {
public:
    static QVariantMap loadPreset(const QString& yamlPath);
    static bool savePreset(const QString& yamlPath, const QVariantMap& params);
    static QStringList listPresets(const QString& directory);

private:
    YamlPresets() = delete;
};

} // namespace dawcast::config
