#pragma once

#include "Common.h"

namespace labx3::dsp
{
    // The "Programmer": a slow, irregular random walk. It hops to new random targets at roughly
    // `rate` Hz and glides toward them, so held notes keep being quietly rewritten.
    class Programmer
    {
    public:
        void seed (uint32_t s) noexcept { rng.seed (s); }

        void reset (float sr) noexcept
        {
            sampleRate = sr;
            value = 0.0f;
            target = rng.bi() * 0.5f;
            countdown = 0.0f;
        }

        // Advances by n samples and returns the current value in [-1, 1].
        float advance (int n, float rateHz) noexcept
        {
            rateHz = std::max (rateHz, 0.005f);
            countdown -= (float) n;
            if (countdown <= 0.0f)
            {
                target = rng.bi() * (0.55f + 0.45f * rng.uni());
                countdown = sampleRate / rateHz * (0.5f + rng.uni());
            }
            const float coef = 1.0f - std::exp (-(float) n * rateHz * 2.0f / sampleRate);
            value += (target - value) * coef;
            return value;
        }

    private:
        Rng rng;
        float sampleRate = 48000.0f, value = 0.0f, target = 0.0f, countdown = 0.0f;
    };
}
