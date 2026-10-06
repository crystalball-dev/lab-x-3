#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace labx3::dsp
{
    inline constexpr float pi    = 3.14159265358979f;
    inline constexpr float twoPi = 6.28318530717959f;

    // 4096-point sine table with linear interpolation. Built once at load time.
    struct SineTable
    {
        static constexpr int size = 4096;
        std::array<float, size + 1> table {};

        SineTable()
        {
            for (int i = 0; i <= size; ++i)
                table[(size_t) i] = (float) std::sin (2.0 * 3.14159265358979323846 * (double) i / (double) size);
        }
    };

    inline const SineTable sineTable;

    inline float wrap01 (float x) noexcept { return x - std::floor (x); }

    // sin (2 * pi * phase) for any phase, measured in cycles.
    inline float sin2pi (float phase) noexcept
    {
        const float p = phase - std::floor (phase);
        const float idx = p * (float) SineTable::size;
        int i = (int) idx;
        if (i >= SineTable::size) i = SineTable::size - 1;
        const float frac = idx - (float) i;
        const float a = sineTable.table[(size_t) i];
        return a + frac * (sineTable.table[(size_t) i + 1] - a);
    }

    // xorshift32: cheap, allocation-free randomness for the audio thread.
    struct Rng
    {
        uint32_t state = 0x9E3779B9u;

        void seed (uint32_t v) noexcept { state = v != 0 ? v : 0x9E3779B9u; }

        uint32_t next() noexcept
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            return state;
        }

        float uni() noexcept { return (float) (next() >> 8) * (1.0f / 16777216.0f); }   // [0, 1)
        float bi() noexcept  { return uni() * 2.0f - 1.0f; }                               // [-1, 1)
    };

    inline float polyBlep (float t, float dt) noexcept
    {
        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0f;
        }
        if (t > 1.0f - dt)
        {
            t = (t - 1.0f) / dt;
            return t * t + t + t + 1.0f;
        }
        return 0.0f;
    }

    // 4-point Hermite interpolation. d[i - 1] .. d[i + 2] must be readable.
    inline float hermite (const float* d, double pos) noexcept
    {
        const int i = (int) pos;
        const float f = (float) (pos - (double) i);
        const float x0 = d[i - 1], x1 = d[i], x2 = d[i + 1], x3 = d[i + 2];
        const float c1 = 0.5f * (x2 - x0);
        const float c2 = x0 - 2.5f * x1 + 2.0f * x2 - 0.5f * x3;
        const float c3 = 0.5f * (x3 - x0) + 1.5f * (x1 - x2);
        return ((c3 * f + c2) * f + c1) * f + x1;
    }

    inline float dbToGain (float db) noexcept       { return std::pow (10.0f, db * 0.05f); }
    inline float semisToRatio (float s) noexcept    { return std::exp2 (s * (1.0f / 12.0f)); }
    inline float midiToHz (float note) noexcept     { return 440.0f * std::exp2 ((note - 69.0f) * (1.0f / 12.0f)); }

    // Pade tanh approximation; exact 1.0 with zero slope at |x| = 3.
    inline float fastTanh (float x) noexcept
    {
        x = std::clamp (x, -3.0f, 3.0f);
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    // Transparent below 0.8, rounds off smoothly to a ceiling of 1.0.
    inline float softClip (float x) noexcept
    {
        const float a = std::abs (x);
        if (a <= 0.8f)
            return x;
        const float y = 0.8f + 0.2f * fastTanh ((a - 0.8f) * 5.0f);
        return x < 0.0f ? -y : y;
    }

    inline float onePoleCoef (float tauSeconds, float sampleRate) noexcept
    {
        if (tauSeconds <= 0.0f)
            return 1.0f;
        return 1.0f - std::exp (-1.0f / (tauSeconds * sampleRate));
    }

    inline float onePoleCoefHz (float hz, float sampleRate) noexcept
    {
        return 1.0f - std::exp (-twoPi * hz / sampleRate);
    }
}
