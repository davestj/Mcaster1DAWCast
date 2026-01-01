// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ModuleRegistry.h"
#include "IModule.h"

namespace dawcast {

ModuleRegistry* ModuleRegistry::s_instance = nullptr;

ModuleRegistry* ModuleRegistry::instance()
{
    if (!s_instance) {
        s_instance = new ModuleRegistry();
    }
    return s_instance;
}

ModuleRegistry::ModuleRegistry(QObject* parent)
    : QObject(parent)
{
}

ModuleRegistry::~ModuleRegistry()
{
    s_instance = nullptr;
}

void ModuleRegistry::registerModule(IModule* module)
{
    if (!module) return;
    m_modules.append(module);
    emit moduleRegistered(module);
}

QList<IModule*> ModuleRegistry::modules() const
{
    return m_modules;
}

IModule* ModuleRegistry::findModule(const QString& name) const
{
    for (auto* mod : m_modules) {
        if (mod && mod->name() == name) {
            return mod;
        }
    }
    return nullptr;
}

} // namespace dawcast
