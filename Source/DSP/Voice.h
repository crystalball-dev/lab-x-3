#pragma once

#include "Common.h"
#include "Envelope.h"
#include "Granular.h"
#include "Programmer.h"
#include "SVF.h"

namespace labx3::dsp
{
    enum class ProgTarget { pitch = 0, filter, formant, specimen, all };

    // Control-rate snapshot of everything a voice needs. Built by the processor every control block.
    struct VoiceParams
    {
        int   oscAWave = 0;
        float oscAOctave = 0.0f, oscADetune = 0.0f, oscAShape = 0.3f, oscALevel = 0.8f;
        int   oscBWave = 0;
        float oscBRatio = 2.0f, oscBFm = 0.0f, oscBLevel = 0.0f;
        float subLevel = 0.3f, noiseLevel = 0.0f, noiseColor = 0.5f;
        float geigerDensity = 0.0f, geigerTone = 3000.0f;

        float specLevel = 0.0f, specPosition = 0.3f, specSpray = 0.2f, specSize = 0.12f, specDensity = 24.0f, specTrack = 1.0f, specTune = 0.0f;
        const SpecimenData* specimen = nullptr;

        int   filterType = 0;
        float cutoff = 1200.0f, resonance = 0.25f, driveDb = 3.0f, filterEnv = 0.3f, keytrack = 0.5f;

        float a1 = 0.01f, d1 = 0.3f, s1 = 0.8f, r1 = 0.6f;
        float a2 = 0.01f, d2 = 0.6f, s2 = 0.3f, r2 = 0.8f;

        float progRate = 0.15f, progDepth = 0.2f;
        int   progTarget = 4;

        float whisperLevel = 0.0f, whisperFormant = 127.0f, whisperKeytrack = 0.5f;
        float presenceLevel = 0.0f, presenceFreq = 3100.0f;

        float dark = 0.0f, glide = 0.0f, width = 0.6f, pitchBend = 0.0f;
        bool panEnabled = true;
    };

    class Voice
    {
    public:
        void prepare (float newSampleRate, uint32_t seedValue) noexcept
        {
            sampleRate = newSampleRate;
            rng.seed (seedValue * 2654435761u + 12345u);
            programmer.seed (seedValue * 40503u + 7u);
            programmer.reset (sampleRate);
            grains.seed (seedValue * 69069u + 1u);
            grains.prepare (sampleRate);
            env1.setSampleRate (sampleRate);
            env2.setSampleRate (sampleRate);
            clickDecay = std::exp (-1.0f / (0.00035f * sampleRate));
            kill();
        }

        void noteOn (int note, float velocity, float startPitch, bool glideFromStart, uint64_t stamp) noexcept
        {
            const bool wasActive = isActive();
            noteNumber = note;
            vel = std::clamp (velocity, 0.0f, 1.0f);
            keyDown = true;
            sustained = false;
            targetPitch = (float) note;
            pitch = glideFromStart ? startPitch : (float) note;

            if (! wasActive)
            {
                phaseA = rng.uni();
                phaseA2 = rng.uni();
                phaseB = rng.uni();
                phaseSub = 0.0f;
                fb1 = fb2 = 0.0f;
                pmPrev = 0.0f;
                presPhase1 = rng.uni();
                presPhase2 = rng.uni();
                vibPhase = rng.uni();
                filter.reset();
                geigerBp.reset();
                for (auto& w : whisperBp)
                    w.reset();
                noiseLp = noiseHp = 0.0f;
                clickEnv = 0.0f;
                gPrev = -1.0f;
                freshStart = true;
                grains.clear();
                programmer.reset (sampleRate);
            }

            panRandom = rng.bi();
            darkDetune = rng.bi();
            presenceDetune = rng.bi();
            age = stamp;
            grains.noteStarted();
            env1.noteOn();
            env2.noteOn();
        }

        // Mono legato: change pitch target without retriggering the envelopes.
        void legatoTo (int note) noexcept
        {
            noteNumber = note;
            targetPitch = (float) note;
            keyDown = true;
            sustained = false;
        }

        void noteOff() noexcept              { keyDown = false; env1.noteOff(); env2.noteOff(); }
        void setSustained (bool s) noexcept  { sustained = s; }
        void kill() noexcept                 { env1.kill(); env2.kill(); keyDown = false; sustained = false; }

        bool isActive() const noexcept       { return env1.isActive(); }
        bool isKeyDown() const noexcept      { return keyDown; }
        bool isSustained() const noexcept    { return sustained; }
        bool isReleasing() const noexcept    { return env1.isReleasing(); }
        int getNote() const noexcept         { return noteNumber; }
        float getPitch() const noexcept      { return pitch; }
        float getLevel() const noexcept      { return env1.getValue(); }
        uint64_t getAge() const noexcept     { return age; }

        int takeClicks() noexcept
        {
            const int c = clicks;
            clicks = 0;
            return c;
        }

        void render (float* outL, float* outR, int n, const VoiceParams& p) noexcept
        {
            env1.setParameters (p.a1, p.d1, p.s1, p.r1);
            env2.setParameters (p.a2, p.d2, p.s2, p.r2);

            // Portamento
            if (p.glide > 0.0005f)
                pitch += (targetPitch - pitch) * (1.0f - std::exp (-(float) n * 3.0f / (p.glide * sampleRate)));
            else
                pitch = targetPitch;

            // Programmer drift
            const float prog = programmer.advance (n, p.progRate) * p.progDepth;
            const bool toAll = p.progTarget == (int) ProgTarget::all;
            const float progPitch   = (toAll || p.progTarget == (int) ProgTarget::pitch)    ? prog         : 0.0f;   // semitones
            const float progFilter  = (toAll || p.progTarget == (int) ProgTarget::filter)   ? prog * 2.0f  : 0.0f;   // octaves
            const float progFormant = (toAll || p.progTarget == (int) ProgTarget::formant)  ? prog         : 0.0f;   // octaves
            const float progSpec    = (toAll || p.progTarget == (int) ProgTarget::specimen) ? prog * 0.35f : 0.0f;   // position

            const float basePitch = pitch + p.pitchBend + progPitch + darkDetune * p.dark * 0.25f;

            // Oscillator increments, interpolated across the block
            const float maxInc = 0.45f;
            const float incAEnd   = std::min (midiToHz (basePitch + 12.0f * p.oscAOctave + p.oscADetune * 0.01f) / sampleRate, maxInc);
            const float incBEnd   = std::min (midiToHz (basePitch) * p.oscBRatio / sampleRate, maxInc);
            const float incSubEnd = std::min (midiToHz (basePitch - 12.0f) / sampleRate, maxInc);

            if (freshStart)
            {
                incA = incAEnd;
                incB = incBEnd;
                incSub = incSubEnd;
            }

            const float invN = 1.0f / (float) n;
            const float dIncA = (incAEnd - incA) * invN;
            const float dIncB = (incBEnd - incB) * invN;
            const float dIncSub = (incSubEnd - incSub) * invN;

            const float shape = p.oscAShape;
            const float unisonRatio = semisToRatio (shape * 0.25f);
            const float fmDepth = p.oscBFm * p.oscBFm * 1.2f;   // phase deviation in cycles

            // Filter envelope is evaluated for the whole block; the cutoff follows it at control rate.
            float e2 = env2.getValue();
            for (int i = 0; i < n; ++i)
                e2 = env2.next();

            const float velScale = 0.55f + 0.45f * vel;
            const float cutoffOct = e2 * p.filterEnv * 6.0f * velScale
                                  + p.keytrack * (basePitch - 60.0f) * (1.0f / 12.0f)
                                  + progFilter
                                  - p.dark;
            const float cutoffHz = std::clamp (p.cutoff * std::exp2 (cutoffOct), 20.0f, sampleRate * 0.45f);
            const float gEnd = SVF::gFor (cutoffHz, sampleRate);
            if (gPrev < 0.0f)
                gPrev = gEnd;
            const float dG = (gEnd - gPrev) * invN;
            float g = gPrev;
            const float k = 2.0f - 1.97f * std::clamp (p.resonance, 0.0f, 1.0f);
            const int ftype = p.filterType;

            // Full-scale signals keep their level as drive rises; quieter material gets compressed upward.
            const float driveGain = dbToGain (p.driveDb);
            const float driveComp = 1.0f / std::tanh (driveGain);

            // Noise colour: low-passed below the midpoint, high-passed above it
            const bool colourHigh = p.noiseColor >= 0.5f;
            float nCoef, nGain;
            if (! colourHigh)
            {
                const float hz = 120.0f * std::exp2 (p.noiseColor * 2.0f * 7.4f);
                nCoef = onePoleCoefHz (std::min (hz, sampleRate * 0.45f), sampleRate);
                nGain = std::clamp (std::sqrt (12000.0f / hz), 1.0f, 6.0f);
            }
            else
            {
                const float hz = 30.0f * std::exp2 ((p.noiseColor - 0.5f) * 2.0f * 7.0f);
                nCoef = onePoleCoefHz (hz, sampleRate);
                nGain = 1.0f + (p.noiseColor - 0.5f);
            }

            // Geiger
            const float clickProb = p.geigerDensity / sampleRate;
            geigerBp.set (SVF::gFor (p.geigerTone, sampleRate), 0.33f);

            // Whisper formants: three resonances at 1:2:3, like the psy voices
            const bool useWhisper = p.whisperLevel > 0.0001f;
            if (useWhisper)
            {
                const float f0 = p.whisperFormant * std::exp2 (p.whisperKeytrack * (basePitch - 60.0f) * (1.0f / 12.0f) + progFormant);
                for (int b = 0; b < 3; ++b)
                    whisperBp[(size_t) b].set (SVF::gFor (f0 * (float) (b + 1), sampleRate), 1.0f / 12.0f);
            }

            // Presence whine
            const bool usePresence = p.presenceLevel > 0.0001f;
            const float presInc = std::min (p.presenceFreq * semisToRatio (presenceDetune * 0.1f) / sampleRate, maxInc);
            const float vibInc = 5.3f / sampleRate;

            // Specimen grains
            const bool useSpec = p.specLevel > 0.0001f && p.specimen != nullptr;
            GrainCloud::Settings gs;
            gs.positionNorm = std::clamp (p.specPosition + progSpec, 0.0f, 1.0f);
            gs.spray = std::clamp (p.specSpray + p.dark * 0.3f, 0.0f, 1.0f);
            gs.sizeSeconds = p.specSize;
            gs.density = p.specDensity;
            gs.rate = std::exp2 ((p.specTrack * (basePitch - 60.0f) + p.specTune) * (1.0f / 12.0f));

            // Equal-power pan: each note gets a random position scaled by WIDTH, or the centre when panning is off.
            const float panTarget = p.panEnabled ? std::clamp (panRandom * p.width, -1.0f, 1.0f) : 0.0f;
            panSmoothed = freshStart ? panTarget : panSmoothed + (panTarget - panSmoothed) * 0.05f;
            const float panL = sin2pi ((panSmoothed + 1.0f) * 0.125f + 0.25f);
            const float panR = sin2pi ((panSmoothed + 1.0f) * 0.125f);
            const float outGain = (0.35f + 0.65f * vel) * 0.7f;

            int clickCount = 0;

            for (int i = 0; i < n; ++i)
            {
                incA += dIncA;
                incB += dIncB;
                incSub += dIncSub;
                g += dG;

                // OSC B (also the FM modulator)
                phaseB += incB;
                phaseB -= std::floor (phaseB);
                float b;
                switch (p.oscBWave)
                {
                    case 0:  b = sin2pi (phaseB); break;
                    case 1:  b = 1.0f - 4.0f * std::abs (phaseB - 0.5f); break;
                    case 2:  b = 2.0f * phaseB - 1.0f - polyBlep (phaseB, incB); break;
                    default:
                    {
                        b = phaseB < 0.5f ? 1.0f : -1.0f;
                        b += polyBlep (phaseB, incB);
                        b -= polyBlep (wrap01 (phaseB + 0.5f), incB);
                        break;
                    }
                }

                // OSC A, phase-modulated by OSC B
                const float pm = fmDepth * b;
                const float dt = std::clamp (std::abs (incA + (pm - pmPrev)), 1.0e-6f, 0.5f);
                pmPrev = pm;
                phaseA += incA;
                phaseA -= std::floor (phaseA);
                const float t = wrap01 (phaseA + pm);

                float a;
                switch (p.oscAWave)
                {
                    case 0:   // saw with a detuned partner; shape sets the detune and blend
                    {
                        phaseA2 += incA * unisonRatio;
                        phaseA2 -= std::floor (phaseA2);
                        const float t2 = wrap01 (phaseA2 + pm);
                        const float s1 = 2.0f * t - 1.0f - polyBlep (t, dt);
                        const float s2 = 2.0f * t2 - 1.0f - polyBlep (t2, std::min (dt * unisonRatio, 0.5f));
                        a = (s1 + shape * s2) / (1.0f + 0.6f * shape);
                        break;
                    }
                    case 1:   // pulse; shape narrows the width
                    {
                        const float pw = 0.5f - 0.45f * shape;
                        float s = t < pw ? 1.0f : -1.0f;
                        s += polyBlep (t, dt);
                        s -= polyBlep (wrap01 (t - pw + 1.0f), dt);
                        a = s - (2.0f * pw - 1.0f);
                        break;
                    }
                    case 2:   // sine with wavefolding
                    {
                        const float s = sin2pi (t);
                        const float folded = sin2pi (0.25f * (1.0f + 4.0f * shape) * s);
                        a = s + (folded - s) * shape;
                        break;
                    }
                    default:  // "subtle matter": feedback-FM sine
                    {
                        const float beta = shape * 1.5f;
                        const float s = sin2pi (t + beta * 0.5f * (fb1 + fb2) * (1.0f / twoPi));
                        fb2 = fb1;
                        fb1 = s;
                        a = s;
                        break;
                    }
                }

                // SUB
                phaseSub += incSub;
                phaseSub -= std::floor (phaseSub);
                const float sub = sin2pi (phaseSub);

                // NOISE
                const float white = rng.bi();
                float noise;
                if (! colourHigh)
                {
                    noiseLp += (white - noiseLp) * nCoef;
                    noise = noiseLp * nGain;
                }
                else
                {
                    noiseHp += (white - noiseHp) * nCoef;
                    noise = (white - noiseHp) * nGain;
                }

                // WHISPER
                float whisper = 0.0f;
                if (useWhisper)
                {
                    const float w = rng.bi();
                    whisper = (whisperBp[0].bandpass (w)
                             + 0.6f  * whisperBp[1].bandpass (w)
                             + 0.35f * whisperBp[2].bandpass (w)) * 0.9f;
                }

                // SPECIMEN
                const float spec = useSpec ? grains.process (p.specimen, gs) * 2.0f : 0.0f;

                const float mix = a * p.oscALevel + b * p.oscBLevel + sub * p.subLevel
                                + noise * p.noiseLevel + whisper * p.whisperLevel + spec * p.specLevel;

                // Drive into the filter
                const float x = fastTanh (mix * driveGain) * driveComp;
                filter.set (g, k);
                float lp, bp, hp;
                filter.process (x, lp, bp, hp);
                const float y = ftype == 0 ? lp : (ftype == 1 ? bp * k * 1.4f : hp);

                // Post-filter layers: geiger clicks and the presence whine
                float post = 0.0f;
                if (clickProb > 0.0f && rng.uni() < clickProb)
                {
                    clickEnv = 0.5f + 0.5f * rng.uni();
                    ++clickCount;
                }
                if (clickEnv > 1.0e-4f)
                {
                    post += geigerBp.bandpass (rng.bi() * clickEnv) * 0.9f;
                    clickEnv *= clickDecay;
                }

                if (usePresence)
                {
                    vibPhase += vibInc;
                    vibPhase -= std::floor (vibPhase);
                    const float vib = 1.0f + 0.004f * sin2pi (vibPhase);
                    presPhase1 += presInc * vib;
                    presPhase1 -= std::floor (presPhase1);
                    presPhase2 += std::min (presInc * 2.0f * vib, maxInc);
                    presPhase2 -= std::floor (presPhase2);
                    post += (sin2pi (presPhase1) + 0.35f * sin2pi (presPhase2)) * p.presenceLevel * 0.18f;
                }

                const float out = (y + post) * env1.next() * outGain;
                outL[i] += out * panL;
                outR[i] += out * panR;
            }

            gPrev = gEnd;
            incA = incAEnd;
            incB = incBEnd;
            incSub = incSubEnd;
            freshStart = false;
            clicks += clickCount;
        }

    private:
        Envelope env1, env2;
        Programmer programmer;
        GrainCloud grains;
        SVF filter, geigerBp;
        std::array<SVF, 3> whisperBp {};
        Rng rng;

        float sampleRate = 48000.0f;
        int noteNumber = 60;
        float vel = 1.0f, pitch = 60.0f, targetPitch = 60.0f;
        bool keyDown = false, sustained = false, freshStart = true;
        uint64_t age = 0;

        float phaseA = 0.0f, phaseA2 = 0.0f, phaseB = 0.0f, phaseSub = 0.0f;
        float incA = 0.0f, incB = 0.0f, incSub = 0.0f;
        float fb1 = 0.0f, fb2 = 0.0f, pmPrev = 0.0f;
        float noiseLp = 0.0f, noiseHp = 0.0f;
        float clickEnv = 0.0f, clickDecay = 0.99f;
        float presPhase1 = 0.0f, presPhase2 = 0.0f, vibPhase = 0.0f;
        float panRandom = 0.0f, panSmoothed = 0.0f, darkDetune = 0.0f, presenceDetune = 0.0f;
        float gPrev = -1.0f;
        int clicks = 0;
    };
}
