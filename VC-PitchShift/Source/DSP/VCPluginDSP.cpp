//==============================================================================
// VC-PitchShift DSP Core Implementation - Phase Vocoder Pitch Shifting
//
// Correct approach: Time-stretch + Resample
//   1. Time-stretch: Use different analysis/synthesis hops
//      - For pitch UP (ratio > 1): time-compress (hopS < hopA)
//      - For pitch DOWN (ratio < 1): time-expand (hopS > hopA)
//   2. Resample: Convert back to original duration
//      - For pitch UP: we have fewer output samples, resample UP
//      - For pitch DOWN: we have more output samples, resample DOWN
//
// Combined: The pitch shift ratio determines the time-stretch factor,
// and then linear interpolation resamples to the original length.
//==============================================================================

#include "VCPluginDSP.h"

#ifdef VC_STANDALONE
#include <algorithm>
#include <cmath>
#include <cstring>
#endif

//==============================================================================
VCPluginDSP::VCPluginDSP() {}
VCPluginDSP::~VCPluginDSP() {}

//==============================================================================
void VCPluginDSP::prepare(double sampleRate, int blockSize)
{
    mSampleRate = sampleRate;
    mBlockSize = blockSize;
    updatePitchRatio();

    for (int ch = 0; ch < 2; ++ch) {
        mPVState[ch].prevPhase.resize(PV_FFT_SIZE_2, 0.0f);
        mPVState[ch].synthPhase.resize(PV_FFT_SIZE_2, 0.0f);
    }

    mHannWindow.resize(PV_FFT_SIZE);
    computeHannWindow(mHannWindow.data(), PV_FFT_SIZE);

    mFFTReal.resize(PV_FFT_SIZE);
    mFFTImag.resize(PV_FFT_SIZE);

    mInternalBuffer.resize(blockSize * 2);
    mInternalPtrs.resize(2);
    mInternalPtrs[0] = mInternalBuffer.data();
    mInternalPtrs[1] = mInternalBuffer.data() + blockSize;

    // Stretched buffer (2x max for safety)
    mStretchedBuffer[0].resize(PV_FFT_SIZE * 8, 0.0f);
    mStretchedBuffer[1].resize(PV_FFT_SIZE * 8, 0.0f);

    mInputCopy[0].resize(PV_FFT_SIZE * 4, 0.0f);
    mInputCopy[1].resize(PV_FFT_SIZE * 4, 0.0f);
}

void VCPluginDSP::updatePitchRatio()
{
    float totalSemitones = static_cast<float>(mParams.semitones) + mParams.cents / 100.0f;
    mPitchRatio = std::pow(2.0f, totalSemitones / 12.0f);
}

//==============================================================================
void VCPluginDSP::process(float* left, float* right, int numSamples)
{
    if (!mEnabled) return;

#ifdef VC_STANDALONE
    processInternal(left, right, numSamples);
#else
    if ((int)mInternalBuffer.size() < numSamples * 2)
        mInternalBuffer.resize(static_cast<size_t>(numSamples) * 2);

    float* leftBuf = mInternalBuffer.data();
    float* rightBuf = mInternalBuffer.data() + numSamples;

    for (int i = 0; i < numSamples; ++i) {
        leftBuf[i] = left[i]; rightBuf[i] = right[i];
    }
    mInternalPtrs[0] = leftBuf;
    mInternalPtrs[1] = rightBuf;

    juce::dsp::AudioBlock<float> block(mInternalPtrs.data(), 2, static_cast<size_t>(numSamples));
    process(block);

    for (int i = 0; i < numSamples; ++i) {
        left[i] = leftBuf[i]; right[i] = rightBuf[i];
    }
#endif
}

#ifndef VC_STANDALONE
void VCPluginDSP::process(juce::dsp::AudioBlock<float>& block)
{
    if (!mEnabled) return;
    auto numSamples = static_cast<int>(block.getNumSamples());
    if (numSamples < 1) return;
    processInternal(block.getChannelPointer(0), block.getChannelPointer(1), numSamples);
}
#endif

//==============================================================================
void VCPluginDSP::computeHannWindow(float* window, int size)
{
    for (int i = 0; i < size; ++i)
        window[i] = 0.5f * (1.0f - std::cos(2.0f * VC_PI * static_cast<float>(i) / static_cast<float>(size)));
}

//==============================================================================
void VCPluginDSP::fft(float* real, float* imag, int N, bool inverse)
{
    int j = 0;
    for (int i = 0; i < N - 1; ++i) {
        if (i < j) { std::swap(real[i], real[j]); std::swap(imag[i], imag[j]); }
        int k = N >> 1;
        while (k <= j) { j -= k; k >>= 1; }
        j += k;
    }
    float dir = inverse ? 1.0f : -1.0f;
    for (int len = 2; len <= N; len <<= 1) {
        float angle = dir * 2.0f * VC_PI / static_cast<float>(len);
        float wR = std::cos(angle), wI = std::sin(angle);
        for (int i = 0; i < N; i += len) {
            float cR = 1.0f, cI = 0.0f;
            for (int k = 0; k < len / 2; ++k) {
                int e = i + k, o = i + k + len / 2;
                float tR = cR * real[o] - cI * imag[o];
                float tI = cR * imag[o] + cI * real[o];
                real[o] = real[e] - tR; imag[o] = imag[e] - tI;
                real[e] += tR; imag[e] += tI;
                float nR = cR * wR - cI * wI; float nI = cR * wI + cI * wR;
                cR = nR; cI = nI;
            }
        }
    }
    if (inverse) {
        float invN = 1.0f / static_cast<float>(N);
        for (int i = 0; i < N; ++i) { real[i] *= invN; imag[i] *= invN; }
    }
}

//==============================================================================
// Time-stretch one channel using phase vocoder
// Returns the number of samples in the stretched output
//==============================================================================
int VCPluginDSP::timeStretchChannel(const float* input, int numSamples,
                                     float* stretchedOut, int maxStretchedSamples,
                                     int channel)
{
    float sampleRateF = static_cast<float>(mSampleRate);
    float binFreq = sampleRateF / static_cast<float>(PV_FFT_SIZE);
    float alpha = mPitchRatio;  // pitch shift ratio

    // For time-stretching by factor 1/alpha:
    // If pitch UP (alpha > 1), time-compress by 1/alpha → shorter output
    // If pitch DOWN (alpha < 1), time-expand by 1/alpha → longer output
    float timeStretchFactor = alpha;

    int hopA = PV_HOP_SIZE;
    int hopS = static_cast<int>(static_cast<float>(hopA) * timeStretchFactor + 0.5f);
    if (hopS < 1) hopS = 1;
    if (hopS > PV_FFT_SIZE) hopS = PV_FFT_SIZE;

    float* prevPhase = mPVState[channel].prevPhase.data();
    float* synthPhase = mPVState[channel].synthPhase.data();

    // Zero the stretched output
    std::fill(stretchedOut, stretchedOut + maxStretchedSamples, 0.0f);

    // Number of analysis frames
    int numFrames = 1;
    if (numSamples > PV_FFT_SIZE) {
        numFrames = (numSamples - PV_FFT_SIZE) / hopA + 1;
    }

    for (int frame = 0; frame < numFrames; ++frame) {
        int inputStart = frame * hopA;

        // Extract and window
        for (int k = 0; k < PV_FFT_SIZE; ++k) {
            int idx = inputStart + k;
            float sample = (idx < numSamples) ? input[idx] : 0.0f;
            mFFTReal[k] = sample * mHannWindow[k];
            mFFTImag[k] = 0.0f;
        }

        fft(mFFTReal.data(), mFFTImag.data(), PV_FFT_SIZE, false);

        float magnitude[2048 / 2 + 1];

        for (int k = 0; k < PV_FFT_SIZE_2; ++k) {
            magnitude[k] = std::sqrt(mFFTReal[k] * mFFTReal[k] + mFFTImag[k] * mFFTImag[k]);
            float phase = std::atan2(mFFTImag[k], mFFTReal[k]);

            float phaseDiff = phase - prevPhase[k];
            float expectedPhaseAdvance = 2.0f * VC_PI * static_cast<float>(k)
                                        * static_cast<float>(hopA)
                                        / static_cast<float>(PV_FFT_SIZE);
            phaseDiff -= expectedPhaseAdvance;
            while (phaseDiff > VC_PI)  phaseDiff -= 2.0f * VC_PI;
            while (phaseDiff < -VC_PI) phaseDiff += 2.0f * VC_PI;

            float trueFreq = static_cast<float>(k) * binFreq
                           + phaseDiff * sampleRateF / (2.0f * VC_PI * static_cast<float>(hopA));

            // Accumulate synthesis phase using synthesis hop
            synthPhase[k] += 2.0f * VC_PI * trueFreq * static_cast<float>(hopS) / sampleRateF;

            prevPhase[k] = phase;
        }

        // Formant preservation
        if (mParams.formant) {
            float savedMag[2048 / 2 + 1];
            for (int k = 0; k < PV_FFT_SIZE_2; ++k) savedMag[k] = magnitude[k];
            for (int k = 0; k < PV_FFT_SIZE_2; ++k) {
                float srcBin = static_cast<float>(k) / alpha;
                int srcIdx0 = static_cast<int>(srcBin);
                int srcIdx1 = srcIdx0 + 1;
                if (srcIdx0 >= 0 && srcIdx1 < PV_FFT_SIZE_2) {
                    float frac = srcBin - static_cast<float>(srcIdx0);
                    magnitude[k] = savedMag[srcIdx0] * (1.0f - frac) + savedMag[srcIdx1] * frac;
                } else if (srcIdx0 >= 0 && srcIdx0 < PV_FFT_SIZE_2) {
                    magnitude[k] = savedMag[srcIdx0];
                } else {
                    magnitude[k] = 0.0f;
                }
            }
        }

        for (int k = 0; k < PV_FFT_SIZE_2; ++k) {
            mFFTReal[k] = magnitude[k] * std::cos(synthPhase[k]);
            mFFTImag[k] = magnitude[k] * std::sin(synthPhase[k]);
        }
        for (int k = PV_FFT_SIZE_2; k < PV_FFT_SIZE; ++k) {
            int mirror = PV_FFT_SIZE - k;
            mFFTReal[k] = mFFTReal[mirror];
            mFFTImag[k] = -mFFTImag[mirror];
        }

        fft(mFFTReal.data(), mFFTImag.data(), PV_FFT_SIZE, true);

        // Overlap-add with synthesis hop
        int outputStart = frame * hopS;
        for (int k = 0; k < PV_FFT_SIZE; ++k) {
            int outIdx = outputStart + k;
            if (outIdx < maxStretchedSamples) {
                stretchedOut[outIdx] += mFFTReal[k] * mHannWindow[k];
            }
        }
    }

    // Normalize
    float normGain = 0.0f;
    for (int k = 0; k < PV_FFT_SIZE; ++k)
        normGain += mHannWindow[k] * mHannWindow[k];
    if (normGain > 1e-10f) {
        float normFactor = static_cast<float>(hopS) / normGain;
        int stretchedLen = (numFrames - 1) * hopS + PV_FFT_SIZE;
        for (int i = 0; i < VC_JMIN(stretchedLen, maxStretchedSamples); ++i)
            stretchedOut[i] *= normFactor;
    }

    // Return the length of the stretched signal
    return (numFrames - 1) * hopS + PV_FFT_SIZE;
}

//==============================================================================
// Resample a signal from stretchedLen to numSamples using linear interpolation
//==============================================================================
void resample(const float* input, int inputLen, float* output, int outputLen)
{
    if (outputLen <= 0 || inputLen <= 0) return;

    float ratio = static_cast<float>(inputLen) / static_cast<float>(outputLen);

    for (int i = 0; i < outputLen; ++i) {
        float srcPos = static_cast<float>(i) * ratio;
        int idx0 = static_cast<int>(srcPos);
        int idx1 = idx0 + 1;
        float frac = srcPos - static_cast<float>(idx0);

        if (idx0 >= 0 && idx1 < inputLen) {
            output[i] = input[idx0] * (1.0f - frac) + input[idx1] * frac;
        } else if (idx0 >= 0 && idx0 < inputLen) {
            output[i] = input[idx0];
        } else {
            output[i] = 0.0f;
        }
    }
}

//==============================================================================
void VCPluginDSP::processInternal(float* left, float* right, int numSamples)
{
    if (std::abs(mPitchRatio - 1.0f) < 1e-6f) return;

    // Ensure buffers are large enough
    // Stretched length is approximately numSamples / pitchRatio
    int maxStretchedLen = static_cast<int>(static_cast<float>(numSamples) / mPitchRatio * 1.5f) + PV_FFT_SIZE;
    maxStretchedLen = VC_JMAX(maxStretchedLen, numSamples * 2);

    if ((int)mStretchedBuffer[0].size() < maxStretchedLen) {
        mStretchedBuffer[0].resize(maxStretchedLen);
        mStretchedBuffer[1].resize(maxStretchedLen);
    }
    if ((int)mInputCopy[0].size() < numSamples) {
        mInputCopy[0].resize(numSamples);
        mInputCopy[1].resize(numSamples);
    }

    // Copy input
    std::copy(left, left + numSamples, mInputCopy[0].data());
    std::copy(right, right + numSamples, mInputCopy[1].data());

    // Time-stretch each channel
    int stretchedLenL = timeStretchChannel(mInputCopy[0].data(), numSamples,
                                           mStretchedBuffer[0].data(), maxStretchedLen, 0);
    int stretchedLenR = timeStretchChannel(mInputCopy[1].data(), numSamples,
                                           mStretchedBuffer[1].data(), maxStretchedLen, 1);

    // Resample from stretched length back to original length
    // This combines the time-stretch and resample to achieve pitch shift
    resample(mStretchedBuffer[0].data(), stretchedLenL, left, numSamples);
    resample(mStretchedBuffer[1].data(), stretchedLenR, right, numSamples);
}

//==============================================================================
void VCPluginDSP::reset()
{
    for (int ch = 0; ch < 2; ++ch) {
        std::fill(mPVState[ch].prevPhase.begin(), mPVState[ch].prevPhase.end(), 0.0f);
        std::fill(mPVState[ch].synthPhase.begin(), mPVState[ch].synthPhase.end(), 0.0f);
    }
}

void VCPluginDSP::setParams(const Params& p) { mParams = p; updatePitchRatio(); }
VCPluginDSP::Params VCPluginDSP::getParams() const { return mParams; }
void VCPluginDSP::setEnabled(bool enabled) { mEnabled = enabled; }
