// Mcaster1DAWCast — Windows platform compatibility shims
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#if !defined(_WIN32) && !defined(_WIN64)
#  error "compat_windows.h is for Windows builds only"
#endif

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#  define NOMINMAX
#endif

#include <windows.h>
#include <cstdint>

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

namespace dawcast::platform::win {

// Narrow <-> wide string conversion for Win32 APIs that only speak UTF-16.
// Free functions, header-only — deliberately tiny so the platform lib stays
// a one-stop include for random Win32-touching code paths elsewhere.

inline std::wstring utf8_to_wide(const char* s, int len = -1) {
    if (!s) return {};
    const int need = MultiByteToWideChar(CP_UTF8, 0, s, len, nullptr, 0);
    if (need <= 0) return {};
    std::wstring w(static_cast<size_t>(len == -1 ? need - 1 : need), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s, len, w.data(), need);
    return w;
}

inline std::string wide_to_utf8(const wchar_t* w, int len = -1) {
    if (!w) return {};
    const int need = WideCharToMultiByte(CP_UTF8, 0, w, len, nullptr, 0, nullptr, nullptr);
    if (need <= 0) return {};
    std::string s(static_cast<size_t>(len == -1 ? need - 1 : need), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, len, s.data(), need, nullptr, nullptr);
    return s;
}

} // namespace dawcast::platform::win
