#include "VCSurgicalDeEsserDSP.h"

#ifdef VC_STANDALONE
#include <algorithm>
#include <cmath>
#include <iomanip>
#endif

//==============================================================================
// Construction / Destruction
//==============================================================================
VCPluginDSP::VCPluginDSP()
{
    mParams.threshold = -30.0f;
    mParams.reduction = 6.0f;
    mParams.minDuration = 20.0f;
    mParams.fadeTime = 5.0f;
    mParams.freqLow = 5000.0f;
    mParams.freqHigh = 9000.0f;
    mParams.mode = 0;
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

    // Envelope follower: attack=1ms, release=50ms
    mAttackCoeff = std::exp(-1.0f / ((float)mSampleRate * 0.001f));
    mReleaseCoeff = std::exp(-1.0f / ((float)mSampleRate * 0.050f));

    // Lookahead buffer: 100ms
    mLookaheadSamples = (int)(0.1 * mSampleRate);
    for (int ch = 0; ch < 2; ++ch) {
        mLookaheadBuffer[ch].resize(mLookaheadSamples, 0.0f);
    }
    mLookaheadWritePos = 0;
    mLookaheadReadPos = 0;
    mLookaheadFilled = false;

    for (int ch = 0; ch < 2; ++ch) {
        mBPState[ch] = BiquadState();
    }

    updateBPCoefficients();
    reset();
}

//==============================================================================
// Update bandpass filter coefficients (Butterworth 2nd order)
//==============================================================================
void VCPluginDSP::updateBPCoefficients()
{
    float fl = mParams.freqLow;
    float fh = mParams.freqHigh;
    float sr = (float)mSampleRate;

    // Clamp frequencies
    fl = std::clamp(fl, 20.0f, sr * 0.49f);
    fh = std::clamp(fh, fl + 100.0f, sr * 0.49f);

    float w0 = 2.0f * (float)VC_PI * std::sqrt(fl * fh) / sr;
    float bw = fh / fl;
    float Q = 1.0f / (bw - 1.0f / bw);
    Q = std::clamp(Q, 0.1f, 20.0f);

    float alpha = std::sin(w0) / (2.0f * Q);
    float cosw0 = std::cos(w0);

    mBPCoeffs.b0 = alpha;
    mBPCoeffs.b1 = 0.0f;
    mBPCoeffs.b2 = -alpha;
    mBPCoeffs.a1 = -2.0f * cosw0;
    mBPCoeffs.a2 = 1.0f - alpha;

    // Normalize
    float a0 = 1.0f + alpha;
    mBPCoeffs.b0 /= a0;
    mBPCoeffs.b1 /= a0;
    mBPCoeffs.b2 /= a0;
    mBPCoeffs.a1 /= a0;
    mBPCoeffs.a2 /= a0;
}

//==============================================================================
// Process biquad filter
//==============================================================================
float VCPluginDSP::processBiquad(float x, BiquadState& s)
{
    float y = mBPCoeffs.b0 * x + mBPCoeffs.b1 * s.x1 + mBPCoeffs.b2 * s.x2
            - mBPCoeffs.a1 * s.y1 - mBPCoeffs.a2 * s.y2;
    s.x2 = s.x1;
    s.x1 = x;
    s.y2 = s.y1;
    s.y1 = y;
    return y;
}

//==============================================================================
// Pass 1: Detect sibilance regions
//==============================================================================
void VCPluginDSP::detectSibilance(float* left, float* right, int numSamples)
{
    mRegions.clear();

    // Envelope follower state
    float envelope = 0.0f;

    // Attack/release for detection
    float attackC = std::exp(-1.0f / ((float)mSampleRate * 0.001f));
    float releaseC = std::exp(-1.0f / ((float)mSampleRate * 0.050f));

    // Minimum sibilance duration in samples
    int minSamples = (int)(mSampleRate * mParams.minDuration / 1000.0f);
    // Margin around sibilance regions
    int margin = (int)(mSampleRate * 0.010f);

    // Reset filter state for detection pass
    BiquadState bpStateL = {}, bpStateR = {};

    // First pass: compute envelope for all samples
    std::vector<float> envelopeDb(numSamples);

    for (int i = 0; i < numSamples; ++i) {
        // Mono mix for detection
        float mono = (left[i] + right[i]) * 0.5f;

        // Bandpass filter to extract sibilance frequencies
        float bp = processBiquad(mono, bpStateL);

        // Envelope follower
        float v = std::abs(bp);
        if (v > envelope) {
            envelope = attackC * envelope + (1.0f - attackC) * v;
        } else {
            envelope = releaseC * envelope + (1.0f - releaseC) * v;
        }

        envelopeDb[i] = linearToDb(envelope);
    }

    // Find regions above threshold
    bool inRegion = false;
    int regionStart = 0;

    for (int i = 0; i < numSamples; ++i) {
        if (envelopeDb[i] > mParams.threshold && !inRegion) {
            regionStart = i;
            inRegion = true;
        } else if (envelopeDb[i] <= mParams.threshold && inRegion) {
            inRegion = false;
            if ((i - regionStart) >= minSamples) {
                SibilanceRegion reg;
                reg.startSample = std::max(0, regionStart - margin);
                reg.endSample = std::min(numSamples, i + margin);

                // Find peak in this region
                float peakDb = -120.0f;
                for (int j = regionStart; j < i; ++j) {
                    if (envelopeDb[j] > peakDb) peakDb = envelopeDb[j];
                }
                reg.peakDb = peakDb;

                // Calculate actual reduction
                float overDb = peakDb - mParams.threshold;
                float actualReduction = std::min(overDb * 0.8f, mParams.reduction);
                reg.actualReductionDb = -actualReduction;  // Negative = attenuation

                mRegions.push_back(reg);
            }
        }
    }

    // Handle region that extends to end
    if (inRegion && (numSamples - regionStart) >= minSamples) {
        SibilanceRegion reg;
        reg.startSample = std::max(0, regionStart - margin);
        reg.endSample = numSamples;

        float peakDb = -120.0f;
        for (int j = regionStart; j < numSamples; ++j) {
            if (envelopeDb[j] > peakDb) peakDb = envelopeDb[j];
        }
        reg.peakDb = peakDb;
        float overDb = peakDb - mParams.threshold;
        float actualReduction = std::min(overDb * 0.8f, mParams.reduction);
        reg.actualReductionDb = -actualReduction;

        mRegions.push_back(reg);
    }
}

//==============================================================================
// Pass 2: Process sibilance regions
//==============================================================================
void VCPluginDSP::processSibilance(float* left, float* right, int numSamples)
{
    int fadeSamples = (int)(mSampleRate * mParams.fadeTime / 1000.0f);

    for (const auto& reg : mRegions) {
        int start = reg.startSample;
        int end = reg.endSample;
        int length = end - start;

        if (length <= 0) continue;

        float gainReduction = dBToLinear(reg.actualReductionDb);

        if (mParams.mode == 0) {
            // Gain mode: apply gain reduction with crossfade
            int nIn = std::min(fadeSamples, length / 3);
            int nOut = std::min(fadeSamples, length / 3);
            int nSus = length - nIn - nOut;

            if (nSus < 0) {
                nIn = length / 3;
                nOut = length / 3;
                nSus = length - nIn - nOut;
            }

            for (int ch = 0; ch < 2; ++ch) {
                float* data = (ch == 0) ? left : right;

                // Fade in (1.0 -> gainReduction)
                for (int i = 0; i < nIn; ++i) {
                    float t = (float)i / (float)nIn;
                    float g = 1.0f + (gainReduction - 1.0f) * t;
                    data[start + i] *= g;
                }

                // Sustain (gainReduction)
                for (int i = 0; i < std::max(nSus, 0); ++i) {
                    data[start + nIn + i] *= gainReduction;
                }

                // Fade out (gainReduction -> 1.0)
                for (int i = 0; i < nOut; ++i) {
                    float t = (float)i / (float)nOut;
                    float g = gainReduction + (1.0f - gainReduction) * t;
                    data[start + nIn + std::max(nSus, 0) + i] *= g;
                }
            }
        } else {
            // Dynamic EQ mode: subtract sibilance band content
            BiquadState bpL = {}, bpR = {};

            // We need to re-filter the region - but since we already detected,
            // we can use a simpler approach: apply gain reduction to the
            // bandpass content and subtract
            float subGain = 1.0f - gainReduction; // e.g., if gainReduction=0.5, subtract 0.5 of bandpass

            for (int ch = 0; ch < 2; ++ch) {
                float* data = (ch == 0) ? left : right;
                BiquadState bpState = {};

                // Need to prime the filter with a few samples before the region
                int primeStart = std::max(0, start - fadeSamples);
                for (int i = primeStart; i < start; ++i) {
                    processBiquad(data[i], bpState);
                }

                // Apply DynEQ with crossfade
                int nIn = std::min(fadeSamples, length / 3);
                int nOut = std::min(fadeSamples, length / 3);
                int nSus = length - nIn - nOut;
                if (nSus < 0) {
                    nIn = length / 3;
                    nOut = length / 3;
                    nSus = length - nIn - nOut;
                }

                for (int i = 0; i < length; ++i) {
                    float bp = processBiquad(data[start + i], bpState);

                    // Compute crossfade factor
                    float cf = 0.0f;
                    if (i < nIn) {
                        cf = (float)i / (float)nIn;
                    } else if (i >= length - nOut) {
                        cf = (float)(length - i) / (float)nOut;
                    } else {
                        cf = 1.0f;
                    }

                    data[start + i] -= bp * subGain * cf;
                }
            }
        }
    }
}

//==============================================================================
// Two-pass processing (detect + process)
//==============================================================================
void VCPluginDSP::processTwoPass(float* left, float* right, int numSamples)
{
    if (!mEnabled)
        return;

    detectSibilance(left, right, numSamples);
    processSibilance(left, right, numSamples);
}

//==============================================================================
// Process (single-pass streaming mode for VST3)
//==============================================================================
void VCPluginDSP::process(float* left, float* right, int numSamples)
{
    if (!mEnabled)
        return;

#ifdef VC_STANDALONE
    // In standalone mode, use two-pass for better results
    processTwoPass(left, right, numSamples);
#else
    // In VST3 mode, use streaming processing with lookahead
    // TODO: Implement streaming mode for VST3
    processTwoPass(left, right, numSamples);
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

    size_t numSamples = block.getNumSamples();
    if ((int)mInternalBuffer.size() < (int)numSamples * 2)
        mInternalBuffer.resize(numSamples * 2);

    std::vector<float> left(numSamples), right(numSamples);
    for (size_t i = 0; i < numSamples; ++i) {
        left[i] = block.getChannelPointer(0)[i];
        right[i] = block.getChannelPointer(1)[i];
    }

    processTwoPass(left.data(), right.data(), (int)numSamples);

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
    for (int ch = 0; ch < 2; ++ch) {
        mBPState[ch] = BiquadState();
    }
    mEnvelope = 0.0f;
    mInSibilance = false;
    mCurrentGain = 1.0f;
    mFadeInCount = 0;
    mFadeOutCount = 0;

    mLookaheadWritePos = 0;
    mLookaheadReadPos = 0;
    mLookaheadFilled = false;
}

//==============================================================================
// Set parameters
//==============================================================================
void VCPluginDSP::setParams(const Params& p)
{
    mParams = p;
    updateBPCoefficients();
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
// Generate report
//==============================================================================
std::string VCPluginDSP::generateReport() const
{
    std::ostringstream ss;
    ss << "=======================================================\n";
    ss << "Surgical De-Esser Report\n";
    ss << "=======================================================\n";
    ss << "Parameters:\n";
    ss << "  Threshold: " << mParams.threshold << " dBFS\n";
    ss << "  Max reduction: " << mParams.reduction << " dB\n";
    ss << "  Min duration: " << mParams.minDuration << " ms\n";
    ss << "  Fade time: " << mParams.fadeTime << " ms\n";
    ss << "  Detection band: " << mParams.freqLow << " - " << mParams.freqHigh << " Hz\n";
    ss << "  Mode: " << (mParams.mode == 0 ? "gain" : "dynEQ") << "\n";
    ss << "-------------------------------------------------------\n";
    ss << "Total sibilance regions: " << mRegions.size() << "\n";

    if (!mRegions.empty()) {
        float totalDurMs = 0.0f;
        float totalReduction = 0.0f;
        float maxReduction = 0.0f;

        for (const auto& r : mRegions) {
            float durMs = (float)(r.endSample - r.startSample) / (float)mSampleRate * 1000.0f;
            totalDurMs += durMs;
            totalReduction += r.actualReductionDb;
            if (r.actualReductionDb < maxReduction) maxReduction = r.actualReductionDb;
        }

        ss << "Total processed duration: " << (int)totalDurMs << " ms\n";
        ss << "Average reduction: " << (totalReduction / (float)mRegions.size()) << " dB\n";
        ss << "Maximum reduction: " << maxReduction << " dB\n";
        ss << "-------------------------------------------------------\n";
        ss << std::right;
        ss << std::setw(4) << "No." << " "
           << std::setw(10) << "Time(s)" << " "
           << std::setw(10) << "Dur(ms)" << " "
           << std::setw(10) << "Peak(dB)" << " "
           << std::setw(10) << "Red(dB)" << "\n";

        int count = std::min((int)mRegions.size(), 50);
        for (int i = 0; i < count; ++i) {
            const auto& r = mRegions[i];
            float startSec = (float)r.startSample / (float)mSampleRate;
            float durMs = (float)(r.endSample - r.startSample) / (float)mSampleRate * 1000.0f;
            ss << std::setw(4) << i << " "
               << std::setw(10) << std::fixed << std::setprecision(3) << startSec << " "
               << std::setw(10) << std::fixed << std::setprecision(1) << durMs << " "
               << std::setw(10) << std::fixed << std::setprecision(1) << r.peakDb << " "
               << std::setw(10) << std::fixed << std::setprecision(1) << r.actualReductionDb << "\n";
        }

        if ((int)mRegions.size() > 50) {
            ss << "... " << mRegions.size() << " total regions\n";
        }
    }
    ss << "=======================================================\n";

    return ss.str();
}
