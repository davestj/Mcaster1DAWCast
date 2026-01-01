// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QList>
#include <QString>

namespace dawcast {

class IModule;

class ModuleRegistry : public QObject
{
    Q_OBJECT

public:
    static ModuleRegistry* instance();

    void registerModule(IModule* module);
    [[nodiscard]] QList<IModule*> modules() const;
    [[nodiscard]] IModule* findModule(const QString& name) const;

signals:
    void moduleRegistered(IModule* module);

private:
    explicit ModuleRegistry(QObject* parent = nullptr);
    ~ModuleRegistry() override;

    static ModuleRegistry* s_instance;
    QList<IModule*> m_modules;
};

} // namespace dawcast
