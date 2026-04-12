/*
 * Mcaster1Tuner — VST3 Chromatic Tuner Plugin
 * src/Mcaster1TunerEditor.cpp — VSTGUI editor open/close (thin shim)
 *
 * The actual drawing lives in TunerView (Mcaster1TunerEditor.h).
 * This file exists only so the build system has a .cpp to compile;
 * all TunerView methods are defined inline in the header.
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "Mcaster1TunerEditor.h"
