#include "VCPluginDSP.h"

#ifdef VC_STANDALONE
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#endif

//==============================================================================
// VCRadix2FFT Implementation
//==============================================================================
VCRadix2FFT::VCRadix2FFT(int fftSize)
    : mSize(fftSize)
{
    computeTwiddles();
    computeBitReversal();
}

VCRadix2FFT::~VCRadix2FFT() {}

void VCRadix2FFT::computeTwiddles()
{
    int half = mSize / 2;
    mSinTable.resize(half);
    mCosTable.resize(half);
    for (int i = 0; i < half; ++i) {
        float angle = -2.0f * (float)VC_PI * i / (float)mSize;
        mCosTable[i] = std::cos(angle);
        mSinTable[i] = std::sin(angle);
    }
}

void VCRadix2FFT::computeBitReversal()
{
    mBitRev.resize(mSize);
    int bits = 0;
    int tmp = mSize;
    while (tmp > 1) { bits++; tmp >>= 1; }

    for (int i = 0; i < mSize; ++i) {
        int rev = 0;
        int val = i;
        for (int b = 0; b < bits; ++b) {
            rev = (rev << 1) | (val & 1);
            val >>= 1;
        }
        mBitRev[i] = rev;
    }
}

void VCRadix2FFT::forward(const float* input, float* realOut, float* imagOut) const
{
    // Bit-reversal permutation
    for (int i = 0; i < mSize; ++i) {
        realOut[mBitRev[i]] = input[i];
        imagOut[mBitRev[i]] = 0.0f;
    }

    // Cooley-Tukey butterfly
    for (int stage = 1; stage < mSize; stage <<= 1) {
        int step = stage << 1;
        int twiddleStep = mSize / step;
        for (int k = 0; k < mSize; k += step) {
            for (int j = 0; j < stage; ++j) {
                int idx1 = k + j;
                int idx2 = k + j + stage;
                int twIdx = j * twiddleStep;
                float cosVal = mCosTable[twIdx];
                float sinVal = mSinTable[twIdx];
                float tr = cosVal * realOut[idx2] - sinVal * imagOut[idx2];
                float ti = cosVal * imagOut[idx2] + sinVal * realOut[idx2];
                realOut[idx2] = realOut[idx1] - tr;
                imagOut[idx2] = imagOut[idx1] - ti;
                realOut[idx1] = realOut[idx1] + tr;
                imagOut[idx1] = imagOut[idx1] + ti;
            }
        }
    }
}

void VCRadix2FFT::inverse(const float* realIn, const float* imagIn, float* output) const
{
    // Conjugate + forward + scale = IFFT
    // IFFT(X) = conj(FFT(conj(X))) / N
    // We implement: forward with conjugated twiddles then scale

    // Bit-reversal with conjugated input
    for (int i = 0; i < mSize; ++i) {
        output[mBitRev[i]] = realIn[i];  // real part same
        // We'll use realOut/imagOut trick: pack into output temporarily
    }

    // Actually, let's do a proper IFFT: forward on conjugated input, then conjugate and scale
    std::vector<float> tmpReal(mSize), tmpImag(mSize);
    for (int i = 0; i < mSize; ++i) {
        tmpReal[mBitRev[i]] = realIn[i];
        tmpImag[mBitRev[i]] = -imagIn[i];  // conjugate
    }

    for (int stage = 1; stage < mSize; stage <<= 1) {
        int step = stage << 1;
        int twiddleStep = mSize / step;
        for (int k = 0; k < mSize; k += step) {
            for (int j = 0; j < stage; ++j) {
                int idx1 = k + j;
                int idx2 = k + j + stage;
                int twIdx = j * twiddleStep;
                float cosVal = mCosTable[twIdx];
                float sinVal = mSinTable[twIdx];
                float tr = cosVal * tmpReal[idx2] - sinVal * tmpImag[idx2];
                float ti = cosVal * tmpImag[idx2] + sinVal * tmpReal[idx2];
                tmpReal[idx2] = tmpReal[idx1] - tr;
                tmpImag[idx2] = tmpImag[idx1] - ti;
                tmpReal[idx1] = tmpReal[idx1] + tr;
                tmpImag[idx1] = tmpImag[idx1] + ti;
            }
        }
    }

    // Conjugate result and scale
    float scale = 1.0f / (float)mSize;
    for (int i = 0; i < mSize; ++i) {
        output[i] = tmpReal[i] * scale;  // real part (imag should be ~0)
    }
}

//==============================================================================
// VCNoiseProfile Implementation
//==============================================================================
VCNoiseProfile::VCNoiseProfile()
    : mFrameCount(0)
{
    reset();
}

VCNoiseProfile::~VCNoiseProfile() {}

void VCNoiseProfile::reset()
{
    for (int i = 0; i < NUM_BANDS; ++i) {
        mBandEnergies[i] = 0.0f;
        mAccumBands[i] = 0.0f;
    }
    mFrameCount = 0;
}

void VCNoiseProfile::computeBandEdges(float sampleRate, int fftSize)
{
    float nyquist = sampleRate / 2.0f;
    // Mel-spaced bands for better perceptual resolution
    float melMin = 0.0f;
    float melMax = 2595.0f * std::log10(1.0f + nyquist / 700.0f);
    float melStep = (melMax - melMin) / (float)NUM_BANDS;

    for (int i = 0; i <= NUM_BANDS; ++i) {
        float mel = melMin + melStep * i;
        float hz = 700.0f * (std::pow(10.0f, mel / 2595.0f) - 1.0f);
        mBandEdges[i] = hz;
    }
}

void VCNoiseProfile::mapSpectrumToBands(const float* magnitude, int fftSize, float sampleRate)
{
    computeBandEdges(sampleRate, fftSize);
    int halfFFT = fftSize / 2 + 1;

    for (int b = 0; b < NUM_BANDS; ++b) {
        float fLow = mBandEdges[b];
        float fHigh = mBandEdges[b + 1];
        int binLow = (int)(fLow * fftSize / sampleRate);
        int binHigh = (int)(fHigh * fftSize / sampleRate);
        binLow = std::max(0, std::min(binLow, halfFFT - 1));
        binHigh = std::max(0, std::min(binHigh, halfFFT - 1));

        float energy = 0.0f;
        int count = 0;
        for (int k = binLow; k <= binHigh; ++k) {
            energy += magnitude[k] * magnitude[k];
            count++;
        }
        if (count > 0) energy /= (float)count;
        mAccumBands[b] += energy;
    }
}

void VCNoiseProfile::accumulate(const float* magnitude, int fftSize)
{
    // Simplified: use linear frequency mapping if sampleRate not yet known
    // This overload used when we already have power spectrum
    int halfFFT = fftSize / 2 + 1;
    int binsPerBand = halfFFT / NUM_BANDS;
    if (binsPerBand < 1) binsPerBand = 1;

    for (int b = 0; b < NUM_BANDS; ++b) {
        float energy = 0.0f;
        int count = 0;
        int startBin = b * binsPerBand;
        int endBin = std::min(startBin + binsPerBand, halfFFT);
        for (int k = startBin; k < endBin; ++k) {
            energy += magnitude[k] * magnitude[k];
            count++;
        }
        if (count > 0) energy /= (float)count;
        mAccumBands[b] += energy;
    }
    mFrameCount++;
}

void VCNoiseProfile::finalize()
{
    if (mFrameCount <= 0) return;
    for (int i = 0; i < NUM_BANDS; ++i) {
        mBandEnergies[i] = mAccumBands[i] / (float)mFrameCount;
    }
}

void VCNoiseProfile::getFullSpectrum(float* outPower, int fftSize, float sampleRate) const
{
    // Interpolate 64 bands back to full FFT resolution
    // Uses linear band mapping (matching accumulate())
    int halfFFT = fftSize / 2 + 1;
    int binsPerBand = halfFFT / NUM_BANDS;
    if (binsPerBand < 1) binsPerBand = 1;

    for (int k = 0; k < halfFFT; ++k) {
        int band = k / binsPerBand;
        if (band >= NUM_BANDS) band = NUM_BANDS - 1;
        outPower[k] = mBandEnergies[band];
    }
    // Mirror for negative frequencies
    for (int k = halfFFT; k < fftSize; ++k) {
        outPower[k] = outPower[fftSize - k];
    }
}

//==============================================================================
// VCSpectralSubtractor Implementation
//==============================================================================
VCSpectralSubtractor::VCSpectralSubtractor()
    : mSampleRate(44100.0)
    , mFFTSize(512)
    , mReduction(10.0f)
    , mFloor(5.0f)
{
}

VCSpectralSubtractor::~VCSpectralSubtractor() {}

void VCSpectralSubtractor::prepare(double sampleRate, int fftSize)
{
    mSampleRate = sampleRate;
    mFFTSize = fftSize;
    mNoisePower.resize(fftSize, 0.0f);
    mSigPower.resize(fftSize, 0.0f);
    mCleanPower.resize(fftSize, 0.0f);
}

void VCSpectralSubtractor::reset()
{
    std::fill(mNoisePower.begin(), mNoisePower.end(), 0.0f);
    std::fill(mSigPower.begin(), mSigPower.end(), 0.0f);
    std::fill(mCleanPower.begin(), mCleanPower.end(), 0.0f);
}

void VCSpectralSubtractor::setReduction(float dB)
{
    mReduction = std::clamp(dB, 0.0f, 30.0f);
}

void VCSpectralSubtractor::setFloor(float ratio)
{
    mFloor = std::clamp(ratio, 1.0f, 20.0f);
}

void VCSpectralSubtractor::process(const float* realIn, const float* imagIn,
                                    float* realOut, float* imagOut,
                                    const VCNoiseProfile& profile)
{
    int halfFFT = mFFTSize / 2 + 1;

    // Get noise power spectrum from profile
    profile.getFullSpectrum(mNoisePower.data(), mFFTSize, (float)mSampleRate);

    // Over-subtraction factor derived from reduction dB
    // α = 10^(reduction/20) maps dB to linear over-subtraction
    float alpha = std::pow(10.0f, mReduction / 20.0f);

    // Spectral floor β (as fraction of input power)
    float beta = mFloor * 0.01f;

    for (int k = 0; k < halfFFT; ++k) {
        // Compute signal power
        float sigPow = realIn[k] * realIn[k] + imagIn[k] * imagIn[k];
        float noisePow = mNoisePower[k];

        // Spectral subtraction: |S|² = max(|X|² - α·|N|², β·|X|²)
        float cleanPow = sigPow - alpha * noisePow;
        float floorPow = beta * sigPow;
        if (cleanPow < floorPow) cleanPow = floorPow;
        if (cleanPow < 0.0f) cleanPow = 0.0f;

        // Compute gain = sqrt(cleanPow / sigPow) but avoid division by zero
        float gain = 1.0f;
        if (sigPow > 1e-10f) {
            gain = std::sqrt(cleanPow / sigPow);
        }

        realOut[k] = realIn[k] * gain;
        imagOut[k] = imagIn[k] * gain;
    }

    // Mirror conjugate symmetry for inverse FFT
    for (int k = halfFFT; k < mFFTSize; ++k) {
        int mirror = mFFTSize - k;
        realOut[k] = realOut[mirror];
        imagOut[k] = -imagOut[mirror];
    }
}

//==============================================================================
// VCNoiseGate Implementation
//==============================================================================
VCNoiseGate::VCNoiseGate()
    : mSampleRate(44100.0)
    , mThreshold(-40.0f)
    , mAttackMs(5.0f)
    , mReleaseMs(50.0f)
    , mEnvelope(0.0f)
    , mGain(0.0f)
    , mAttackCoeff(0.0f)
    , mReleaseCoeff(0.0f)
{
    updateCoefficients();
}

VCNoiseGate::~VCNoiseGate() {}

void VCNoiseGate::prepare(double sampleRate)
{
    mSampleRate = sampleRate;
    updateCoefficients();
}

void VCNoiseGate::reset()
{
    mEnvelope = 0.0f;
    mGain = 0.0f;
}

void VCNoiseGate::updateCoefficients()
{
    // Exponential smoothing coefficients
    if (mAttackMs > 0.0f)
        mAttackCoeff = 1.0f - std::exp(-1.0f / (mAttackMs * 0.001f * (float)mSampleRate));
    else
        mAttackCoeff = 1.0f;

    if (mReleaseMs > 0.0f)
        mReleaseCoeff = 1.0f - std::exp(-1.0f / (mReleaseMs * 0.001f * (float)mSampleRate));
    else
        mReleaseCoeff = 1.0f;
}

void VCNoiseGate::process(float* left, float* right, int numSamples)
{
    float threshLin = VCPluginDSP::dBToLinear(mThreshold);

    for (int i = 0; i < numSamples; ++i) {
        // Compute instantaneous level (peak envelope)
        float level = std::max(std::abs(left[i]), std::abs(right[i]));

        // Envelope follower
        if (level > mEnvelope) {
            mEnvelope += mAttackCoeff * (level - mEnvelope);
        } else {
            mEnvelope += mReleaseCoeff * (level - mEnvelope);
        }

        // Gate logic
        float targetGain = (mEnvelope >= threshLin) ? 1.0f : 0.0f;

        // Smooth gain transition
        if (targetGain > mGain) {
            mGain += mAttackCoeff * (targetGain - mGain);
        } else {
            mGain += mReleaseCoeff * (targetGain - mGain);
        }

        left[i] *= mGain;
        right[i] *= mGain;
    }
}

//==============================================================================
// VCPluginDSP Implementation (Gen2)
//==============================================================================
VCPluginDSP::VCPluginDSP()
    : mFFT(FFT_SIZE)
    , mProfileLearned(false)
    , mLearnSamplesRemaining(0)
    , mInputWritePos(0)
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
    mParams.learnMs = 500.0f;
    mParams.reduction = 10.0f;
    mParams.floor = 5.0f;
    mParams.threshold = -40.0f;
    mParams.attack = 5.0f;
    mParams.release = 50.0f;
    mParams.mode = 2;  // Both
}

VCPluginDSP::~VCPluginDSP() {}

void VCPluginDSP::prepare(double sampleRate, int blockSize)
{
    mSampleRate = sampleRate;
    mBlockSize = blockSize;

    mInternalBuffer.resize(blockSize * 2);
    mInternalPtrs.resize(2);
    mInternalPtrs[0] = mInternalBuffer.data();
    mInternalPtrs[1] = mInternalBuffer.data() + blockSize;

    // Gen2: Initialize STFT buffers
    mInputBufferL.resize(FFT_SIZE, 0.0f);
    mInputBufferR.resize(FFT_SIZE, 0.0f);
    mOverlapL.resize(FFT_SIZE, 0.0f);
    mOverlapR.resize(FFT_SIZE, 0.0f);
    mFFTReal.resize(FFT_SIZE);
    mFFTImag.resize(FFT_SIZE);
    mOutFrameL.resize(FFT_SIZE, 0.0f);
    mOutFrameR.resize(FFT_SIZE, 0.0f);

    // Hann window
    mWindow.resize(FFT_SIZE);
    for (int i = 0; i < FFT_SIZE; ++i) {
        mWindow[i] = 0.5f * (1.0f - std::cos(2.0f * (float)VC_PI * i / (float)(FFT_SIZE - 1)));
    }

    mInputWritePos = 0;

    // Prepare spectral subtractor
    mSpectralSub.prepare(sampleRate, FFT_SIZE);

    // Prepare noise gate
    mNoiseGate.prepare(sampleRate);

    reset();
}

void VCPluginDSP::reset()
{
    // Gen1 reset
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

    // Gen2 reset
    std::fill(mInputBufferL.begin(), mInputBufferL.end(), 0.0f);
    std::fill(mInputBufferR.begin(), mInputBufferR.end(), 0.0f);
    std::fill(mOverlapL.begin(), mOverlapL.end(), 0.0f);
    std::fill(mOverlapR.begin(), mOverlapR.end(), 0.0f);
    std::fill(mOutFrameL.begin(), mOutFrameL.end(), 0.0f);
    std::fill(mOutFrameR.begin(), mOutFrameR.end(), 0.0f);
    mInputWritePos = 0;
    mProfileLearned = false;
    mLearnSamplesRemaining = 0;
    mNoiseProfile.reset();
    mSpectralSub.reset();
    mNoiseGate.reset();
}

//==============================================================================
// Gen1 noise generators (preserved verbatim)
//==============================================================================
float VCPluginDSP::randomUniform()
{
    mRandState = mRandState * 1103515245u + 12345u;
    return (2.0f * static_cast<float>(mRandState) / 4294967295.0f) - 1.0f;
}

float VCPluginDSP::generateWhite() { return randomUniform(); }

float VCPluginDSP::generatePink()
{
    mPinkIndex = (mPinkIndex + 1) & 0xFFFF;
    int numZeros = 0;
    int temp = mPinkIndex;
    while ((temp & 1) == 0 && numZeros < PINK_NUM_ROWS) { numZeros++; temp >>= 1; }
    if (numZeros < PINK_NUM_ROWS) {
        mPinkRunningSum -= mPinkRows[numZeros];
        mPinkRows[numZeros] = randomUniform();
        mPinkRunningSum += mPinkRows[numZeros];
    }
    float white = randomUniform();
    return (mPinkRunningSum + white) / (float)(PINK_NUM_ROWS + 1);
}

float VCPluginDSP::generateBrown()
{
    float white = randomUniform();
    mBrownState += 0.02f * white;
    mBrownState *= 0.998f;
    return mBrownState;
}

float VCPluginDSP::generateSine()
{
    float freq = mParams.frequency;
    float sample = std::sin(2.0f * (float)VC_PI * freq * (float)mPhase);
    mPhase += 1.0 / mSampleRate;
    if (mPhase >= 1.0) mPhase -= 1.0;
    return sample;
}

float VCPluginDSP::generateSweep()
{
    float startFreq = mParams.frequency;
    float endFreq = mParams.endFreq;
    float duration = mParams.sweepDuration;
    double t = mSamplePos / mSampleRate;
    float sample;
    if (mParams.sweepLog) {
        double ratio = (double)endFreq / (double)startFreq;
        double exponent = t / (double)duration;
        double currentFreq = (double)startFreq * std::pow(ratio, exponent);
        sample = (float)std::sin(2.0 * VC_PI * mPhase);
        mPhase += currentFreq / mSampleRate;
    } else {
        double currentFreq = (double)startFreq + ((double)endFreq - (double)startFreq) * t / (double)duration;
        sample = (float)std::sin(2.0 * VC_PI * mPhase);
        mPhase += currentFreq / mSampleRate;
    }
    if (t >= (double)duration) { mPhase = 0.0; mSamplePos = 0; }
    return sample;
}

float VCPluginDSP::generateImpulse()
{
    float periodSamples = mParams.pulsePeriod * (float)mSampleRate;
    if (mParams.pulsePeriod <= 0.0f) {
        if (!mImpulseFired) { mImpulseFired = true; return 1.0f; }
        return 0.0f;
    } else {
        if (mImpulseCounter == 0) { mImpulseCounter = (long long)periodSamples; return 1.0f; }
        mImpulseCounter--;
        return 0.0f;
    }
}

void VCPluginDSP::generate(float* left, float* right, int numSamples)
{
    if (!mEnabled) return;
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
        switch (mParams.channelMode) {
            case 0: left[i] = sample; right[i] = sample; break;
            case 1: left[i] = sample; right[i] = 0.0f; break;
            case 2: left[i] = 0.0f; right[i] = sample; break;
            case 3: left[i] = sample; right[i] = -sample; break;
            default: left[i] = sample; right[i] = sample; break;
        }
        mSamplePos++;
    }
}

//==============================================================================
// Gen2: Learn noise profile from input audio
//==============================================================================
void VCPluginDSP::learnNoiseProfile(const float* input, int numFrames, int channels)
{
    mNoiseProfile.reset();
    mProfileLearned = false;

    int learnSamples = (int)(mParams.learnMs * 0.001 * mSampleRate);
    int samplesToUse = std::min(learnSamples, numFrames);

    // Process in FFT-sized frames with hop
    for (int start = 0; start + FFT_SIZE <= samplesToUse; start += HOP_SIZE) {
        std::vector<float> windowed(FFT_SIZE);
        if (channels == 1) {
            for (int i = 0; i < FFT_SIZE; ++i) {
                windowed[i] = input[start + i] * mWindow[i];
            }
        } else {
            for (int i = 0; i < FFT_SIZE; ++i) {
                float l = input[(start + i) * channels];
                float r = input[(start + i) * channels + 1];
                windowed[i] = 0.5f * (l + r) * mWindow[i];
            }
        }

        std::vector<float> real(FFT_SIZE), imag(FFT_SIZE);
        mFFT.forward(windowed.data(), real.data(), imag.data());

        std::vector<float> mag(FFT_SIZE / 2 + 1);
        for (int k = 0; k <= FFT_SIZE / 2; ++k) {
            mag[k] = std::sqrt(real[k] * real[k] + imag[k] * imag[k]);
        }

        mNoiseProfile.accumulate(mag.data(), FFT_SIZE);
    }

    if (mNoiseProfile.getFrameCount() > 0) {
        mNoiseProfile.finalize();
        mProfileLearned = true;
    }
}//==============================================================================
// Gen2: Process input with learned noise profile (STFT-based)
//==============================================================================
void VCPluginDSP::processWithProfile(float* left, float* right, int numSamples)
{
    if (!mProfileLearned) return;

    ProcessMode mode = static_cast<ProcessMode>(mParams.mode);

    // Configure spectral subtractor
    mSpectralSub.setReduction(mParams.reduction);
    mSpectralSub.setFloor(mParams.floor);

    // Configure noise gate
    mNoiseGate.setThreshold(mParams.threshold);
    mNoiseGate.setAttack(mParams.attack);
    mNoiseGate.setRelease(mParams.release);

    // For Analyze mode, just output the noise profile info (silence output)
    if (mode == ProcessMode::Analyze) {
        // In analyze mode, pass through original signal unchanged
        // (the CLI will print the profile separately)
        return;
    }

    // STFT processing with overlap-add
    for (int i = 0; i < numSamples; ++i) {
        // Push into circular input buffer
        mInputBufferL[mInputWritePos] = left[i];
        mInputBufferR[mInputWritePos] = right[i];

        mInputWritePos++;
        if (mInputWritePos >= FFT_SIZE) {
            mInputWritePos = 0;
        }

        // Check if we have a full frame (at hop intervals)
        // We use a simpler approach: process when input buffer wraps
    }

    // Process in hop-based frames
    int totalInputSamples = numSamples;

    // Reconstruct from circular buffer and process frame by frame
    // Use a simpler block-based approach for the CLI
    // We'll process the entire block using overlap-add

    // Accumulate into a working buffer
    static std::vector<float> workL, workR;
    // Instead of complex overlap-add streaming, do frame-based processing on the input

    // Simple approach: process in FFT_SIZE blocks with HOP_SIZE advance
    for (int frameStart = 0; frameStart + FFT_SIZE <= numSamples; frameStart += HOP_SIZE) {
        float frameRealL[FFT_SIZE], frameImagL[FFT_SIZE];
        float frameRealR[FFT_SIZE], frameImagR[FFT_SIZE];
        float windowed[FFT_SIZE];

        // Process left channel
        for (int k = 0; k < FFT_SIZE; ++k) {
            windowed[k] = left[frameStart + k] * mWindow[k];
        }
        mFFT.forward(windowed, frameRealL, frameImagL);

        // Process right channel
        for (int k = 0; k < FFT_SIZE; ++k) {
            windowed[k] = right[frameStart + k] * mWindow[k];
        }
        mFFT.forward(windowed, frameRealR, frameImagR);

        // Spectral subtraction (if denoise mode)
        float cleanRealL[FFT_SIZE], cleanImagL[FFT_SIZE];
        float cleanRealR[FFT_SIZE], cleanImagR[FFT_SIZE];

        if (mode == ProcessMode::Denoise || mode == ProcessMode::Both) {
            mSpectralSub.process(frameRealL, frameImagL, cleanRealL, cleanImagL, mNoiseProfile);
            mSpectralSub.process(frameRealR, frameImagR, cleanRealR, cleanImagR, mNoiseProfile);
        } else {
            // Gate-only mode: pass through spectrum
            std::memcpy(cleanRealL, frameRealL, FFT_SIZE * sizeof(float));
            std::memcpy(cleanImagL, frameImagL, FFT_SIZE * sizeof(float));
            std::memcpy(cleanRealR, frameRealR, FFT_SIZE * sizeof(float));
            std::memcpy(cleanImagR, frameImagR, FFT_SIZE * sizeof(float));
        }

        // IFFT
        float outL[FFT_SIZE], outR[FFT_SIZE];
        mFFT.inverse(cleanRealL, cleanImagL, outL);
        mFFT.inverse(cleanRealR, cleanImagR, outR);

        // Apply window again and overlap-add
        for (int k = 0; k < FFT_SIZE; ++k) {
            float sampleL = outL[k] * mWindow[k];
            float sampleR = outR[k] * mWindow[k];

            int outIdx = frameStart + k;
            if (outIdx < numSamples) {
                left[outIdx] += sampleL;
                right[outIdx] += sampleR;
            }
        }
    }

    // Apply noise gate (if gate mode)
    if (mode == ProcessMode::Gate || mode == ProcessMode::Both) {
        mNoiseGate.process(left, right, numSamples);
    }

    // 输出限幅保护，防止谱减法过冲
    for (int i = 0; i < numSamples; ++i) {
        left[i] = std::clamp(left[i], -1.0f, 1.0f);
        right[i] = std::clamp(right[i], -1.0f, 1.0f);
    }
}

//==============================================================================
// Process (Gen2: now supports both generate and noise profile processing)
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
    if (!mEnabled) return;

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
// Set parameters
//==============================================================================
void VCPluginDSP::setParams(const Params& p) { mParams = p; }
VCPluginDSP::Params VCPluginDSP::getParams() const { return mParams; }
void VCPluginDSP::setEnabled(bool enabled) { mEnabled = enabled; }
