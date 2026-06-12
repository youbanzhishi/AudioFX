#include "VCPluginDSP.h"

//==============================================================================
// Construction / Destruction
//==============================================================================
VCPluginDSP::VCPluginDSP()
{
    mParams.frequency = 200.0f;
    mParams.gain = -6.0f;
    mParams.q = 1.0f;
    mParams.threshold = -12.0f;
    mParams.range = -12.0f;
    mParams.attack = 10.0f;
    mParams.release = 100.0f;
    mParams.mix = 100.0f;
    mParams.enabled = true;
    mParams.bands = 1;
    mParams.sidechain = 0;
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

    // Initialize all band processors
    for (int b = 0; b < VC_DYN_EQ_MAX_BANDS; ++b) {
        for (int ch = 0; ch < 2; ++ch) {
            mBandProc[b].eqState[ch].reset();
            mBandProc[b].bpState[ch].reset();
            mBandProc[b].envelope[ch] = 0.0f;
            mBandProc[b].smoothGain[ch] = 1.0f;
        }
    }

    updateAllBandCoefficients();
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
// JUCE AudioBlock processing (non-interleaved)
//==============================================================================
#ifndef VC_STANDALONE
void VCPluginDSP::process(juce::dsp::AudioBlock<float>& block)
{
    if (!mEnabled)
        return;

    float wet = mParams.mix / 100.0f;
    float dry = 1.0f - wet;
    int numBands = VC_JCLAMP(mParams.bands, 1, VC_DYN_EQ_MAX_BANDS);

    // Process each band serially
    for (int b = 0; b < numBands; ++b) {
        const auto& bp = mParams.band[b];
        float threshGain = dBToLinear(bp.threshold);
        float attackCoeff = std::exp(-1.0f / (bp.attack * 0.001f * mSampleRate));
        float releaseCoeff = std::exp(-1.0f / (bp.release * 0.001f * mSampleRate));
        float ratio = VC_JMAX(bp.ratio, 1.0f);

        for (size_t ch = 0; ch < block.getNumChannels(); ++ch) {
            auto* data = block.getChannelPointer(ch);
            int chIdx = (int)ch;

            for (size_t i = 0; i < block.getNumSamples(); ++i) {
                float in = data[i];

                // 1. Apply EQ filter
                float eqOut = mBandProc[b].eqState[chIdx].process(in);

                // 2. Bandpass detection
                float det = mBandProc[b].bpState[chIdx].process(in);
                float env = std::abs(det);

                if (env > mBandProc[b].envelope[chIdx]) {
                    mBandProc[b].envelope[chIdx] = attackCoeff * mBandProc[b].envelope[chIdx] + (1.0f - attackCoeff) * env;
                } else {
                    mBandProc[b].envelope[chIdx] = releaseCoeff * mBandProc[b].envelope[chIdx] + (1.0f - releaseCoeff) * env;
                }

                // 3. Dynamic gain calculation
                float dynamicGain = 1.0f;
                if (mBandProc[b].envelope[chIdx] > threshGain) {
                    float overDb = VCStandalone::gainToDecibels(mBandProc[b].envelope[chIdx] / threshGain);
                    float gainReductionDb = -overDb * (1.0f - 1.0f / ratio);
                    dynamicGain = dBToLinear(gainReductionDb);
                }

                // 4. Smooth the gain
                mBandProc[b].smoothGain[chIdx] = 0.999f * mBandProc[b].smoothGain[chIdx] + 0.001f * dynamicGain;

                // 5. Apply: dry + (eq - dry) * smoothGain = dry + eq*smoothGain - dry*smoothGain
                // Simplified: out = in + (eqOut - in) * smoothGain
                float out = in + (eqOut - in) * mBandProc[b].smoothGain[chIdx];
                data[i] = dry * in + wet * out;
            }
        }
    }
}
#endif

//==============================================================================
// Reset processing state
//==============================================================================
void VCPluginDSP::reset()
{
    for (int b = 0; b < VC_DYN_EQ_MAX_BANDS; ++b) {
        for (int ch = 0; ch < 2; ++ch) {
            mBandProc[b].eqState[ch].reset();
            mBandProc[b].bpState[ch].reset();
            mBandProc[b].envelope[ch] = 0.0f;
            mBandProc[b].smoothGain[ch] = 1.0f;
        }
    }
}

//==============================================================================
// Set parameters
//==============================================================================
void VCPluginDSP::setParams(const Params& p)
{
    mParams = p;

    // Sync Gen1 compat params to band[0]
    mParams.band[0].frequency = mParams.frequency;
    mParams.band[0].gain = mParams.gain;
    mParams.band[0].q = mParams.q;
    mParams.band[0].threshold = mParams.threshold;
    mParams.band[0].attack = mParams.attack;
    mParams.band[0].release = mParams.release;

    updateAllBandCoefficients();
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
// Update band coefficients (used by both JUCE and standalone modes)
//==============================================================================
void VCPluginDSP::updateBandCoefficients(int bandIdx)
{
    if (bandIdx < 0 || bandIdx >= VC_DYN_EQ_MAX_BANDS) return;

    const auto& bp = mParams.band[bandIdx];
    float freq = VC_JCLAMP(bp.frequency, 20.0f, (float)mSampleRate * 0.49f);
    float Q = VC_JCLAMP(bp.q, 0.1f, 10.0f);
    float gainDB = bp.gain;

    float omega = 2.0f * VC_PI * freq / static_cast<float>(mSampleRate);
    float sn = std::sin(omega);
    float cs = std::cos(omega);
    float alpha = sn / (2.0f * Q);
    float A = std::pow(10.0f, gainDB / 40.0f);

    float b0, b1, b2, a0, a1, a2;

    switch (bp.type) {
    case VCBandType::LowShelf: {
        float sqA = std::sqrt(A);
        b0 = A * ((A + 1.0f) - (A - 1.0f) * cs + 2.0f * sqA * alpha);
        b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cs);
        b2 = A * ((A + 1.0f) - (A - 1.0f) * cs - 2.0f * sqA * alpha);
        a0 = (A + 1.0f) + (A - 1.0f) * cs + 2.0f * sqA * alpha;
        a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cs);
        a2 = (A + 1.0f) + (A - 1.0f) * cs - 2.0f * sqA * alpha;
        break;
    }
    case VCBandType::HighShelf: {
        float sqA = std::sqrt(A);
        b0 = A * ((A + 1.0f) + (A - 1.0f) * cs + 2.0f * sqA * alpha);
        b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cs);
        b2 = A * ((A + 1.0f) + (A - 1.0f) * cs - 2.0f * sqA * alpha);
        a0 = (A + 1.0f) - (A - 1.0f) * cs + 2.0f * sqA * alpha;
        a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cs);
        a2 = (A + 1.0f) - (A - 1.0f) * cs - 2.0f * sqA * alpha;
        break;
    }
    case VCBandType::Notch: {
        b0 = 1.0f;
        b1 = -2.0f * cs;
        b2 = 1.0f;
        a0 = 1.0f + alpha;
        a1 = -2.0f * cs;
        a2 = 1.0f - alpha;
        break;
    }
    case VCBandType::Bell:
    default: {
        // Peaking EQ (Robert Bristow-Johnson)
        b0 = 1.0f + alpha * A;
        b1 = -2.0f * cs;
        b2 = 1.0f - alpha * A;
        a0 = 1.0f + alpha / A;
        a1 = -2.0f * cs;
        a2 = 1.0f - alpha / A;
        break;
    }
    }

    // Normalize
    for (int ch = 0; ch < 2; ++ch) {
        mBandProc[bandIdx].eqState[ch].b0 = b0 / a0;
        mBandProc[bandIdx].eqState[ch].b1 = b1 / a0;
        mBandProc[bandIdx].eqState[ch].b2 = b2 / a0;
        mBandProc[bandIdx].eqState[ch].a1 = a1 / a0;
        mBandProc[bandIdx].eqState[ch].a2 = a2 / a0;
    }

    // Bandpass for detection (always bandpass regardless of band type)
    float bpAlpha = sn / (2.0f * Q);
    float bp_b0 = bpAlpha;
    float bp_b1 = 0.0f;
    float bp_b2 = -bpAlpha;
    float bp_a0 = 1.0f + bpAlpha;
    float bp_a1 = -2.0f * cs;
    float bp_a2 = 1.0f - bpAlpha;

    for (int ch = 0; ch < 2; ++ch) {
        mBandProc[bandIdx].bpState[ch].b0 = bp_b0 / bp_a0;
        mBandProc[bandIdx].bpState[ch].b1 = bp_b1 / bp_a0;
        mBandProc[bandIdx].bpState[ch].b2 = bp_b2 / bp_a0;
        mBandProc[bandIdx].bpState[ch].a1 = bp_a1 / bp_a0;
        mBandProc[bandIdx].bpState[ch].a2 = bp_a2 / bp_a0;
    }
}

void VCPluginDSP::updateAllBandCoefficients()
{
    for (int b = 0; b < VC_DYN_EQ_MAX_BANDS; ++b) {
        updateBandCoefficients(b);
    }
}

//==============================================================================
// Standalone IIR processing - Gen2 Multi-band Dynamic EQ
//==============================================================================
#ifdef VC_STANDALONE

void VCPluginDSP::processInternal(float* left, float* right, int numSamples)
{
    float wet = mParams.mix / 100.0f;
    float dry = 1.0f - wet;
    int numBands = VC_JCLAMP(mParams.bands, 1, VC_DYN_EQ_MAX_BANDS);

    // Process bands serially - each band processes the output of the previous
    for (int b = 0; b < numBands; ++b) {
        const auto& bp = mParams.band[b];
        float threshGain = VCStandalone::decibelsToGain(bp.threshold);
        float attackCoeff = std::exp(-1.0f / (bp.attack * 0.001f * mSampleRate));
        float releaseCoeff = std::exp(-1.0f / (bp.release * 0.001f * mSampleRate));
        float ratio = VC_JMAX(bp.ratio, 1.0f);

        float* channels[2] = { left, right };

        for (int i = 0; i < numSamples; ++i) {
            for (int ch = 0; ch < 2; ++ch) {
                float in = channels[ch][i];

                // 1. Apply EQ filter for this band
                float eqOut = mBandProc[b].eqState[ch].process(in);

                // 2. Bandpass detection on input signal
                float det = mBandProc[b].bpState[ch].process(in);
                float env = std::abs(det);

                // 3. Envelope follower
                if (env > mBandProc[b].envelope[ch]) {
                    mBandProc[b].envelope[ch] = attackCoeff * mBandProc[b].envelope[ch] + (1.0f - attackCoeff) * env;
                } else {
                    mBandProc[b].envelope[ch] = releaseCoeff * mBandProc[b].envelope[ch] + (1.0f - releaseCoeff) * env;
                }

                // 4. Dynamic gain calculation
                float dynamicGain = 1.0f;
                if (mBandProc[b].envelope[ch] > threshGain) {
                    float overDb = VCStandalone::gainToDecibels(mBandProc[b].envelope[ch] / threshGain);
                    float gainReductionDb = -overDb * (1.0f - 1.0f / ratio);
                    dynamicGain = VCStandalone::decibelsToGain(gainReductionDb);
                }

                // 5. Smooth the dynamic gain
                mBandProc[b].smoothGain[ch] = 0.999f * mBandProc[b].smoothGain[ch] + 0.001f * dynamicGain;

                // 6. Apply: blend between dry and dynamically-gained EQ output
                // out = in + (eqOut - in) * smoothGain
                float out = in + (eqOut - in) * mBandProc[b].smoothGain[ch];
                channels[ch][i] = dry * in + wet * out;
            }
        }
    }
}
#endif
