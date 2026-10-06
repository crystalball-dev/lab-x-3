#pragma once

#include "Common.h"

namespace labx3::dsp
{
    // Topology-preserving state-variable filter (Zavalishin). Stable under per-sample modulation.
    struct SVF
    {
        float ic1 = 0.0f, ic2 = 0.0f;
        float g = 0.0f, k = 2.0f, a1 = 1.0f, a2 = 0.0f, a3 = 0.0f;

        void reset() noexcept { ic1 = ic2 = 0.0f; }

        void set (float newG, float newK) noexcept
        {
            g = newG;
            k = newK;
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }

        static float gFor (float hz, float sampleRate) noexcept
        {
            const float f = std::clamp (hz, 10.0f, sampleRate * 0.45f);
            return std::tan (pi * f / sampleRate);
        }

        void process (float v0, float& lp, float& bp, float& hp) noexcept
        {
            const float v3 = v0 - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            lp = v2;
            bp = v1;
            hp = v0 - k * v1 - v2;
        }

        // Band-pass whose peak gain is 1 / k.
        float bandpass (float v0) noexcept
        {
            float lp, bp, hp;
            process (v0, lp, bp, hp);
            return bp;
        }
    };
}
