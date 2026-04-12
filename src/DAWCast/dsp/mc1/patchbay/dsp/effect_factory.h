/*
 * Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
 * dsp/mc1/patchbay/dsp/effect_factory.h — MC1 DSP factory
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Slimmed-down EffectFactory for DAWCast — knows the 12 official MC1
 * (Mediacast One) effects shipped with this app. Does NOT include the
 * AudioPipe-only `fx_audiounit.h` / `plugin_scanner.h` integration —
 * DAWCast loads third-party plugins via its own plugin host layer.
 *
 * Used by preset_manager.cpp::EffectFactory::create(id) when applying
 * factory presets, and indirectly by anything that wants to enumerate
 * the official MC1 catalog from a single place.
 */

#pragma once

#include "dsp_effect.h"
#include "fx_parametric_eq.h"
#include "fx_compressor.h"
#include "fx_sonic_enhancer.h"
#include "fx_dbx_voice.h"
#include "fx_dual_eq15.h"
#include "fx_graphic_eq31.h"
#include "fx_dbx166xs.h"
#include "fx_xenyx_preamp.h"
#include "fx_broadcast_agc.h"
#include "fx_tube_preamp.h"
#include "fx_castertube.h"
#include "fx_mic_modeler.h"
#include "fx_lexicon_224.h"
#include "fx_lexicon_pcm70.h"
#include "fx_lexicon_pcm96.h"
#include "fx_lexicon_480l.h"
#include "fx_lexicon_mpx1.h"

#include "fx_mc1_voice_lift.h"
#include "fx_mc1_plosive_killer.h"
#include "fx_mc1_mouth_click.h"
#include "fx_mc1_bleed_suppressor.h"
#include "fx_mc1_phone_line.h"
#include "fx_mc1_remote_restorer.h"
#include "fx_mc1_loudness_match.h"
#include "fx_mc1_stinger_bed.h"
#include "fx_mc1_vodcast_lipsync.h"
#include "fx_mc1_signal_hill_a.h"
#include "fx_mc1_vocal_producer.h"
#include "fx_mc1_key_finder.h"
#include "fx_dbx_676.h"
#include "fx_dbx_580.h"
#include "fx_dbx_266xs.h"
#include "fx_dbx_560a.h"
#include "fx_dbx_520.h"
#include "fx_dbx_510.h"
#include "fx_dbx_530.h"
#include "fx_bbe_d82.h"
#include "fx_bbe_h82.h"
#include "fx_bbe_l82.h"
#include "fx_bbe_mach3.h"
#include "fx_mc1_tidemark_a.h"
#include "fx_mc1_tidemark_b.h"
#include "fx_mc1_tidemark_vault.h"
#include "fx_mc1_granite_a.h"

#include <memory>
#include <string>
#include <vector>

namespace mc1dsp {

struct EffectInfo {
    const char* id;
    const char* name;
    const char* version;
    EffectCategory category;
};

class EffectFactory {
public:
    /* Create an effect instance by ID. Returns nullptr for unknown ids. */
    static std::unique_ptr<DspEffect> create(const std::string& effectId) {
        if (effectId == "mc1.eq.parametric10")
            return std::make_unique<FxParametricEq>();
        if (effectId == "mc1.dynamics.compressor")
            return std::make_unique<FxCompressor>();
        if (effectId == "mc1.enhancer.sonic")
            return std::make_unique<FxSonicEnhancer>();
        if (effectId == "mc1.channel.dbx286s")
            return std::make_unique<FxDbxVoice>();
        if (effectId == "mc1.eq.dual15")
            return std::make_unique<FxDualEq15>();
        if (effectId == "mc1.eq.graphic31")
            return std::make_unique<FxGraphicEq31>();
        if (effectId == "mc1.dynamics.dbx166xs")
            return std::make_unique<FxDbx166xs>();
        if (effectId == "mc1.channel.xenyx")
            return std::make_unique<FxXenyxPreamp>();
        if (effectId == "mc1.dynamics.agc")
            return std::make_unique<FxBroadcastAgc>();
        if (effectId == "mc1.analog.tube_preamp")
            return std::make_unique<FxTubePreamp>();
        if (effectId == "mc1.analog.castertube")
            return std::make_unique<FxCasterTube>();
        if (effectId == "mc1.modeling.mic")
            return std::make_unique<FxMicModeler>();
        if (effectId == "mc1.lexicon.l224")
            return std::make_unique<FxLexicon224>();
        if (effectId == "mc1.lexicon.pcm70")
            return std::make_unique<FxLexiconPcm70>();
        if (effectId == "mc1.lexicon.pcm96")
            return std::make_unique<FxLexiconPcm96>();
        if (effectId == "mc1.lexicon.l480l")
            return std::make_unique<FxLexicon480L>();
        if (effectId == "mc1.lexicon.mpx1")
            return std::make_unique<FxLexiconMpx1>();

        // MC1 Podcast plugin family
        if (effectId == "mc1.podcast.voice_lift")
            return std::make_unique<FxVoiceLift>();
        if (effectId == "mc1.podcast.plosive")
            return std::make_unique<FxPlosiveKiller>();
        if (effectId == "mc1.podcast.mouth_click")
            return std::make_unique<FxMouthClickRemover>();
        if (effectId == "mc1.podcast.bleed")
            return std::make_unique<FxBleedSuppressor>();
        if (effectId == "mc1.podcast.phone_line")
            return std::make_unique<FxPhoneLineSim>();
        if (effectId == "mc1.podcast.remote_restore")
            return std::make_unique<FxRemoteRestorer>();
        if (effectId == "mc1.podcast.loudness_match")
            return std::make_unique<FxLoudnessMatch>();
        if (effectId == "mc1.podcast.stinger")
            return std::make_unique<FxStingerBed>();
        if (effectId == "mc1.podcast.vodcast_lipsync")
            return std::make_unique<FxVodcastLipsync>();

        // MC1 Flagship
        if (effectId == "mc1.studio.vocal_producer")
            return std::make_unique<FxVocalProducer>();
        if (effectId == "mc1.analyzer.key_finder")
            return std::make_unique<FxKeyFinder>();

        // dbx 500/600 series family
        if (effectId == "mc1.dbx.676")
            return std::make_unique<FxDbx676>();
        if (effectId == "mc1.dbx.580")
            return std::make_unique<FxDbx580>();
        if (effectId == "mc1.dbx.266xs")
            return std::make_unique<FxDbx266xs>();
        if (effectId == "mc1.dbx.560a")
            return std::make_unique<FxDbx560A>();
        if (effectId == "mc1.dbx.520")
            return std::make_unique<FxDbx520>();
        if (effectId == "mc1.dbx.510")
            return std::make_unique<FxDbx510>();
        if (effectId == "mc1.dbx.530")
            return std::make_unique<FxDbx530>();

        // BBE Sonic Sweet family
        if (effectId == "mc1.bbe.d82")
            return std::make_unique<FxBbeD82>();
        if (effectId == "mc1.bbe.h82")
            return std::make_unique<FxBbeH82>();
        if (effectId == "mc1.bbe.l82")
            return std::make_unique<FxBbeL82>();
        if (effectId == "mc1.bbe.mach3bass")
            return std::make_unique<FxBbeMach3>();

        // MC1 Studios family
        if (effectId == "mc1.studio.signal_hill_a")
            return std::make_unique<FxSignalHillA>();
        if (effectId == "mc1.studio.tidemark_a")
            return std::make_unique<FxTidemarkA>();
        if (effectId == "mc1.studio.tidemark_b")
            return std::make_unique<FxTidemarkB>();
        if (effectId == "mc1.studio.tidemark_vault")
            return std::make_unique<FxTidemarkVault>();
        if (effectId == "mc1.studio.granite_a")
            return std::make_unique<FxGraniteA>();

        return nullptr;
    }

    /* List all 12 MC1 effects (used by preset_manager for factory preset gen) */
    static std::vector<EffectInfo> availableEffects() {
        return {
            { "mc1.eq.parametric10",    "10-Band Parametric EQ",       "1.0.0", EffectCategory::EQ },
            { "mc1.dynamics.compressor","Compressor / Gate / Limiter", "1.0.0", EffectCategory::Dynamics },
            { "mc1.enhancer.sonic",     "Sonic Enhancer (BBE 882I)",   "1.0.0", EffectCategory::Enhancer },
            { "mc1.channel.dbx286s",    "DBX 286S Voice Processor",    "1.0.0", EffectCategory::ChannelStrip },
            { "mc1.eq.dual15",          "Dual 15-Band EQ (L/R)",       "1.0.0", EffectCategory::EQ },
            { "mc1.eq.graphic31",       "31-Band Graphic EQ",          "1.0.0", EffectCategory::EQ },
            { "mc1.dynamics.dbx166xs",  "DBX 166xs Comp/Gate",         "1.0.0", EffectCategory::Dynamics },
            { "mc1.channel.xenyx",      "Mackie Xenyx Preamp",         "1.0.0", EffectCategory::ChannelStrip },
            { "mc1.dynamics.agc",       "Broadcast AGC",               "1.0.0", EffectCategory::Dynamics },
            { "mc1.analog.tube_preamp", "Tube Mic Preamp",             "1.0.0", EffectCategory::ChannelStrip },
            { "mc1.analog.castertube",  "CasterTube Vocal",            "1.0.0", EffectCategory::ChannelStrip },
            { "mc1.modeling.mic",       "Mic Modeler",                 "1.0.0", EffectCategory::ChannelStrip },
            { "mc1.lexicon.l224",       "Lexicon 224 Digital Reverb",  "1.0.0", EffectCategory::Enhancer },
            { "mc1.lexicon.pcm70",      "Lexicon PCM 70 Multi-FX",     "1.0.0", EffectCategory::Enhancer },
            { "mc1.lexicon.pcm96",      "Lexicon PCM 96 Stereo Reverb","1.0.0", EffectCategory::Enhancer },
            { "mc1.lexicon.l480l",      "Lexicon 480L Random Hall",    "1.0.0", EffectCategory::Enhancer },
            { "mc1.lexicon.mpx1",       "Lexicon MPX 1 Pitch + Delay", "1.0.0", EffectCategory::Enhancer },

            // MC1 Podcast plugin family
            { "mc1.podcast.voice_lift",       "MC1 Voice Lift Pro",              "1.0.0", EffectCategory::Utility },
            { "mc1.podcast.plosive",          "MC1 Plosive Killer",              "1.0.0", EffectCategory::Dynamics },
            { "mc1.podcast.mouth_click",      "MC1 Mouth Click Remover",         "1.0.0", EffectCategory::Utility },
            { "mc1.podcast.bleed",            "MC1 Multi-Host Bleed Suppressor", "1.0.0", EffectCategory::Dynamics },
            { "mc1.podcast.phone_line",       "MC1 Phone Line Sim",              "1.0.0", EffectCategory::Utility },
            { "mc1.podcast.remote_restore",   "MC1 Remote Guest Restorer",       "1.0.0", EffectCategory::Utility },
            { "mc1.podcast.loudness_match",   "MC1 Loudness Match",              "1.0.0", EffectCategory::Dynamics },
            { "mc1.podcast.stinger",          "MC1 Stinger Bed",                 "1.0.0", EffectCategory::Utility },
            { "mc1.podcast.vodcast_lipsync",  "MC1 Vodcast Lipsync",             "1.0.0", EffectCategory::Utility },

            // MC1 Flagship
            { "mc1.studio.vocal_producer", "MC1 Vocal Producer Pro",           "1.0.0", EffectCategory::ChannelStrip },
            { "mc1.analyzer.key_finder",   "MC1 Topline Key Finder",           "1.0.0", EffectCategory::Utility },

            // dbx 500/600 series family
            { "mc1.dbx.676",              "dbx 676 Tube Mic Preamp",          "1.0.0", EffectCategory::ChannelStrip },
            { "mc1.dbx.580",              "dbx 580 Mic Preamp",               "1.0.0", EffectCategory::ChannelStrip },
            { "mc1.dbx.266xs",            "dbx 266xs Compressor/Gate",        "1.0.0", EffectCategory::Dynamics },
            { "mc1.dbx.560a",             "dbx 560A Compressor/Limiter",      "1.0.0", EffectCategory::Dynamics },
            { "mc1.dbx.520",              "dbx 520 De-Esser",                 "1.0.0", EffectCategory::Dynamics },
            { "mc1.dbx.510",              "dbx 510 Subharmonic Synthesizer",  "1.0.0", EffectCategory::Enhancer },
            { "mc1.dbx.530",              "dbx 530 Parametric EQ",            "1.0.0", EffectCategory::EQ },

            // BBE Sonic Sweet family
            { "mc1.bbe.d82",              "BBE D82 Sonic Maximizer",         "1.0.0", EffectCategory::Enhancer },
            { "mc1.bbe.h82",              "BBE H82 Harmonic Maximizer",      "1.0.0", EffectCategory::Enhancer },
            { "mc1.bbe.l82",              "BBE L82 Loudness Maximizer",      "1.0.0", EffectCategory::Dynamics },
            { "mc1.bbe.mach3bass",        "BBE Mach 3 Bass",                 "1.0.0", EffectCategory::Enhancer },

            // MC1 Studios family
            { "mc1.studio.signal_hill_a",     "MC1 Signal Hill Broadcasting A",  "1.0.0", EffectCategory::ChannelStrip },
            { "mc1.studio.tidemark_a",        "MC1 Tidemark Studios A",          "1.0.0", EffectCategory::ChannelStrip },
            { "mc1.studio.tidemark_b",        "MC1 Tidemark Studios B",          "1.0.0", EffectCategory::ChannelStrip },
            { "mc1.studio.tidemark_vault",    "MC1 Tidemark Vault Chambers",     "1.0.0", EffectCategory::Enhancer },
            { "mc1.studio.granite_a",         "MC1 Granite Hall Studios A",      "1.0.0", EffectCategory::ChannelStrip },
        };
    }

    /* DAWCast doesn't host third-party plugins through this path — they
       go through the dedicated plugin host (VST3/AU/CLAP) layer. So
       allEffectsIncludingPlugins() just returns the built-in MC1 list. */
    static std::vector<EffectInfo> allEffectsIncludingPlugins() {
        return availableEffects();
    }
};

} // namespace mc1dsp
