#pragma once

#include <iterator>

namespace labx3
{
    // Sounds the SPECIMEN layer streams from the user's own disk. Only relative paths live here:
    // this repository contains no game audio.
    //
    // Two local libraries are supported:
    //  - flPack:    FL Studio's legacy "stalker sounds" pack (paths relative to that folder).
    //  - specimens: the user's STALKER SPECIMENS library, extracted from their own copies of the
    //               games, laid out as <root>/<game>/sounds/<path as inside the game>. For
    //               S.T.A.L.K.E.R. 2 that path is the Wwise asset path below Content/_STALKER2/Audio,
    //               decoded to FLAC.
    //
    // Append new entries at the end only. Choice indices are saved inside host projects.
    // Music folders are deliberately absent: they hold commercial tracks by other artists.
    enum class SpecimenSource { flPack, specimens };

    struct SpecimenEntry
    {
        const char* name;
        const char* relativePath;
        SpecimenSource source;
        const char* group;
    };

    inline constexpr const char* groupFlPack   = "FL STUDIO PACK";
    inline constexpr const char* groupShadow   = "SHADOW OF CHORNOBYL";
    inline constexpr const char* groupClearSky = "CLEAR SKY";
    inline constexpr const char* groupPrypiat  = "CALL OF PRYPIAT";
    inline constexpr const char* groupHeart    = "HEART OF CHORNOBYL";

    inline constexpr SpecimenEntry specimenCatalog[] =
    {
        { "Psi Storm",           "anomaly/psi_storm_01.ogg",                           SpecimenSource::flPack, groupFlPack },
        { "Psy Voices",          "anomaly/psy_voices_1_l.ogg",                         SpecimenSource::flPack, groupFlPack },
        { "Controller Aura",     "monsters/controller/controller_psy_aura_l.ogg",      SpecimenSource::flPack, groupFlPack },
        { "Controller Presence", "monsters/controller/controller_presence_l.ogg",      SpecimenSource::flPack, groupFlPack },
        { "Psi Drone",           "soundtrack/psi/psi_01.ogg",                          SpecimenSource::flPack, groupFlPack },
        { "Psi Cluster",         "soundtrack/psi/psi_05.ogg",                          SpecimenSource::flPack, groupFlPack },
        { "Static",              "soundtrack/static/static_01.ogg",                    SpecimenSource::flPack, groupFlPack },
        { "Underground",         "ambient/background/underground_bkg_1.ogg",           SpecimenSource::flPack, groupFlPack },
        { "Transformer",         "ambient/transformer_hum1.ogg",                       SpecimenSource::flPack, groupFlPack },
        { "Cooling Plant",       "ambient/cooling_run.ogg",                            SpecimenSource::flPack, groupFlPack },
        { "Blowout Siren",       "ambient/blowout/blowout_siren.ogg",                  SpecimenSource::flPack, groupFlPack },
        { "Particle Wave",       "ambient/blowout/blowout_particle_wave.ogg",          SpecimenSource::flPack, groupFlPack },
        { "Blowout Rumble",      "ambient/blowout/blowout_ambient_rumble_01.ogg",      SpecimenSource::flPack, groupFlPack },
        { "Geiger",              "detectors/geiger_3.ogg",                             SpecimenSource::flPack, groupFlPack },
        { "Heartbeat",           "affects/heartbeat.ogg",                              SpecimenSource::flPack, groupFlPack },
        { "Poltergeist",         "monsters/poltergeist/tele_idle_0.ogg",               SpecimenSource::flPack, groupFlPack },
        { "Burer Gravity",       "monsters/burer/burer_gravi_wave_0.ogg",              SpecimenSource::flPack, groupFlPack },
        { "Servo",               "device/door_servomotor.ogg",                         SpecimenSource::flPack, groupFlPack },

        // STALKER SPECIMENS library, v0.2.0
        { "SoC / X-16 Psi Emitter",      "shadow_of_chornobyl/sounds/ambient/x16/x16_psy.ogg",               SpecimenSource::specimens, groupShadow },
        { "SoC / X-16 Brain Machine",    "shadow_of_chornobyl/sounds/ambient/x16/x16_brain_run.ogg",         SpecimenSource::specimens, groupShadow },
        { "SoC / X-16 Engine",           "shadow_of_chornobyl/sounds/ambient/x16/x16_engine2_run.ogg",       SpecimenSource::specimens, groupShadow },
        { "SoC / X-16 Hum",              "shadow_of_chornobyl/sounds/ambient/x16/x16_hum_2.ogg",             SpecimenSource::specimens, groupShadow },
        { "SoC / X-18 Noise",            "shadow_of_chornobyl/sounds/ambient/x18/x18_noise_1.ogg",           SpecimenSource::specimens, groupShadow },
        { "SoC / X-18 Wind",             "shadow_of_chornobyl/sounds/ambient/x18/x18_wind_1.ogg",            SpecimenSource::specimens, groupShadow },
        { "SoC / Psy Blackout",          "shadow_of_chornobyl/sounds/affects/psy_blackout_r.ogg",            SpecimenSource::specimens, groupShadow },
        { "SoC / Tinnitus",              "shadow_of_chornobyl/sounds/affects/tinnitus3a.ogg",                SpecimenSource::specimens, groupShadow },
        { "SoC / Monolith",              "shadow_of_chornobyl/sounds/anomaly/monolith_idle.ogg",             SpecimenSource::specimens, groupShadow },
        { "SoC / Sarcophagus Dream",     "shadow_of_chornobyl/sounds/intro/dream_sarcofag_l.ogg",            SpecimenSource::specimens, groupShadow },
        { "SoC / Yantar Dream",          "shadow_of_chornobyl/sounds/intro/yantar_dream_l.ogg",              SpecimenSource::specimens, groupShadow },
        { "SoC / Radar Spin-Down",       "shadow_of_chornobyl/sounds/device/radar_stop.ogg",                 SpecimenSource::specimens, groupShadow },
        { "SoC / Underground Breath",    "shadow_of_chornobyl/sounds/ambient/underground/breath_1.ogg",      SpecimenSource::specimens, groupShadow },
        { "SoC / Strange Noise",         "shadow_of_chornobyl/sounds/ambient/underground/strange_noise_3.ogg", SpecimenSource::specimens, groupShadow },

        { "CS / Mine Wind",              "clear_sky/sounds/ambient/mine/wind_mine_1.ogg",                    SpecimenSource::specimens, groupClearSky },
        { "CS / Underground Bed",        "clear_sky/sounds/ambient/background/underground_bkg_1.ogg",        SpecimenSource::specimens, groupClearSky },
        { "CS / Device Hum",             "clear_sky/sounds/ambient/special/device_hum_1.ogg",                SpecimenSource::specimens, groupClearSky },
        { "CS / Marsh Generator",        "clear_sky/sounds/ambient/special/marsh_generator.ogg",             SpecimenSource::specimens, groupClearSky },
        { "CS / Substation Hum",         "clear_sky/sounds/ambient/special/lim_transformer.ogg",             SpecimenSource::specimens, groupClearSky },
        { "CS / Marsh Bubbles",          "clear_sky/sounds/ambient/special/marsh_bubbles.ogg",               SpecimenSource::specimens, groupClearSky },
        { "CS / Geiger Loop",            "clear_sky/sounds/detectors/geiger_loop_2.ogg",                     SpecimenSource::specimens, groupClearSky },
        { "CS / Gravity Rumble",         "clear_sky/sounds/anomaly/gravi_rumble1.ogg",                       SpecimenSource::specimens, groupClearSky },
        { "CS / Emission Idle",          "clear_sky/sounds/anomaly/emi_idle.ogg",                            SpecimenSource::specimens, groupClearSky },

        { "CoP / Lab X-8 Crying",        "call_of_prypiat/sounds/ambient/labx8/labx8_crying.ogg",            SpecimenSource::specimens, groupPrypiat },
        { "CoP / Oasis Noise",           "call_of_prypiat/sounds/ambient/jupiter/jup_b16_oasis_noise.ogg",   SpecimenSource::specimens, groupPrypiat },
        { "CoP / Spatial Bubble",        "call_of_prypiat/sounds/ambient/jupiter/jup_b46_spatial_bubble_idle.ogg",    SpecimenSource::specimens, groupPrypiat },
        { "CoP / Spatial Rupture",       "call_of_prypiat/sounds/ambient/jupiter/jup_b46_spatial_bubble_rupture.ogg", SpecimenSource::specimens, groupPrypiat },
        { "CoP / Jupiter Generator",     "call_of_prypiat/sounds/ambient/jupiter/jup_b219_generator_looped.ogg",      SpecimenSource::specimens, groupPrypiat },
        { "CoP / Pripyat Generator",     "call_of_prypiat/sounds/ambient/pripyat/pri_b306_generator_work.ogg",        SpecimenSource::specimens, groupPrypiat },
        { "CoP / Underpass Transformer", "call_of_prypiat/sounds/ambient/underpass/pas_b400_transformer.ogg",         SpecimenSource::specimens, groupPrypiat },
        { "CoP / Airtight Gates",        "call_of_prypiat/sounds/device/airtight_gates_idle.ogg",            SpecimenSource::specimens, groupPrypiat },
        { "CoP / Electra Ball",          "call_of_prypiat/sounds/anomaly/gen_electra_ball_idle.ogg",         SpecimenSource::specimens, groupPrypiat },
        { "CoP / Teleport",              "call_of_prypiat/sounds/anomaly/teleport_work_1.ogg",               SpecimenSource::specimens, groupPrypiat },
        { "CoP / Buzz Anomaly",          "call_of_prypiat/sounds/anomaly/buzz_idle.ogg",                     SpecimenSource::specimens, groupPrypiat },

        // STALKER SPECIMENS library, S.T.A.L.K.E.R. 2, v0.3.0
        { "S2 / Lab Ambience",          "heart_of_chornobyl/sounds/Events/GSCAudioVolumes/AV_Labs/SFX_AV_Labs_Large_Loop_46.flac",
          SpecimenSource::specimens, groupHeart },
        { "S2 / Ward Laboratory",       "heart_of_chornobyl/sounds/Events/Cutscenes/SFX_E16_MQ03_F1_TheWard/SFX_E16_MQ03_F1_TheWard_Ambience_Laboratory.flac",
          SpecimenSource::specimens, groupHeart },
        { "S2 / X-Lab Powered",         "heart_of_chornobyl/sounds/Events/DLC-1/GSCAudioVolumes/AV_XLabOn/SFX_XLabOn_Large_Loop_104.flac",
          SpecimenSource::specimens, groupHeart },
        { "S2 / X-19 Alarm",            "heart_of_chornobyl/sounds/Events/DLC-1/Quests/MQ/MQ12/Alarm_X19_Enter_Lab_Loop/SFX_MQ12_Alarm_X19_Enter_Lab_Play.flac",
          SpecimenSource::specimens, groupHeart },
        { "S2 / X3 TV Wall",            "heart_of_chornobyl/sounds/Events/Cutscenes/SFX_E11_MQ02_Strelok_X3/SFX_E11_MQ02_Strelok_X3_TVWall_Loop_L.flac",
          SpecimenSource::specimens, groupHeart },
        { "S2 / Electro",               "heart_of_chornobyl/sounds/Events/Anomalies/Electro/SFX_Anomaly_Electro_01.flac",
          SpecimenSource::specimens, groupHeart },
        { "S2 / Breach",                "heart_of_chornobyl/sounds/Events/Anomalies/Breach/SFX_Anomaly_Breach_04.flac",
          SpecimenSource::specimens, groupHeart },
        { "S2 / Bulb",                  "heart_of_chornobyl/sounds/Events/Anomalies/Bulb/SFX_Anomaly_Bulb_Idle_01.flac",
          SpecimenSource::specimens, groupHeart },
        { "S2 / Zenith",                "heart_of_chornobyl/sounds/Events/Anomalies/Visual_Anomalies/Zenith/SFX_Anomaly_Zenith_Loop_01.flac",
          SpecimenSource::specimens, groupHeart },
        { "S2 / Witch Stone",           "heart_of_chornobyl/sounds/Events/Anomalies/Visual_Anomalies/WitchStone/SFX_Anomaly_WitchStone_Loop_01.flac",
          SpecimenSource::specimens, groupHeart },
        { "S2 / Emission",              "heart_of_chornobyl/sounds/Events/Emission/SFX_Emission_Active_01.flac",
          SpecimenSource::specimens, groupHeart },
        { "S2 / Psy Noise",             "heart_of_chornobyl/sounds/Events/Effects/Psy/SFX_Psy_Noise_Play_25.flac",
          SpecimenSource::specimens, groupHeart },
        { "S2 / Psy Ringing",           "heart_of_chornobyl/sounds/Events/Effects/Psy/SFX_Psy_Noise_Play_41.flac",
          SpecimenSource::specimens, groupHeart },
        { "S2 / Phantom Spawn",         "heart_of_chornobyl/sounds/Events/Effects/Psy/SFX_Psy_PhantomSpawn_08.flac",
          SpecimenSource::specimens, groupHeart },
        { "S2 / Kaymanov Psy House",    "heart_of_chornobyl/sounds/Events/Ambient_Actors/Kaymanov_Psy_House/SFX_Kaymanov_Psy_House_03.flac",
          SpecimenSource::specimens, groupHeart },
        { "S2 / Controller Idle",       "heart_of_chornobyl/sounds/Events/S2_Live/Mutants/controller/Controller_Voice/Necrophage/Voice/SFX_Controller_Necro_Voice_Idle_01.flac",
          SpecimenSource::specimens, groupHeart },
    };

    inline constexpr int specimenCatalogSize = (int) std::size (specimenCatalog);

    // Choice index 0 is the user's own file. Catalog entry i is choice i + 1.
    inline constexpr int specimenUserChoice = 0;

    // Location of FL Studio's pack inside an FL Studio installation folder.
    inline constexpr const char* specimenPackSubPath = "Data/Patches/Packs/Legacy/stalker sounds";

    // Default location of the STALKER SPECIMENS library, checked on each local drive.
    inline constexpr const char* specimensLibrarySubPath = "_AUDIO/STALKER SPECIMENS";

    // Game folders inside the specimens library; any one with a sounds/ folder identifies it.
    inline constexpr const char* specimensGameFolders[] = { "shadow_of_chornobyl", "clear_sky", "call_of_prypiat", "heart_of_chornobyl" };
}
