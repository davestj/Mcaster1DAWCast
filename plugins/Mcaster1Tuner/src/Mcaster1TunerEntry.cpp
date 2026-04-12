/*
 * Mcaster1Tuner — VST3 Chromatic Tuner Plugin
 * src/Mcaster1TunerEntry.cpp — VST3 plugin factory registration
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "Mcaster1Tuner.h"
#include "Mcaster1TunerController.h"

#include "public.sdk/source/main/pluginfactory.h"

#define MC1_VENDOR   "MC1 Studios / David St. John"
#define MC1_URL      "https://mcaster1.com"
#define MC1_EMAIL    "davestj@gmail.com"

BEGIN_FACTORY_DEF(MC1_VENDOR, MC1_URL, MC1_EMAIL)

    DEF_CLASS2(
        INLINE_UID_FROM_FUID(kMcaster1TunerProcessorUID),
        PClassInfo::kManyInstances,
        kVstAudioEffectClass,
        "Mcaster1 Tuner",
        Vst::kDistributable,
        Vst::PlugType::kFxAnalyzer,
        "1.0.0",
        kVstVersionString,
        Mcaster1::Mcaster1TunerProcessor::createInstance)

    DEF_CLASS2(
        INLINE_UID_FROM_FUID(kMcaster1TunerControllerUID),
        PClassInfo::kManyInstances,
        kVstComponentControllerClass,
        "Mcaster1 Tuner Controller",
        0,
        "",
        "1.0.0",
        kVstVersionString,
        Mcaster1::Mcaster1TunerController::createInstance)

END_FACTORY
