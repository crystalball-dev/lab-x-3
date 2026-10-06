#include "FactoryPresets.h"
#include "Params.h"

namespace labx3
{
    namespace
    {
        // Choice indices, spelled out for readability.
        constexpr float saw = 0, square = 1, sine = 2, subtle = 3;            // OSC A
        constexpr float bSine = 0, bTriangle = 1, bSaw = 2, bSquare = 3;      // OSC B
        constexpr float lowpass = 0, bandpass = 1;                            // filter
        constexpr float toPitch = 0, toFilter = 1, toFormant = 2, toSpecimen = 3, toAll = 4;

        float spec (const char* name) { return (float) specimenChoiceForName (name); }

        std::vector<Preset> build()
        {
            std::vector<Preset> p;

            // 1. The lab at rest: sub-hum, a slow drifting sine stack, the underground recording breathing underneath.
            p.push_back ({ "Oasis-3 Idle", {
                { "osc_a_wave", sine }, { "osc_a_shape", 0.25f }, { "osc_a_level", 0.55f },
                { "osc_b_wave", bSine }, { "osc_b_ratio", 2.0f }, { "osc_b_fm", 0.18f }, { "osc_b_level", 0.22f },
                { "sub_level", 0.55f }, { "noise_level", 0.04f }, { "noise_color", 0.25f },
                { "geiger_density", 0.6f }, { "geiger_tone", 3000.0f },
                { "specimen_source", spec ("Underground") }, { "specimen_level", 0.35f }, { "specimen_position", 0.2f },
                { "specimen_spray", 0.45f }, { "specimen_size", 320.0f }, { "specimen_density", 9.0f }, { "specimen_track", 0.0f },
                { "filter_type", lowpass }, { "filter_cutoff", 420.0f }, { "filter_res", 0.3f }, { "filter_drive", 6.0f },
                { "filter_env", 0.15f }, { "filter_keytrack", 0.4f },
                { "env1_attack", 1.2f }, { "env1_decay", 2.0f }, { "env1_sustain", 0.9f }, { "env1_release", 3.5f },
                { "env2_attack", 2.0f }, { "env2_decay", 3.0f }, { "env2_sustain", 0.5f }, { "env2_release", 3.0f },
                { "prog_rate", 0.08f }, { "prog_depth", 0.25f }, { "prog_target", toAll },
                { "whisper_level", 0.08f }, { "whisper_formant", 127.0f }, { "whisper_keytrack", 0.3f },
                { "noo_size", 0.7f }, { "noo_decay", 0.62f }, { "noo_mix", 0.3f },
                { "dark", 0.3f }, { "master_volume", -9.0f }, { "stereo_width", 0.7f } } });

            // 2. Feedback-FM tone through a resonant band-pass, wavering in pitch: the engineered illusion.
            p.push_back ({ "Subtle Matter", {
                { "osc_a_wave", subtle }, { "osc_a_shape", 0.75f }, { "osc_a_level", 0.6f },
                { "osc_b_wave", bSine }, { "osc_b_ratio", 1.414f }, { "osc_b_fm", 0.38f }, { "osc_b_level", 0.1f },
                { "sub_level", 0.25f }, { "noise_level", 0.03f }, { "noise_color", 0.6f },
                { "specimen_source", spec ("Psy Voices") }, { "specimen_level", 0.25f }, { "specimen_position", 0.35f },
                { "specimen_spray", 0.5f }, { "specimen_size", 180.0f }, { "specimen_density", 20.0f }, { "specimen_track", 1.0f },
                { "filter_type", bandpass }, { "filter_cutoff", 900.0f }, { "filter_res", 0.55f }, { "filter_drive", 7.0f },
                { "filter_env", 0.25f }, { "filter_keytrack", 0.6f },
                { "env1_attack", 0.9f }, { "env1_decay", 1.5f }, { "env1_sustain", 0.85f }, { "env1_release", 4.0f },
                { "env2_attack", 1.5f }, { "env2_decay", 2.0f }, { "env2_sustain", 0.6f }, { "env2_release", 3.0f },
                { "prog_rate", 0.2f }, { "prog_depth", 0.45f }, { "prog_target", toPitch },
                { "whisper_level", 0.22f }, { "whisper_formant", 127.0f }, { "whisper_keytrack", 0.6f },
                { "noo_size", 0.85f }, { "noo_decay", 0.75f }, { "noo_mix", 0.45f },
                { "dark", 0.25f }, { "master_volume", -4.0f } } });

            // 3. A wide detuned pad dissolving into the psi drone: consciousness leaving the body.
            p.push_back ({ "Noosphere Transfer", {
                { "osc_a_wave", saw }, { "osc_a_shape", 0.65f }, { "osc_a_level", 0.55f },
                { "osc_b_wave", bTriangle }, { "osc_b_ratio", 0.5f }, { "osc_b_fm", 0.05f }, { "osc_b_level", 0.3f },
                { "sub_level", 0.2f }, { "noise_level", 0.03f }, { "noise_color", 0.45f },
                { "specimen_source", spec ("Psi Drone") }, { "specimen_level", 0.4f }, { "specimen_position", 0.4f },
                { "specimen_spray", 0.55f }, { "specimen_size", 260.0f }, { "specimen_density", 22.0f }, { "specimen_track", 1.0f },
                { "filter_type", lowpass }, { "filter_cutoff", 1400.0f }, { "filter_res", 0.32f }, { "filter_drive", 4.0f },
                { "filter_env", 0.2f }, { "filter_keytrack", 0.5f },
                { "env1_attack", 2.5f }, { "env1_decay", 3.0f }, { "env1_sustain", 0.85f }, { "env1_release", 6.0f },
                { "env2_attack", 3.0f }, { "env2_decay", 4.0f }, { "env2_sustain", 0.6f }, { "env2_release", 5.0f },
                { "prog_rate", 0.05f }, { "prog_depth", 0.3f }, { "prog_target", toAll },
                { "whisper_level", 0.1f }, { "whisper_formant", 160.0f }, { "whisper_keytrack", 0.5f },
                { "noo_size", 1.0f }, { "noo_decay", 0.85f }, { "noo_mix", 0.55f },
                { "dark", 0.25f }, { "master_volume", -5.5f }, { "stereo_width", 0.9f } } });

            // 4. Mono, gliding, driven and crushed: the programmed agent.
            p.push_back ({ "Dark's Programming", {
                { "voices", 1.0f }, { "glide", 0.12f },
                { "osc_a_wave", square }, { "osc_a_shape", 0.35f }, { "osc_a_level", 0.75f }, { "osc_a_octave", -1.0f },
                { "osc_b_wave", bSaw }, { "osc_b_ratio", 0.5f }, { "osc_b_fm", 0.22f }, { "osc_b_level", 0.35f },
                { "sub_level", 0.45f }, { "noise_level", 0.05f }, { "noise_color", 0.3f },
                { "geiger_density", 2.0f }, { "geiger_tone", 2800.0f },
                { "filter_type", lowpass }, { "filter_cutoff", 520.0f }, { "filter_res", 0.62f }, { "filter_drive", 15.0f },
                { "filter_env", 0.55f }, { "filter_keytrack", 0.5f },
                { "env1_attack", 0.004f }, { "env1_decay", 0.4f }, { "env1_sustain", 0.75f }, { "env1_release", 0.25f },
                { "env2_attack", 0.002f }, { "env2_decay", 0.35f }, { "env2_sustain", 0.15f }, { "env2_release", 0.3f },
                { "scrub_bits", 9.0f }, { "scrub_rate", 2.0f },
                { "prog_rate", 2.5f }, { "prog_depth", 0.22f }, { "prog_target", toFilter },
                { "noo_size", 0.35f }, { "noo_decay", 0.35f }, { "noo_mix", 0.15f },
                { "dark", 0.5f }, { "master_volume", -7.0f }, { "stereo_width", 0.2f } } });

            // 5. Grains of the psi storm over a 48 Hz root, radiation crackling.
            p.push_back ({ "Psi Storm", {
                { "osc_a_wave", sine }, { "osc_a_shape", 0.5f }, { "osc_a_level", 0.25f },
                { "osc_b_wave", bSine }, { "osc_b_ratio", 2.76f }, { "osc_b_fm", 0.3f }, { "osc_b_level", 0.12f },
                { "sub_level", 0.5f }, { "noise_level", 0.15f }, { "noise_color", 0.3f },
                { "geiger_density", 6.0f }, { "geiger_tone", 3200.0f },
                { "specimen_source", spec ("Psi Storm") }, { "specimen_level", 0.75f }, { "specimen_position", 0.3f },
                { "specimen_spray", 0.7f }, { "specimen_size", 90.0f }, { "specimen_density", 45.0f }, { "specimen_track", 1.0f },
                { "filter_type", lowpass }, { "filter_cutoff", 2400.0f }, { "filter_res", 0.4f }, { "filter_drive", 10.0f },
                { "filter_env", 0.3f }, { "filter_keytrack", 0.3f },
                { "env1_attack", 0.6f }, { "env1_decay", 2.0f }, { "env1_sustain", 0.9f }, { "env1_release", 3.0f },
                { "env2_attack", 1.0f }, { "env2_decay", 2.5f }, { "env2_sustain", 0.5f }, { "env2_release", 3.0f },
                { "prog_rate", 0.6f }, { "prog_depth", 0.5f }, { "prog_target", toAll },
                { "noo_size", 0.6f }, { "noo_decay", 0.62f }, { "noo_mix", 0.35f },
                { "dark", 0.6f }, { "master_volume", -11.5f } } });

            // 6. Inharmonic mid cluster, whispering formants and the controller's 3.1 kHz whine.
            p.push_back ({ "Controller Presence", {
                { "osc_a_wave", subtle }, { "osc_a_shape", 0.5f }, { "osc_a_level", 0.45f },
                { "osc_b_wave", bSine }, { "osc_b_ratio", 2.76f }, { "osc_b_fm", 0.25f }, { "osc_b_level", 0.15f },
                { "sub_level", 0.2f },
                { "specimen_source", spec ("Controller Aura") }, { "specimen_level", 0.4f }, { "specimen_position", 0.3f },
                { "specimen_spray", 0.3f }, { "specimen_size", 150.0f }, { "specimen_density", 30.0f }, { "specimen_track", 0.5f },
                { "filter_type", bandpass }, { "filter_cutoff", 800.0f }, { "filter_res", 0.45f }, { "filter_drive", 6.0f },
                { "filter_env", 0.1f }, { "filter_keytrack", 0.4f },
                { "env1_attack", 1.5f }, { "env1_decay", 2.0f }, { "env1_sustain", 0.9f }, { "env1_release", 3.5f },
                { "env2_attack", 2.0f }, { "env2_decay", 2.0f }, { "env2_sustain", 0.7f }, { "env2_release", 3.0f },
                { "prog_rate", 0.3f }, { "prog_depth", 0.35f }, { "prog_target", toFormant },
                { "whisper_level", 0.25f }, { "whisper_formant", 127.0f }, { "whisper_keytrack", 0.4f },
                { "presence_level", 0.35f }, { "presence_freq", 3100.0f },
                { "noo_size", 0.75f }, { "noo_decay", 0.7f }, { "noo_mix", 0.4f },
                { "dark", 0.35f }, { "master_volume", -7.5f } } });

            // 7. Play F4 for the 350 / 700 / 1050 Hz siren triad; the Programmer bends it like the real warning.
            p.push_back ({ "Blowout Siren", {
                { "voices", 1.0f }, { "glide", 0.8f },
                { "osc_a_wave", saw }, { "osc_a_shape", 0.4f }, { "osc_a_level", 0.45f },
                { "osc_b_wave", bSine }, { "osc_b_ratio", 3.0f }, { "osc_b_fm", 0.08f }, { "osc_b_level", 0.35f },
                { "sub_level", 0.15f },
                { "specimen_source", spec ("Blowout Siren") }, { "specimen_level", 0.45f }, { "specimen_position", 0.25f },
                { "specimen_spray", 0.25f }, { "specimen_size", 220.0f }, { "specimen_density", 18.0f }, { "specimen_track", 1.0f },
                { "filter_type", bandpass }, { "filter_cutoff", 1000.0f }, { "filter_res", 0.55f }, { "filter_drive", 9.0f },
                { "filter_env", 0.0f }, { "filter_keytrack", 0.8f },
                { "env1_attack", 0.8f }, { "env1_decay", 1.0f }, { "env1_sustain", 1.0f }, { "env1_release", 3.0f },
                { "prog_rate", 0.12f }, { "prog_depth", 0.85f }, { "prog_target", toPitch },
                { "noo_size", 0.9f }, { "noo_decay", 0.72f }, { "noo_mix", 0.4f },
                { "dark", 0.35f }, { "master_volume", -7.0f }, { "stereo_width", 0.5f } } });

            // 8. Dense detector crackle over a thin, bright bed.
            p.push_back ({ "Geiger Rain", {
                { "geiger_density", 30.0f }, { "geiger_tone", 3000.0f },
                { "noise_level", 0.08f }, { "noise_color", 0.75f },
                { "osc_a_wave", sine }, { "osc_a_shape", 0.1f }, { "osc_a_level", 0.2f }, { "sub_level", 0.3f },
                { "specimen_source", spec ("Geiger") }, { "specimen_level", 0.3f }, { "specimen_position", 0.5f },
                { "specimen_spray", 1.0f }, { "specimen_size", 40.0f }, { "specimen_density", 60.0f }, { "specimen_track", 0.0f },
                { "filter_type", lowpass }, { "filter_cutoff", 7000.0f }, { "filter_res", 0.15f }, { "filter_drive", 3.0f },
                { "filter_env", 0.0f }, { "filter_keytrack", 0.0f },
                { "env1_attack", 0.01f }, { "env1_decay", 0.5f }, { "env1_sustain", 0.8f }, { "env1_release", 1.5f },
                { "scrub_bits", 11.0f },
                { "prog_rate", 0.4f }, { "prog_depth", 0.2f }, { "prog_target", toFilter },
                { "noo_size", 0.3f }, { "noo_decay", 0.3f }, { "noo_mix", 0.2f },
                { "dark", 0.4f }, { "master_volume", -5.5f } } });

            // 9. 86 Hz mains hum, a 2x partial and the transformer recording: the facility's heartbeat.
            p.push_back ({ "Underground Hum", {
                { "osc_a_wave", sine }, { "osc_a_shape", 0.1f }, { "osc_a_level", 0.6f },
                { "osc_b_wave", bSine }, { "osc_b_ratio", 2.0f }, { "osc_b_fm", 0.05f }, { "osc_b_level", 0.25f },
                { "sub_level", 0.5f }, { "noise_level", 0.03f }, { "noise_color", 0.15f },
                { "specimen_source", spec ("Transformer") }, { "specimen_level", 0.4f }, { "specimen_position", 0.3f },
                { "specimen_spray", 0.2f }, { "specimen_size", 400.0f }, { "specimen_density", 8.0f }, { "specimen_track", 0.0f },
                { "filter_type", lowpass }, { "filter_cutoff", 320.0f }, { "filter_res", 0.2f }, { "filter_drive", 8.0f },
                { "filter_env", 0.05f }, { "filter_keytrack", 0.3f },
                { "env1_attack", 0.8f }, { "env1_decay", 1.0f }, { "env1_sustain", 1.0f }, { "env1_release", 2.5f },
                { "prog_rate", 0.05f }, { "prog_depth", 0.15f }, { "prog_target", toAll },
                { "noo_size", 0.45f }, { "noo_decay", 0.4f }, { "noo_mix", 0.2f },
                { "dark", 0.3f }, { "master_volume", -9.0f } } });

            // 10. Bit-starved plucks with shredded static: the 2008 database purge.
            p.push_back ({ "Scrubbed 2008", {
                { "osc_a_wave", square }, { "osc_a_shape", 0.2f }, { "osc_a_level", 0.6f },
                { "osc_b_wave", bSquare }, { "osc_b_ratio", 1.5f }, { "osc_b_fm", 0.4f }, { "osc_b_level", 0.2f },
                { "sub_level", 0.2f },
                { "specimen_source", spec ("Static") }, { "specimen_level", 0.3f }, { "specimen_position", 0.4f },
                { "specimen_spray", 0.8f }, { "specimen_size", 30.0f }, { "specimen_density", 50.0f }, { "specimen_track", 0.7f },
                { "filter_type", lowpass }, { "filter_cutoff", 3200.0f }, { "filter_res", 0.3f }, { "filter_drive", 6.0f },
                { "filter_env", 0.35f }, { "filter_keytrack", 0.5f },
                { "env1_attack", 0.002f }, { "env1_decay", 0.35f }, { "env1_sustain", 0.2f }, { "env1_release", 0.45f },
                { "env2_attack", 0.001f }, { "env2_decay", 0.25f }, { "env2_sustain", 0.1f }, { "env2_release", 0.3f },
                { "scrub_bits", 5.0f }, { "scrub_rate", 8.0f },
                { "prog_rate", 1.2f }, { "prog_depth", 0.15f }, { "prog_target", toSpecimen },
                { "noo_size", 0.4f }, { "noo_decay", 0.4f }, { "noo_mix", 0.25f },
                { "dark", 0.4f }, { "master_volume", -5.5f } } });

            // 11. Heavy, slightly unstable brass-like saw stack: Soviet-era machinery.
            p.push_back ({ "MDST Black Site", {
                { "osc_a_wave", saw }, { "osc_a_shape", 0.4f }, { "osc_a_level", 0.7f },
                { "osc_b_wave", bSaw }, { "osc_b_ratio", 0.5f }, { "osc_b_fm", 0.0f }, { "osc_b_level", 0.25f },
                { "sub_level", 0.35f }, { "noise_level", 0.02f },
                { "filter_type", lowpass }, { "filter_cutoff", 800.0f }, { "filter_res", 0.35f }, { "filter_drive", 10.0f },
                { "filter_env", 0.45f }, { "filter_keytrack", 0.6f },
                { "env1_attack", 0.05f }, { "env1_decay", 1.2f }, { "env1_sustain", 0.7f }, { "env1_release", 1.0f },
                { "env2_attack", 0.08f }, { "env2_decay", 0.9f }, { "env2_sustain", 0.35f }, { "env2_release", 1.0f },
                { "prog_rate", 0.1f }, { "prog_depth", 0.15f }, { "prog_target", toPitch },
                { "whisper_level", 0.1f }, { "whisper_formant", 140.0f }, { "whisper_keytrack", 0.5f },
                { "noo_size", 0.5f }, { "noo_decay", 0.5f }, { "noo_mix", 0.25f },
                { "dark", 0.35f }, { "master_volume", -9.0f } } });

            // 12. Almost all texture: long grains of the blowout particle wave drifting through a huge space.
            p.push_back ({ "Cordon Ruin", {
                { "osc_a_level", 0.0f }, { "osc_b_level", 0.0f }, { "sub_level", 0.25f },
                { "noise_level", 0.08f }, { "noise_color", 0.4f },
                { "geiger_density", 1.5f },
                { "specimen_source", spec ("Particle Wave") }, { "specimen_level", 0.8f }, { "specimen_position", 0.5f },
                { "specimen_spray", 0.6f }, { "specimen_size", 500.0f }, { "specimen_density", 6.0f }, { "specimen_track", 0.3f },
                { "filter_type", lowpass }, { "filter_cutoff", 3000.0f }, { "filter_res", 0.25f }, { "filter_drive", 5.0f },
                { "filter_env", 0.0f }, { "filter_keytrack", 0.0f },
                { "env1_attack", 1.5f }, { "env1_decay", 2.0f }, { "env1_sustain", 1.0f }, { "env1_release", 5.0f },
                { "prog_rate", 0.07f }, { "prog_depth", 0.4f }, { "prog_target", toSpecimen },
                { "noo_size", 1.0f }, { "noo_decay", 0.8f }, { "noo_mix", 0.5f },
                { "dark", 0.5f }, { "master_volume", -7.5f } } });

            return p;
        }
    }

    const std::vector<Preset>& factoryPresets()
    {
        static const std::vector<Preset> presets = build();
        return presets;
    }
}
