#pragma once

#include <iterator>

namespace labx3
{
    // Sounds the SPECIMEN layer can stream from a local copy of the S.T.A.L.K.E.R. sound pack.
    // Only relative paths live here. This repository contains no game audio: the plugin reads
    // the files from the user's own disk at runtime.
    //
    // Append new entries at the end only. Choice indices are saved inside host projects.
    // The pack's music folder is deliberately absent: it holds commercial tracks by other artists.
    struct SpecimenEntry
    {
        const char* name;
        const char* relativePath;
    };

    inline constexpr SpecimenEntry specimenCatalog[] =
    {
        { "Psi Storm",           "anomaly/psi_storm_01.ogg" },
        { "Psy Voices",          "anomaly/psy_voices_1_l.ogg" },
        { "Controller Aura",     "monsters/controller/controller_psy_aura_l.ogg" },
        { "Controller Presence", "monsters/controller/controller_presence_l.ogg" },
        { "Psi Drone",           "soundtrack/psi/psi_01.ogg" },
        { "Psi Cluster",         "soundtrack/psi/psi_05.ogg" },
        { "Static",              "soundtrack/static/static_01.ogg" },
        { "Underground",         "ambient/background/underground_bkg_1.ogg" },
        { "Transformer",         "ambient/transformer_hum1.ogg" },
        { "Cooling Plant",       "ambient/cooling_run.ogg" },
        { "Blowout Siren",       "ambient/blowout/blowout_siren.ogg" },
        { "Particle Wave",       "ambient/blowout/blowout_particle_wave.ogg" },
        { "Blowout Rumble",      "ambient/blowout/blowout_ambient_rumble_01.ogg" },
        { "Geiger",              "detectors/geiger_3.ogg" },
        { "Heartbeat",           "affects/heartbeat.ogg" },
        { "Poltergeist",         "monsters/poltergeist/tele_idle_0.ogg" },
        { "Burer Gravity",       "monsters/burer/burer_gravi_wave_0.ogg" },
        { "Servo",               "device/door_servomotor.ogg" },
    };

    inline constexpr int specimenCatalogSize = (int) std::size (specimenCatalog);

    // Choice index 0 is the user's own file. Catalog entry i is choice i + 1.
    inline constexpr int specimenUserChoice = 0;

    // Location of the pack inside an FL Studio installation folder.
    inline constexpr const char* specimenPackSubPath = "Data/Patches/Packs/Legacy/stalker sounds";
}
