#pragma once

#include "Common.h"

namespace labx3::dsp
{
    // SCRUB: sample-and-hold downsampling plus bit-depth reduction. "The 2008 purge."
    class Scrub
    {
    public:
        void reset() noexcept
        {
            phase = 1.0f;
            heldL = heldR = 0.0f;
        }

        void process (float* left, float* right, int n, float bits, float factor) noexcept
        {
            const bool crush = bits < 15.95f;
            const bool hold = factor > 1.001f;
            if (! crush && ! hold)
                return;

            const float q = std::exp2 (std::clamp (bits, 1.0f, 16.0f) - 1.0f);
            const float invQ = 1.0f / q;
            const float step = 1.0f / std::max (factor, 1.0f);

            for (int i = 0; i < n; ++i)
            {
                float l = left[i], r = right[i];

                if (hold)
                {
                    phase += step;
                    if (phase >= 1.0f)
                    {
                        phase -= 1.0f;
                        heldL = l;
                        heldR = r;
                    }
                    l = heldL;
                    r = heldR;
                }

                if (crush)
                {
                    l = std::round (l * q) * invQ;
                    r = std::round (r * q) * invQ;
                }

                left[i] = l;
                right[i] = r;
            }
        }

    private:
        float phase = 1.0f, heldL = 0.0f, heldR = 0.0f;
    };
}
