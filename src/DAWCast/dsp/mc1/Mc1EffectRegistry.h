// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Mc1EffectRegistry — Official MC1 (Mediacast One) DSP Plugin Catalog
// ───────────────────────────────────────────────────────────────────
// Static catalog of the official MC1 series DSP plugins from Mediacast
// One. These are the same in-house effects that ship with Mcaster1AMP,
// Mcaster1AudioPipe, Mcaster1Studio and now DAWCast — header-only,
// real-time safe, lock-free C++17.
//
// Each entry in kMc1Effects[] provides:
//   - displayName : human-readable label for the Add Effect menu
//   - category    : grouping (EQ / Dynamics / Channel Strip / Analog / ...)
//   - id          : official MC1 plugin id (mc1.<category>.<name>)
//   - create()    : factory that builds a wrapped Mc1EffectAdapter so the
//                   existing DspChain / EffectsRackWidget can host it
//                   without any changes to those classes.
//
// To add a new MC1 effect: copy its fx_*.h header next to this file,
// add the #include below, and add a new entry to kMc1Effects[].

#pragma once

#include "Mc1EffectAdapter.h"

#include "patchbay/dsp/fx_parametric_eq.h"
#include "patchbay/dsp/fx_dual_eq15.h"
#include "patchbay/dsp/fx_graphic_eq31.h"
#include "patchbay/dsp/fx_compressor.h"
#include "patchbay/dsp/fx_dbx166xs.h"
#include "patchbay/dsp/fx_broadcast_agc.h"
#include "patchbay/dsp/fx_dbx_voice.h"
#include "patchbay/dsp/fx_xenyx_preamp.h"
#include "patchbay/dsp/fx_sonic_enhancer.h"
#include "patchbay/dsp/fx_tube_preamp.h"
#include "patchbay/dsp/fx_castertube.h"
#include "patchbay/dsp/fx_mic_modeler.h"
#include "patchbay/dsp/fx_lexicon_224.h"
#include "patchbay/dsp/fx_lexicon_pcm70.h"
#include "patchbay/dsp/fx_lexicon_pcm96.h"
#include "patchbay/dsp/fx_lexicon_480l.h"
#include "patchbay/dsp/fx_lexicon_mpx1.h"

// MC1 Podcast plugin family
#include "patchbay/dsp/fx_mc1_voice_lift.h"
#include "patchbay/dsp/fx_mc1_plosive_killer.h"
#include "patchbay/dsp/fx_mc1_mouth_click.h"
#include "patchbay/dsp/fx_mc1_bleed_suppressor.h"
#include "patchbay/dsp/fx_mc1_phone_line.h"
#include "patchbay/dsp/fx_mc1_remote_restorer.h"
#include "patchbay/dsp/fx_mc1_loudness_match.h"
#include "patchbay/dsp/fx_mc1_stinger_bed.h"
#include "patchbay/dsp/fx_mc1_vodcast_lipsync.h"

// dbx 500/600 series family
#include "patchbay/dsp/fx_dbx_676.h"
#include "patchbay/dsp/fx_dbx_580.h"
#include "patchbay/dsp/fx_dbx_266xs.h"
#include "patchbay/dsp/fx_dbx_560a.h"
#include "patchbay/dsp/fx_dbx_520.h"
#include "patchbay/dsp/fx_dbx_510.h"
#include "patchbay/dsp/fx_dbx_530.h"

// BBE Sonic Sweet family
#include "patchbay/dsp/fx_bbe_d82.h"
#include "patchbay/dsp/fx_bbe_h82.h"
#include "patchbay/dsp/fx_bbe_l82.h"
#include "patchbay/dsp/fx_bbe_mach3.h"

// MC1 Vocal Producer + Key Finder
#include "patchbay/dsp/fx_mc1_vocal_producer.h"
#include "patchbay/dsp/fx_mc1_key_finder.h"

// MC1 Studios family — composite end-to-end studios
#include "patchbay/dsp/fx_mc1_signal_hill_a.h"
#include "patchbay/dsp/fx_mc1_tidemark_a.h"
#include "patchbay/dsp/fx_mc1_tidemark_b.h"
#include "patchbay/dsp/fx_mc1_tidemark_vault.h"
#include "patchbay/dsp/fx_mc1_granite_a.h"

#include <QString>
#include <memory>

namespace dawcast::dsp {

struct Mc1EffectInfo {
    const char* displayName;   // "MC1 -10-Band Parametric EQ"
    const char* category;      // "EQ" / "Dynamics" / "Channel Strip" / ...
    const char* id;             // "mc1.eq.parametric10"
    Mc1EffectAdapter* (*create)(int sampleRate);
};

namespace detail {

template <typename Fx>
inline Mc1EffectAdapter* makeMc1(int sampleRate)
{
    return new Mc1EffectAdapter(std::make_unique<Fx>(), sampleRate);
}

} // namespace detail

inline const Mc1EffectInfo* mc1EffectCatalog(int* outCount)
{
    static const Mc1EffectInfo kMc1Effects[] = {
        // EQ
        {"MC1 10-Band Parametric EQ",
         "MC1 EQ",
         "mc1.eq.parametric10",
         &detail::makeMc1<mc1dsp::FxParametricEq>},

        {"MC1 Dual 15-Band EQ (L/R)",
         "MC1 EQ",
         "mc1.eq.dual15",
         &detail::makeMc1<mc1dsp::FxDualEq15>},

        {"MC1 31-Band Graphic EQ",
         "MC1 EQ",
         "mc1.eq.graphic31",
         &detail::makeMc1<mc1dsp::FxGraphicEq31>},

        // Dynamics
        {"MC1 Compressor / Gate / Limiter",
         "MC1 Dynamics",
         "mc1.dynamics.compressor",
         &detail::makeMc1<mc1dsp::FxCompressor>},

        {"MC1 Broadcast AGC",
         "MC1 Dynamics",
         "mc1.dynamics.agc",
         &detail::makeMc1<mc1dsp::FxBroadcastAgc>},

        // ── MC1 dbx family ────────────────────────────────────────
        {"MC1 DBX 286S Voice Processor",
         "MC1 dbx",
         "mc1.channel.dbx286s",
         &detail::makeMc1<mc1dsp::FxDbxVoice>},

        {"MC1 DBX 166xs Comp/Gate",
         "MC1 dbx",
         "mc1.dynamics.dbx166xs",
         &detail::makeMc1<mc1dsp::FxDbx166xs>},

        {"MC1 dbx 676 Tube Mic Preamp",
         "MC1 dbx",
         "mc1.dbx.676",
         &detail::makeMc1<mc1dsp::FxDbx676>},

        {"MC1 dbx 580 Mic Preamp",
         "MC1 dbx",
         "mc1.dbx.580",
         &detail::makeMc1<mc1dsp::FxDbx580>},

        {"MC1 dbx 266xs Compressor/Gate",
         "MC1 dbx",
         "mc1.dbx.266xs",
         &detail::makeMc1<mc1dsp::FxDbx266xs>},

        {"MC1 dbx 560A Compressor/Limiter",
         "MC1 dbx",
         "mc1.dbx.560a",
         &detail::makeMc1<mc1dsp::FxDbx560A>},

        {"MC1 dbx 520 De-Esser",
         "MC1 dbx",
         "mc1.dbx.520",
         &detail::makeMc1<mc1dsp::FxDbx520>},

        {"MC1 dbx 510 Subharmonic Synthesizer",
         "MC1 dbx",
         "mc1.dbx.510",
         &detail::makeMc1<mc1dsp::FxDbx510>},

        {"MC1 dbx 530 Parametric EQ",
         "MC1 dbx",
         "mc1.dbx.530",
         &detail::makeMc1<mc1dsp::FxDbx530>},

        {"MC1 Mackie Xenyx Preamp",
         "MC1 Channel Strip",
         "mc1.channel.xenyx",
         &detail::makeMc1<mc1dsp::FxXenyxPreamp>},

        // ── MC1 BBE family ──────────────────────────────────────────
        {"MC1 Sonic Enhancer (BBE 882i)",
         "MC1 BBE",
         "mc1.enhancer.sonic",
         &detail::makeMc1<mc1dsp::FxSonicEnhancer>},

        {"MC1 BBE D82 Sonic Maximizer",
         "MC1 BBE",
         "mc1.bbe.d82",
         &detail::makeMc1<mc1dsp::FxBbeD82>},

        {"MC1 BBE H82 Harmonic Maximizer",
         "MC1 BBE",
         "mc1.bbe.h82",
         &detail::makeMc1<mc1dsp::FxBbeH82>},

        {"MC1 BBE L82 Loudness Maximizer",
         "MC1 BBE",
         "mc1.bbe.l82",
         &detail::makeMc1<mc1dsp::FxBbeL82>},

        {"MC1 BBE Mach 3 Bass",
         "MC1 BBE",
         "mc1.bbe.mach3bass",
         &detail::makeMc1<mc1dsp::FxBbeMach3>},

        // Analog Modeling
        {"MC1 Tube Mic Preamp",
         "MC1 Analog",
         "mc1.analog.tube_preamp",
         &detail::makeMc1<mc1dsp::FxTubePreamp>},

        {"MC1 CasterTube Vocal",
         "MC1 Analog",
         "mc1.analog.castertube",
         &detail::makeMc1<mc1dsp::FxCasterTube>},

        {"MC1 Mic Modeler",
         "MC1 Analog",
         "mc1.modeling.mic",
         &detail::makeMc1<mc1dsp::FxMicModeler>},

        // Lexicon series — flagship reverbs and FX
        {"MC1 Lexicon 224 Digital Reverb",
         "MC1 Lexicon",
         "mc1.lexicon.l224",
         &detail::makeMc1<mc1dsp::FxLexicon224>},

        {"MC1 Lexicon PCM 70 Multi-FX",
         "MC1 Lexicon",
         "mc1.lexicon.pcm70",
         &detail::makeMc1<mc1dsp::FxLexiconPcm70>},

        {"MC1 Lexicon PCM 96 Stereo Reverb",
         "MC1 Lexicon",
         "mc1.lexicon.pcm96",
         &detail::makeMc1<mc1dsp::FxLexiconPcm96>},

        {"MC1 Lexicon 480L Random Hall",
         "MC1 Lexicon",
         "mc1.lexicon.l480l",
         &detail::makeMc1<mc1dsp::FxLexicon480L>},

        {"MC1 Lexicon MPX 1 Pitch + Delay",
         "MC1 Lexicon",
         "mc1.lexicon.mpx1",
         &detail::makeMc1<mc1dsp::FxLexiconMpx1>},

        // ── MC1 Podcast plugin family ───────────────────────────
        {"MC1 Voice Lift Pro",
         "MC1 Podcast",
         "mc1.podcast.voice_lift",
         &detail::makeMc1<mc1dsp::FxVoiceLift>},

        {"MC1 Plosive Killer",
         "MC1 Podcast",
         "mc1.podcast.plosive",
         &detail::makeMc1<mc1dsp::FxPlosiveKiller>},

        {"MC1 Mouth Click Remover",
         "MC1 Podcast",
         "mc1.podcast.mouth_click",
         &detail::makeMc1<mc1dsp::FxMouthClickRemover>},

        {"MC1 Multi-Host Bleed Suppressor",
         "MC1 Podcast",
         "mc1.podcast.bleed",
         &detail::makeMc1<mc1dsp::FxBleedSuppressor>},

        {"MC1 Phone Line Sim",
         "MC1 Podcast",
         "mc1.podcast.phone_line",
         &detail::makeMc1<mc1dsp::FxPhoneLineSim>},

        {"MC1 Remote Guest Restorer",
         "MC1 Podcast",
         "mc1.podcast.remote_restore",
         &detail::makeMc1<mc1dsp::FxRemoteRestorer>},

        {"MC1 Loudness Match",
         "MC1 Podcast",
         "mc1.podcast.loudness_match",
         &detail::makeMc1<mc1dsp::FxLoudnessMatch>},

        {"MC1 Stinger Bed",
         "MC1 Podcast",
         "mc1.podcast.stinger",
         &detail::makeMc1<mc1dsp::FxStingerBed>},

        {"MC1 Vodcast Lipsync",
         "MC1 Podcast",
         "mc1.podcast.vodcast_lipsync",
         &detail::makeMc1<mc1dsp::FxVodcastLipsync>},

        // ── MC1 Studios family ──────────────────────────────────
        {"MC1 Signal Hill Broadcasting A",
         "MC1 Studios",
         "mc1.studio.signal_hill_a",
         &detail::makeMc1<mc1dsp::FxSignalHillA>},

        {"MC1 Tidemark Studios A",
         "MC1 Studios",
         "mc1.studio.tidemark_a",
         &detail::makeMc1<mc1dsp::FxTidemarkA>},

        {"MC1 Tidemark Studios B",
         "MC1 Studios",
         "mc1.studio.tidemark_b",
         &detail::makeMc1<mc1dsp::FxTidemarkB>},

        {"MC1 Tidemark Vault Chambers",
         "MC1 Studios",
         "mc1.studio.tidemark_vault",
         &detail::makeMc1<mc1dsp::FxTidemarkVault>},

        {"MC1 Granite Hall Studios A",
         "MC1 Studios",
         "mc1.studio.granite_a",
         &detail::makeMc1<mc1dsp::FxGraniteA>},

        // ── MC1 Flagship ────────────────────────────────────────────
        {"MC1 Vocal Producer Pro",
         "MC1 Flagship",
         "mc1.studio.vocal_producer",
         &detail::makeMc1<mc1dsp::FxVocalProducer>},

        {"MC1 Topline Key Finder",
         "MC1 Analyzer",
         "mc1.analyzer.key_finder",
         &detail::makeMc1<mc1dsp::FxKeyFinder>},
    };

    if (outCount) {
        *outCount = static_cast<int>(sizeof(kMc1Effects) / sizeof(kMc1Effects[0]));
    }
    return kMc1Effects;
}

/// Look up an MC1 effect by display name and instantiate it. Returns
/// nullptr if no entry matches. Used by EffectsRackWidget when the user
/// clicks an item in the Add Effect menu.
inline Mc1EffectAdapter* createMc1EffectByName(const QString& displayName,
                                                int sampleRate = 48000)
{
    int count = 0;
    const Mc1EffectInfo* catalog = mc1EffectCatalog(&count);
    for (int i = 0; i < count; ++i) {
        if (displayName == QLatin1String(catalog[i].displayName)) {
            return catalog[i].create(sampleRate);
        }
    }
    return nullptr;
}

} // namespace dawcast::dsp
