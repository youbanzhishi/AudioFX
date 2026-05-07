#include "VCPluginDSP.h"

#include <algorithm>
#include <cmath>
#include <cstring>

//==============================================================================
// VCOscillator Implementation
//==============================================================================

VCOscillator::VCOscillator()
{
    for (int i = 0; i < MAX_UNISON; ++i) {
        mVoices[i].phase = (double)i / (double)MAX_UNISON;  // spread initial phases
        mVoices[i].phaseInc = 0.0;
        mVoices[i].lastValue = 0.0;
        mVoices[i].detuneRatio = 1.0f;
        mVoices[i].panL = 1.0f;
        mVoices[i].panR = 1.0f;
    }
}

void VCOscillator::setSampleRate(double sr)
{
    mSampleRate = sr;
    updatePhaseIncrements();
}

void VCOscillator::setOscType(VCOscType type)
{
    mType = type;
}

void VCOscillator::setFrequency(float freq)
{
    mFrequency = std::max(freq, 20.0f);
    updatePhaseIncrements();
}

void VCOscillator::setUnison(int voices)
{
    mUnison = std::clamp(voices, 1, MAX_UNISON);
    updatePhaseIncrements();
}

void VCOscillator::setDetune(float cents)
{
    mDetune = cents;
    updatePhaseIncrements();
}

void VCOscillator::updatePhaseIncrements()
{
    if (mUnison == 1) {
        // Single voice: no detune
        double freq = (double)mFrequency;
        mVoices[0].phaseInc = freq / mSampleRate;
        mVoices[0].detuneRatio = 1.0f;
        mVoices[0].panL = 1.0f;
        mVoices[0].panR = 1.0f;
        return;
    }

    // Distribute voices evenly across detune range
    // Voice 0 = center, voices spread symmetrically
    for (int i = 0; i < mUnison; ++i) {
        // Spread: voice i gets detune from -detune/2 to +detune/2
        float detuneOffset = 0.0f;
        if (mUnison > 1) {
            detuneOffset = ((float)i / (float)(mUnison - 1) - 0.5f) * mDetune;
        }
        // Convert cents to frequency ratio: ratio = 2^(cents/1200)
        float ratio = std::pow(2.0f, detuneOffset / 1200.0f);
        mVoices[i].detuneRatio = ratio;
        double freq = (double)mFrequency * (double)ratio;
        mVoices[i].phaseInc = freq / mSampleRate;

        // Stereo pan: spread voices across stereo field
        if (mUnison > 1) {
            float pan = (float)i / (float)(mUnison - 1);  // 0..1
            mVoices[i].panL = std::cos(pan * (float)VC_PI * 0.5f);
            mVoices[i].panR = std::sin(pan * (float)VC_PI * 0.5f);
        } else {
            mVoices[i].panL = 1.0f;
            mVoices[i].panR = 1.0f;
        }
    }
}

double VCOscillator::naiveWaveform(double phase) const
{
    // phase is 0..1
    switch (mType) {
    case VC_OSC_SINE:
        return std::sin(2.0 * VC_PI_D * phase);

    case VC_OSC_SAW:
        // Naive saw: -1 at phase=0, +1 at phase=1
        return 2.0 * phase - 1.0;

    case VC_OSC_SQUARE:
        // Naive square: +1 for phase < 0.5, -1 for phase >= 0.5
        return (phase < 0.5) ? 1.0 : -1.0;

    case VC_OSC_TRIANGLE:
        // Naive triangle
        if (phase < 0.25)
            return 4.0 * phase;
        else if (phase < 0.75)
            return 2.0 - 4.0 * phase;
        else
            return -4.0 + 4.0 * phase;

    case VC_OSC_NOISE:
        // White noise (random, no phase dependency)
        return 2.0 * ((double)rand() / (double)RAND_MAX) - 1.0;

    default:
        return 0.0;
    }
}

double VCOscillator::polyblep(double phase, double phaseInc, double boundaryValue) const
{
    // Polyblep correction: adds a tiny correction around discontinuities
    // boundaryValue is the jump at the discontinuity (e.g., +2 for saw, -2 for square)

    double correction = 0.0;

    // Check if we just crossed phase=0 (wrapping point)
    if (phase < phaseInc) {
        double t = phase / phaseInc;  // 0..1 how far past the boundary
        correction += boundaryValue * t * (t - 1.0) * 0.5;  // quadratic correction
    }

    // Check if we're about to cross phase=0 (at phase close to 1)
    if (phase > 1.0 - phaseInc) {
        double t = (phase - 1.0) / phaseInc;  // 0..1
        correction += boundaryValue * (t + 1.0) * t * 0.5;
    }

    // For square wave, also correct at phase=0.5
    if (mType == VC_OSC_SQUARE) {
        if (phase > 0.5 - phaseInc && phase < 0.5) {
            double t = (phase - 0.5) / phaseInc;
            correction -= boundaryValue * t * (t + 1.0) * 0.5;
        }
        if (phase >= 0.5 && phase < 0.5 + phaseInc) {
            double t = (phase - 0.5) / phaseInc;
            correction -= boundaryValue * t * (t - 1.0) * 0.5;
        }
    }

    return correction;
}

float VCOscillator::process()
{
    if (mType == VC_OSC_NOISE) {
        // Noise doesn't benefit from unison/polyblep — just average random values
        float sum = 0.0f;
        for (int i = 0; i < mUnison; ++i) {
            sum += (float)(2.0 * ((double)rand() / (double)RAND_MAX) - 1.0);
        }
        return sum / (float)mUnison;
    }

    float outL = 0.0f;
    float outR = 0.0f;

    for (int i = 0; i < mUnison; ++i) {
        UnisonVoice& v = mVoices[i];

        // Get naive waveform value
        double naive = naiveWaveform(v.phase);

        // Apply polyblep correction for band-limited waveforms
        double corrected = naive;
        if (mType == VC_OSC_SAW) {
            // Saw has a +2 jump at phase=0 (wrapping from +1 to -1)
            corrected += polyblep(v.phase, v.phaseInc, 2.0);
        } else if (mType == VC_OSC_SQUARE) {
            // Square has -2 jump at phase=0 and -2 jump at phase=0.5
            corrected += polyblep(v.phase, v.phaseInc, -2.0);
        }
        // Sine and triangle don't need polyblep (no discontinuities)

        v.lastValue = corrected;

        // Advance phase
        v.phase += v.phaseInc;
        while (v.phase >= 1.0) v.phase -= 1.0;

        // Accumulate with panning
        outL += (float)corrected * v.panL;
        outR += (float)corrected * v.panR;
    }

    // Normalize by unison count
    float scale = 1.0f / (float)mUnison;

    // Return mono mix (average of L and R)
    return (outL + outR) * scale * 0.5f;
}

void VCOscillator::reset()
{
    for (int i = 0; i < MAX_UNISON; ++i) {
        mVoices[i].phase = (double)i / (double)MAX_UNISON;
        mVoices[i].lastValue = 0.0;
    }
}

//==============================================================================
// VCMoogFilter Implementation
// Based on Antti Huovilainen's improved Moog model
//==============================================================================

VCMoogFilter::VCMoogFilter()
{
    updateCoefficients();
}

void VCMoogFilter::setSampleRate(double sr)
{
    mSampleRate = sr;
    updateCoefficients();
}

void VCMoogFilter::setCutoff(float hz)
{
    mCutoff = std::clamp(hz, 20.0f, 20000.0f);
    updateCoefficients();
}

void VCMoogFilter::setResonance(float res)
{
    mResonance = std::clamp(res, 0.0f, 1.0f);
    updateCoefficients();
}

void VCMoogFilter::setFilterType(VCFilterType type)
{
    mFilterType = type;
}

void VCMoogFilter::updateCoefficients()
{
    // Huovilainen's Moog ladder model
    // Each stage: y[n] = y[n-1] + alpha * (tanh(x) - tanh(y[n-1]))
    // alpha = 1 - exp(-2*pi*cutoff/sampleRate)

    float cutoffClamped = std::clamp(mCutoff, 20.0f, (float)(mSampleRate * 0.49));
    mAlpha = 1.0f - std::exp(-2.0f * (float)VC_PI * cutoffClamped / (float)mSampleRate);

    // Resonance: k ranges from 0 to ~4 (self-oscillation at 4)
    mK = 4.0f * mResonance;
}

float VCMoogFilter::process(float input)
{
    // Huovilainen's Moog ladder model
    // Each stage: y += alpha * (tanh(x) - tanh(y))
    // Feedback from 4th stage output (previous sample)
    float fb = input - mK * mStage[3];

    // Process 4 stages sequentially (zero-delay feedback variant)
    for (int i = 0; i < 4; ++i) {
        float in = (i == 0) ? fb : mStage[i - 1];
        mStage[i] += mAlpha * (std::tanh(in) - std::tanh(mStage[i]));
    }

    // Output based on filter type
    switch (mFilterType) {
    case VC_FILTER_LP:
        return mStage[3];
    case VC_FILTER_HP:
        return mStage[0] - mStage[3];  // HP = first stage - last stage
    case VC_FILTER_BP:
        return mStage[1] - mStage[3];   // BP = second stage - last stage
    default:
        return mStage[3];
    }
}

void VCMoogFilter::reset()
{
    for (int i = 0; i < 4; ++i) {
        mStage[i] = 0.0f;
    }
}

//==============================================================================
// VCADSR Implementation
//==============================================================================

VCADSR::VCADSR()
{
    updateCoefficients();
}

void VCADSR::setSampleRate(double sr)
{
    mSampleRate = sr;
    updateCoefficients();
}

void VCADSR::setAttack(float ms)
{
    mAttackMs = std::max(ms, 0.01f);
    updateCoefficients();
}

void VCADSR::setDecay(float ms)
{
    mDecayMs = std::max(ms, 0.01f);
    updateCoefficients();
}

void VCADSR::setSustain(float level)
{
    mSustainLevel = std::clamp(level, 0.0f, 1.0f);
}

void VCADSR::setRelease(float ms)
{
    mReleaseMs = std::max(ms, 0.01f);
    updateCoefficients();
}

float VCADSR::timeToCoeff(float timeMs, double sampleRate)
{
    // For exponential approach: coeff = 1 - exp(-1 / (time * sampleRate))
    // This gives a smooth exponential curve reaching ~63% at the time constant
    float timeSeconds = timeMs / 1000.0f;
    return 1.0f - std::exp(-1.0f / (timeSeconds * (float)sampleRate));
}

void VCADSR::updateCoefficients()
{
    mAttackCoeff = timeToCoeff(mAttackMs, mSampleRate);
    mDecayCoeff = timeToCoeff(mDecayMs, mSampleRate);
    mReleaseCoeff = timeToCoeff(mReleaseMs, mSampleRate);
}

void VCADSR::noteOn()
{
    mState = VC_ADSR_ATTACK;
    // mCurrentLevel stays where it is (for re-triggering)
}

void VCADSR::noteOff()
{
    if (mState != VC_ADSR_IDLE) {
        mReleaseLevel = mCurrentLevel;
        mState = VC_ADSR_RELEASE;
    }
}

float VCADSR::process()
{
    switch (mState) {
    case VC_ADSR_IDLE:
        mCurrentLevel = 0.0f;
        break;

    case VC_ADSR_ATTACK:
        mCurrentLevel += mAttackCoeff * (1.0f - mCurrentLevel);
        if (mCurrentLevel >= 0.999f) {
            mCurrentLevel = 1.0f;
            mState = VC_ADSR_DECAY;
        }
        break;

    case VC_ADSR_DECAY:
        mCurrentLevel += mDecayCoeff * (mSustainLevel - mCurrentLevel);
        if (std::fabs(mCurrentLevel - mSustainLevel) < 0.001f) {
            mCurrentLevel = mSustainLevel;
            mState = VC_ADSR_SUSTAIN;
        }
        break;

    case VC_ADSR_SUSTAIN:
        mCurrentLevel = mSustainLevel;
        break;

    case VC_ADSR_RELEASE:
        mCurrentLevel += mReleaseCoeff * (0.0f - mCurrentLevel);
        if (mCurrentLevel < 0.001f) {
            mCurrentLevel = 0.0f;
            mState = VC_ADSR_IDLE;
        }
        break;
    }

    return mCurrentLevel;
}

void VCADSR::reset()
{
    mState = VC_ADSR_IDLE;
    mCurrentLevel = 0.0f;
    mReleaseLevel = 0.0f;
}

//==============================================================================
// VCSimpleDelay Implementation
//==============================================================================

VCSimpleDelay::VCSimpleDelay()
{
    updateDelay();
}

void VCSimpleDelay::setSampleRate(double sr)
{
    mSampleRate = sr;
    updateDelay();
}

void VCSimpleDelay::setDelayTime(float ms)
{
    mDelayTimeMs = std::max(ms, 1.0f);
    updateDelay();
}

void VCSimpleDelay::setMix(float mix)
{
    mMix = std::clamp(mix, 0.0f, 1.0f);
}

void VCSimpleDelay::updateDelay()
{
    mDelaySamples = (int)(mDelayTimeMs * mSampleRate / 1000.0);
    mDelaySamples = std::max(mDelaySamples, 1);

    int bufSize = mDelaySamples + 1;
    if ((int)mBuffer.size() < bufSize) {
        mBuffer.resize(bufSize, 0.0f);
    }
}

float VCSimpleDelay::process(float input)
{
    if (mMix < 0.001f) return input;

    // Read delayed sample
    int readPos = mWritePos - mDelaySamples;
    if (readPos < 0) readPos += (int)mBuffer.size();

    float delayed = mBuffer[readPos];

    // Write input + feedback
    mBuffer[mWritePos] = input + delayed * mFeedback;
    mWritePos = (mWritePos + 1) % (int)mBuffer.size();

    // Mix dry and wet
    return input * (1.0f - mMix) + delayed * mMix;
}

void VCSimpleDelay::reset()
{
    std::fill(mBuffer.begin(), mBuffer.end(), 0.0f);
    mWritePos = 0;
}

//==============================================================================
// VCSimpleReverb Implementation
//==============================================================================

VCSimpleReverb::VCSimpleReverb()
{
    initDelayLines();
}

void VCSimpleReverb::setSampleRate(double sr)
{
    mSampleRate = sr;
    initDelayLines();
}

void VCSimpleReverb::setMix(float mix)
{
    mMix = std::clamp(mix, 0.0f, 1.0f);
}

int VCSimpleReverb::scaleDelay(int baseDelay) const
{
    // Scale delay length proportional to sample rate
    return (int)(baseDelay * mSampleRate / 44100.0);
}

void VCSimpleReverb::initDelayLines()
{
    // Initialize comb filters
    float combFeedbacks[NUM_COMBS] = { 0.84f, 0.82f, 0.80f, 0.78f };
    for (int i = 0; i < NUM_COMBS; ++i) {
        int len = scaleDelay(COMB_DELAYS[i]);
        mCombs[i].length = len;
        mCombs[i].buffer.assign(len, 0.0f);
        mCombs[i].writePos = 0;
        mCombs[i].feedback = combFeedbacks[i];
        mCombs[i].lpState = 0.0f;
        mCombs[i].lpCoeff = 0.3f;  // low damping
    }

    // Initialize allpass filters
    for (int i = 0; i < NUM_ALLPASS; ++i) {
        int len = scaleDelay(ALLPASS_DELAYS[i]);
        mAllpass[i].length = len;
        mAllpass[i].buffer.assign(len, 0.0f);
        mAllpass[i].writePos = 0;
        mAllpass[i].feedback = 0.5f;
    }
}

float VCSimpleReverb::process(float input)
{
    if (mMix < 0.001f) return input;

    // Parallel comb filters
    float combSum = 0.0f;
    for (int i = 0; i < NUM_COMBS; ++i) {
        CombFilter& c = mCombs[i];
        int readPos = c.writePos - c.length;
        if (readPos < 0) readPos += c.length;

        float delayed = c.buffer[readPos];

        // Lowpass in feedback path
        float filtered = delayed + c.lpCoeff * (c.lpState - delayed);
        c.lpState = filtered;

        // Write with feedback
        c.buffer[c.writePos] = input + filtered * c.feedback;
        c.writePos = (c.writePos + 1) % c.length;

        combSum += filtered;
    }

    // Scale comb sum
    float wet = combSum / (float)NUM_COMBS;

    // Series allpass filters
    for (int i = 0; i < NUM_ALLPASS; ++i) {
        AllpassFilter& a = mAllpass[i];
        int readPos = a.writePos - a.length;
        if (readPos < 0) readPos += a.length;

        float delayed = a.buffer[readPos];
        float output = -wet + delayed;
        a.buffer[a.writePos] = wet + delayed * a.feedback;
        a.writePos = (a.writePos + 1) % a.length;
        wet = output;
    }

    return input * (1.0f - mMix) + wet * mMix;
}

void VCSimpleReverb::reset()
{
    for (int i = 0; i < NUM_COMBS; ++i) {
        std::fill(mCombs[i].buffer.begin(), mCombs[i].buffer.end(), 0.0f);
        mCombs[i].writePos = 0;
        mCombs[i].lpState = 0.0f;
    }
    for (int i = 0; i < NUM_ALLPASS; ++i) {
        std::fill(mAllpass[i].buffer.begin(), mAllpass[i].buffer.end(), 0.0f);
        mAllpass[i].writePos = 0;
    }
}

//==============================================================================
// VCPluginDSP (Main Synth Class) Implementation
//==============================================================================

// Preset definitions
struct SynthPreset {
    const char* name;
    VCPluginDSP::Params params;
};

static const SynthPreset synthPresets[] = {
    // bypass — silent output
    { "bypass", { VC_OSC_SAW, 1, 0.0f, 8000.0f, 0.5f, VC_FILTER_LP,
                  10.0f, 100.0f, 0.0f, 200.0f, 0.0f, 0.0f, 375.0f, -120.0f, false } },
    // init — basic saw with moderate settings
    { "init", { VC_OSC_SAW, 1, 10.0f, 8000.0f, 0.5f, VC_FILTER_LP,
                10.0f, 100.0f, 0.7f, 200.0f, 0.0f, 0.0f, 375.0f, 0.0f, true } },
    // pad — slow attack, long release, low-pass filtered
    { "pad", { VC_OSC_SAW, 3, 15.0f, 3000.0f, 0.3f, VC_FILTER_LP,
               800.0f, 500.0f, 0.6f, 1000.0f, 0.3f, 0.0f, 375.0f, 0.0f, true } },
    // lead — snappy, bright saw
    { "lead", { VC_OSC_SAW, 2, 8.0f, 6000.0f, 0.6f, VC_FILTER_LP,
                5.0f, 150.0f, 0.8f, 150.0f, 0.1f, 0.1f, 375.0f, 0.0f, true } },
    // bass — low cutoff, short decay, thick
    { "bass", { VC_OSC_SAW, 2, 20.0f, 800.0f, 0.7f, VC_FILTER_LP,
                5.0f, 200.0f, 0.4f, 100.0f, 0.0f, 0.0f, 375.0f, 0.0f, true } },
    // pluck — fast attack, short decay, no sustain
    { "pluck", { VC_OSC_TRIANGLE, 1, 10.0f, 5000.0f, 0.4f, VC_FILTER_LP,
                 2.0f, 300.0f, 0.0f, 300.0f, 0.15f, 0.0f, 375.0f, 0.0f, true } },
    // strings — slow attack, high sustain, triangle
    { "strings", { VC_OSC_TRIANGLE, 3, 12.0f, 4000.0f, 0.2f, VC_FILTER_LP,
                   500.0f, 300.0f, 0.85f, 800.0f, 0.25f, 0.0f, 375.0f, 0.0f, true } },
    // organ — instant attack, full sustain, sine
    { "organ", { VC_OSC_SINE, 1, 5.0f, 10000.0f, 0.3f, VC_FILTER_LP,
                 1.0f, 50.0f, 1.0f, 50.0f, 0.2f, 0.1f, 375.0f, 0.0f, true } },
    // synth-brass — saw, medium attack, resonant filter
    { "synth-brass", { VC_OSC_SAW, 2, 15.0f, 4000.0f, 0.8f, VC_FILTER_LP,
                       50.0f, 200.0f, 0.6f, 200.0f, 0.1f, 0.1f, 375.0f, 0.0f, true } },
    // supersaw — 7 unison voices, wide detune
    { "supersaw", { VC_OSC_SAW, 7, 40.0f, 8000.0f, 0.4f, VC_FILTER_LP,
                    10.0f, 200.0f, 0.7f, 300.0f, 0.2f, 0.15f, 375.0f, 0.0f, true } },
};

static constexpr int NUM_PRESETS = (int)(sizeof(synthPresets) / sizeof(synthPresets[0]));

//==============================================================================
// Construction / Destruction
//==============================================================================

VCPluginDSP::VCPluginDSP()
{
}

VCPluginDSP::~VCPluginDSP()
{
}

//==============================================================================
// Prepare
//==============================================================================

void VCPluginDSP::prepare(double sampleRate, int blockSize)
{
    mSampleRate = sampleRate;
    mBlockSize = blockSize;

    // Initialize all voices
    for (int i = 0; i < MAX_VOICES; ++i) {
        mVoices[i].oscillator.setSampleRate(sampleRate);
        mVoices[i].filter.setSampleRate(sampleRate);
        mVoices[i].adsr.setSampleRate(sampleRate);
        mVoices[i].resetAll();
    }

    // Initialize effects
    mReverbL.setSampleRate(sampleRate);
    mReverbR.setSampleRate(sampleRate);
    mDelayL.setSampleRate(sampleRate);
    mDelayR.setSampleRate(sampleRate);

    updateVoiceParams();
}

//==============================================================================
// Render — main audio rendering loop
//==============================================================================

void VCPluginDSP::render(float* left, float* right, int numSamples)
{
    if (!mEnabled) {
        std::memset(left, 0, numSamples * sizeof(float));
        std::memset(right, 0, numSamples * sizeof(float));
        return;
    }

    for (int s = 0; s < numSamples; ++s) {
        float sampleL = 0.0f;
        float sampleR = 0.0f;

        // Sum all active voices
        for (int v = 0; v < MAX_VOICES; ++v) {
            if (!mVoices[v].active && mVoices[v].adsr.getState() == VC_ADSR_IDLE)
                continue;

            ActiveNote& voice = mVoices[v];

            // Generate oscillator sample
            float osc = voice.oscillator.process();

            // Apply filter
            float filtered = voice.filter.process(osc);

            // Apply ADSR envelope
            float env = voice.adsr.process();
            float voiced = filtered * env;

            // Add to output with velocity scaling
            float amp = voice.velocity * mVolumeLinear;
            sampleL += voiced * amp;
            sampleR += voiced * amp;

            // Deactivate voice when envelope finishes
            if (!voice.adsr.isActive()) {
                voice.active = false;
            }
        }

        // Apply effects to mono signal, spread to stereo
        // Simple stereo: delay L/R slightly different, reverb L/R
        float fxL = mDelayL.process(mReverbL.process(sampleL));
        float fxR = mDelayR.process(mReverbR.process(sampleR));

        // Soft-clip to prevent harsh clipping
        auto softClip = [](float x) -> float {
            if (x > 1.0f) return 1.0f - 0.5f * std::exp(-(x - 1.0f) * 4.0f);
            if (x < -1.0f) return -1.0f + 0.5f * std::exp((x + 1.0f) * 4.0f);
            return x;
        };

        left[s] = softClip(fxL);
        right[s] = softClip(fxR);
    }
}

//==============================================================================
// JUCE AudioBlock processing
//==============================================================================

#ifndef VC_STANDALONE
void VCPluginDSP::process(juce::dsp::AudioBlock<float>& block)
{
    if (!mEnabled) {
        block.clear();
        return;
    }

    int numSamples = (int)block.getNumSamples();

    // Allocate temp buffers
    if ((int)mInternalBuffer.size() < numSamples * 2)
        mInternalBuffer.resize(numSamples * 2);

    float* leftBuf = mInternalBuffer.data();
    float* rightBuf = mInternalBuffer.data() + numSamples;

    std::memset(leftBuf, 0, numSamples * sizeof(float));
    std::memset(rightBuf, 0, numSamples * sizeof(float));

    render(leftBuf, rightBuf, numSamples);

    // Copy to output block
    for (size_t ch = 0; ch < block.getNumChannels() && ch < 2; ++ch) {
        auto* data = block.getChannelPointer(ch);
        float* src = (ch == 0) ? leftBuf : rightBuf;
        std::memcpy(data, src, numSamples * sizeof(float));
    }
}
#endif

//==============================================================================
// Reset
//==============================================================================

void VCPluginDSP::reset()
{
    for (int i = 0; i < MAX_VOICES; ++i) {
        mVoices[i].resetAll();
    }
    mReverbL.reset();
    mReverbR.reset();
    mDelayL.reset();
    mDelayR.reset();
}

//==============================================================================
// Note On / Note Off
//==============================================================================

void VCPluginDSP::noteOn(int noteNumber, float velocity)
{
    // Find a free voice or steal the oldest
    int voiceIdx = -1;

    // First try to find an idle voice
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (!mVoices[i].active && mVoices[i].adsr.getState() == VC_ADSR_IDLE) {
            voiceIdx = i;
            break;
        }
    }

    // If no free voice, steal using round-robin
    if (voiceIdx < 0) {
        voiceIdx = mVoiceAllocIdx % MAX_VOICES;
        mVoiceAllocIdx++;
    }

    ActiveNote& voice = mVoices[voiceIdx];
    voice.noteNumber = noteNumber;
    voice.velocity = std::clamp(velocity, 0.0f, 1.0f);
    voice.active = true;

    // Set oscillator frequency
    float freq = midiNoteToFreq(noteNumber);
    voice.oscillator.setSampleRate(mSampleRate);
    voice.oscillator.setOscType(mParams.oscType);
    voice.oscillator.setFrequency(freq);
    voice.oscillator.setUnison(mParams.unison);
    voice.oscillator.setDetune(mParams.detune);
    voice.oscillator.reset();

    // Set filter
    voice.filter.setSampleRate(mSampleRate);
    voice.filter.setCutoff(mParams.cutoff);
    voice.filter.setResonance(mParams.resonance);
    voice.filter.setFilterType(mParams.filterType);
    voice.filter.reset();

    // Set ADSR
    voice.adsr.setSampleRate(mSampleRate);
    voice.adsr.setAttack(mParams.attack);
    voice.adsr.setDecay(mParams.decay);
    voice.adsr.setSustain(mParams.sustain);
    voice.adsr.setRelease(mParams.release);
    voice.adsr.reset();
    voice.adsr.noteOn();
}

void VCPluginDSP::noteOff(int noteNumber)
{
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (mVoices[i].active && mVoices[i].noteNumber == noteNumber) {
            mVoices[i].adsr.noteOff();
            // Don't set active=false here; let it finish release
            break;
        }
    }
}

//==============================================================================
// Render a specific note for a given duration (CLI convenience)
//==============================================================================

void VCPluginDSP::renderNote(int noteNumber, float velocity, float durationSeconds,
                              float* left, float* right, int& numSamplesRendered)
{
    // Calculate total samples including release tail
    int mainSamples = (int)(durationSeconds * mSampleRate);
    // Add extra time for release
    int releaseSamples = (int)(mParams.release * mSampleRate / 1000.0f * 1.5f);
    int totalSamples = mainSamples + releaseSamples;

    // Ensure buffers are large enough
    numSamplesRendered = totalSamples;

    // Trigger note
    noteOn(noteNumber, velocity);

    // Render main duration
    render(left, right, mainSamples);

    // Trigger note off
    noteOff(noteNumber);

    // Render release tail
    render(left + mainSamples, right + mainSamples, releaseSamples);
}

//==============================================================================
// Parameter access
//==============================================================================

void VCPluginDSP::setParams(const Params& p)
{
    mParams = p;
    mVolumeLinear = dBToLinear(mParams.volumeDB);

    // Update effects
    mReverbL.setMix(mParams.reverbMix);
    mReverbR.setMix(mParams.reverbMix);
    mDelayL.setMix(mParams.delayMix);
    mDelayR.setMix(mParams.delayMix);
    mDelayL.setDelayTime(mParams.delayTime);
    mDelayR.setDelayTime(mParams.delayTime);

    // Update active voices
    updateVoiceParams();
}

void VCPluginDSP::updateVoiceParams()
{
    mVolumeLinear = dBToLinear(mParams.volumeDB);

    for (int i = 0; i < MAX_VOICES; ++i) {
        if (mVoices[i].active) {
            mVoices[i].oscillator.setOscType(mParams.oscType);
            mVoices[i].oscillator.setUnison(mParams.unison);
            mVoices[i].oscillator.setDetune(mParams.detune);

            mVoices[i].filter.setCutoff(mParams.cutoff);
            mVoices[i].filter.setResonance(mParams.resonance);
            mVoices[i].filter.setFilterType(mParams.filterType);

            mVoices[i].adsr.setAttack(mParams.attack);
            mVoices[i].adsr.setDecay(mParams.decay);
            mVoices[i].adsr.setSustain(mParams.sustain);
            mVoices[i].adsr.setRelease(mParams.release);
        }
    }
}

VCPluginDSP::Params VCPluginDSP::getParams() const
{
    return mParams;
}

void VCPluginDSP::setEnabled(bool enabled)
{
    mEnabled = enabled;
}

//==============================================================================
// Preset management
//==============================================================================

const char* VCPluginDSP::getPresetName(int index)
{
    if (index < 0 || index >= NUM_PRESETS) return nullptr;
    return synthPresets[index].name;
}

int VCPluginDSP::getNumPresets()
{
    return NUM_PRESETS;
}

bool VCPluginDSP::getPreset(int index, Params& p)
{
    if (index < 0 || index >= NUM_PRESETS) return false;
    p = synthPresets[index].params;
    return true;
}

bool VCPluginDSP::getPresetByName(const char* name, Params& p)
{
    for (int i = 0; i < NUM_PRESETS; ++i) {
        if (strcmp(name, synthPresets[i].name) == 0) {
            p = synthPresets[i].params;
            return true;
        }
    }
    return false;
}
