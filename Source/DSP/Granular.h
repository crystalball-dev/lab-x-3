#pragma once

#include <vector>
#include "Common.h"

namespace labx3::dsp
{
    // Decoded, normalised mono copy of one source file. Guard zeros on both ends let the
    // Hermite reader run without bounds checks.
    struct SpecimenData
    {
        static constexpr int guard = 4;
        std::vector<float> storage;
        int length = 0;
        double sampleRate = 44100.0;
        uint64_t id = 0;   // unique per load, so a reused memory address is never mistaken for the same source

        const float* data() const noexcept { return storage.data() + guard; }
    };

    // Per-voice granular cloud reading from a SpecimenData buffer.
    class GrainCloud
    {
    public:
        static constexpr int maxGrains = 24;

        struct Settings
        {
            float positionNorm = 0.3f;   // 0..1, already modulated
            float spray = 0.2f;          // 0..1
            float sizeSeconds = 0.12f;
            float density = 24.0f;       // grains per second
            float rate = 1.0f;           // playback ratio from key tracking
        };

        void seed (uint32_t s) noexcept       { rng.seed (s); }
        void prepare (float sr) noexcept      { hostRate = sr; clear(); }
        void clear() noexcept                 { for (auto& g : grains) g.active = false; spawnPhase = 1.0f; }

        // New notes spawn a grain immediately. Grains already sounding fade out on their own window.
        void noteStarted() noexcept           { spawnPhase = 1.0f; }

        float process (const SpecimenData* sp, const Settings& s) noexcept
        {
            const uint64_t id = sp != nullptr ? sp->id : 0;
            if (id != lastId)
            {
                for (auto& g : grains)
                    g.active = false;
                lastId = id;
            }

            if (sp == nullptr || sp->length < 64)
                return 0.0f;

            spawnPhase += s.density / hostRate;
            if (spawnPhase >= 1.0f)
            {
                spawnPhase -= 1.0f + 0.35f * rng.uni();   // irregular spacing
                spawn (*sp, s);
            }

            float sum = 0.0f;
            const float* d = sp->data();
            const double maxPos = (double) sp->length - 2.0;

            for (auto& g : grains)
            {
                if (! g.active)
                    continue;

                const float w = sin2pi ((float) g.age * g.invLength * 0.5f);   // sin (pi * x)
                sum += hermite (d, g.pos) * (w * w) * g.amp;
                g.pos += g.inc;

                if (++g.age >= g.length || g.pos < 1.0 || g.pos >= maxPos)
                    g.active = false;
            }

            return sum;
        }

    private:
        struct Grain
        {
            double pos = 0.0, inc = 1.0;
            float amp = 0.0f, invLength = 0.0f;
            int length = 0, age = 0;
            bool active = false;
        };

        void spawn (const SpecimenData& sp, const Settings& s) noexcept
        {
            Grain* slot = nullptr;
            for (auto& g : grains)
            {
                if (! g.active)
                {
                    slot = &g;
                    break;
                }
            }
            if (slot == nullptr)
                return;

            const int len = std::max (32, (int) (s.sizeSeconds * hostRate));
            const float scatter = rng.bi() * s.spray * 0.12f;   // semitones
            double inc = (double) (s.rate * semisToRatio (scatter)) * sp.sampleRate / (double) hostRate;
            const bool reverse = rng.uni() < std::max (0.0f, s.spray - 0.5f) * 0.5f;
            const double span = inc * (double) len;
            const double usable = std::max (2.0, (double) sp.length - 4.0 - span);

            double start = (double) s.positionNorm * usable
                         + (double) (rng.bi() * s.spray) * 0.5 * (double) sp.length;
            start = std::clamp (start, 2.0, usable);

            if (reverse)
            {
                start = std::min (start + span, (double) sp.length - 3.0);
                inc = -inc;
            }

            slot->pos = start;
            slot->inc = inc;
            slot->length = len;
            slot->invLength = 1.0f / (float) len;
            slot->age = 0;

            const float overlap = std::clamp (s.density * s.sizeSeconds, 1.0f, (float) maxGrains);
            slot->amp = (0.75f + 0.25f * rng.uni()) / std::sqrt (overlap);
            slot->active = true;
        }

        std::array<Grain, maxGrains> grains {};
        uint64_t lastId = 0;
        Rng rng;
        float hostRate = 48000.0f, spawnPhase = 1.0f;
    };
}
