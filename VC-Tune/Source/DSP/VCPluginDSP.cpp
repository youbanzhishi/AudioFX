#include "VCPluginDSP.h"

#ifdef VC_STANDALONE
#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>
#include <cstdio>
#endif

//==============================================================================
// YIN Pitch Detector Implementation (Gen2: exposes last refined tau)
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

    mLastRefinedTau = refinedTau;  // Gen2: store for PSOLA pitch marks

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
// Scale Quantizer Implementation (unchanged from Gen1)
//==============================================================================

const bool ScaleQuantizer::mScales[6][12] = {
    {1,1,1,1,1,1,1,1,1,1,1,1},
    {1,0,1,0,1,1,0,1,0,1,0,1},
    {1,0,1,1,0,1,0,1,1,0,1,0},
    {1,0,1,0,1,0,0,1,0,1,0,0},
    {1,0,0,1,0,1,1,1,0,0,1,0},
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
}

int ScaleQuantizer::frequencyToNote(float frequency)
{
    if (frequency <= 0.0f) return 0;
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

    // Apply fine-tuning in cents
    if (std::fabs(cents) > 0.001f) {
        targetFreq *= std::pow(2.0f, cents / 1200.0f);
    }

    return targetFreq;
}

//==============================================================================
// Gen2: LPC Formant Extractor Implementation
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

    // Compute autocorrelation
    autocorrelation(frame, frameSize);

    // Check for silence
    if (mAutocorr[0] < 1e-10f) return result;

    // Levinson-Durbin recursion
    float E = mAutocorr[0];  // Error energy

    for (int i = 1; i <= LPC_ORDER; i++) {
        // Compute reflection coefficient
        float lambda = 0.0f;
        for (int j = 1; j < i; j++) {
            lambda += result.a[j] * mAutocorr[i - j];
        }
        lambda = (mAutocorr[i] - lambda) / E;

        // Update LPC coefficients
        float aCopy[LPC_ORDER + 1];
        std::copy(result.a, result.a + LPC_ORDER + 1, aCopy);

        for (int j = 1; j < i; j++) {
            result.a[j] = aCopy[j] - lambda * aCopy[i - j];
        }
        result.a[i] = -lambda;

        // Update error energy
        E = E * (1.0f - lambda * lambda);

        if (E <= 0.0f) {
            // Numerical instability - reset
            result.a[0] = 1.0f;
            for (int j = 1; j <= LPC_ORDER; j++) result.a[j] = 0.0f;
            result.gain = std::sqrt(mAutocorr[0]);
            return result;
        }
    }

    result.gain = std::sqrt(E);
    return result;
}

void LPCFormantExtractor::inverseFilter(const float* input, int numSamples,
                                         const float* a, float* excitation)
{
    // A(z) = 1 + a[1]z^-1 + ... + a[P]z^-P
    // excitation[n] = input[n] + a[1]*input[n-1] + ... + a[P]*input[n-P]
    for (int n = 0; n < numSamples; n++) {
        float sample = input[n];
        for (int k = 1; k <= LPC_ORDER && k <= n; k++) {
            sample += a[k] * input[n - k];
        }
        excitation[n] = sample;
    }
}

void LPCFormantExtractor::synthesisFilter(const float* excitation, int numSamples,
                                           const float* a, float gain, float* output)
{
    // 1/A(z): y[n] = excitation[n] - a[1]*y[n-1] - ... - a[P]*y[n-P]
    for (int n = 0; n < numSamples; n++) {
        float sample = excitation[n] / gain;
        for (int k = 1; k <= LPC_ORDER && k <= n; k++) {
            sample -= a[k] * output[n - k];
        }
        output[n] = sample;
    }
}

void LPCFormantExtractor::shiftFormants(const float* aIn, float shiftSemitones,
                                         float sampleRate, float* aOut)
{
    // Formant shifting by frequency-scaling LPC coefficients
    // Scale factor: ratio of new formant frequencies to old
    // shiftSemitones > 0 => formants shift up, < 0 => shift down
    float ratio = std::pow(2.0f, shiftSemitones / 12.0f);

    // Simple frequency warping approach:
    // Replace a[k] with a[k] * alpha^k where alpha adjusts the frequency axis
    // This is an approximation of bilinear transform frequency warping
    float alpha = 1.0f / ratio;  // Inverse because higher alpha compresses frequency

    aOut[0] = 1.0f;
    for (int k = 1; k <= LPC_ORDER; k++) {
        aOut[k] = aIn[k] * std::pow(alpha, static_cast<float>(k));
    }
}

//==============================================================================
// Gen2: PSOLA Engine Implementation
//==============================================================================

PSOLAEngine::PSOLAEngine(double sampleRate)
    : mSampleRate(sampleRate)
{
}

void PSOLAEngine::reset()
{
    mHannWindow.clear();
    mLastWindowSize = 0;
}

void PSOLAEngine::buildHannWindow(int size)
{
    if (size == mLastWindowSize) return;
    mLastWindowSize = size;
    mHannWindow.resize(size);
    for (int i = 0; i < size; i++) {
        mHannWindow[i] = 0.5f * (1.0f - std::cos(2.0f * VC_PI * i / static_cast<float>(size - 1)));
    }
}

std::vector<int> PSOLAEngine::detectPitchMarks(const float* input, int numSamples,
                                                 YINPitchDetector& detector,
                                                 int frameSize, int hopSize)
{
    std::vector<int> marks;

    // Walk through the signal using YIN to find pitch periods
    // Place marks at estimated glottal closure instants
    int pos = 0;
    float lastPeriod = 0.0f;

    while (pos + frameSize <= numSamples) {
        auto result = detector.detect(input + pos, frameSize);

        if (result.voiced && result.frequency > 0.0f) {
            float period = static_cast<float>(mSampleRate) / result.frequency;

            if (lastPeriod <= 0.0f) {
                // First voiced frame: place mark at frame start
                marks.push_back(pos);
                lastPeriod = period;
            } else {
                // Place marks at period intervals from last mark
                // Use smoothed period for stability
                float smoothPeriod = 0.8f * lastPeriod + 0.2f * period;

                while (pos + static_cast<int>(smoothPeriod) < numSamples) {
                    int nextMark = marks.back() + static_cast<int>(std::round(smoothPeriod));
                    if (nextMark >= numSamples) break;
                    marks.push_back(nextMark);

                    // Re-detect pitch at new mark for updated period
                    if (nextMark + frameSize <= numSamples) {
                        auto nextResult = detector.detect(input + nextMark, frameSize);
                        if (nextResult.voiced && nextResult.frequency > 0.0f) {
                            float newPeriod = static_cast<float>(mSampleRate) / nextResult.frequency;
                            smoothPeriod = 0.8f * smoothPeriod + 0.2f * newPeriod;
                        }
                    }
                }

                lastPeriod = smoothPeriod;
            }

            pos = marks.back() + static_cast<int>(std::round(lastPeriod));
        } else {
            // Unvoiced: skip forward by hopSize
            pos += hopSize;
            lastPeriod = 0.0f;
        }
    }

    return marks;
}

void PSOLAEngine::process(const ProcessParams& params, float* output)
{
    const float* input = params.input;
    int numSamples = params.numSamples;

    // Clear output
    std::fill(output, output + numSamples, 0.0f);

    if (numSamples < params.frameSize || params.numFrames <= 0) return;

    //==========================================================================
    // Step 1: Detect pitch marks
    //==========================================================================
    // We'll use a simplified PSOLA approach:
    // For each analysis frame (at analysis hop intervals), extract a windowed
    // segment centered on the frame position, then place it at the synthesis
    // position adjusted by the pitch ratio.

    int analysisHop = params.analysisHop;
    int numFrames = params.numFrames;
    float formantPreserve = params.formantPreserve / 100.0f;  // normalize to 0-1
    float vibratoPreserve = params.vibratoPreserve / 100.0f;
    float transitionSmooth = params.transitionSmooth / 100.0f;

    //==========================================================================
    // Step 2: Smooth pitch ratio contour based on transition smoothing
    //==========================================================================
    std::vector<float> smoothRatios(numFrames, 1.0f);
    std::copy(params.pitchRatios, params.pitchRatios + numFrames, smoothRatios.data());

    // Apply transition smoothing: low-pass filter the ratio contour
    if (transitionSmooth > 0.01f) {
        float alpha = 1.0f - transitionSmooth;  // Higher smooth => lower alpha => more smoothing
        alpha = std::max(0.01f, alpha);  // Don't go to zero

        // Forward pass
        for (int f = 1; f < numFrames; f++) {
            smoothRatios[f] = alpha * smoothRatios[f] + (1.0f - alpha) * smoothRatios[f - 1];
        }
        // Backward pass (for zero-phase smoothing)
        for (int f = numFrames - 2; f >= 0; f--) {
            smoothRatios[f] = alpha * smoothRatios[f] + (1.0f - alpha) * smoothRatios[f + 1];
        }
    }

    // Vibrato preservation: identify vibrato regions and reduce correction
    // Simple approach: if the pitch is oscillating, reduce the correction ratio toward 1
    if (vibratoPreserve > 0.01f) {
        for (int f = 2; f < numFrames - 2; f++) {
            // Detect local pitch oscillation (vibrato indicator)
            float r0 = params.pitchRatios[f - 2];
            float r1 = params.pitchRatios[f - 1];
            float r2 = params.pitchRatios[f];
            float r3 = params.pitchRatios[f + 1];

            // Check for alternating direction (vibrato pattern)
            bool vibrato = ((r1 > r0 && r2 < r1) || (r1 < r0 && r2 > r1)) &&
                           ((r2 > r1 && r3 < r2) || (r2 < r1 && r3 > r2));

            if (vibrato) {
                // Blend toward 1.0 (preserve original vibrato)
                smoothRatios[f] = smoothRatios[f] * (1.0f - vibratoPreserve) +
                                  1.0f * vibratoPreserve;
            }
        }
    }

    //==========================================================================
    // Step 3: Per-frame PSOLA processing
    // For each analysis frame:
    //   a) Extract windowed segment (2-3 periods)
    //   b) If formant preserve: extract excitation via LPC inverse filter
    //   c) Resample/resize the segment according to pitch ratio
    //   d) If formant preserve: re-apply formants via LPC synthesis filter
    //   e) OLA into output buffer
    //==========================================================================

    // Working buffers
    std::vector<float> frameBuf(params.frameSize, 0.0f);
    std::vector<float> excitationBuf(params.frameSize, 0.0f);
    std::vector<float> resampledBuf(params.frameSize * 2, 0.0f);  // extra space for ratio > 1
    std::vector<float> synthBuf(params.frameSize * 2, 0.0f);
    std::vector<float> windowSum(numSamples, 0.0f);

    for (int f = 0; f < numFrames; f++) {
        int frameStart = f * analysisHop;
        float ratio = smoothRatios[f];

        // Skip if ratio is near 1.0 (no correction needed) - with some tolerance
        bool needsCorrection = (std::fabs(ratio - 1.0f) > 0.001f);

        if (!needsCorrection) {
            // Just OLA the original windowed segment
            int halfFrame = params.frameSize / 2;
            buildHannWindow(params.frameSize);

            for (int i = 0; i < params.frameSize; i++) {
                int outIdx = frameStart + i - halfFrame;
                if (outIdx >= 0 && outIdx < numSamples) {
                    output[outIdx] += input[frameStart + i] * mHannWindow[i];
                    windowSum[outIdx] += mHannWindow[i];
                }
            }
            continue;
        }

        //======================================================================
        // (a) Extract windowed analysis frame
        //======================================================================
        int halfFrame = params.frameSize / 2;
        buildHannWindow(params.frameSize);

        for (int i = 0; i < params.frameSize; i++) {
            int srcIdx = frameStart + i - halfFrame;
            if (srcIdx >= 0 && srcIdx < numSamples) {
                frameBuf[i] = input[srcIdx] * mHannWindow[i];
            } else {
                frameBuf[i] = 0.0f;
            }
        }

        //======================================================================
        // (b) LPC inverse filter (if formant preservation enabled)
        //======================================================================
        const float* processBuf = frameBuf.data();
        int processLen = params.frameSize;

        if (formantPreserve > 0.01f && params.useLPC && params.lpcResults != nullptr) {
            const auto& lpc = params.lpcResults[f];

            // Inverse filter: remove formants to get excitation
            std::fill(excitationBuf.begin(), excitationBuf.end(), 0.0f);
            mLPC.inverseFilter(frameBuf.data(), params.frameSize, lpc.a, excitationBuf.data());

            processBuf = excitationBuf.data();
        }

        //======================================================================
        // (c) Pitch-shift via resampling (TD-PSOLA core)
        // Resample the segment: if ratio > 1, compress time (raise pitch),
        // if ratio < 1, stretch time (lower pitch)
        //======================================================================
        int resampledLen = static_cast<int>(std::round(params.frameSize / ratio));
        resampledLen = std::max(16, std::min(resampledLen, static_cast<int>(resampledBuf.size())));

        // Sinc-like interpolation using linear interpolation for efficiency
        for (int i = 0; i < resampledLen; i++) {
            float srcPos = static_cast<float>(i) * ratio;
            int idx = static_cast<int>(srcPos);
            float frac = srcPos - static_cast<float>(idx);

            if (idx + 1 < params.frameSize) {
                resampledBuf[i] = processBuf[idx] * (1.0f - frac) + processBuf[idx + 1] * frac;
            } else if (idx < params.frameSize) {
                resampledBuf[i] = processBuf[idx];
            } else {
                resampledBuf[i] = 0.0f;
            }
        }

        //======================================================================
        // (d) LPC synthesis filter (re-apply formants if preserved)
        //======================================================================
        const float* olaBuf = resampledBuf.data();
        int olaLen = resampledLen;

        if (formantPreserve > 0.01f && params.useLPC && params.lpcResults != nullptr) {
            const auto& lpc = params.lpcResults[f];

            // Optionally shift formants
            float shiftedA[LPCFormantExtractor::LPC_ORDER + 1];
            const float* useA = lpc.a;

            if (std::fabs(params.formantShift) > 0.01f) {
                mLPC.shiftFormants(lpc.a, params.formantShift, static_cast<float>(mSampleRate), shiftedA);
                useA = shiftedA;
            }

            // Synthesis filter: re-apply formants
            std::fill(synthBuf.begin(), synthBuf.end(), 0.0f);
            mLPC.synthesisFilter(resampledBuf.data(), resampledLen, useA, lpc.gain, synthBuf.data());

            olaBuf = synthBuf.data();
        }

        //======================================================================
        // (e) OLA into output buffer at synthesis position
        // The synthesis position is frameStart (maintaining original duration)
        // We center the resampled frame at frameStart
        //======================================================================
        int halfOla = olaLen / 2;

        // Build a Hanning window for the OLA segment
        buildHannWindow(olaLen);

        for (int i = 0; i < olaLen; i++) {
            int outIdx = frameStart + i - halfOla;
            if (outIdx >= 0 && outIdx < numSamples) {
                output[outIdx] += olaBuf[i] * mHannWindow[i];
                windowSum[outIdx] += mHannWindow[i];
            }
        }
    }

    //==========================================================================
    // Step 4: Normalize by window sum to handle overlap regions
    //==========================================================================
    for (int i = 0; i < numSamples; i++) {
        if (windowSum[i] > 0.01f) {
            output[i] /= windowSum[i];
        }
    }
}

//==============================================================================
// Pitch Corrector Implementation (Gen2)
//==============================================================================

PitchCorrector::PitchCorrector(double sampleRate)
    : mSampleRate(sampleRate)
    , mPSOLA(sampleRate)
{
    rebuildWindow();
}

void PitchCorrector::rebuildWindow()
{
    mWindow.resize(mFrameSize);
    for (int i = 0; i < mFrameSize; i++) {
        mWindow[i] = 0.5f * (1.0f - std::cos(2.0f * VC_PI * i / static_cast<float>(mFrameSize - 1)));
    }
}

void PitchCorrector::reset()
{
    mCurrentRatio = 1.0f;
    mFrameInfo.clear();
    mPSOLA.reset();
    mLPC.reset();
}

void PitchCorrector::process(float* left, float* right, int numSamples,
                              YINPitchDetector& detector,
                              ScaleQuantizer& quantizer,
                              float speed,
                              ScaleQuantizer::Scale scale,
                              int keyOffset,
                              float transpose,
                              float cents,
                              float formantPreserve,
                              float formantShift,
                              float vibratoPreserve,
                              float transitionSmooth)
{
    if (numSamples < mFrameSize) return;
    if (mReportMode) { mFrameInfo.clear(); }

    // Make copies of input for reading
    std::vector<float> inL(left, left + numSamples);
    std::vector<float> inR(right, right + numSamples);

    //==========================================================================
    // Step 1: Analyze pitch for all frames and build ratio contour
    //==========================================================================
    int analysisHop = mFrameSize / 2;
    int numFrames = (numSamples - mFrameSize) / analysisHop + 1;
    std::vector<float> frameRatios(numFrames, 1.0f);

    for (int f = 0; f < numFrames; f++) {
        int pos = f * analysisHop;
        auto pitch = detector.detect(inL.data() + pos, mFrameSize);

        float targetF0 = pitch.frequency;
        int targetNote = 0, detectedNote = 0;

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
            FrameInfo fi;
            fi.detectedF0 = pitch.frequency;
            fi.targetF0 = targetF0;
            fi.ratio = ratio;
            fi.confidence = pitch.confidence;
            fi.voiced = pitch.voiced;
            fi.detectedNote = detectedNote;
            fi.targetNote = targetNote;
            mFrameInfo.push_back(fi);
        }
    }

    //==========================================================================
    // Step 2: LPC analysis per frame (if formant preservation is enabled)
    //==========================================================================
    bool useLPC = (formantPreserve > 0.1f);
    std::vector<LPCFormantExtractor::LPCResult> lpcResults;

    if (useLPC) {
        lpcResults.resize(numFrames);
        for (int f = 0; f < numFrames; f++) {
            int pos = f * analysisHop;
            // Pre-emphasis for better LPC analysis
            std::vector<float> emphasized(mFrameSize);
            float preEmph = 0.97f;
            emphasized[0] = inL[pos];
            for (int i = 1; i < mFrameSize && (pos + i) < numSamples; i++) {
                emphasized[i] = inL[pos + i] - preEmph * inL[pos + i - 1];
            }

            lpcResults[f] = mLPC.analyze(emphasized.data(), mFrameSize);
        }
    }

    //==========================================================================
    // Step 3: PSOLA processing for left channel
    //==========================================================================
    PSOLAEngine::ProcessParams psolaParams;
    psolaParams.input = inL.data();
    psolaParams.numSamples = numSamples;
    psolaParams.pitchRatios = frameRatios.data();
    psolaParams.numFrames = numFrames;
    psolaParams.analysisHop = analysisHop;
    psolaParams.frameSize = mFrameSize;
    psolaParams.speed = speed;
    psolaParams.formantPreserve = formantPreserve;
    psolaParams.formantShift = formantShift;
    psolaParams.vibratoPreserve = vibratoPreserve;
    psolaParams.transitionSmooth = transitionSmooth;
    psolaParams.useLPC = useLPC;
    psolaParams.lpcResults = useLPC ? lpcResults.data() : nullptr;

    std::vector<float> outL(numSamples, 0.0f);
    mPSOLA.process(psolaParams, outL.data());

    //==========================================================================
    // Step 4: PSOLA processing for right channel
    //==========================================================================
    psolaParams.input = inR.data();

    // If using LPC, re-analyze right channel (it may differ from left)
    std::vector<LPCFormantExtractor::LPCResult> lpcResultsR;
    if (useLPC) {
        lpcResultsR.resize(numFrames);
        for (int f = 0; f < numFrames; f++) {
            int pos = f * analysisHop;
            std::vector<float> emphasized(mFrameSize);
            float preEmph = 0.97f;
            emphasized[0] = inR[pos];
            for (int i = 1; i < mFrameSize && (pos + i) < numSamples; i++) {
                emphasized[i] = inR[pos + i] - preEmph * inR[pos + i - 1];
            }
            lpcResultsR[f] = mLPC.analyze(emphasized.data(), mFrameSize);
        }
        psolaParams.lpcResults = lpcResultsR.data();
    }

    std::vector<float> outR(numSamples, 0.0f);
    mPSOLA.process(psolaParams, outR.data());

    //==========================================================================
    // Step 5: Copy results to output
    //==========================================================================
    std::copy(outL.begin(), outL.end(), left);
    std::copy(outR.begin(), outR.end(), right);
}

//==============================================================================
// Key Detector Implementation (Krumhansl-Schmuckler) - unchanged from Gen1
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
// VCTuneDSP Main Class Implementation (Gen2)
//==============================================================================

VCTuneDSP::VCTuneDSP()
{
    mParams.speed = 50.0f;
    mParams.scale = 0;
    mParams.transpose = 0.0f;
    mParams.cents = 0.0f;
    mParams.formantPreserve = 100.0f;    // Gen2: default full preservation
    mParams.formantShift = 0.0f;
    mParams.vibratoPreserve = 0.0f;
    mParams.transitionSmooth = 50.0f;
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

    // Auto key detection
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
    float effectiveTranspose = mParams.transpose;

    if (mParams.autoKey && mKeyResult.detected) {
        effectiveScale = mAutoScale;
        effectiveKeyOffset = mAutoKeyOffset;
    }

    mCorrector.setReportMode(mReportMode);

    // Gen2: pass all new parameters to PitchCorrector
    mCorrector.process(left, right, numSamples,
                       mPitchDetector, mQuantizer,
                       mParams.speed,
                       effectiveScale,
                       effectiveKeyOffset,
                       effectiveTranspose,
                       mParams.cents,
                       mParams.formantPreserve,
                       mParams.formantShift,
                       mParams.vibratoPreserve,
                       mParams.transitionSmooth);

    // Build report
    if (mReportMode) {
        mReport.clear();
        const auto& frameInfo = mCorrector.getFrameInfo();
        double hopDuration = 1024.0 / mSampleRate * 1000.0;

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
