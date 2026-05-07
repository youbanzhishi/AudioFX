#include "VCPluginDSP.h"

#ifdef VC_STANDALONE
#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>
#include <cstdio>
#endif

//==============================================================================
// YIN Pitch Detector Implementation
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
}

void YINPitchDetector::differenceFunction(const float* samples, int numSamples)
{
    int halfN = mBufferSize / 2;
    // Use at most halfN samples
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

    // Search for first tau where d' < threshold
    // Limit search to valid frequency range (50~2000Hz)
    int minTau = std::max(2, static_cast<int>(mSampleRate / 2000.0));
    int maxTau = std::min(halfN - 1, static_cast<int>(mSampleRate / 50.0));

    for (int tau = minTau; tau <= maxTau; tau++) {
        if (mDPrime[tau] < mThreshold) {
            // Find the local minimum after crossing threshold
            while (tau + 1 <= maxTau && mDPrime[tau + 1] < mDPrime[tau]) {
                tau++;
            }
            return tau;
        }
    }

    // No tau below threshold: find global minimum in valid range
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
    // Clamp shift to reasonable range
    return static_cast<float>(tau) + VC_JCLAMP(shift, -0.5f, 0.5f);
}

YINPitchDetector::Result YINPitchDetector::detect(const float* samples, int numSamples)
{
    Result result;

    if (numSamples < mBufferSize / 2) {
        result.voiced = false;
        return result;
    }

    // Step 1: Difference function
    differenceFunction(samples, numSamples);

    // Step 2: Cumulative mean normalization
    cumulativeMeanNormalized();

    // Step 3: Absolute threshold
    int tau = absoluteThreshold();

    // Step 4: Parabolic interpolation for sub-sample precision
    float refinedTau = parabolicInterpolation(tau);

    // Step 5: Convert to frequency
    if (refinedTau > 0.0f) {
        result.frequency = static_cast<float>(mSampleRate) / refinedTau;
        result.confidence = 1.0f - mDPrime[tau];

        // Check if the signal is voiced
        // Confidence threshold: if d' at the found tau is too high, likely unvoiced
        result.voiced = (result.confidence > 0.3f) &&
                        (result.frequency >= 50.0f && result.frequency <= 2000.0f);
    }

    if (!result.voiced) {
        result.frequency = 0.0f;
        result.confidence = 0.0f;
    }

    return result;
}

//==============================================================================
// Scale Quantizer Implementation
//==============================================================================

const bool ScaleQuantizer::mScales[6][12] = {
    // Chromatic: all 12 semitones
    {1,1,1,1,1,1,1,1,1,1,1,1},
    // Major: C D E F G A B
    {1,0,1,0,1,1,0,1,0,1,0,1},
    // Minor (natural): C D Eb F G Ab Bb
    {1,0,1,1,0,1,0,1,1,0,1,0},
    // Pentatonic (major): C D E G A
    {1,0,1,0,1,0,0,1,0,1,0,0},
    // Blues (minor blues): C Eb F Gb G Bb
    {1,0,0,1,0,1,1,1,0,0,1,0},
    // Custom (default: all off)
    {0,0,0,0,0,0,0,0,0,0,0,0}
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
    // Copy to mScales[5] is not possible (const), so we use mCustomScale directly
}

int ScaleQuantizer::frequencyToNote(float frequency)
{
    if (frequency <= 0.0f) return 0;
    // MIDI note: A4 = 69 = 440Hz
    return static_cast<int>(std::round(69.0f + 12.0f * std::log2(frequency / 440.0f)));
}

float ScaleQuantizer::noteToFrequency(int note)
{
    return 440.0f * std::pow(2.0f, (note - 69) / 12.0f);
}

float ScaleQuantizer::noteToFrequencyFloat(float note)
{
    return 440.0f * std::pow(2.0f, (note - 69.0f) / 12.0f);
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
    int pc = ((note % 12) + 12) % 12;  // pitch class 0-11
    // Apply key offset (rotate the scale)
    int shifted = ((pc - keyOffset) % 12 + 12) % 12;

    if (scale == Custom) {
        return mCustomScale[shifted];
    }
    return mScales[static_cast<int>(scale)][shifted];
}

int ScaleQuantizer::quantizeToNote(float frequency, Scale scale, int keyOffset, float transpose)
{
    if (frequency <= 0.0f) return 0;

    // Convert to MIDI note (float for precision)
    float noteFloat = 69.0f + 12.0f * std::log2(frequency / 440.0f);
    // Apply transpose
    noteFloat += transpose;

    // Round to nearest semitone
    int noteInt = static_cast<int>(std::round(noteFloat));

    // If the note is already in the scale, we're done
    if (isNoteInScale(noteInt, scale, keyOffset)) {
        return noteInt;
    }

    // Find the nearest note in the scale
    int bestNote = noteInt;
    float bestDist = 1e10f;

    // Search within +/- 6 semitones (worst case: need to go to next note in scale)
    for (int offset = -6; offset <= 6; offset++) {
        int candidate = noteInt + offset;
        if (isNoteInScale(candidate, scale, keyOffset)) {
            float dist = std::fabs(static_cast<float>(candidate) - noteFloat);
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

    // Apply cents offset (fractional semitone)
    float centsOffset = cents / 100.0f;  // Convert cents to semitones
    return noteToFrequencyFloat(static_cast<float>(targetNote) + centsOffset);
}

//==============================================================================
// Pitch Corrector Implementation (Phase 1: Resampling + IIR Smoothing)
//==============================================================================

PitchCorrector::PitchCorrector(double sampleRate)
    : mSampleRate(sampleRate)
{
    rebuildWindow();
}

void PitchCorrector::rebuildWindow()
{
    mWindow.resize(mFrameSize);
    for (int i = 0; i < mFrameSize; i++) {
        // Hann window
        mWindow[i] = 0.5f * (1.0f - std::cos(2.0f * VC_PI * i / static_cast<float>(mFrameSize - 1)));
    }
}

void PitchCorrector::reset()
{
    mCurrentRatio = 1.0f;
    mFrameInfo.clear();
}

void PitchCorrector::process(float* left, float* right, int numSamples,
                              YINPitchDetector& detector,
                              ScaleQuantizer& quantizer,
                              float speed,
                              ScaleQuantizer::Scale scale,
                              int keyOffset,
                              float transpose,
                              float cents,
                              float formantPreserve)
{
    if (numSamples < mFrameSize) return;

    if (mReportMode) {
        mFrameInfo.clear();
    }

    // Make copies of input for reading
    std::vector<float> inL(left, left + numSamples);
    std::vector<float> inR(right, right + numSamples);

    //==========================================================================
    // Step 1: Analyze pitch for all frames and build ratio contour
    //==========================================================================
    int analysisHop = mFrameSize / 2;  // 50% overlap for analysis
    int numAnalysisFrames = (numSamples - mFrameSize) / analysisHop + 1;
    std::vector<float> frameRatios(numAnalysisFrames, 1.0f);
    // Also store frame info for report
    std::vector<FrameInfo> frameInfos(numAnalysisFrames);

    for (int f = 0; f < numAnalysisFrames; f++) {
        int pos = f * analysisHop;
        auto pitch = detector.detect(inL.data() + pos, mFrameSize);

        float targetF0 = pitch.frequency;
        int targetNote = 0;
        int detectedNote = 0;

        if (pitch.voiced && pitch.frequency > 0.0f) {
            detectedNote = ScaleQuantizer::frequencyToNote(pitch.frequency);
            targetF0 = quantizer.quantize(pitch.frequency, scale, keyOffset, transpose, cents);
            targetNote = quantizer.quantizeToNote(pitch.frequency, scale, keyOffset, transpose);
        }

        float ratio = 1.0f;
        if (pitch.voiced && pitch.frequency > 0.0f && speed > 0.0f) {
            float targetRatio = targetF0 / pitch.frequency;
            if (speed >= 100.0f) {
                mCurrentRatio = targetRatio;
            } else {
                float alpha = speed / 100.0f;
                mCurrentRatio += (targetRatio - mCurrentRatio) * alpha;
            }
            ratio = mCurrentRatio;
        } else if (!pitch.voiced) {
            float alpha = 0.01f;
            mCurrentRatio += (1.0f - mCurrentRatio) * alpha;
            ratio = mCurrentRatio;
        }

        frameRatios[f] = ratio;

        if (mReportMode) {
            frameInfos[f].detectedF0 = pitch.frequency;
            frameInfos[f].targetF0 = targetF0;
            frameInfos[f].ratio = ratio;
            frameInfos[f].confidence = pitch.confidence;
            frameInfos[f].voiced = pitch.voiced;
            frameInfos[f].detectedNote = detectedNote;
            frameInfos[f].targetNote = targetNote;
            mFrameInfo.push_back(frameInfos[f]);
        }
    }

    //==========================================================================
    // Step 2: Build per-sample ratio contour (linear interpolation between frames)
    //==========================================================================
    std::vector<float> sampleRatios(numSamples, 1.0f);
    for (int f = 0; f < numAnalysisFrames; f++) {
        int start = f * analysisHop;
        int end = (f + 1 < numAnalysisFrames) ? (f + 1) * analysisHop : numSamples;
        float r = frameRatios[f];
        float rNext = (f + 1 < numAnalysisFrames) ? frameRatios[f + 1] : r;

        for (int i = start; i < end; i++) {
            float t = static_cast<float>(i - start) / static_cast<float>(end - start);
            sampleRatios[i] = r * (1.0f - t) + rNext * t;
        }
    }

    //==========================================================================
    // Step 3: Apply pitch shift using overlap-add resampling
    // Uses 75% overlap with Hann window for smooth crossfading
    //==========================================================================
    int synthHop = mFrameSize / 4;  // 75% overlap for synthesis
    std::vector<float> outL(numSamples, 0.0f);
    std::vector<float> outR(numSamples, 0.0f);
    std::vector<float> winSum(numSamples, 0.0f);

    for (int pos = 0; pos + mFrameSize <= numSamples; pos += synthHop) {
        // Get the average ratio for this synthesis frame
        float avgRatio = 0.0f;
        for (int i = 0; i < mFrameSize; i++) {
            avgRatio += sampleRatios[pos + i];
        }
        avgRatio /= mFrameSize;

        // Resample: to shift pitch by ratio r, read input at rate r
        // output[i] = input[pos + i * ratio]
        // r > 1 → read faster → pitch up
        // r < 1 → read slower → pitch down
        for (int i = 0; i < mFrameSize; i++) {
            float srcPos = static_cast<float>(i) * avgRatio;
            int idx = static_cast<int>(srcPos);
            float frac = srcPos - static_cast<float>(idx);
            int readPos = pos + idx;

            float sampleL = 0.0f, sampleR = 0.0f;
            if (readPos >= 0 && readPos + 1 < numSamples) {
                sampleL = inL[readPos] * (1.0f - frac) + inL[readPos + 1] * frac;
                sampleR = inR[readPos] * (1.0f - frac) + inR[readPos + 1] * frac;
            } else if (readPos >= 0 && readPos < numSamples) {
                sampleL = inL[readPos];
                sampleR = inR[readPos];
            }

            outL[pos + i] += sampleL * mWindow[i];
            outR[pos + i] += sampleR * mWindow[i];
            winSum[pos + i] += mWindow[i];
        }
    }

    // Normalize by window sum and write output
    for (int i = 0; i < numSamples; i++) {
        if (winSum[i] > 1e-6f) {
            left[i] = outL[i] / winSum[i];
            right[i] = outR[i] / winSum[i];
        } else {
            left[i] = inL[i];
            right[i] = inR[i];
        }
    }
}

//==============================================================================
// Key Detector Implementation (Krumhansl-Schmuckler)
//==============================================================================

// Krumhansl-Kessler key profiles
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
        sumX += x[i];
        sumY += y[i];
        sumXY += x[i] * y[i];
        sumX2 += x[i] * x[i];
        sumY2 += y[i] * y[i];
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

    // Analyze up to 8 seconds of audio
    int maxAnalysisSamples = static_cast<int>(8.0 * mSampleRate);
    int analysisLength = std::min(numSamples, maxAnalysisSamples);

    // Build chroma histogram from YIN detections
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
        // Not enough voiced detections
        result.detected = false;
        return result;
    }

    // Normalize chroma vector
    float maxChroma = *std::max_element(mChroma, mChroma + 12);
    if (maxChroma > 0.0f) {
        for (int i = 0; i < 12; i++) {
            mChroma[i] /= maxChroma;
        }
    }

    // Krumhansl-Schmuckler: correlate chroma with all 24 key profiles
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
    result.detected = (bestCorr > 0.3f);  // Minimum confidence threshold

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
// VCTuneDSP Main Class Implementation
//==============================================================================

VCTuneDSP::VCTuneDSP()
{
    mParams.speed = 50.0f;
    mParams.scale = 0;
    mParams.transpose = 0.0f;
    mParams.cents = 0.0f;
    mParams.formantPreserve = 1.0f;
    mParams.bypass = false;
    mParams.autoKey = false;

    mAB.paramsA = mParams;
    mAB.paramsB = mParams;
    mAB.activeIsA = true;
}

VCTuneDSP::~VCTuneDSP()
{
}

void VCTuneDSP::prepare(double sampleRate, int blockSize)
{
    mSampleRate = sampleRate;
    mBlockSize = blockSize;

    mPitchDetector.setSampleRate(sampleRate);
    mCorrector.setSampleRate(sampleRate);
    mKeyDetector = KeyDetector(sampleRate);

    mInternalBuffer.resize(blockSize * 2);
    mInternalPtrs.resize(2);
    mInternalPtrs[0] = mInternalBuffer.data();
    mInternalPtrs[1] = mInternalBuffer.data() + blockSize;
}

void VCTuneDSP::process(float* left, float* right, int numSamples)
{
    if (!mEnabled || mParams.bypass) return;

    // Auto key detection (runs once per prepare cycle)
    if (mParams.autoKey && !mKeyDetected) {
        mKeyResult = mKeyDetector.detect(left, right, numSamples, mPitchDetector);
        mKeyDetected = true;

        if (mKeyResult.detected) {
            mAutoKeyOffset = mKeyResult.key;
            mAutoScale = mKeyResult.isMajor ? ScaleQuantizer::Major : ScaleQuantizer::Minor;
        }
    }

    // Determine effective scale and key offset
    ScaleQuantizer::Scale effectiveScale = static_cast<ScaleQuantizer::Scale>(mParams.scale);
    int effectiveKeyOffset = 0;
    float effectiveTranspose = mParams.transpose;

    if (mParams.autoKey && mKeyResult.detected) {
        effectiveScale = mAutoScale;
        effectiveKeyOffset = mAutoKeyOffset;
        // Don't override user transpose - add auto key offset via keyOffset param
    }

    // Run pitch correction with report mode if needed
    mCorrector.setReportMode(mReportMode);

    mCorrector.process(left, right, numSamples,
                       mPitchDetector, mQuantizer,
                       mParams.speed,
                       effectiveScale,
                       effectiveKeyOffset,
                       effectiveTranspose,
                       mParams.cents,
                       mParams.formantPreserve);

    // Build report if in report mode
    if (mReportMode) {
        mReport.clear();
        const auto& frameInfo = mCorrector.getFrameInfo();
        double hopDuration = 1024.0 / mSampleRate * 1000.0;  // ms per hop

        for (size_t i = 0; i < frameInfo.size(); i++) {
            const auto& fi = frameInfo[i];
            PitchReport pr;
            pr.timeMs = static_cast<float>(i * hopDuration);
            pr.detectedF0 = fi.detectedF0;
            pr.targetF0 = fi.targetF0;
            pr.confidence = fi.confidence;
            pr.voiced = fi.voiced;
            pr.detectedNote = fi.detectedNote;
            pr.targetNote = fi.targetNote;

            // Calculate deviation in cents
            if (fi.detectedF0 > 0.0f && fi.targetF0 > 0.0f) {
                pr.deviationCents = 1200.0f * std::log2(fi.detectedF0 / fi.targetF0);
            }

            mReport.push_back(pr);
        }
    }
}

#ifndef VC_STANDALONE
void VCTuneDSP::process(juce::dsp::AudioBlock<float>& block)
{
    if (!mEnabled || mParams.bypass) return;

    auto numSamples = static_cast<int>(block.getNumSamples());
    auto numChannels = static_cast<int>(block.getNumChannels());

    // Copy to interleaved buffers
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

    // Copy back
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

void VCTuneDSP::reset()
{
    mPitchDetector.reset();
    mCorrector.reset();
    mKeyDetector.reset();
    mKeyDetected = false;
    mReport.clear();
}

void VCTuneDSP::setParams(const Params& p)
{
    mParams = p;
}

VCTuneDSP::Params VCTuneDSP::getParams() const
{
    return mParams;
}

void VCTuneDSP::setEnabled(bool enabled)
{
    mEnabled = enabled;
}

void VCTuneDSP::setABParams(const Params& a, const Params& b)
{
    mAB.paramsA = a;
    mAB.paramsB = b;
}

void VCTuneDSP::switchAB()
{
    mAB.activeIsA = !mAB.activeIsA;
    mParams = mAB.activeIsA ? mAB.paramsA : mAB.paramsB;
}

VCTuneDSP::Params VCTuneDSP::getActiveParams() const
{
    return mAB.activeIsA ? mAB.paramsA : mAB.paramsB;
}

void VCTuneDSP::setReportMode(bool enable)
{
    mReportMode = enable;
}
