#include "VCDistortionDSP.h"

#ifdef VC_STANDALONE
#include <algorithm>
#include <cmath>
#endif

//==============================================================================
// Construction / Destruction
//==============================================================================
VCPluginDSP::VCPluginDSP()
{
    mParams.type = 0;
    mParams.drive = 0.5f;
    mParams.mix = 1.0f;
    mParams.tone = 0.5f;
    mParams.makeup = 0.0f;
    mParams.enabled = true;
    mBitCrushHold[0] = 0.0f;
    mBitCrushHold[1] = 0.0f;
    mBitCrushCounter = 0;
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

#ifdef VC_STANDALONE
    for (int ch = 0; ch < 2; ++ch) {
        mToneFilter[ch] = OnePoleState();
        mTapeLP[ch] = OnePoleState();
    }
    updateToneFilter();
#endif

    mSmoothDrive = mParams.drive;
}

//==============================================================================
// Distortion Algorithms
//==============================================================================

// Tube: tanh soft clipping - warm, musical saturation
float VCPluginDSP::processTube(float x, float driveGain)
{
    return std::tanh(driveGain * x);
}

// Tape: soft clip + one-pole lowpass - magnetic tape saturation
float VCPluginDSP::processTape(float x, float driveGain)
{
    // Soft saturation with slight asymmetry (mimics tape hysteresis)
    float driven = driveGain * x;
    float sign = (driven >= 0.0f) ? 1.0f : -1.0f;
    float abs_d = std::abs(driven);
    // Slightly asymmetric soft clip
    float softClip = sign * (1.0f - std::exp(-abs_d));
    // Post-saturation lowpass (tape rolls off highs)
#ifdef VC_STANDALONE
    softClip = applyTapeLP(softClip, 0); // channel handled externally
#endif
    return softClip;
}

// Transistor: hard clipping - aggressive, punchy
float VCPluginDSP::processTransistor(float x, float driveGain)
{
    float driven = driveGain * x;
    float thresh = 1.0f;
    return std::clamp(driven, -thresh, thresh);
}

// Fuzz: extreme asymmetric distortion
float VCPluginDSP::processFuzz(float x, float driveGain)
{
    float driven = driveGain * x;
    if (std::abs(driven) < 1e-10f) return 0.0f;
    float exp = 1.0f / std::max(driveGain, 0.01f);
    float sign = (driven >= 0.0f) ? 1.0f : -1.0f;
    return sign * std::pow(std::abs(driven), exp);
}

// BitCrush: quantize + downsample
float VCPluginDSP::processBitCrush(float x, float driveGain, int channel)
{
    // drive controls both bit depth and downsample factor
    // Bit depth: from 16 bits (drive=0) down to 2 bits (drive=1)
    int bitDepth = static_cast<int>(16.0f - 14.0f * driveGain);
    bitDepth = std::clamp(bitDepth, 2, 16);
    float levels = static_cast<float>(1 << bitDepth);

    // Downsample factor: 1x (drive=0) to 32x (drive=1)
    int downsampleFactor = static_cast<int>(1.0f + 31.0f * driveGain * driveGain);
    downsampleFactor = std::clamp(downsampleFactor, 1, 32);

    // Hold sample for downsampling
    mBitCrushCounter++;
    if (mBitCrushCounter >= downsampleFactor) {
        mBitCrushCounter = 0;
        mBitCrushHold[channel] = x;
    }

    // Quantize
    float held = mBitCrushHold[channel];
    return std::round(held * levels) / levels;
}

//==============================================================================
// Process interleaved stereo buffer
//==============================================================================
void VCPluginDSP::process(float* left, float* right, int numSamples)
{
    if (!mEnabled)
        return;

#ifdef VC_STANDALONE
    processInternal(left, right, numSamples);
#else
    // JUCE: copy to non-interleaved, process, copy back
    if ((int)mInternalBuffer.size() < numSamples * 2)
        mInternalBuffer.resize(numSamples * 2);

    float* leftBuf = mInternalBuffer.data();
    float* rightBuf = mInternalBuffer.data() + numSamples;

    for (int i = 0; i < numSamples; ++i) {
        leftBuf[i] = left[i];
        rightBuf[i] = right[i];
    }

    mInternalPtrs[0] = leftBuf;
    mInternalPtrs[1] = rightBuf;

    juce::dsp::AudioBlock<float> block(mInternalPtrs.data(), 2, numSamples);
    process(block);

    for (int i = 0; i < numSamples; ++i) {
        left[i] = leftBuf[i];
        right[i] = rightBuf[i];
    }
#endif
}

//==============================================================================
// JUCE AudioBlock processing
//==============================================================================
#ifndef VC_STANDALONE
void VCPluginDSP::process(juce::dsp::AudioBlock<float>& block)
{
    if (!mEnabled)
        return;

    // Use same internal processing
    size_t numSamples = block.getNumSamples();
    if ((int)mInternalBuffer.size() < (int)numSamples * 2)
        mInternalBuffer.resize(numSamples * 2);

    std::vector<float> left(numSamples), right(numSamples);
    for (size_t i = 0; i < numSamples; ++i) {
        left[i] = block.getChannelPointer(0)[i];
        right[i] = block.getChannelPointer(1)[i];
    }

    processInternal(left.data(), right.data(), (int)numSamples);

    for (size_t i = 0; i < numSamples; ++i) {
        block.getChannelPointer(0)[i] = left[i];
        block.getChannelPointer(1)[i] = right[i];
    }
}
#endif

//==============================================================================
// Internal DSP processing
//==============================================================================
void VCPluginDSP::processInternal(float* left, float* right, int numSamples)
{
    float drive = mParams.drive;
    float mix = mParams.mix;
    float makeupGain = dBToLinear(mParams.makeup);
    int type = mParams.type;

    // Drive maps to gain: 0->1x, 1->100x (exponential curve for musical feel)
    float driveGain = 1.0f + 99.0f * drive * drive;

    // Smooth drive parameter
    float smoothCoeff = 0.999f;

    for (int i = 0; i < numSamples; ++i) {
        mSmoothDrive += (driveGain - mSmoothDrive) * (1.0f - smoothCoeff);
        float currentDriveGain = mSmoothDrive;

        for (int ch = 0; ch < 2; ++ch) {
            float* data = (ch == 0) ? left : right;
            float dry = data[i];
            float wet = dry;

            switch (type) {
                case 0: wet = processTube(dry, currentDriveGain); break;
                case 1: wet = processTape(dry, currentDriveGain); break;
                case 2: wet = processTransistor(dry, currentDriveGain); break;
                case 3: wet = processFuzz(dry, currentDriveGain); break;
                case 4: wet = processBitCrush(dry, drive, ch); break;
                default: wet = processTube(dry, currentDriveGain); break;
            }

            // Apply makeup gain
            wet *= makeupGain;

            // Apply tone filter
#ifdef VC_STANDALONE
            wet = applyToneFilter(wet, ch);
#endif

            // Dry/Wet mix
            data[i] = dry * (1.0f - mix) + wet * mix;
        }
    }
}

//==============================================================================
// Reset processing state
//==============================================================================
void VCPluginDSP::reset()
{
#ifdef VC_STANDALONE
    for (int ch = 0; ch < 2; ++ch) {
        mToneFilter[ch] = OnePoleState();
        mTapeLP[ch] = OnePoleState();
    }
#endif
    mBitCrushHold[0] = 0.0f;
    mBitCrushHold[1] = 0.0f;
    mBitCrushCounter = 0;
    mSmoothDrive = mParams.drive;
}

//==============================================================================
// Set parameters
//==============================================================================
void VCPluginDSP::setParams(const Params& p)
{
    mParams = p;

#ifdef VC_STANDALONE
    updateToneFilter();
#endif
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

//==============================================================================
// Standalone: Tone filter and Tape LP
//==============================================================================
#ifdef VC_STANDALONE
void VCPluginDSP::updateToneFilter()
{
    // Tone: 0=dark (lowpass at 500Hz), 1=bright (bypass/20kHz)
    float cutoff = 500.0f + 19500.0f * mParams.tone * mParams.tone;
    float rc = 1.0f / (2.0f * VC_PI * cutoff);
    float dt = 1.0f / static_cast<float>(mSampleRate);
    mToneCoeff = rc / (rc + dt);

    // Tape LP: fixed ~4kHz rolloff
    float tapeCutoff = 4000.0f;
    float rcTape = 1.0f / (2.0f * VC_PI * tapeCutoff);
    mTapeLPCoeff = rcTape / (rcTape + dt);
}

float VCPluginDSP::applyToneFilter(float x, int channel)
{
    OnePoleState& s = mToneFilter[channel];
    s.y1 = mToneCoeff * s.y1 + (1.0f - mToneCoeff) * x;
    return s.y1;
}

float VCPluginDSP::applyTapeLP(float x, int channel)
{
    OnePoleState& s = mTapeLP[channel];
    s.y1 = mTapeLPCoeff * s.y1 + (1.0f - mTapeLPCoeff) * x;
    return s.y1;
}
#endif
