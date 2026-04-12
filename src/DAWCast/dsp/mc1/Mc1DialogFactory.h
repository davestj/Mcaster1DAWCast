// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Mc1DialogFactory
// ────────────────
// Maps an MC1 plugin id (e.g. "mc1.eq.parametric10") to its original
// hand-built Q_OBJECT editor dialog from the AudioPipe fx_ui pack.
// Each MC1 plugin keeps its OWN unique interface — VST-style — they do
// NOT inherit the host DAW's theme.
//
// Used by EffectsRackWidget when the user clicks "Edit" on an MC1
// effect slot in a track's effects rack.

#pragma once

#include "patchbay/dsp/dsp_effect.h"

#include "fx_ui/ParametricEqDialog.h"
#include "fx_ui/DualEq15Dialog.h"
#include "fx_ui/GraphicEq31Dialog.h"
#include "fx_ui/CompressorDialog.h"
#include "fx_ui/Dbx166xsDialog.h"
#include "fx_ui/BroadcastAgcDialog.h"
#include "fx_ui/DbxVoiceDialog.h"
#include "fx_ui/XenyxPreampDialog.h"
#include "fx_ui/SonicEnhancerDialog.h"
#include "fx_ui/TubePreampDialog.h"
#include "fx_ui/CasterTubeDialog.h"
#include "fx_ui/MicModelerDialog.h"
#include "fx_ui/Lexicon224Dialog.h"
#include "fx_ui/LexiconPcm70Dialog.h"
#include "fx_ui/LexiconPcm96Dialog.h"
#include "fx_ui/Lexicon480LDialog.h"
#include "fx_ui/LexiconMpx1Dialog.h"
#include "fx_ui/PodcastPluginDialog.h"
#include "fx_ui/SignalHillStudioDialog.h"
#include "fx_ui/MusicStudioDialogs.h"
#include "fx_ui/BBESonicSweetDialogs.h"
#include "fx_ui/DbxDialogs.h"
#include "fx_ui/FlagshipDialogs.h"

#include <QDialog>
#include <QString>

namespace dawcast::dsp {

/// Open the original MC1 plugin editor for the given effect.
/// Returns nullptr if no dialog is registered for the effect's id.
/// The returned dialog is parent-owned (Qt::WA_DeleteOnClose).
inline QDialog* openMc1EditorFor(mc1dsp::DspEffect* fx, QWidget* parent)
{
    if (!fx) return nullptr;

    const QString id = QString::fromLatin1(fx->id());
    QDialog* dlg = nullptr;

    if (id == QLatin1String("mc1.eq.parametric10")) {
        dlg = new ParametricEqDialog(fx, parent);
    } else if (id == QLatin1String("mc1.eq.dual15")) {
        dlg = new DualEq15Dialog(fx, parent);
    } else if (id == QLatin1String("mc1.eq.graphic31")) {
        dlg = new GraphicEq31Dialog(fx, parent);
    } else if (id == QLatin1String("mc1.dynamics.compressor")) {
        dlg = new CompressorDialog(fx, parent);
    } else if (id == QLatin1String("mc1.dynamics.dbx166xs")) {
        dlg = new Dbx166xsDialog(fx, parent);
    } else if (id == QLatin1String("mc1.dynamics.agc")) {
        dlg = new BroadcastAgcDialog(fx, parent);
    } else if (id == QLatin1String("mc1.channel.dbx286s")) {
        dlg = new DbxVoiceDialog(fx, parent);
    } else if (id == QLatin1String("mc1.channel.xenyx")) {
        dlg = new XenyxPreampDialog(fx, parent);
    } else if (id == QLatin1String("mc1.enhancer.sonic")) {
        dlg = new SonicEnhancerDialog(fx, parent);
    } else if (id == QLatin1String("mc1.analog.tube_preamp")) {
        dlg = new TubePreampDialog(fx, parent);
    } else if (id == QLatin1String("mc1.analog.castertube")) {
        dlg = new CasterTubeDialog(fx, parent);
    } else if (id == QLatin1String("mc1.modeling.mic")) {
        dlg = new MicModelerDialog(fx, parent);
    } else if (id == QLatin1String("mc1.lexicon.l224")) {
        dlg = new Lexicon224Dialog(fx, parent);
    } else if (id == QLatin1String("mc1.lexicon.pcm70")) {
        dlg = new LexiconPcm70Dialog(fx, parent);
    } else if (id == QLatin1String("mc1.lexicon.pcm96")) {
        dlg = new LexiconPcm96Dialog(fx, parent);
    } else if (id == QLatin1String("mc1.lexicon.l480l")) {
        dlg = new Lexicon480LDialog(fx, parent);
    } else if (id == QLatin1String("mc1.lexicon.mpx1")) {
        dlg = new LexiconMpx1Dialog(fx, parent);
    } else if (id == QLatin1String("mc1.podcast.voice_lift")) {
        dlg = new VoiceLiftDialog(fx, parent);
    } else if (id == QLatin1String("mc1.podcast.plosive")) {
        dlg = new PlosiveKillerDialog(fx, parent);
    } else if (id == QLatin1String("mc1.podcast.mouth_click")) {
        dlg = new MouthClickDialog(fx, parent);
    } else if (id == QLatin1String("mc1.podcast.bleed")) {
        dlg = new BleedSuppressorDialog(fx, parent);
    } else if (id == QLatin1String("mc1.podcast.phone_line")) {
        dlg = new PhoneLineDialog(fx, parent);
    } else if (id == QLatin1String("mc1.podcast.remote_restore")) {
        dlg = new RemoteRestorerDialog(fx, parent);
    } else if (id == QLatin1String("mc1.podcast.loudness_match")) {
        dlg = new LoudnessMatchDialog(fx, parent);
    } else if (id == QLatin1String("mc1.podcast.stinger")) {
        dlg = new StingerBedDialog(fx, parent);
    } else if (id == QLatin1String("mc1.podcast.vodcast_lipsync")) {
        dlg = new VodcastLipsyncDialog(fx, parent);
    } else if (id == QLatin1String("mc1.studio.signal_hill_a")) {
        dlg = new SignalHillStudioDialog(fx, parent);
    } else if (id == QLatin1String("mc1.studio.tidemark_a")) {
        dlg = new TidemarkAStudioDialog(fx, parent);
    } else if (id == QLatin1String("mc1.studio.tidemark_b")) {
        dlg = new TidemarkBStudioDialog(fx, parent);
    } else if (id == QLatin1String("mc1.studio.tidemark_vault")) {
        dlg = new TidemarkVaultDialog(fx, parent);
    } else if (id == QLatin1String("mc1.studio.granite_a")) {
        dlg = new GraniteAStudioDialog(fx, parent);
    } else if (id == QLatin1String("mc1.bbe.d82")) {
        dlg = new BbeD82Dialog(fx, parent);
    } else if (id == QLatin1String("mc1.bbe.h82")) {
        dlg = new BbeH82Dialog(fx, parent);
    } else if (id == QLatin1String("mc1.bbe.l82")) {
        dlg = new BbeL82Dialog(fx, parent);
    } else if (id == QLatin1String("mc1.bbe.mach3bass")) {
        dlg = new BbeMach3Dialog(fx, parent);
    } else if (id == QLatin1String("mc1.dbx.676")) {
        dlg = new Dbx676Dialog(fx, parent);
    } else if (id == QLatin1String("mc1.dbx.580")) {
        dlg = new Dbx580Dialog(fx, parent);
    } else if (id == QLatin1String("mc1.dbx.266xs")) {
        dlg = new Dbx266xsDialog(fx, parent);
    } else if (id == QLatin1String("mc1.dbx.560a")) {
        dlg = new Dbx560ADialog(fx, parent);
    } else if (id == QLatin1String("mc1.dbx.520")) {
        dlg = new Dbx520Dialog(fx, parent);
    } else if (id == QLatin1String("mc1.dbx.510")) {
        dlg = new Dbx510Dialog(fx, parent);
    } else if (id == QLatin1String("mc1.dbx.530")) {
        dlg = new Dbx530Dialog(fx, parent);
    } else if (id == QLatin1String("mc1.studio.vocal_producer")) {
        dlg = new VocalProducerDialog(fx, parent);
    } else if (id == QLatin1String("mc1.analyzer.key_finder")) {
        dlg = new KeyFinderDialog(fx, parent);
    }

    if (dlg) {
        dlg->setAttribute(Qt::WA_DeleteOnClose);
    }
    return dlg;
}

} // namespace dawcast::dsp
