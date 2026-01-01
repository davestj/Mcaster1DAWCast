// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QColor>
#include <QVariantMap>

namespace dawcast::widgets {

class ThemeEngine : public QObject {
    Q_OBJECT

public:
    static ThemeEngine* instance();

    QStringList availableThemes() const;
    bool loadTheme(const QString& name);
    QString currentTheme() const;

    QColor  color(const QString& key) const;
    QString buttonStyle() const;
    QString knobStyle() const;
    QString meterStyle() const;

signals:
    void themeChanged(const QString& themeName);

private:
    explicit ThemeEngine(QObject* parent = nullptr);
    ~ThemeEngine() override;

    QString processStylesheet(const QString& qss, const QVariantMap& colors) const;
    QStringList themeSearchPaths() const;

    QVariantMap m_colors;
    QString     m_currentTheme;
    QString     m_buttonStyle;
    QString     m_knobStyle;
    QString     m_meterStyle;
    QString     m_basePalette;

    static ThemeEngine* s_instance;
};

} // namespace dawcast::widgets
