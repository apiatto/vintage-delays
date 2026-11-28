// MIDI map of the Novation DrumStation rack (owner's manual, MIDI implementation v1.0, 10 May 1996):
// controller numbers from the "Controller Map" (pages 42-43), note numbers from the
// "MIDI Percussion Maps" (page 47). No JUCE dependency.

#pragma once

namespace ds
{

enum Kit { kit808 = 0, kit909 = 1, numKits = 2 };

// Panel sections, left to right.
enum Section
{
    secBass = 0, secSnare, secToms, secRimClapBell, secHiHat, secCymbals, secCongas, numSections
};

// One drum sound. Controller numbers are -1 where the sound has no such control.
// "tune" is the top-left knob of the section, "b1" the bottom-left knob, "b2" the bottom-right knob.
struct Voice
{
    const char* id;        // parameter id prefix, unique
    const char* name;      // shown in the host and on the panel
    Kit kit;
    Section section;
    int select;            // position of the section's SELECT switch(es) that picks this sound
    int ccFrontCut, ccPan, ccDistortion;
    int ccTune, ccB1, ccB2;
    const char* tuneName;  // "Tune" or "Tone"
    const char* b1Name;
    const char* b2Name;
    int noteGm808, noteGm909; // trigger note with "Gen MIDI Set" on 808 or on 909 (-1: not mapped)
};

// Select positions: Toms 0..2 (low/mid/high), Rim/Clap/Bell 0..2, Hi hat 0..1 (closed/open),
// Cymbals 0..1 (crash/ride), Congas section: 0..2 low/mid/high conga, 3 maracas, 4 claves.
inline constexpr Voice voices[] = {
    // id        name                kit     section          sel  FC   Pan  Dist Tune B1   B2   names                              notes
    { "k808_bd", "808 Bass Drum",    kit808, secBass,         0,  20,  21,  22,  23,  24,  25,  "Tune", "Tone",   "Decay",  36, 24 },
    { "k808_sd", "808 Snare Drum",   kit808, secSnare,        0,  26,  27,  28,  29,  30,  31,  "Tune", "Tone",   "Snappy", 38, 26 },
    { "k808_lt", "808 Low Tom",      kit808, secToms,         0,  32,  33,  34,  35,  36,  -1,  "Tune", "Decay",  "",       41, 28 },
    { "k808_mt", "808 Mid Tom",      kit808, secToms,         1,  37,  38,  39,  40,  41,  -1,  "Tune", "Decay",  "",       45, 29 },
    { "k808_ht", "808 High Tom",     kit808, secToms,         2,  42,  43,  44,  45,  46,  -1,  "Tune", "Decay",  "",       48, 31 },
    { "k808_rs", "808 Rim Shot",     kit808, secRimClapBell,  0,  -1,  47,  -1,  48,  -1,  -1,  "Tune", "",       "",       37, 25 },
    { "k808_cp", "808 Hand Clap",    kit808, secRimClapBell,  1,  -1,  49,  -1,  50,  -1,  -1,  "Tune", "",       "",       39, 27 },
    { "k808_cb", "808 Cowbell",      kit808, secRimClapBell,  2,  -1,  51,  52,  53,  -1,  -1,  "Tune", "",       "",       56, 56 },
    { "k808_ch", "808 Closed Hi Hat",kit808, secHiHat,        0,  -1,  54,  -1,  55,  56,  -1,  "Tune", "Decay",  "",       42, 30 },
    { "k808_oh", "808 Open Hi Hat",  kit808, secHiHat,        1,  -1,  57,  -1,  58,  59,  -1,  "Tune", "Decay",  "",       46, -1 },
    { "k808_cy", "808 Cymbal",       kit808, secCymbals,      0,  -1,  60,  -1,  61,  62,  -1,  "Tone", "Decay",  "",       49, 32 },
    { "k808_lc", "808 Low Conga",    kit808, secCongas,       0,  -1,  63,  64,  65,  -1,  -1,  "Tune", "",       "",       64, 64 },
    { "k808_mc", "808 Mid Conga",    kit808, secCongas,       1,  -1,  66,  67,  68,  -1,  -1,  "Tune", "",       "",       63, 63 },
    { "k808_hc", "808 High Conga",   kit808, secCongas,       2,  -1,  69,  70,  71,  -1,  -1,  "Tune", "",       "",       62, 62 },
    { "k808_ma", "808 Maracas",      kit808, secCongas,       3,  -1,  72,  -1,  73,  -1,  -1,  "Tune", "",       "",       70, 70 },
    { "k808_cl", "808 Claves",       kit808, secCongas,       4,  -1,  74,  -1,  75,  -1,  -1,  "Tune", "",       "",       75, 75 },

    { "k909_bd", "909 Bass Drum",    kit909, secBass,         0, 105, 106, 107,  76,  77,  78,  "Tune", "Attack", "Decay",  24, 36 },
    { "k909_sd", "909 Snare Drum",   kit909, secSnare,        0, 108, 109, 110,  79,  80,  81,  "Tune", "Tone",   "Snappy", 26, 38 },
    { "k909_lt", "909 Low Tom",      kit909, secToms,         0,  82,  83,  84,  85,  86,  -1,  "Tune", "Decay",  "",       28, 41 },
    { "k909_mt", "909 Mid Tom",      kit909, secToms,         1,  87,  88,  89,  90,  91,  -1,  "Tune", "Decay",  "",       29, 45 },
    { "k909_ht", "909 High Tom",     kit909, secToms,         2,  92,  93,  94,  95,  96,  -1,  "Tune", "Decay",  "",       31, 48 },
    { "k909_rs", "909 Rim Shot",     kit909, secRimClapBell,  0,  -1,  97,  -1,  98,  -1,  -1,  "Tune", "",       "",       25, 37 },
    { "k909_cp", "909 Hand Clap",    kit909, secRimClapBell,  1,  -1,  99,  -1, 100,  -1,  -1,  "Tune", "",       "",       27, 39 },
    { "k909_ch", "909 Closed Hi Hat",kit909, secHiHat,        0,  -1, 111, 101, 102, 103,  -1,  "Tune", "Decay",  "",       30, 42 },
    { "k909_oh", "909 Open Hi Hat",  kit909, secHiHat,        1,  -1, 112,  -1, 104, 113,  -1,  "Tune", "Decay",  "",       34, 46 },
    { "k909_cr", "909 Crash Cymbal", kit909, secCymbals,      0,  -1, 114,  -1, 115, 116,  -1,  "Tune", "Decay",  "",       32, 49 },
    { "k909_ri", "909 Ride Cymbal",  kit909, secCymbals,      1,  -1, 117,  -1, 118, 119,  -1,  "Tune", "Decay",  "",       33, 51 },
};

inline constexpr int numVoices = (int) (sizeof (voices) / sizeof (voices[0]));

// The controls a voice can have, in the order the parameters are created.
enum Control { ctlTune = 0, ctlB1, ctlB2, ctlFrontCut, ctlPan, ctlDistortion, numControls };

inline int controllerFor (const Voice& v, Control c)
{
    switch (c)
    {
        case ctlTune:       return v.ccTune;
        case ctlB1:         return v.ccB1;
        case ctlB2:         return v.ccB2;
        case ctlFrontCut:   return v.ccFrontCut;
        case ctlPan:        return v.ccPan;
        case ctlDistortion: return v.ccDistortion;
        case numControls:   break;
    }
    return -1;
}

inline const char* controlName (const Voice& v, Control c)
{
    switch (c)
    {
        case ctlTune:       return v.tuneName;
        case ctlB1:         return v.b1Name;
        case ctlB2:         return v.b2Name;
        case ctlFrontCut:   return "Front Cut";
        case ctlPan:        return "Pan";
        case ctlDistortion: return "Distortion";
        case numControls:   break;
    }
    return "";
}

inline const char* controlSuffix (Control c)
{
    static const char* suffixes[] = { "tune", "b1", "b2", "fcut", "pan", "dist" };
    return suffixes[(int) c];
}

// Index of the voice at a panel position, or -1 if the kit has no sound there
// (e.g. no cowbell, ride or congas on the 909, no ride on the 808).
inline int findVoice (Kit kit, Section section, int select)
{
    for (int i = 0; i < numVoices; ++i)
        if (voices[i].kit == kit && voices[i].section == section && voices[i].select == select)
            return i;
    return -1;
}

inline constexpr int ccVolume = 7;
inline constexpr int numPrograms = 40;   // 00-24 factory, 25-39 user

} // namespace ds
