/*
 * Mcaster1Tuner — VST3 Chromatic Tuner Plugin
 * src/TuningTables.h — Instrument tuning preset library
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MCASTER1_TUNING_TABLES_H
#define MCASTER1_TUNING_TABLES_H

#include <cmath>
#include <cstddef>

// ===================================================================
//  TuningPreset — describes one instrument tuning configuration
// ===================================================================

struct TuningPreset {
    const char*         name;           // "Standard E"
    const char*         category;       // "Guitar 6-String", "Bass", "Vocal", etc.
    int                 stringCount;    // number of strings (0 for vocal/drums/piano)
    const float*        frequencies;    // pointer to static array of target frequencies (Hz)
    const char* const*  stringNames;    // pointer to static array of string labels ("E2", "A2", etc.)
};

// ===================================================================
//  Static frequency and string-name arrays
// ===================================================================

namespace TuningTablesData {

// --- Cent-offset helpers (compile-time where the compiler allows) ---
// pow(2, cents / 1200.0) is not constexpr in C++17, so we use static
// const arrays initialised at first use via inline functions.

inline float centOffset(float baseHz, float cents)
{
    return baseHz * std::pow(2.0f, cents / 1200.0f);
}

// ---------------------------------------------------------------
//  Guitar 6-String
// ---------------------------------------------------------------

static const float kStandardE[] = {
    82.41f, 110.00f, 146.83f, 196.00f, 246.94f, 329.63f
};
static const char* const kStandardENames[] = {
    "E2", "A2", "D3", "G3", "B3", "E4"
};

static const float kDropD[] = {
    73.42f, 110.00f, 146.83f, 196.00f, 246.94f, 329.63f
};
static const char* const kDropDNames[] = {
    "D2", "A2", "D3", "G3", "B3", "E4"
};

static const float kOpenG[] = {
    73.42f, 98.00f, 146.83f, 196.00f, 246.94f, 293.66f
};
static const char* const kOpenGNames[] = {
    "D2", "G2", "D3", "G3", "B3", "D4"
};

static const float kOpenD[] = {
    73.42f, 110.00f, 146.83f, 185.00f, 220.00f, 293.66f
};
static const char* const kOpenDNames[] = {
    "D2", "A2", "D3", "F#3", "A3", "D4"
};

static const float kDADGAD[] = {
    73.42f, 110.00f, 146.83f, 196.00f, 220.00f, 293.66f
};
static const char* const kDADGADNames[] = {
    "D2", "A2", "D3", "G3", "A3", "D4"
};

static const float kHalfStepDown[] = {
    77.78f, 103.83f, 138.59f, 185.00f, 233.08f, 311.13f
};
static const char* const kHalfStepDownNames[] = {
    "Eb2", "Ab2", "Db3", "Gb3", "Bb3", "Eb4"
};

// ---------------------------------------------------------------
//  Guitar 7-String
// ---------------------------------------------------------------

static const float kStandardB7[] = {
    61.74f, 82.41f, 110.00f, 146.83f, 196.00f, 246.94f, 329.63f
};
static const char* const kStandardB7Names[] = {
    "B1", "E2", "A2", "D3", "G3", "B3", "E4"
};

static const float kDropA7[] = {
    55.00f, 82.41f, 110.00f, 146.83f, 196.00f, 246.94f, 329.63f
};
static const char* const kDropA7Names[] = {
    "A1", "E2", "A2", "D3", "G3", "B3", "E4"
};

// ---------------------------------------------------------------
//  Guitar 8-String
// ---------------------------------------------------------------

static const float kStandardFs8[] = {
    46.25f, 61.74f, 82.41f, 110.00f, 146.83f, 196.00f, 246.94f, 329.63f
};
static const char* const kStandardFs8Names[] = {
    "F#1", "B1", "E2", "A2", "D3", "G3", "B3", "E4"
};

static const float kDropE8[] = {
    41.20f, 61.74f, 82.41f, 110.00f, 146.83f, 196.00f, 246.94f, 329.63f
};
static const char* const kDropE8Names[] = {
    "E1", "B1", "E2", "A2", "D3", "G3", "B3", "E4"
};

// ---------------------------------------------------------------
//  Les Paul Style (+2 cent warm offset)
// ---------------------------------------------------------------

inline const float* lesPaulStandard()
{
    static const float v[] = {
        centOffset(82.41f,  2.0f), centOffset(110.00f, 2.0f),
        centOffset(146.83f, 2.0f), centOffset(196.00f, 2.0f),
        centOffset(246.94f, 2.0f), centOffset(329.63f, 2.0f)
    };
    return v;
}
static const char* const kLesPaulStdNames[] = {
    "E2", "A2", "D3", "G3", "B3", "E4"
};

inline const float* lesPaulDropD()
{
    static const float v[] = {
        centOffset(73.42f,  2.0f), centOffset(110.00f, 2.0f),
        centOffset(146.83f, 2.0f), centOffset(196.00f, 2.0f),
        centOffset(246.94f, 2.0f), centOffset(329.63f, 2.0f)
    };
    return v;
}
static const char* const kLesPaulDropDNames[] = {
    "D2", "A2", "D3", "G3", "B3", "E4"
};

// ---------------------------------------------------------------
//  Strat Style (-1 cent bright offset)
// ---------------------------------------------------------------

inline const float* stratStandard()
{
    static const float v[] = {
        centOffset(82.41f,  -1.0f), centOffset(110.00f, -1.0f),
        centOffset(146.83f, -1.0f), centOffset(196.00f, -1.0f),
        centOffset(246.94f, -1.0f), centOffset(329.63f, -1.0f)
    };
    return v;
}
static const char* const kStratStdNames[] = {
    "E2", "A2", "D3", "G3", "B3", "E4"
};

inline const float* stratDropD()
{
    static const float v[] = {
        centOffset(73.42f,  -1.0f), centOffset(110.00f, -1.0f),
        centOffset(146.83f, -1.0f), centOffset(196.00f, -1.0f),
        centOffset(246.94f, -1.0f), centOffset(329.63f, -1.0f)
    };
    return v;
}
static const char* const kStratDropDNames[] = {
    "D2", "A2", "D3", "G3", "B3", "E4"
};

// ---------------------------------------------------------------
//  Bass Guitar
// ---------------------------------------------------------------

static const float kBass4[] = {
    41.20f, 55.00f, 73.42f, 98.00f
};
static const char* const kBass4Names[] = {
    "E1", "A1", "D2", "G2"
};

static const float kBass5[] = {
    30.87f, 41.20f, 55.00f, 73.42f, 98.00f
};
static const char* const kBass5Names[] = {
    "B0", "E1", "A1", "D2", "G2"
};

static const float kBass6[] = {
    30.87f, 41.20f, 55.00f, 73.42f, 98.00f, 130.81f
};
static const char* const kBass6Names[] = {
    "B0", "E1", "A1", "D2", "G2", "C3"
};

static const float kBassDropD[] = {
    36.71f, 55.00f, 73.42f, 98.00f
};
static const char* const kBassDropDNames[] = {
    "D1", "A1", "D2", "G2"
};

static const float kBassDropC[] = {
    32.70f, 49.00f, 65.41f, 87.31f
};
static const char* const kBassDropCNames[] = {
    "C1", "G1", "C2", "F2"
};

// ---------------------------------------------------------------
//  Jazz ES-335
// ---------------------------------------------------------------

// ES-335 Standard reuses kStandardE / kStandardENames
// ES-335 Jazz reuses kHalfStepDown / kHalfStepDownNames

// ---------------------------------------------------------------
//  Drums (stringCount=0, single target frequency per preset)
// ---------------------------------------------------------------

static const float kDrumKick[]     = { 60.0f };
static const float kDrumSnare[]    = { 200.0f };
static const float kDrumHiTom[]    = { 300.0f };
static const float kDrumMidTom[]   = { 200.0f };
static const float kDrumFloorTom[] = { 100.0f };

// ---------------------------------------------------------------
//  Piano (stringCount=0)
//  Railsback stretch: octaves 1-2 are ~-5 to -15 cents flat,
//  octaves 7-8 are ~+5 to +15 cents sharp relative to equal
//  temperament.  A single representative A4 target is stored;
//  the Railsback curve is applied in applyRailsbackStretch().
// ---------------------------------------------------------------

static const float kPianoA440[] = { 440.0f };
static const float kPianoA442[] = { 442.0f };

} // namespace TuningTablesData

// ===================================================================
//  Master preset table
// ===================================================================

namespace TuningTables {

namespace detail {

// Lazy-init wrapper so Les Paul / Strat offset arrays are built once.
inline const TuningPreset* allPresets()
{
    static const TuningPreset table[] = {

        // --- Vocal ---------------------------------------------------
        {
            "Chromatic", "Vocal", 0,
            nullptr, nullptr
        },

        // --- Guitar 6-String ----------------------------------------
        {
            "Standard E", "Guitar 6-String", 6,
            TuningTablesData::kStandardE,
            TuningTablesData::kStandardENames
        },
        {
            "Drop D", "Guitar 6-String", 6,
            TuningTablesData::kDropD,
            TuningTablesData::kDropDNames
        },
        {
            "Open G", "Guitar 6-String", 6,
            TuningTablesData::kOpenG,
            TuningTablesData::kOpenGNames
        },
        {
            "Open D", "Guitar 6-String", 6,
            TuningTablesData::kOpenD,
            TuningTablesData::kOpenDNames
        },
        {
            "DADGAD", "Guitar 6-String", 6,
            TuningTablesData::kDADGAD,
            TuningTablesData::kDADGADNames
        },
        {
            "Half Step Down", "Guitar 6-String", 6,
            TuningTablesData::kHalfStepDown,
            TuningTablesData::kHalfStepDownNames
        },

        // --- Guitar 7-String ----------------------------------------
        {
            "Standard B", "Guitar 7-String", 7,
            TuningTablesData::kStandardB7,
            TuningTablesData::kStandardB7Names
        },
        {
            "Drop A", "Guitar 7-String", 7,
            TuningTablesData::kDropA7,
            TuningTablesData::kDropA7Names
        },

        // --- Guitar 8-String ----------------------------------------
        {
            "Standard F#", "Guitar 8-String", 8,
            TuningTablesData::kStandardFs8,
            TuningTablesData::kStandardFs8Names
        },
        {
            "Drop E", "Guitar 8-String", 8,
            TuningTablesData::kDropE8,
            TuningTablesData::kDropE8Names
        },

        // --- Les Paul Style (+2 cent warm offset) --------------------
        {
            "Les Paul Standard", "Les Paul Style", 6,
            TuningTablesData::lesPaulStandard(),
            TuningTablesData::kLesPaulStdNames
        },
        {
            "Les Paul Drop D", "Les Paul Style", 6,
            TuningTablesData::lesPaulDropD(),
            TuningTablesData::kLesPaulDropDNames
        },

        // --- Strat Style (-1 cent bright offset) --------------------
        {
            "Strat Standard", "Strat Style", 6,
            TuningTablesData::stratStandard(),
            TuningTablesData::kStratStdNames
        },
        {
            "Strat Drop D", "Strat Style", 6,
            TuningTablesData::stratDropD(),
            TuningTablesData::kStratDropDNames
        },

        // --- Bass Guitar ---------------------------------------------
        {
            "Bass 4-String", "Bass Guitar", 4,
            TuningTablesData::kBass4,
            TuningTablesData::kBass4Names
        },
        {
            "Bass 5-String", "Bass Guitar", 5,
            TuningTablesData::kBass5,
            TuningTablesData::kBass5Names
        },
        {
            "Bass 6-String", "Bass Guitar", 6,
            TuningTablesData::kBass6,
            TuningTablesData::kBass6Names
        },
        {
            "Bass Drop D", "Bass Guitar", 4,
            TuningTablesData::kBassDropD,
            TuningTablesData::kBassDropDNames
        },
        {
            "Bass Drop C", "Bass Guitar", 4,
            TuningTablesData::kBassDropC,
            TuningTablesData::kBassDropCNames
        },

        // --- Jazz ES-335 ---------------------------------------------
        {
            "ES-335 Standard", "Jazz ES-335", 6,
            TuningTablesData::kStandardE,
            TuningTablesData::kStandardENames
        },
        {
            "ES-335 Jazz", "Jazz ES-335", 6,
            TuningTablesData::kHalfStepDown,
            TuningTablesData::kHalfStepDownNames
        },

        // --- Drums (stringCount=0, single target freq) ---------------
        {
            "Kick", "Drums", 0,
            TuningTablesData::kDrumKick, nullptr
        },
        {
            "Snare", "Drums", 0,
            TuningTablesData::kDrumSnare, nullptr
        },
        {
            "Hi Tom", "Drums", 0,
            TuningTablesData::kDrumHiTom, nullptr
        },
        {
            "Mid Tom", "Drums", 0,
            TuningTablesData::kDrumMidTom, nullptr
        },
        {
            "Floor Tom", "Drums", 0,
            TuningTablesData::kDrumFloorTom, nullptr
        },

        // --- Piano (stringCount=0, concert pitch reference) ----------
        {
            "Piano A440", "Piano", 0,
            TuningTablesData::kPianoA440, nullptr
        },
        {
            "Piano A442", "Piano", 0,
            TuningTablesData::kPianoA442, nullptr
        },
    };
    return table;
}

constexpr int kPresetCount = 29;

} // namespace detail

// ---------------------------------------------------------------
//  Public API
// ---------------------------------------------------------------

/** Total number of built-in presets. */
inline int presetCount()
{
    return detail::kPresetCount;
}

/** Return the preset at the given index (0-based). */
inline const TuningPreset& preset(int index)
{
    return detail::allPresets()[index];
}

/**
 * Iterate all presets and call the visitor for each one whose
 * category matches (case-sensitive).
 *
 * Example:
 *   TuningTables::forCategory("Bass Guitar", [](int idx, const TuningPreset& p) {
 *       // ...
 *   });
 */
template <typename Fn>
inline void forCategory(const char* category, Fn&& visitor)
{
    const TuningPreset* table = detail::allPresets();
    for (int i = 0; i < detail::kPresetCount; ++i) {
        // Simple C-string comparison (no <cstring> needed — inlined)
        const char* a = table[i].category;
        const char* b = category;
        bool match = true;
        while (*a && *b) {
            if (*a != *b) { match = false; break; }
            ++a; ++b;
        }
        if (match && *a == *b)
            visitor(i, table[i]);
    }
}

/**
 * Find the first preset whose name matches exactly.
 * Returns nullptr if not found.
 */
inline const TuningPreset* findByName(const char* name)
{
    const TuningPreset* table = detail::allPresets();
    for (int i = 0; i < detail::kPresetCount; ++i) {
        const char* a = table[i].name;
        const char* b = name;
        bool match = true;
        while (*a && *b) {
            if (*a != *b) { match = false; break; }
            ++a; ++b;
        }
        if (match && *a == *b)
            return &table[i];
    }
    return nullptr;
}

// ---------------------------------------------------------------
//  Concert pitch adjustment
// ---------------------------------------------------------------

/**
 * Adjust a frequency from A440 to a different concert pitch.
 *
 * @param hz           Original frequency (assumed relative to A4 = 440 Hz).
 * @param concertPitch Desired A4 reference (e.g. 442 Hz for European pitch).
 * @return             Adjusted frequency.
 */
inline float adjustConcertPitch(float hz, float concertPitch)
{
    return hz * (concertPitch / 440.0f);
}

// ---------------------------------------------------------------
//  Railsback stretch curve for piano tuning
// ---------------------------------------------------------------

/**
 * Apply Railsback stretch to a piano frequency.
 *
 * Real pianos are tuned with stretched octaves: the lowest notes
 * are tuned slightly flat and the highest notes slightly sharp
 * relative to equal temperament.  This approximation applies a
 * linear stretch in cents based on octave distance from the
 * middle of the keyboard (A4).
 *
 * @param hz           12-TET frequency for the note.
 * @param concertPitch Concert pitch reference (default 440 Hz).
 * @return             Stretched frequency.
 */
inline float applyRailsbackStretch(float hz, float concertPitch = 440.0f)
{
    if (hz <= 0.0f)
        return hz;

    // Octave distance from A4 (positive = above, negative = below)
    const float octavesFromA4 = std::log2(hz / concertPitch);

    // Stretch coefficient in cents per octave.
    // Typical Railsback values: ~2-3 cents/octave at extremes,
    // near zero in the middle register.
    // We use a simple quadratic model: stretch grows with the
    // square of the distance from the center.
    constexpr float kStretchCoeff = 0.6f; // cents per octave^2
    const float stretchCents = kStretchCoeff * octavesFromA4
                               * std::fabs(octavesFromA4);

    return hz * std::pow(2.0f, stretchCents / 1200.0f);
}

// ---------------------------------------------------------------
//  Drum frequency ranges
// ---------------------------------------------------------------

struct DrumRange {
    float targetHz;
    float lowHz;
    float highHz;
};

/**
 * Return the expected tuning range for a drum preset.
 * Only meaningful when the preset category is "Drums".
 * Returns {0,0,0} for non-drum presets.
 */
inline DrumRange drumRange(const TuningPreset& p)
{
    if (p.frequencies == nullptr)
        return { 0.0f, 0.0f, 0.0f };

    const float target = p.frequencies[0];

    // Per-drum ranges based on common tuning practice
    if (target <= 65.0f)                      // Kick  (~60 Hz)
        return { target, 50.0f, 80.0f };
    if (target >= 280.0f)                     // Hi Tom (~300 Hz)
        return { target, 250.0f, 350.0f };
    if (target >= 180.0f && target <= 220.0f) // Snare / Mid Tom (~200 Hz)
        return { target, 150.0f, 250.0f };
    if (target >= 80.0f && target <= 120.0f)  // Floor Tom (~100 Hz)
        return { target, 80.0f, 130.0f };

    // Fallback: +/- 20%
    return { target, target * 0.8f, target * 1.2f };
}

} // namespace TuningTables

#endif // MCASTER1_TUNING_TABLES_H
