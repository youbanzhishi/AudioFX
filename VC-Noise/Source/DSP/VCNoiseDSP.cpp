#include "VCNoiseDSP.h"

#ifdef VC_STANDALONE
#include <algorithm>
#include <cmath>
#include <cstdlib>
#endif

//==============================================================================
// Construction / Destruction
//==============================================================================
VCPluginDSP::VCPluginDSP()
{
    mParams.type = 0;
    mParams.frequency = 1000.0f;
    mParams.endFreq = 20000.0f;
    mParams.sweepDuration = 5.0f;
    mParams.sweepLog = true;
    mParams.volume = -6.0f;
    mParams.channelMode = 0;
    mParams.pulsePeriod = 0.0f;
    mParams.enabled = true;
}

VCPluginDSP::~VCPluginDSP()
{
}

//==============================================================================
// Prepare DSP for processing
//==============================================================================
void VCPluginDSP::prepare(double sampleRate, int blockSize)
{
    mSampleRate = sampleRate;
    mBlockSize = blockSize;

    mInternalBuffer.resize(blockSize * 2);
    mInternalPtrs.resize(2);
    mInternalPtrs[0] = mInternalBuffer.data();
    mInternalPtrs[1] = mInternalBuffer.data() + blockSize;

    reset();
}

//==============================================================================
// Random number generator: uniform [-1, 1] using LCG
//==============================================================================
float VCPluginDSP::randomUniform()
{
    // LCG: state = state * 1103515245 + 12345
    mRandState = mRandState * 1103515245u + 12345u;
    // Map to [-1.0, 1.0]
    return (2.0f * static_cast<float>(mRandState) / 4294967295.0f) - 1.0f;
}

//==============================================================================
// White noise: uniform random [-1, 1]
//==============================================================================
float VCPluginDSP::generateWhite()
{
    return randomUniform();
}

//==============================================================================
// Pink noise: Voss-McCartney algorithm (-3dB/octave)
//==============================================================================
float VCPluginDSP::generatePink()
{
    // Voss-McCartney: update rows based on counter bit pattern
    mPinkIndex = (mPinkIndex + 1) & 0xFFFF;  // 16-bit counter

    // Determine which rows to update
    int numZeros = 0;
    int temp = mPinkIndex;
    while ((temp & 1) == 0 && numZeros < PINK_NUM_ROWS) {
        numZeros++;
        temp >>= 1;
    }

    if (numZeros < PINK_NUM_ROWS) {
        mPinkRunningSum -= mPinkRows[numZeros];
        mPinkRows[numZeros] = randomUniform();
        mPinkRunningSum += mPinkRows[numZeros];
    }

    // Add a white noise sample for the highest octave
    float white = randomUniform();
    return (mPinkRunningSum + white) / (float)(PINK_NUM_ROWS + 1);
}

//==============================================================================
// Brown noise: integrate white noise
//==============================================================================
float VCPluginDSP::generateBrown()
{
    float white = randomUniform();
    mBrownState += 0.02f * white;
    mBrownState *= 0.998f;  // Prevent DC drift
    return mBrownState;
}

//==============================================================================
// Sine wave
//==============================================================================
float VCPluginDSP::generateSine()
{
    float freq = mParams.frequency;
    float sample = std::sin(2.0f * (float)VC_PI * freq * (float)mPhase);
    mPhase += 1.0 / mSampleRate;
    // Keep phase in [0, 1) to avoid precision loss
    if (mPhase >= 1.0) mPhase -= 1.0;
    return sample;
}

//==============================================================================
// Frequency sweep (linear or logarithmic)
//==============================================================================
float VCPluginDSP::generateSweep()
{
    float startFreq = mParams.frequency;
    float endFreq = mParams.endFreq;
    float duration = mParams.sweepDuration;

    double t = mSamplePos / mSampleRate;

    float sample;
    if (mParams.sweepLog) {
        // Logarithmic sweep: freq(t) = f0 * (f1/f0)^(t/T)
        double ratio = (double)endFreq / (double)startFreq;
        double exponent = t / (double)duration;
        double currentFreq = (double)startFreq * std::pow(ratio, exponent);

        // Phase accumulation for accurate sweep
        sample = (float)std::sin(2.0 * VC_PI * mPhase);
        mPhase += currentFreq / mSampleRate;
    } else {
        // Linear sweep: freq(t) = f0 + (f1-f0)*(t/T)
        double currentFreq = (double)startFreq + ((double)endFreq - (double)startFreq) * t / (double)duration;

        sample = (float)std::sin(2.0 * VC_PI * mPhase);
        mPhase += currentFreq / mSampleRate;
    }

    // Reset after sweep completes
    if (t >= (double)duration) {
        mPhase = 0.0;
        mSamplePos = 0;
    }

    return sample;
}

//==============================================================================
// Impulse: first sample = 1.0, rest = 0.0
//==============================================================================
float VCPluginDSP::generateImpulse()
{
    float periodSamples = mParams.pulsePeriod * (float)mSampleRate;

    if (mParams.pulsePeriod <= 0.0f) {
        // Single impulse mode
        if (!mImpulseFired) {
            mImpulseFired = true;
            return 1.0f;
        }
        return 0.0f;
    } else {
        // Periodic impulse mode
        if (mImpulseCounter == 0) {
            mImpulseCounter = (long long)periodSamples;
            return 1.0f;
        }
        mImpulseCounter--;
        return 0.0f;
    }
}

//==============================================================================
// Generate signal (main entry for generator mode)
//==============================================================================
void VCPluginDSP::generate(float* left, float* right, int numSamples)
{
    if (!mEnabled)
        return;

    float gain = dBToLinear(mParams.volume);

    for (int i = 0; i < numSamples; ++i) {
        float sample = 0.0f;

        switch (mParams.type) {
            case 0: sample = generateWhite(); break;
            case 1: sample = generatePink(); break;
            case 2: sample = generateBrown(); break;
            case 3: sample = generateSine(); break;
            case 4: sample = generateSweep(); break;
            case 5: sample = generateImpulse(); break;
            default: sample = generateWhite(); break;
        }

        sample *= gain;

        // Apply channel mode
        switch (mParams.channelMode) {
            case 0: // Stereo (same signal on both channels)
                left[i] = sample;
                right[i] = sample;
                break;
            case 1: // Left only
                left[i] = sample;
                right[i] = 0.0f;
                break;
            case 2: // Right only
                left[i] = 0.0f;
                right[i] = sample;
                break;
            case 3: // Anti-phase (right = inverted)
                left[i] = sample;
                right[i] = -sample;
                break;
            default:
                left[i] = sample;
                right[i] = sample;
                break;
        }

        mSamplePos++;
    }
}

//==============================================================================
// Process (pass-through in generator mode, but applies generation)
//==============================================================================
void VCPluginDSP::process(float* left, float* right, int numSamples)
{
    generate(left, right, numSamples);
}

//==============================================================================
// JUCE AudioBlock processing
//==============================================================================
#ifndef VC_STANDALONE
void VCPluginDSP::process(juce::dsp::AudioBlock<float>& block)
{
    if (!mEnabled)
        return;

    size_t numSamples = block.getNumSamples();
    if ((int)mInternalBuffer.size() < (int)numSamples * 2)
        mInternalBuffer.resize(numSamples * 2);

    std::vector<float> left(numSamples), right(numSamples);
    generate(left.data(), right.data(), (int)numSamples);

    for (size_t i = 0; i < numSamples; ++i) {
        block.getChannelPointer(0)[i] = left[i];
        block.getChannelPointer(1)[i] = right[i];
    }
}
#endif

//==============================================================================
// Reset processing state
//==============================================================================
void VCPluginDSP::reset()
{
    mPhase = 0.0;
    mBrownState = 0.0f;
    mPinkIndex = 0;
    mPinkRunningSum = 0.0f;
    for (int i = 0; i < PINK_NUM_ROWS; ++i) {
        mPinkRows[i] = 0.0f;
    }
    mImpulseFired = false;
    mImpulseCounter = 0;
    mSamplePos = 0;
    mRandState = 12345;
}

//==============================================================================
// Set parameters
//==============================================================================
void VCPluginDSP::setParams(const Params& p)
{
    mParams = p;
}

//==============================================================================
// Get parameters
//==============================================================================
VCPluginDSP::Params VCPluginDSP::getParams() const
{
    return mParams;
}

//==============================================================================
// Set enabled state
//==============================================================================
void VCPluginDSP::setEnabled(bool enabled)
{
    mEnabled = enabled;
}
