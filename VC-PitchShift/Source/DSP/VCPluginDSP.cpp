//==============================================================================
// VC-PitchShift DSP Core Implementation - Phase Vocoder Pitch Shifting
//
// Correct Phase Vocoder pitch shift algorithm:
//   1. STFT analysis with hop size hopA = PV_HOP_SIZE
//   2. Phase unwrapping → instantaneous frequency
//   3. Phase accumulation for synthesis (NO frequency scaling)
//   4. ISTFT synthesis with hop size hopS = hopA / pitchRatio
//      - For pitch UP: hopS < hopA (time compression → pitch up)
//      - For pitch DOWN: hopS > hopA (time expansion → pitch down)
//   5. The output signal duration remains the same because we process
//      all frames from the same input block
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
        mSpectralEnvelope[ch].resize(PV_FFT_SIZE_2, 0.0f);
    }

    mHannWindow.resize(PV_FFT_SIZE);
    computeHannWindow(mHannWindow.data(), PV_FFT_SIZE);

    mFFTReal.resize(PV_FFT_SIZE);
    mFFTImag.resize(PV_FFT_SIZE);

    mInternalBuffer.resize(blockSize * 2);
    mInternalPtrs.resize(2);
    mInternalPtrs[0] = mInternalBuffer.data();
    mInternalPtrs[1] = mInternalBuffer.data() + blockSize;

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
    float* leftBuf = block.getChannelPointer(0);
    float* rightBuf = block.getChannelPointer(1);
    processInternal(leftBuf, rightBuf, numSamples);
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
// Phase Vocoder for one channel
// Key: analysis hop = hopA, synthesis hop = hopS
// For pitch shift ratio α: hopS = hopA / α
// Phase accumulation uses TRUE frequencies (not scaled)
//==============================================================================
void VCPluginDSP::processChannelPV(const float* input, float* output, int numSamples, int channel)
{
    float pitchRatio = mPitchRatio;
    float sampleRateF = static_cast<float>(mSampleRate);
    float binFreq = sampleRateF / static_cast<float>(PV_FFT_SIZE);

    int hopA = PV_HOP_SIZE;  // Analysis hop
    // Synthesis hop: for pitch UP, hopS < hopA (frames more densely packed → higher pitch)
    int hopS = static_cast<int>(static_cast<float>(hopA) / pitchRatio + 0.5f);
    if (hopS < 1) hopS = 1;
    if (hopS > PV_FFT_SIZE) hopS = PV_FFT_SIZE;

    float* prevPhase = mPVState[channel].prevPhase.data();
    float* synthPhase = mPVState[channel].synthPhase.data();

    // Zero the output
    std::fill(output, output + numSamples, 0.0f);

    // Number of analysis frames
    int numFrames = 1;
    if (numSamples > PV_FFT_SIZE) {
        numFrames = (numSamples - PV_FFT_SIZE) / hopA + 1;
    }

    for (int frame = 0; frame < numFrames; ++frame) {
        int inputStart = frame * hopA;

        // Extract and window the analysis frame
        for (int k = 0; k < PV_FFT_SIZE; ++k) {
            int idx = inputStart + k;
            float sample = (idx < numSamples) ? input[idx] : 0.0f;
            mFFTReal[k] = sample * mHannWindow[k];
            mFFTImag[k] = 0.0f;
        }

        // Forward FFT
        fft(mFFTReal.data(), mFFTImag.data(), PV_FFT_SIZE, false);

        // Process frequency bins
        float magnitude[2048 / 2 + 1];

        for (int k = 0; k < PV_FFT_SIZE_2; ++k) {
            magnitude[k] = std::sqrt(mFFTReal[k] * mFFTReal[k] + mFFTImag[k] * mFFTImag[k]);
            float phase = std::atan2(mFFTImag[k], mFFTReal[k]);

            // Phase difference from previous frame
            float phaseDiff = phase - prevPhase[k];

            // Expected phase advance for bin k with analysis hop
            float expectedPhaseAdvance = 2.0f * VC_PI * static_cast<float>(k)
                                        * static_cast<float>(hopA)
                                        / static_cast<float>(PV_FFT_SIZE);

            // Remove expected phase advance
            phaseDiff -= expectedPhaseAdvance;

            // Wrap to [-pi, pi]
            while (phaseDiff > VC_PI)  phaseDiff -= 2.0f * VC_PI;
            while (phaseDiff < -VC_PI) phaseDiff += 2.0f * VC_PI;

            // True instantaneous frequency
            float trueFreq = static_cast<float>(k) * binFreq
                           + phaseDiff * sampleRateF / (2.0f * VC_PI * static_cast<float>(hopA));

            // Accumulate synthesis phase using TRUE frequency and SYNTHESIS hop
            // Do NOT scale the frequency - the pitch shift comes from the different hop size
            synthPhase[k] += 2.0f * VC_PI * trueFreq * static_cast<float>(hopS) / sampleRateF;

            prevPhase[k] = phase;
        }

        // Formant preservation
        if (mParams.formant) {
            float savedMag[2048 / 2 + 1];
            for (int k = 0; k < PV_FFT_SIZE_2; ++k) savedMag[k] = magnitude[k];
            for (int k = 0; k < PV_FFT_SIZE_2; ++k) {
                float srcBin = static_cast<float>(k) / pitchRatio;
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

        // Reconstruct frequency domain
        for (int k = 0; k < PV_FFT_SIZE_2; ++k) {
            mFFTReal[k] = magnitude[k] * std::cos(synthPhase[k]);
            mFFTImag[k] = magnitude[k] * std::sin(synthPhase[k]);
        }
        for (int k = PV_FFT_SIZE_2; k < PV_FFT_SIZE; ++k) {
            int mirror = PV_FFT_SIZE - k;
            mFFTReal[k] = mFFTReal[mirror];
            mFFTImag[k] = -mFFTImag[mirror];
        }

        // Inverse FFT
        fft(mFFTReal.data(), mFFTImag.data(), PV_FFT_SIZE, true);

        // Apply synthesis window and overlap-add with SYNTHESIS hop
        int outputStart = frame * hopS;
        for (int k = 0; k < PV_FFT_SIZE; ++k) {
            int outIdx = outputStart + k;
            if (outIdx < numSamples) {
                output[outIdx] += mFFTReal[k] * mHannWindow[k];
            }
        }
    }

    // Normalize by overlap-add gain
    float normGain = 0.0f;
    for (int k = 0; k < PV_FFT_SIZE; ++k)
        normGain += mHannWindow[k] * mHannWindow[k];

    if (normGain > 1e-10f) {
        float normFactor = static_cast<float>(hopS) / normGain;
        for (int i = 0; i < numSamples; ++i)
            output[i] *= normFactor;
    }
}

//==============================================================================
void VCPluginDSP::processInternal(float* left, float* right, int numSamples)
{
    if (std::abs(mPitchRatio - 1.0f) < 1e-6f) return;

    if ((int)mInputCopy[0].size() < numSamples) {
        mInputCopy[0].resize(numSamples);
        mInputCopy[1].resize(numSamples);
    }

    std::copy(left, left + numSamples, mInputCopy[0].data());
    std::copy(right, right + numSamples, mInputCopy[1].data());

    processChannelPV(mInputCopy[0].data(), left, numSamples, 0);
    processChannelPV(mInputCopy[1].data(), right, numSamples, 1);
}

//==============================================================================
void VCPluginDSP::reset()
{
    for (int ch = 0; ch < 2; ++ch) {
        std::fill(mPVState[ch].prevPhase.begin(), mPVState[ch].prevPhase.end(), 0.0f);
        std::fill(mPVState[ch].synthPhase.begin(), mPVState[ch].synthPhase.end(), 0.0f);
        std::fill(mSpectralEnvelope[ch].begin(), mSpectralEnvelope[ch].end(), 0.0f);
    }
}

void VCPluginDSP::setParams(const Params& p) { mParams = p; updatePitchRatio(); }
VCPluginDSP::Params VCPluginDSP::getParams() const { return mParams; }
void VCPluginDSP::setEnabled(bool enabled) { mEnabled = enabled; }
