#pragma once

#include <vector>
#include "Common.h"

namespace labx3::dsp
{
    // NOOSPHERE: an 8-line feedback delay network with Hadamard mixing, damping and slow
    // delay modulation. Size stretches every line; decay sets the RT60; damping darkens the tail.
    class Noosphere
    {
    public:
        static constexpr int numLines = 8;

        void prepare (float sr)
        {
            sampleRate = sr;
            const int maxLen = (int) std::ceil (sr * 0.205f) + 16;   // longest line at maximum size plus modulation
            for (auto& line : lines)
            {
                line.buffer.assign ((size_t) maxLen, 0.0f);
                line.write = 0;
                line.lp = 0.0f;
            }
            bufferLen = maxLen;
            scale = scaleTarget;
            mix = mixTarget;
            lfoPhase1 = 0.0f;
            lfoPhase2 = 0.37f;
        }

        void reset()
        {
            for (auto& line : lines)
            {
                std::fill (line.buffer.begin(), line.buffer.end(), 0.0f);
                line.lp = 0.0f;
            }
        }

        // size, decay, damping: 0..1. Call at control rate.
        void setParameters (float size, float decay, float damping, float newMix) noexcept
        {
            scaleTarget = 0.3f + 2.2f * std::clamp (size, 0.0f, 1.0f);
            rt60 = 0.3f * std::exp2 (std::clamp (decay, 0.0f, 1.0f) * 6.0f);
            dampCoef = onePoleCoefHz (9000.0f * std::exp2 (-std::clamp (damping, 0.0f, 1.0f) * 2.6f), sampleRate);
            mixTarget = std::clamp (newMix, 0.0f, 1.0f);
        }

        void process (float* left, float* right, int n) noexcept
        {
            if (bufferLen == 0)
                return;

            if (mixTarget < 1.0e-4f && mix < 1.0e-4f)
            {
                // Fully dry for a second: clear the network and stop processing until it is needed again.
                if (asleep)
                    return;
                quietCounter += n;
                if (quietCounter > (int) sampleRate)
                {
                    reset();
                    asleep = true;
                    return;
                }
            }
            else
            {
                quietCounter = 0;
                asleep = false;
            }

            // Per-line feedback gains for the current size, recomputed once per call.
            float gains[numLines];
            for (int k = 0; k < numLines; ++k)
            {
                const float delaySamples = baseMs[k] * 0.001f * sampleRate * scaleTarget;
                gains[k] = std::pow (10.0f, -3.0f * delaySamples / (rt60 * sampleRate));
            }

            const float lfoInc1 = 0.13f / sampleRate, lfoInc2 = 0.21f / sampleRate;
            const float modDepth = 0.0006f * sampleRate;   // +/- 0.6 ms

            for (int i = 0; i < n; ++i)
            {
                scale += (scaleTarget - scale) * 0.0005f;
                mix += (mixTarget - mix) * 0.002f;
                lfoPhase1 = wrap01 (lfoPhase1 + lfoInc1);
                lfoPhase2 = wrap01 (lfoPhase2 + lfoInc2);
                const float m1 = sin2pi (lfoPhase1) * modDepth;
                const float m2 = sin2pi (lfoPhase2) * modDepth;

                float o[numLines];
                for (int k = 0; k < numLines; ++k)
                {
                    float d = baseMs[k] * 0.001f * sampleRate * scale;
                    if (k == 0 || k == 5) d += m1;
                    if (k == 2 || k == 7) d += m2;
                    o[k] = lines[(size_t) k].read (d, bufferLen);
                }

                const float wetL = o[0] - o[2] + o[4] - o[6];
                const float wetR = o[1] - o[3] + o[5] - o[7];

                float f[numLines];
                for (int k = 0; k < numLines; ++k)
                {
                    auto& line = lines[(size_t) k];
                    line.lp += (o[k] - line.lp) * dampCoef;
                    f[k] = line.lp * gains[k];
                }

                hadamard (f);

                const float inL = left[i] * 0.35f;
                const float inR = right[i] * 0.35f;
                for (int k = 0; k < numLines; ++k)
                    lines[(size_t) k].push ((k & 1) == 0 ? f[k] + inL : f[k] + inR, bufferLen);

                const float dryGain = sin2pi (mix * 0.25f + 0.25f);   // cos (mix * pi / 2)
                const float wetGain = sin2pi (mix * 0.25f) * 0.5f;    // sin (mix * pi / 2)
                left[i]  = left[i]  * dryGain + wetL * wetGain;
                right[i] = right[i] * dryGain + wetR * wetGain;
            }
        }

    private:
        struct Line
        {
            std::vector<float> buffer;
            int write = 0;
            float lp = 0.0f;

            float read (float delay, int len) const noexcept
            {
                delay = std::clamp (delay, 1.0f, (float) (len - 3));
                float pos = (float) write - delay;
                if (pos < 0.0f)
                    pos += (float) len;
                const int i0 = (int) pos;
                const int i1 = i0 + 1 < len ? i0 + 1 : 0;
                const float frac = pos - (float) i0;
                return buffer[(size_t) i0] + frac * (buffer[(size_t) i1] - buffer[(size_t) i0]);
            }

            void push (float x, int len) noexcept
            {
                buffer[(size_t) write] = x;
                if (++write >= len)
                    write = 0;
            }
        };

        static void hadamard (float* x) noexcept
        {
            for (int h = 1; h < numLines; h <<= 1)
                for (int i = 0; i < numLines; i += h << 1)
                    for (int j = i; j < i + h; ++j)
                    {
                        const float a = x[j], b = x[j + h];
                        x[j] = a + b;
                        x[j + h] = a - b;
                    }
            const float norm = 0.35355339f;   // 1 / sqrt (8)
            for (int i = 0; i < numLines; ++i)
                x[i] *= norm;
        }

        static constexpr float baseMs[numLines] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.7f, 73.1f };

        std::array<Line, numLines> lines {};
        int bufferLen = 0, quietCounter = 0;
        bool asleep = false;
        float sampleRate = 48000.0f;
        float scale = 1.4f, scaleTarget = 1.4f, rt60 = 2.4f, dampCoef = 0.5f;
        float mix = 0.25f, mixTarget = 0.25f;
        float lfoPhase1 = 0.0f, lfoPhase2 = 0.37f;
    };
}
