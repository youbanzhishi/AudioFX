//==============================================================================
// VC-PitchShift DSP Core Implementation - Phase Vocoder Pitch Shifting
//
// Algorithm:
//   1. STFT analysis with hop size PV_HOP_SIZE
//   2. Compute instantaneous frequency via phase unwrapping
//   3. Scale instantaneous frequencies by pitch ratio
//   4. Accumulate synthesis phases using scaled frequencies
//   5. ISTFT synthesis with same hop size (overlap-add)
//
// This approach directly scales frequencies in the phase domain,
// which shifts pitch without changing duration.
//==============================================================================

#include "VCPluginDSP.h"

#ifdef VC_STANDALONE
#include <algorithm>
#include <cmath>
#include <cstring>
#endif

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
// Prepare DSP for processing
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

//==============================================================================
// Update pitch shift ratio from parameters
//==============================================================================
void VCPluginDSP::updatePitchRatio()
{
    float totalSemitones = static_cast<float>(mParams.semitones) + mParams.cents / 100.0f;
    mPitchRatio = std::pow(2.0f, totalSemitones / 12.0f);
}

//==============================================================================
// Process stereo buffer
//==============================================================================
void VCPluginDSP::process(float* left, float* right, int numSamples)
{
    if (!mEnabled)
        return;

#ifdef VC_STANDALONE
    processInternal(left, right, numSamples);
#else
    if ((int)mInternalBuffer.size() < numSamples * 2)
        mInternalBuffer.resize(static_cast<size_t>(numSamples) * 2);

    float* leftBuf = mInternalBuffer.data();
    float* rightBuf = mInternalBuffer.data() + numSamples;

    for (int i = 0; i < numSamples; ++i) {
        leftBuf[i] = left[i];
        rightBuf[i] = right[i];
    }

    mInternalPtrs[0] = leftBuf;
    mInternalPtrs[1] = rightBuf;

    juce::dsp::AudioBlock<float> block(mInternalPtrs.data(), 2, static_cast<size_t>(numSamples));
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

    auto numSamples = static_cast<int>(block.getNumSamples());
    if (numSamples < 1)
        return;

    float* leftBuf = block.getChannelPointer(0);
    float* rightBuf = block.getChannelPointer(1);

    processInternal(leftBuf, rightBuf, numSamples);
}
#endif

//==============================================================================
// Compute Hann window
//==============================================================================
void VCPluginDSP::computeHannWindow(float* window, int size)
{
    for (int i = 0; i < size; ++i) {
        window[i] = 0.5f * (1.0f - std::cos(2.0f * VC_PI * static_cast<float>(i)
                                              / static_cast<float>(size)));
    }
}

//==============================================================================
// Standalone FFT (Cooley-Tukey radix-2, in-place)
//==============================================================================
void VCPluginDSP::fft(float* real, float* imag, int N, bool inverse)
{
    int j = 0;
    for (int i = 0; i < N - 1; ++i) {
        if (i < j) {
            std::swap(real[i], real[j]);
            std::swap(imag[i], imag[j]);
        }
        int k = N >> 1;
        while (k <= j) { j -= k; k >>= 1; }
        j += k;
    }

    float dir = inverse ? 1.0f : -1.0f;
    for (int len = 2; len <= N; len <<= 1) {
        float angle = dir * 2.0f * VC_PI / static_cast<float>(len);
        float wReal = std::cos(angle);
        float wImag = std::sin(angle);

        for (int i = 0; i < N; i += len) {
            float curReal = 1.0f, curImag = 0.0f;
            for (int k = 0; k < len / 2; ++k) {
                int evenIdx = i + k;
                int oddIdx = i + k + len / 2;

                float tReal = curReal * real[oddIdx] - curImag * imag[oddIdx];
                float tImag = curReal * imag[oddIdx] + curImag * real[oddIdx];

                real[oddIdx] = real[evenIdx] - tReal;
                imag[oddIdx] = imag[evenIdx] - tImag;
                real[evenIdx] += tReal;
                imag[evenIdx] += tImag;

                float newCurReal = curReal * wReal - curImag * wImag;
                float newCurImag = curReal * wImag + curImag * wReal;
                curReal = newCurReal;
                curImag = newCurImag;
            }
        }
    }

    if (inverse) {
        float invN = 1.0f / static_cast<float>(N);
        for (int i = 0; i < N; ++i) {
            real[i] *= invN;
            imag[i] *= invN;
        }
    }
}

//==============================================================================
// Process one channel through Phase Vocoder
// Uses frequency-scaling approach: same hop for analysis and synthesis,
// scale instantaneous frequencies by pitch ratio.
//==============================================================================
void VCPluginDSP::processChannelPV(const float* input, float* output, int numSamples, int channel)
{
    float pitchRatio = mPitchRatio;
    float sampleRateF = static_cast<float>(mSampleRate);
    float binFreq = sampleRateF / static_cast<float>(PV_FFT_SIZE);
    float expectedPhaseInc = 2.0f * VC_PI * static_cast<float>(PV_HOP_SIZE)
                           / static_cast<float>(PV_FFT_SIZE);

    float* prevPhase = mPVState[channel].prevPhase.data();
    float* synthPhase = mPVState[channel].synthPhase.data();

    // Zero the output
    std::fill(output, output + numSamples, 0.0f);

    // Process the input in overlapping frames (same hop for analysis and synthesis)
    int hopSize = PV_HOP_SIZE;

    int numFrames = 1;
    if (numSamples > PV_FFT_SIZE) {
        numFrames = (numSamples - PV_FFT_SIZE) / hopSize + 1;
    }

    for (int frame = 0; frame < numFrames; ++frame) {
        int inputStart = frame * hopSize;

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
        float magnitude[2048 / 2 + 1];  // PV_FFT_SIZE_2

        for (int k = 0; k < PV_FFT_SIZE_2; ++k) {
            magnitude[k] = std::sqrt(mFFTReal[k] * mFFTReal[k] + mFFTImag[k] * mFFTImag[k]);
            float phase = std::atan2(mFFTImag[k], mFFTReal[k]);

            // Phase difference and unwrapping
            float phaseDiff = phase - prevPhase[k];

            // Remove expected phase advance for this bin
            phaseDiff -= expectedPhaseInc * static_cast<float>(k);

            // Wrap to [-pi, pi]
            while (phaseDiff > VC_PI)  phaseDiff -= 2.0f * VC_PI;
            while (phaseDiff < -VC_PI) phaseDiff += 2.0f * VC_PI;

            // True instantaneous frequency (deviation from expected)
            float trueFreq = static_cast<float>(k) * binFreq
                           + phaseDiff * sampleRateF / (2.0f * VC_PI * static_cast<float>(hopSize));

            // Scale frequency for pitch shift
            float synthFreq = trueFreq * pitchRatio;

            // Accumulate synthesis phase using the SAME hop size
            // This is the key: we accumulate phase based on the scaled frequency
            // using the analysis hop, which gives us pitch-shifted output
            synthPhase[k] += 2.0f * VC_PI * synthFreq * static_cast<float>(hopSize) / sampleRateF;

            prevPhase[k] = phase;
        }

        // Formant preservation: resample magnitude spectrum
        if (mParams.formant) {
            float savedMag[2048 / 2 + 1];
            for (int k = 0; k < PV_FFT_SIZE_2; ++k) {
                savedMag[k] = magnitude[k];
            }
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

        // Reconstruct frequency domain from magnitude + synthesis phase
        for (int k = 0; k < PV_FFT_SIZE_2; ++k) {
            mFFTReal[k] = magnitude[k] * std::cos(synthPhase[k]);
            mFFTImag[k] = magnitude[k] * std::sin(synthPhase[k]);
        }

        // Mirror for negative frequencies
        for (int k = PV_FFT_SIZE_2; k < PV_FFT_SIZE; ++k) {
            int mirror = PV_FFT_SIZE - k;
            mFFTReal[k] = mFFTReal[mirror];
            mFFTImag[k] = -mFFTImag[mirror];
        }

        // Inverse FFT
        fft(mFFTReal.data(), mFFTImag.data(), PV_FFT_SIZE, true);

        // Apply synthesis window and overlap-add (same hop as analysis)
        int outputStart = frame * hopSize;
        for (int k = 0; k < PV_FFT_SIZE; ++k) {
            int outIdx = outputStart + k;
            if (outIdx < numSamples) {
                output[outIdx] += mFFTReal[k] * mHannWindow[k];
            }
        }
    }

    // Normalize by overlap-add gain (Hann window with 75% overlap)
    // For Hann window with 75% overlap, the COLA gain is 1.5 * hop
    float normGain = 0.0f;
    for (int k = 0; k < PV_FFT_SIZE; ++k) {
        normGain += mHannWindow[k] * mHannWindow[k];
    }
    if (normGain > 1e-10f) {
        float normFactor = static_cast<float>(hopSize) / normGain;
        for (int i = 0; i < numSamples; ++i) {
            output[i] *= normFactor;
        }
    }
}

//==============================================================================
// Internal pitch shifting processing
//==============================================================================
void VCPluginDSP::processInternal(float* left, float* right, int numSamples)
{
    if (std::abs(mPitchRatio - 1.0f) < 1e-6f) {
        return;
    }

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
// Reset
//==============================================================================
void VCPluginDSP::reset()
{
    for (int ch = 0; ch < 2; ++ch) {
        std::fill(mPVState[ch].prevPhase.begin(), mPVState[ch].prevPhase.end(), 0.0f);
        std::fill(mPVState[ch].synthPhase.begin(), mPVState[ch].synthPhase.end(), 0.0f);
        std::fill(mSpectralEnvelope[ch].begin(), mSpectralEnvelope[ch].end(), 0.0f);
    }
}

void VCPluginDSP::setParams(const Params& p)
{
    mParams = p;
    updatePitchRatio();
}

VCPluginDSP::Params VCPluginDSP::getParams() const
{
    return mParams;
}

void VCPluginDSP::setEnabled(bool enabled)
{
    mEnabled = enabled;
}
