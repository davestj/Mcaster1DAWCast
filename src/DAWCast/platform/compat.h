// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// Platform detection macros
#if defined(__APPLE__)
    #define DAWCAST_MACOS 1
    #include <TargetConditionals.h>
    #include <sys/sysctl.h>
    #include <mach/mach.h>
#elif defined(_WIN32) || defined(_WIN64)
    #define DAWCAST_WINDOWS 1
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#elif defined(__linux__)
    #define DAWCAST_LINUX 1
    #include <unistd.h>
    #include <sys/sysinfo.h>
#else
    #error "Unsupported platform"
#endif

// Ensure unset platforms are 0
#ifndef DAWCAST_MACOS
    #define DAWCAST_MACOS 0
#endif
#ifndef DAWCAST_WINDOWS
    #define DAWCAST_WINDOWS 0
#endif
#ifndef DAWCAST_LINUX
    #define DAWCAST_LINUX 0
#endif
