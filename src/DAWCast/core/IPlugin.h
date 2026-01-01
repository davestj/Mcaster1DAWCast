// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstdint>

namespace dawcast {

struct PluginInfo {
    const char* name;
    const char* version;
    const char* author;
    uint32_t    apiVersion;
};

} // namespace dawcast

// C ABI boundary for plugin loading
extern "C" {

typedef void* (*plugin_create_fn)();
typedef void  (*plugin_destroy_fn)(void* instance);
typedef dawcast::PluginInfo (*plugin_info_fn)();

} // extern "C"
