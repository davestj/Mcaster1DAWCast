// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ThemeEngine.h"
#include "config/YamlPresets.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSettings>
#include <QStyleFactory>

namespace dawcast::widgets {

ThemeEngine* ThemeEngine::s_instance = nullptr;

ThemeEngine::ThemeEngine(QObject* parent)
    : QObject(parent)
{
}

ThemeEngine::~ThemeEngine() = default;

ThemeEngine* ThemeEngine::instance()
{
    if (!s_instance) {
        s_instance = new ThemeEngine(qApp);
    }
    return s_instance;
}

QStringList ThemeEngine::themeSearchPaths() const
{
    QStringList paths;

    // 1. Relative to application binary (deployed layout)
    QString appDir = QCoreApplication::applicationDirPath();
    paths << appDir + QStringLiteral("/themes");
    paths << appDir + QStringLiteral("/../themes");       // macOS .app bundle: Contents/MacOS/../themes
    paths << appDir + QStringLiteral("/../Resources/themes"); // macOS bundle resources

    // 2. Current working directory (development)
    paths << QStringLiteral("themes");

    return paths;
}

QStringList ThemeEngine::availableThemes() const
{
    QStringList themes;
    const QStringList searchPaths = themeSearchPaths();

    for (const QString& basePath : searchPaths) {
        QDir themesDir(basePath);
        if (!themesDir.exists()) continue;

        const auto entries = themesDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const auto& entry : entries) {
            if (themes.contains(entry)) continue; // skip duplicates
            if (QFile::exists(themesDir.filePath(entry + QStringLiteral("/theme.yaml")))) {
                themes.append(entry);
            }
        }
    }

    return themes;
}

bool ThemeEngine::loadTheme(const QString& name)
{
    // Search for the theme directory across all search paths
    QString themeDir;
    QString yamlPath;
    QString qssPath;

    const QStringList searchPaths = themeSearchPaths();
    for (const QString& basePath : searchPaths) {
        QString candidate = basePath + QStringLiteral("/%1").arg(name);
        QString candidateYaml = candidate + QStringLiteral("/theme.yaml");
        if (QFile::exists(candidateYaml)) {
            themeDir = candidate;
            yamlPath = candidateYaml;
            qssPath  = candidate + QStringLiteral("/style.qss");
            break;
        }
    }

    if (yamlPath.isEmpty() || !QFile::exists(yamlPath)) {
        return false;
    }

    // Parse theme.yaml via YamlPresets (libyaml-backed)
    QVariantMap themeData = config::YamlPresets::loadPreset(yamlPath);
    if (themeData.isEmpty()) {
        return false;
    }

    // Extract the colors sub-map
    m_colors.clear();
    if (themeData.contains(QStringLiteral("colors"))) {
        QVariant colorsVar = themeData.value(QStringLiteral("colors"));
        if (colorsVar.typeId() == QMetaType::QVariantMap) {
            m_colors = colorsVar.toMap();
        }
    }

    // Extract style strings
    m_buttonStyle = themeData.value(QStringLiteral("button_style"),
                                    QStringLiteral("beveled")).toString();
    m_knobStyle   = themeData.value(QStringLiteral("knob_style"),
                                    QStringLiteral("embossed")).toString();
    m_meterStyle  = themeData.value(QStringLiteral("meter_style"),
                                    QStringLiteral("gradient")).toString();
    m_basePalette = themeData.value(QStringLiteral("base_palette"),
                                    QStringLiteral("fusion")).toString();

    m_currentTheme = name;

    // Apply base palette style (Fusion for consistent cross-platform appearance)
    if (m_basePalette.compare(QStringLiteral("fusion"), Qt::CaseInsensitive) == 0) {
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    }

    // Process and apply stylesheet if present
    if (QFile::exists(qssPath)) {
        QFile qssFile(qssPath);
        if (qssFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString rawQss = QString::fromUtf8(qssFile.readAll());
            QString processed = processStylesheet(rawQss, m_colors);
            qApp->setStyleSheet(processed);
        }
    } else {
        // No QSS file — clear any previous stylesheet
        qApp->setStyleSheet(QString());
    }

    // Persist the choice so it survives restarts
    QSettings settings;
    settings.setValue(QStringLiteral("appearance/theme"), name);

    emit themeChanged(name);
    return true;
}

QString ThemeEngine::currentTheme() const
{
    return m_currentTheme;
}

QColor ThemeEngine::color(const QString& key) const
{
    if (m_colors.contains(key)) {
        return QColor(m_colors.value(key).toString());
    }
    return QColor();
}

QString ThemeEngine::buttonStyle() const
{
    return m_buttonStyle;
}

QString ThemeEngine::knobStyle() const
{
    return m_knobStyle;
}

QString ThemeEngine::meterStyle() const
{
    return m_meterStyle;
}

QString ThemeEngine::processStylesheet(const QString& qss, const QVariantMap& colors) const
{
    QString result = qss;
    static const QRegularExpression varRegex(QStringLiteral("\\$\\{(\\w+)\\}"));

    // Build a replacement map first, then apply all substitutions.
    // This avoids issues with iterator invalidation from in-place replacement.
    QRegularExpressionMatchIterator it = varRegex.globalMatch(qss);
    QList<QPair<QString, QString>> replacements;

    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString varName = match.captured(1);
        if (colors.contains(varName)) {
            replacements.append({match.captured(0), colors.value(varName).toString()});
        }
    }

    // Apply replacements (replace all occurrences of each variable token)
    for (const auto& [token, value] : replacements) {
        result.replace(token, value);
    }

    return result;
}

} // namespace dawcast::widgets
