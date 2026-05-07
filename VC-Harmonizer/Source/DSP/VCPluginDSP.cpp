#include "VCPluginDSP.h"

#ifdef VC_STANDALONE
#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>
#include <cstdio>
#endif

//==============================================================================
// YIN Pitch Detector Implementation (from VC-Tune)
//==============================================================================

YINPitchDetector::YINPitchDetector(double sampleRate, int bufferSize)
    : mSampleRate(sampleRate)
    , mBufferSize(bufferSize)
{
    mD.resize(bufferSize / 2, 0.0f);
    mDPrime.resize(bufferSize / 2, 0.0f);
}

void YINPitchDetector::reset()
{
    std::fill(mD.begin(), mD.end(), 0.0f);
    std::fill(mDPrime.begin(), mDPrime.end(), 0.0f);
    mLastRefinedTau = 0.0f;
}

void YINPitchDetector::differenceFunction(const float* samples, int numSamples)
{
    int halfN = mBufferSize / 2;
    int W = std::min(halfN, numSamples / 2);

    for (int tau = 0; tau < halfN; tau++) {
        float sum = 0.0f;
        for (int j = 0; j < W; j++) {
            float diff = samples[j] - samples[j + tau];
            sum += diff * diff;
        }
        mD[tau] = sum;
    }
}

void YINPitchDetector::cumulativeMeanNormalized()
{
    int halfN = mBufferSize / 2;
    mDPrime[0] = 1.0f;
    float runningSum = 0.0f;

    for (int tau = 1; tau < halfN; tau++) {
        runningSum += mD[tau];
        mDPrime[tau] = (runningSum > 0.0f) ? mD[tau] * tau / runningSum : 1.0f;
    }
}

int YINPitchDetector::absoluteThreshold()
{
    int halfN = mBufferSize / 2;
    int minTau = std::max(2, static_cast<int>(mSampleRate / 2000.0));
    int maxTau = std::min(halfN - 1, static_cast<int>(mSampleRate / 50.0));

    for (int tau = minTau; tau <= maxTau; tau++) {
        if (mDPrime[tau] < mThreshold) {
            while (tau + 1 <= maxTau && mDPrime[tau + 1] < mDPrime[tau]) {
                tau++;
            }
            return tau;
        }
    }

    int minIdx = minTau;
    float minVal = mDPrime[minTau];
    for (int tau = minTau + 1; tau <= maxTau; tau++) {
        if (mDPrime[tau] < minVal) {
            minVal = mDPrime[tau];
            minIdx = tau;
        }
    }
    return minIdx;
}

float YINPitchDetector::parabolicInterpolation(int tau)
{
    if (tau <= 0 || tau >= static_cast<int>(mDPrime.size()) - 1) {
        return static_cast<float>(tau);
    }
    float s0 = mDPrime[tau - 1];
    float s1 = mDPrime[tau];
    float s2 = mDPrime[tau + 1];

    float denominator = 2.0f * (2.0f * s1 - s2 - s0);
    if (std::fabs(denominator) < 1e-10f) {
        return static_cast<float>(tau);
    }
    float shift = (s2 - s0) / denominator;
    return static_cast<float>(tau) + VC_JCLAMP(shift, -0.5f, 0.5f);
}

YINPitchDetector::Result YINPitchDetector::detect(const float* samples, int numSamples)
{
    Result result;

    if (numSamples < mBufferSize / 2) {
        result.voiced = false;
        return result;
    }

    differenceFunction(samples, numSamples);
    cumulativeMeanNormalized();
    int tau = absoluteThreshold();
    float refinedTau = parabolicInterpolation(tau);

    mLastRefinedTau = refinedTau;

    if (refinedTau > 0.0f) {
        result.frequency = static_cast<float>(mSampleRate) / refinedTau;
        result.confidence = 1.0f - mDPrime[tau];
        result.voiced = (result.confidence > 0.3f) &&
                        (result.frequency >= 50.0f && result.frequency <= 2000.0f);
    }

    if (!result.voiced) {
        result.frequency = 0.0f;
        result.confidence = 0.0f;
        mLastRefinedTau = 0.0f;
    }

    return result;
}

//==============================================================================
// Scale Quantizer Implementation (from VC-Tune)
//==============================================================================

const bool ScaleQuantizer::mScales[6][12] = {
    {1,1,1,1,1,1,1,1,1,1,1,1},   // Chromatic
    {1,0,1,0,1,1,0,1,0,1,0,1},   // Major
    {1,0,1,1,0,1,0,1,1,0,1,0},   // Minor
    {1,0,1,0,1,0,0,1,0,1,0,0},   // Pentatonic
    {1,0,0,1,0,1,1,1,0,0,1,0},   // Blues
    {0,0,0,0,0,0,0,0,0,0,0,0}    // Custom
};

ScaleQuantizer::ScaleQuantizer()
{
    std::fill(mCustomScale, mCustomScale + 12, false);
}

void ScaleQuantizer::setCustomScale(const bool scale[12])
{
    for (int i = 0; i < 12; i++) {
        mCustomScale[i] = scale[i];
    }
}

int ScaleQuantizer::frequencyToNote(float frequency)
{
    if (frequency <= 0.0f) return 0;
    return static_cast<int>(std::round(69.0f + 12.0f * std::log2(frequency / 440.0f)));
}

float ScaleQuantizer::noteToFrequency(int note)
{
    return 440.0f * std::pow(2.0f, (note - 69) / 12.0);
}

float ScaleQuantizer::noteToFrequencyFloat(float note)
{
    return 440.0f * std::pow(2.0f, (note - 69.0f) / 12.0);
}

const char* ScaleQuantizer::noteName(int note)
{
    static const char* names[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    return names[((note % 12) + 12) % 12];
}

const char* ScaleQuantizer::noteNameWithOctave(int note)
{
    static thread_local char buf[16];
    int octave = (note / 12) - 1;
    snprintf(buf, sizeof(buf), "%s%d", noteName(note), octave);
    return buf;
}

const char* ScaleQuantizer::scaleName(Scale s)
{
    static const char* names[] = {"Chromatic","Major","Minor","Pentatonic","Blues","Custom"};
    return names[static_cast<int>(s)];
}

bool ScaleQuantizer::isNoteInScale(int note, Scale scale, int keyOffset) const
{
    int pc = ((note % 12) + 12) % 12;
    int shifted = ((pc - keyOffset) % 12 + 12) % 12;
    if (scale == Custom) {
        return mCustomScale[shifted];
    }
    return mScales[static_cast<int>(scale)][shifted];
}

int ScaleQuantizer::quantizeToNote(float frequency, Scale scale, int keyOffset, float transpose)
{
    if (frequency <= 0.0f) return 0;

    float noteFloat = 69.0f + 12.0f * std::log2(frequency / 440.0f);
    noteFloat += transpose;

    int noteInt = static_cast<int>(std::round(noteFloat));

    if (isNoteInScale(noteInt, scale, keyOffset)) {
        return noteInt;
    }

    int bestNote = noteInt;
    float bestDist = 1e10f;

    for (int offset = -6; offset <= 6; offset++) {
        int candidate = noteInt + offset;
        if (isNoteInScale(candidate, scale, keyOffset)) {
            float dist = std::fabs(static_cast<float>(candidate) - noteFloat);
            if (candidate < noteInt) dist -= 0.02f;
            if (dist < bestDist) {
                bestDist = dist;
                bestNote = candidate;
            }
        }
    }

    return bestNote;
}

float ScaleQuantizer::quantize(float frequency, Scale scale, int keyOffset,
                                float transpose, float cents)
{
    if (frequency <= 0.0f) return 0.0f;

    int targetNote = quantizeToNote(frequency, scale, keyOffset, transpose);
    float targetFreq = noteToFrequency(targetNote);

    if (std::fabs(cents) > 0.001f) {
        targetFreq *= std::pow(2.0f, cents / 1200.0f);
    }

    return targetFreq;
}

//==============================================================================
// Key Detector Implementation (Krumhansl-Schmuckler) (from VC-Tune)
//==============================================================================

const float KeyDetector::KSMajorProfile[12] = {
    6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f
};

const float KeyDetector::KSMinorProfile[12] = {
    6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f
};

KeyDetector::KeyDetector(double sampleRate)
    : mSampleRate(sampleRate)
{
    reset();
}

void KeyDetector::reset()
{
    std::fill(mChroma, mChroma + 12, 0.0f);
}

void KeyDetector::rotateProfile(const float* in, float* out, int shift)
{
    for (int i = 0; i < 12; i++) {
        out[i] = in[((i - shift) % 12 + 12) % 12];
    }
}

float KeyDetector::pearsonCorrelation(const float* x, const float* y, int n)
{
    float sumX = 0.0f, sumY = 0.0f;
    float sumXY = 0.0f, sumX2 = 0.0f, sumY2 = 0.0f;

    for (int i = 0; i < n; i++) {
        sumX += x[i]; sumY += y[i];
        sumXY += x[i] * y[i];
        sumX2 += x[i] * x[i]; sumY2 += y[i] * y[i];
    }

    float num = n * sumXY - sumX * sumY;
    float den = std::sqrt((n * sumX2 - sumX * sumX) * (n * sumY2 - sumY * sumY));

    if (den < 1e-10f) return 0.0f;
    return num / den;
}

KeyDetector::Result KeyDetector::detect(const float* left, const float* right,
                                         int numSamples, YINPitchDetector& yin)
{
    Result result;
    reset();

    int maxAnalysisSamples = static_cast<int>(8.0 * mSampleRate);
    int analysisLength = std::min(numSamples, maxAnalysisSamples);

    int numDetections = 0;
    for (int pos = 0; pos + mAnalysisFrameSize <= analysisLength; pos += mAnalysisHopSize) {
        auto pitch = yin.detect(left + pos, mAnalysisFrameSize);

        if (pitch.voiced && pitch.frequency > 0.0f && pitch.confidence > 0.3f) {
            int note = ScaleQuantizer::frequencyToNote(pitch.frequency);
            int pitchClass = ((note % 12) + 12) % 12;
            mChroma[pitchClass] += pitch.confidence;
            numDetections++;
        }
    }

    if (numDetections < 10) {
        result.detected = false;
        return result;
    }

    float maxChroma = *std::max_element(mChroma, mChroma + 12);
    if (maxChroma > 0.0f) {
        for (int i = 0; i < 12; i++) {
            mChroma[i] /= maxChroma;
        }
    }

    float bestCorr = -2.0f;
    int bestKey = 0;
    bool bestIsMajor = true;

    for (int key = 0; key < 12; key++) {
        float rotatedMajor[12], rotatedMinor[12];
        rotateProfile(KSMajorProfile, rotatedMajor, key);
        rotateProfile(KSMinorProfile, rotatedMinor, key);

        float corrMajor = pearsonCorrelation(mChroma, rotatedMajor, 12);
        float corrMinor = pearsonCorrelation(mChroma, rotatedMinor, 12);

        if (corrMajor > bestCorr) {
            bestCorr = corrMajor;
            bestKey = key;
            bestIsMajor = true;
        }
        if (corrMinor > bestCorr) {
            bestCorr = corrMinor;
            bestKey = key;
            bestIsMajor = false;
        }
    }

    result.key = bestKey;
    result.isMajor = bestIsMajor;
    result.confidence = bestCorr;
    result.detected = (bestCorr > 0.3f);

    return result;
}

const char* KeyDetector::keyName(int key, bool isMajor)
{
    static const char* keyNames[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    static thread_local char buf[16];
    snprintf(buf, sizeof(buf), "%s %s", keyNames[key], isMajor ? "Major" : "Minor");
    return buf;
}

//==============================================================================
// LPC Formant Extractor Implementation (from VC-Tune Gen2)
//==============================================================================

LPCFormantExtractor::LPCFormantExtractor()
{
    reset();
}

void LPCFormantExtractor::reset()
{
    std::fill(mAutocorr, mAutocorr + LPC_ORDER + 1, 0.0f);
    std::fill(mReflection, mReflection + LPC_ORDER + 1, 0.0f);
}

void LPCFormantExtractor::autocorrelation(const float* frame, int frameSize)
{
    for (int k = 0; k <= LPC_ORDER; k++) {
        float sum = 0.0f;
        for (int n = 0; n < frameSize - k; n++) {
            sum += frame[n] * frame[n + k];
        }
        mAutocorr[k] = sum;
    }
}

LPCFormantExtractor::LPCResult LPCFormantExtractor::analyze(const float* frame, int frameSize)
{
    LPCResult result;
    std::fill(result.a, result.a + LPC_ORDER + 1, 0.0f);
    result.a[0] = 1.0f;
    result.gain = 1.0f;

    if (frameSize <= LPC_ORDER + 1) return result;

    autocorrelation(frame, frameSize);

    // Levinson-Durbin recursion
    float a[LPC_ORDER + 1];
    float aPrev[LPC_ORDER + 1];
    std::fill(a, a + LPC_ORDER + 1, 0.0f);
    std::fill(aPrev, aPrev + LPC_ORDER + 1, 0.0f);
    a[0] = 1.0f;
    aPrev[0] = 1.0f;

    float error = mAutocorr[0];
    if (error < 1e-10f) return result;

    for (int p = 1; p <= LPC_ORDER; p++) {
        float k = 0.0f;
        for (int j = 0; j < p; j++) {
            k += aPrev[j] * mAutocorr[p - j];
        }
        k = -k / error;

        mReflection[p] = k;

        for (int j = 0; j <= p; j++) {
            a[j] = aPrev[j] + k * aPrev[p - j];
        }

        error = error * (1.0f - k * k);
        if (error < 1e-10f) break;

        for (int j = 0; j <= p; j++) {
            aPrev[j] = a[j];
        }
    }

    for (int i = 0; i <= LPC_ORDER; i++) {
        result.a[i] = a[i];
    }
    result.gain = std::sqrt(std::max(error, 1e-10f));

    return result;
}

void LPCFormantExtractor::inverseFilter(const float* input, int numSamples,
                                         const float* a, float* excitation)
{
    for (int n = 0; n < numSamples; n++) {
        float x = input[n];
        float y = x;
        for (int k = 1; k <= LPC_ORDER; k++) {
            if (n - k >= 0) {
                y += a[k] * input[n - k];
            }
        }
        excitation[n] = y;
    }
}

void LPCFormantExtractor::synthesisFilter(const float* excitation, int numSamples,
                                           const float* a, float gain, float* output)
{
    for (int n = 0; n < numSamples; n++) {
        float y = excitation[n] * gain;
        for (int k = 1; k <= LPC_ORDER; k++) {
            if (n - k >= 0) {
                y -= a[k] * output[n - k];
            }
        }
        output[n] = y;
    }
}

void LPCFormantExtractor::shiftFormants(const float* aIn, float shiftSemitones,
                                         float sampleRate, float* aOut)
{
    float ratio = std::pow(2.0f, shiftSemitones / 12.0f);

    // Initialize output
    aOut[0] = 1.0f;
    for (int i = 1; i <= LPC_ORDER; i++) {
        aOut[i] = aIn[i];
    }

    // Convert LPC to LSF (Line Spectral Frequencies), shift, convert back
    // Simplified approach: frequency-scale the reflection coefficients
    for (int i = 1; i <= LPC_ORDER; i++) {
        aOut[i] = aIn[i] * std::pow(ratio, static_cast<float>(i) * 0.1f);
    }

    // Normalize
    float sumSq = 0.0f;
    for (int i = 1; i <= LPC_ORDER; i++) {
        sumSq += aOut[i] * aOut[i];
    }
    if (sumSq > 0.95f) {
        float scale = 0.95f / std::sqrt(sumSq);
        for (int i = 1; i <= LPC_ORDER; i++) {
            aOut[i] *= scale;
        }
    }
}

//==============================================================================
// Simple Resampler Implementation
//==============================================================================

void SimpleResampler::resample(const float* input, int inputLen,
                                float* output, int outputLen, float pitchRatio)
{
    if (outputLen <= 0 || inputLen <= 0) return;

    // For pitch shift up (ratio > 1), we need to "slow down" the playback
    // which means more input samples per output sample
    float ratio = pitchRatio;  // input step per output sample

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
// Harmony Voice Implementation
//==============================================================================

HarmonyVoice::HarmonyVoice()
{
}

void HarmonyVoice::prepare(double sampleRate, int blockSize)
{
    mSampleRate = sampleRate;
    mBlockSize = blockSize;
    mShiftedBuffer.resize(blockSize * 2, 0.0f);
    mExcitationBuffer.resize(blockSize * 2, 0.0f);
    mSynthesisBuffer.resize(blockSize * 2, 0.0f);
}

void HarmonyVoice::processVoice(const float* input, int numSamples,
                                 float intervalSemitones,
                                 float formantPreserve,
                                 const LPCFormantExtractor::LPCResult* lpcResult,
                                 float* outLeft, float* outRight)
{
    // Ensure buffers are large enough
    if ((int)mShiftedBuffer.size() < numSamples) {
        mShiftedBuffer.resize(numSamples, 0.0f);
        mExcitationBuffer.resize(numSamples, 0.0f);
        mSynthesisBuffer.resize(numSamples, 0.0f);
    }

    float pitchRatio = std::pow(2.0f, intervalSemitones / 12.0f);
    float gainLinear = VCPluginDSP::dBToLinear(mVoiceParams.gainDB);

    // Method: Simple resampling for pitch shift
    // Resample input at different rate to shift pitch
    // output[i] = input[i * pitchRatio] (linear interpolated)
    SimpleResampler::resample(input, numSamples,
                              mShiftedBuffer.data(), numSamples, pitchRatio);

    // LPC formant preservation: extract original formants and re-apply
    if (formantPreserve > 0.1f && lpcResult != nullptr) {
        // Inverse filter to get excitation (removes formants from shifted signal)
        // For the shifted signal, we need to analyze its own formants
        // Simple approach: use the original LPC coefficients to shape the shifted output

        // The shifted signal has shifted formants. We want to restore original formants.
        // Step 1: Remove shifted formants (inverse filter with shifted LPC)
        // Step 2: Re-apply original formants (synthesis filter with original LPC)

        // Simplified: blend between shifted output and formant-corrected output
        float blend = formantPreserve / 100.0f;

        if (blend > 0.01f) {
            // Apply synthesis filter with original LPC to shape the excitation
            // First, extract excitation from shifted signal
            LPCFormantExtractor lpcLocal;
            auto shiftedLPC = lpcLocal.analyze(mShiftedBuffer.data(),
                                                std::min(numSamples, 2048));

            lpcLocal.inverseFilter(mShiftedBuffer.data(), numSamples,
                                   shiftedLPC.a, mExcitationBuffer.data());

            // Re-apply original formants
            lpcLocal.synthesisFilter(mExcitationBuffer.data(), numSamples,
                                     lpcResult->a, lpcResult->gain,
                                     mSynthesisBuffer.data());

            // Blend: formantPreserve controls how much we restore original formants
            for (int i = 0; i < numSamples; i++) {
                mShiftedBuffer[i] = mShiftedBuffer[i] * (1.0f - blend) +
                                    mSynthesisBuffer[i] * blend;
            }
        }
    }

    // Apply gain and pan
    float panL = std::cos((mVoiceParams.pan + 1.0f) * 0.25f * VC_PI);
    float panR = std::sin((mVoiceParams.pan + 1.0f) * 0.25f * VC_PI);

    for (int i = 0; i < numSamples; i++) {
        float sample = mShiftedBuffer[i] * gainLinear;
        outLeft[i] += sample * panL;
        outRight[i] += sample * panR;
    }
}

void HarmonyVoice::reset()
{
    std::fill(mShiftedBuffer.begin(), mShiftedBuffer.end(), 0.0f);
    std::fill(mExcitationBuffer.begin(), mExcitationBuffer.end(), 0.0f);
    std::fill(mSynthesisBuffer.begin(), mSynthesisBuffer.end(), 0.0f);
}

//==============================================================================
// VCPluginDSP (VC-Harmonizer) Main Class Implementation
//==============================================================================

VCPluginDSP::VCPluginDSP()
{
    mParams.numVoices = 2;
    mParams.intervals[0] = 3;    // minor 3rd up
    mParams.intervals[1] = 7;    // 5th up
    mParams.intervals[2] = 12;   // octave up
    mParams.intervals[3] = -5;   // 4th down
    mParams.voiceGain[0] = 0.0f;
    mParams.voiceGain[1] = 0.0f;
    mParams.voiceGain[2] = 0.0f;
    mParams.voiceGain[3] = 0.0f;
    mParams.voicePan[0] = -0.5f;
    mParams.voicePan[1] = 0.5f;
    mParams.voicePan[2] = 0.7f;
    mParams.voicePan[3] = -0.7f;
    mParams.formantPreserve = 100.0f;
    mParams.autoKey = false;
    mParams.scale = 0;
    mParams.direction = 0;  // both
    mParams.bypass = false;
    mParams.midiTrack = -1;
}

VCPluginDSP::~VCPluginDSP()
{
}

void VCPluginDSP::prepare(double sampleRate, int blockSize)
{
    mSampleRate = sampleRate;
    mBlockSize = blockSize;

    mPitchDetector.setSampleRate(sampleRate);
    mKeyDetector = KeyDetector(sampleRate);

    for (int v = 0; v < MAX_VOICES; v++) {
        mVoices[v].prepare(sampleRate, blockSize);
    }

    mInternalBuffer.resize(blockSize * 2);
    mInternalPtrs.resize(2);
    mInternalPtrs[0] = mInternalBuffer.data();
    mInternalPtrs[1] = mInternalBuffer.data() + blockSize;
}

int VCPluginDSP::computeEffectiveInterval(int baseInterval) const
{
    // direction: 0=both (use as-is), 1=up only (force positive), 2=down only (force negative)
    if (mParams.direction == 1 && baseInterval < 0) {
        return -baseInterval;  // Flip negative intervals to positive
    }
    if (mParams.direction == 2 && baseInterval > 0) {
        return -baseInterval;  // Flip positive intervals to negative
    }
    return baseInterval;
}

int VCPluginDSP::quantizeNoteToScale(int note, ScaleQuantizer::Scale scale, int keyOffset) const
{
    // Find the nearest note in scale
    if (mQuantizer.isNoteInScale(note, scale, keyOffset)) {
        return note;
    }

    int bestNote = note;
    float bestDist = 1e10f;

    for (int offset = -6; offset <= 6; offset++) {
        int candidate = note + offset;
        if (mQuantizer.isNoteInScale(candidate, scale, keyOffset)) {
            float dist = std::fabs(static_cast<float>(offset));
            if (dist < bestDist) {
                bestDist = dist;
                bestNote = candidate;
            }
        }
    }

    return bestNote;
}

void VCPluginDSP::process(float* left, float* right, int numSamples)
{
    if (!mEnabled || mParams.bypass) return;

    // Auto key detection (only on first call)
    if (mParams.autoKey && !mKeyDetected) {
        mKeyResult = mKeyDetector.detect(left, right, numSamples, mPitchDetector);
        mKeyDetected = true;

        if (mKeyResult.detected) {
            mAutoKeyOffset = mKeyResult.key;
            mAutoScale = mKeyResult.isMajor ? ScaleQuantizer::Major : ScaleQuantizer::Minor;
        }
    }

    ScaleQuantizer::Scale effectiveScale = static_cast<ScaleQuantizer::Scale>(mParams.scale);
    int effectiveKeyOffset = 0;

    if (mParams.autoKey && mKeyResult.detected) {
        effectiveScale = mAutoScale;
        effectiveKeyOffset = mAutoKeyOffset;
    }

    // Use left channel as lead vocal for pitch detection
    const float* leadSignal = left;

    // Frame-by-frame analysis and harmony generation
    int numFrames = 1;
    if (numSamples > mFrameSize) {
        numFrames = (numSamples - mFrameSize) / mHopSize + 1;
    }

    // Allocate per-voice stereo output buffers (accumulate harmony voices)
    std::vector<float> harmonyL(numSamples, 0.0f);
    std::vector<float> harmonyR(numSamples, 0.0f);

    // For each voice, allocate mono buffer
    for (int v = 0; v < MAX_VOICES; v++) {
        if ((int)mVoiceMono[v].size() < numSamples) {
            mVoiceMono[v].resize(numSamples, 0.0f);
        }
        std::fill(mVoiceMono[v].begin(), mVoiceMono[v].end(), 0.0f);
    }

    // Per-frame processing
    for (int frame = 0; frame < numFrames; frame++) {
        int frameStart = frame * mHopSize;
        int frameLen = std::min(mFrameSize, numSamples - frameStart);

        // Detect pitch in this frame
        auto pitchResult = mPitchDetector.detect(leadSignal + frameStart, frameLen);

        // Build frame info for reporting
        FrameInfo fi;
        fi.detectedF0 = pitchResult.frequency;
        fi.confidence = pitchResult.confidence;
        fi.voiced = pitchResult.voiced;

        if (pitchResult.voiced && pitchResult.frequency > 0.0f) {
            int detectedNote = ScaleQuantizer::frequencyToNote(pitchResult.frequency);
            fi.detectedNote = detectedNote;

            // For each active voice, compute the harmony pitch
            for (int v = 0; v < mParams.numVoices && v < MAX_VOICES; v++) {
                int interval = computeEffectiveInterval(mParams.intervals[v]);
                int targetNote = detectedNote + interval;

                // Quantize to scale if not chromatic
                if (effectiveScale != ScaleQuantizer::Chromatic) {
                    targetNote = quantizeNoteToScale(targetNote, effectiveScale, effectiveKeyOffset);
                    // Recalculate effective interval after quantization
                    interval = targetNote - detectedNote;
                }

                fi.voiceNote[v] = targetNote;
                fi.voiceInterval[v] = interval;
                fi.voiceF0[v] = ScaleQuantizer::noteToFrequency(targetNote);
            }
        }

        if (mReportMode) {
            mFrameInfo.push_back(fi);
        }
    }

    // Now process each voice across the entire signal
    // For simplicity and audio quality, we process the whole signal per voice
    // using a smooth pitch contour

    // LPC analysis of original signal for formant preservation
    std::vector<LPCFormantExtractor::LPCResult> lpcResults;
    bool useLPC = (mParams.formantPreserve > 0.1f);

    if (useLPC) {
        lpcResults.resize(numFrames);
        for (int f = 0; f < numFrames; f++) {
            int pos = f * mHopSize;
            int frameLen = std::min(mFrameSize, numSamples - pos);

            // Pre-emphasis for better LPC analysis
            std::vector<float> emphasized(frameLen);
            float preEmph = 0.97f;
            emphasized[0] = (pos < numSamples) ? leadSignal[pos] : 0.0f;
            for (int i = 1; i < frameLen && (pos + i) < numSamples; i++) {
                emphasized[i] = leadSignal[pos + i] - preEmph * leadSignal[pos + i - 1];
            }

            lpcResults[f] = mLPC.analyze(emphasized.data(), frameLen);
        }
    }

    // Process each harmony voice
    for (int v = 0; v < mParams.numVoices && v < MAX_VOICES; v++) {
        // Set voice params from main params
        HarmonyVoice::Params vp;
        vp.intervalSemitones = computeEffectiveInterval(mParams.intervals[v]);
        vp.gainDB = mParams.voiceGain[v];
        vp.pan = mParams.voicePan[v];
        vp.enabled = true;
        mVoices[v].setVoiceParams(vp);  // We need to add setParams... let me handle differently

        // We'll pass interval and formant preserve directly
        int interval = computeEffectiveInterval(mParams.intervals[v]);

        // For scale-quantized harmonies, we need per-frame pitch shifting
        // Simplified approach: use the average interval for the whole signal
        // then do per-frame correction via overlap-add

        if (effectiveScale != ScaleQuantizer::Chromatic && numFrames > 0) {
            // Compute average interval across voiced frames for this voice
            float avgInterval = 0.0f;
            int voicedCount = 0;
            for (int f = 0; f < numFrames; f++) {
                if (mReportMode && mFrameInfo.size() > static_cast<size_t>(f)) {
                    if (mFrameInfo[f].voiced) {
                        avgInterval += static_cast<float>(mFrameInfo[f].voiceInterval[v]);
                        voicedCount++;
                    }
                }
            }

            if (voicedCount > 0) {
                avgInterval /= voicedCount;
                interval = static_cast<int>(std::round(avgInterval));
            }
        }

        // Get LPC result for this voice (use middle frame)
        const LPCFormantExtractor::LPCResult* lpcPtr = nullptr;
        if (useLPC && lpcResults.size() > 0) {
            lpcPtr = &lpcResults[lpcResults.size() / 2];
        }

        // Set voice pan and gain via a temporary params approach
        HarmonyVoice::Params voiceP;
        voiceP.gainDB = mParams.voiceGain[v];
        voiceP.pan = mParams.voicePan[v];
        mVoices[v].setVoiceParams(voiceP);

        // Process voice: input → pitch shift → formant preserve → pan → accumulate
        mVoices[v].processVoice(leadSignal, numSamples,
                                static_cast<float>(interval),
                                mParams.formantPreserve,
                                lpcPtr,
                                harmonyL.data(), harmonyR.data());
    }

    // Mix: output = original (dry) + harmony (wet)
    // Apply normalization factor to prevent clipping from voice stacking
    // N voices + 1 dry signal = (N+1) sources; normalize by 1/(N+1) then scale to taste
    int numActiveVoices = std::min(mParams.numVoices, MAX_VOICES);
    float normFactor = 1.0f / (1.0f + static_cast<float>(numActiveVoices));

    for (int i = 0; i < numSamples; i++) {
        // Normalize: dry + wet scaled so total energy is preserved
        left[i] = left[i] * normFactor + harmonyL[i] * normFactor;
        right[i] = right[i] * normFactor + harmonyR[i] * normFactor;
    }

    // Output hard-clip protection (safety net)
    for (int i = 0; i < numSamples; i++) {
        left[i] = std::clamp(left[i], -1.0f, 1.0f);
        right[i] = std::clamp(right[i], -1.0f, 1.0f);
    }
}

#ifndef VC_STANDALONE
void VCPluginDSP::process(juce::dsp::AudioBlock<float>& block)
{
    if (!mEnabled || mParams.bypass) return;

    auto numSamples = static_cast<int>(block.getNumSamples());
    auto numChannels = static_cast<int>(block.getNumChannels());

    std::vector<float> left(numSamples), right(numSamples);
    if (numChannels >= 2) {
        auto* lCh = block.getChannelPointer(0);
        auto* rCh = block.getChannelPointer(1);
        std::memcpy(left.data(), lCh, numSamples * sizeof(float));
        std::memcpy(right.data(), rCh, numSamples * sizeof(float));
    } else {
        auto* ch = block.getChannelPointer(0);
        std::memcpy(left.data(), ch, numSamples * sizeof(float));
        std::memcpy(right.data(), ch, numSamples * sizeof(float));
    }

    process(left.data(), right.data(), numSamples);

    if (numChannels >= 2) {
        std::memcpy(block.getChannelPointer(0), left.data(), numSamples * sizeof(float));
        std::memcpy(block.getChannelPointer(1), right.data(), numSamples * sizeof(float));
    } else {
        for (int i = 0; i < numSamples; i++) {
            block.getChannelPointer(0)[i] = (left[i] + right[i]) * 0.5f;
        }
    }
}
#endif

void VCPluginDSP::reset()
{
    mPitchDetector.reset();
    mKeyDetector.reset();
    mLPC.reset();
    for (int v = 0; v < MAX_VOICES; v++) {
        mVoices[v].reset();
    }
    mKeyDetected = false;
    mFrameInfo.clear();
}

void VCPluginDSP::setParams(const Params& p)
{
    mParams = p;
}

VCPluginDSP::Params VCPluginDSP::getParams() const
{
    return mParams;
}

void VCPluginDSP::setEnabled(bool enabled)
{
    mEnabled = enabled;
}
