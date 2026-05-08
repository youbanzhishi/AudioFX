#include "VCPluginDSP.h"

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

    // Envelope follower for detection band: fast attack, moderate release
    // Attack 1ms, Release 50ms (fast tracking of breath onset)
    float envAttackMs = 1.0f;
    float envReleaseMs = 50.0f;
    mAttackCoef = std::exp(-1.0f / (envAttackMs * 0.001f * static_cast<float>(sampleRate)));
    mAttackCoefInv = 1.0f - mAttackCoef;
    mReleaseCoef = std::exp(-1.0f / (envReleaseMs * 0.001f * static_cast<float>(sampleRate)));
    mReleaseCoefInv = 1.0f - mReleaseCoef;

    // Full-band envelope for spectral flatness estimation
    mFullBandEnvelope.reset();
    mFullBandEnvelope.setAttackTime(envAttackMs, static_cast<float>(sampleRate));
    mFullBandEnvelope.setReleaseTime(envReleaseMs, static_cast<float>(sampleRate));

    // Lookahead buffer
    mLookahead.prepare(mParams.lookahead, sampleRate);
    mLookahead.reset();

    // Reset filter state
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
    float sr = static_cast<float>(mSampleRate);

    // Clamp frequencies
    fl = std::clamp(fl, 20.0f, sr * 0.49f);
    fh = std::clamp(fh, fl + 100.0f, sr * 0.49f);

    float w0 = 2.0f * static_cast<float>(VC_PI) * std::sqrt(fl * fh) / sr;
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
// Pass 1: Detect breath regions
//==============================================================================
void VCPluginDSP::detectBreaths(float* left, float* right, int numSamples)
{
    mBreathRegions.clear();

    // Envelope followers
    BreathEnvelopeFollower bpEnvelope;
    bpEnvelope.setAttackTime(1.0f, static_cast<float>(mSampleRate));
    bpEnvelope.setReleaseTime(50.0f, static_cast<float>(mSampleRate));

    BreathEnvelopeFollower fullEnvelope;
    fullEnvelope.setAttackTime(1.0f, static_cast<float>(mSampleRate));
    fullEnvelope.setReleaseTime(50.0f, static_cast<float>(mSampleRate));

    // Minimum duration in samples
    int minDurationSamples = static_cast<int>(mSampleRate * mParams.minBreathDuration / 1000.0f);

    // Hysteresis: exit threshold is 3dB above enter threshold
    float enterThreshold = mParams.threshold;
    float exitThreshold = mParams.threshold + 3.0f;

    // Noise floor: don't detect below -60dBFS (avoids operating on silence/noise)
    float noiseFloor = -60.0f;

    // Reset filter state for detection pass
    BiquadState bpStateL = {};

    // Compute per-sample envelope and spectral flatness
    std::vector<float> envelopeBpDb(numSamples);
    std::vector<float> sfValues(numSamples);

    for (int i = 0; i < numSamples; ++i) {
        // Mono mix for detection
        float mono = (left[i] + right[i]) * 0.5f;

        // Bandpass filter to extract breath frequencies
        float bp = processBiquad(mono, bpStateL);

        // Bandpass envelope
        float bpEnvSq = bpEnvelope.processSample(bp);
        float bpEnvLin = std::sqrt(bpEnvSq);
        float bpEnvDb = linearToDb(bpEnvLin);

        // Full-band envelope (for spectral flatness estimation)
        float fullEnvSq = fullEnvelope.processSample(mono);
        float fullEnvLin = std::sqrt(fullEnvSq);
        float fullEnvDb = linearToDb(fullEnvLin);

        // Simplified spectral flatness estimation:
        // SF = E_bp / E_full, clamped to [0,1]
        // High SF means most energy is in the breath band → likely breath/noise
        // Low SF means energy is distributed outside → likely singing (harmonics)
        float sf = 0.0f;
        if (fullEnvLin > 1e-10f) {
            sf = bpEnvLin / fullEnvLin;
            sf = std::clamp(sf, 0.0f, 1.0f);
        }

        // Apply sensitivity weighting:
        // sensitivity controls how much spectral flatness contributes
        // At sensitivity=1.0, SF must be > sfThreshold
        // At sensitivity=0.1, SF requirement is relaxed
        // Effective SF threshold = sfThreshold * (1.0 - sensitivity * 0.5)
        // Actually, let's use: effective_sf = sf * sensitivity + (1.0 - sensitivity) * 0.5
        // This way, at sensitivity=0, SF is always 0.5 (neutral), 
        // and at sensitivity=1, SF is its actual value
        float effectiveSf = sf * mParams.sensitivity + (1.0f - mParams.sensitivity) * 0.5f;

        envelopeBpDb[i] = bpEnvDb;
        sfValues[i] = effectiveSf;
    }

    // State machine with hysteresis to detect breath regions
    BreathState state = BreathState::IDLE;
    int breathStart = 0;

    for (int i = 0; i < numSamples; ++i) {
        float envDb = envelopeBpDb[i];
        float sf = sfValues[i];

        bool energyCondition = false;
        bool sfCondition = sf > mParams.sfThreshold;
        bool notSilence = envDb > noiseFloor;

        if (state == BreathState::IDLE) {
            // Enter condition: energy below enter threshold
            energyCondition = envDb < enterThreshold;
        } else {
            // Exit condition: energy above exit threshold (higher due to hysteresis)
            energyCondition = envDb < exitThreshold;
        }

        if (state == BreathState::IDLE) {
            if (energyCondition && sfCondition && notSilence) {
                state = BreathState::BREATH;
                breathStart = i;
            }
        } else { // BREATH
            if (!energyCondition || !sfCondition) {
                // Check duration
                int duration = i - breathStart;
                if (duration >= minDurationSamples) {
                    // Confirmed breath region - compute statistics
                    BreathRegion reg;
                    reg.startSample = breathStart;
                    reg.endSample = i;
                    reg.peakDb = -120.0f;
                    reg.avgDb = 0.0f;
                    reg.spectralFlatness = 0.0f;
                    reg.appliedReductionDb = mParams.reduction;

                    float sumDb = 0.0f;
                    float sumSf = 0.0f;
                    int count = 0;

                    for (int j = breathStart; j < i; ++j) {
                        if (envelopeBpDb[j] > reg.peakDb) reg.peakDb = envelopeBpDb[j];
                        sumDb += envelopeBpDb[j];
                        sumSf += sfValues[j];
                        count++;
                    }

                    if (count > 0) {
                        reg.avgDb = sumDb / count;
                        reg.spectralFlatness = sumSf / count;
                    }

                    mBreathRegions.push_back(reg);
                }
                state = BreathState::IDLE;
            }
        }
    }

    // Handle region that extends to end
    if (state == BreathState::BREATH) {
        int duration = numSamples - breathStart;
        if (duration >= minDurationSamples) {
            BreathRegion reg;
            reg.startSample = breathStart;
            reg.endSample = numSamples;
            reg.peakDb = -120.0f;
            reg.avgDb = 0.0f;
            reg.spectralFlatness = 0.0f;
            reg.appliedReductionDb = mParams.reduction;

            float sumDb = 0.0f;
            float sumSf = 0.0f;
            int count = 0;

            for (int j = breathStart; j < numSamples; ++j) {
                if (envelopeBpDb[j] > reg.peakDb) reg.peakDb = envelopeBpDb[j];
                sumDb += envelopeBpDb[j];
                sumSf += sfValues[j];
                count++;
            }

            if (count > 0) {
                reg.avgDb = sumDb / count;
                reg.spectralFlatness = sumSf / count;
            }

            mBreathRegions.push_back(reg);
        }
    }

    // Merge overlapping/adjacent regions with small gaps (< 30ms)
    if (mBreathRegions.size() > 1) {
        std::vector<BreathRegion> merged;
        merged.push_back(mBreathRegions[0]);

        int gapThreshold = static_cast<int>(mSampleRate * 0.030f); // 30ms gap tolerance

        for (size_t i = 1; i < mBreathRegions.size(); ++i) {
            auto& last = merged.back();
            const auto& cur = mBreathRegions[i];

            if (cur.startSample - last.endSample <= gapThreshold) {
                // Merge: extend end, update stats
                last.endSample = cur.endSample;
                last.peakDb = std::max(last.peakDb, cur.peakDb);
                last.avgDb = (last.avgDb + cur.avgDb) * 0.5f;
                last.spectralFlatness = (last.spectralFlatness + cur.spectralFlatness) * 0.5f;
            } else {
                merged.push_back(cur);
            }
        }

        mBreathRegions = std::move(merged);
    }
}

//==============================================================================
// Pass 2: Process breath regions
//==============================================================================
void VCPluginDSP::processBreaths(float* left, float* right, int numSamples)
{
    // Build gain curve for entire audio
    std::vector<float> gainCurve(numSamples, 0.0f); // 0dB = no change

    for (const auto& reg : mBreathRegions) {
        // Fill the breath region with the reduction amount
        for (int i = reg.startSample; i < reg.endSample && i < numSamples; ++i) {
            gainCurve[i] = reg.appliedReductionDb;
        }
    }

    // Apply gain curve with appropriate smoothing
    if (mParams.autoSmooth) {
        // Mode A: auto attack/release smoothing
        float attackCoef = std::exp(-1.0f / (mParams.attack * 0.001f * static_cast<float>(mSampleRate)));
        float attackCoefInv = 1.0f - attackCoef;
        float releaseCoef = std::exp(-1.0f / (mParams.release * 0.001f * static_cast<float>(mSampleRate)));
        float releaseCoefInv = 1.0f - releaseCoef;

        float currentGainDb = 0.0f;

        for (int i = 0; i < numSamples; ++i) {
            float targetGainDb = gainCurve[i];

            if (currentGainDb > targetGainDb) {
                // Going down (attacking into breath): fast
                currentGainDb += (targetGainDb - currentGainDb) * attackCoefInv;
            } else if (currentGainDb < targetGainDb) {
                // Going up (releasing from breath): slower
                currentGainDb += (targetGainDb - currentGainDb) * releaseCoefInv;
            }

            gainCurve[i] = currentGainDb;
        }
    } else {
        // Mode B: step reduction with micro-fade
        int fadeInSamples = static_cast<int>(mSampleRate * mParams.fadeIn / 1000.0f);
        int fadeOutSamples = static_cast<int>(mSampleRate * mParams.fadeOut / 1000.0f);

        // Apply fades at region boundaries
        for (const auto& reg : mBreathRegions) {
            int start = reg.startSample;
            int end = std::min(reg.endSample, numSamples);
            int length = end - start;
            if (length <= 0) continue;

            float reductionDb = reg.appliedReductionDb;
            float reductionGain = dBToLinear(reductionDb);

            // FadeIn: linear ramp from 1.0 to reductionGain
            int nIn = std::min(fadeInSamples, length / 3);
            int nOut = std::min(fadeOutSamples, length / 3);

            if (nIn + nOut > length) {
                nIn = length / 3;
                nOut = length / 3;
            }

            // Fade in region
            for (int i = 0; i < nIn && (start + i) < numSamples; ++i) {
                float t = static_cast<float>(i) / static_cast<float>(std::max(nIn, 1));
                float g = 1.0f + (reductionGain - 1.0f) * t;
                gainCurve[start + i] = linearToDb(g);
            }

            // Sustain region (already set to reductionDb)
            // (gainCurve already set above)

            // Fade out region
            int fadeOutStart = end - nOut;
            if (fadeOutStart < start + nIn) fadeOutStart = start + nIn;
            for (int i = fadeOutStart; i < end && i < numSamples; ++i) {
                float t = static_cast<float>(i - fadeOutStart) / static_cast<float>(std::max(nOut, 1));
                float g = reductionGain + (1.0f - reductionGain) * t;
                gainCurve[i] = linearToDb(g);
            }
        }
    }

    // Apply lookahead: shift gain curve forward by lookahead samples
    int lookaheadSamples = static_cast<int>(mSampleRate * mParams.lookahead / 1000.0f);
    if (lookaheadSamples > 0) {
        // Shift gain curve so that gain decisions are applied 'lookahead' samples earlier
        // This means the gain at sample i should be what was originally at i + lookahead
        std::vector<float> shifted(numSamples, 0.0f);
        for (int i = 0; i < numSamples; ++i) {
            int srcIdx = std::min(i + lookaheadSamples, numSamples - 1);
            shifted[i] = gainCurve[srcIdx];
        }
        gainCurve = std::move(shifted);
    }

    // Apply gain curve to audio
    for (int i = 0; i < numSamples; ++i) {
        float gainLin = dBToLinear(gainCurve[i]);
        left[i] *= gainLin;
        right[i] *= gainLin;
    }
}

//==============================================================================
// Two-pass processing (detect + process)
//==============================================================================
void VCPluginDSP::processTwoPass(float* left, float* right, int numSamples)
{
    if (!mEnabled)
        return;

    detectBreaths(left, right, numSamples);
    processBreaths(left, right, numSamples);
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
    // In VST3 mode, use two-pass as well (streaming mode could be implemented later)
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
    mFullBandEnvelope.reset();
    mLookahead.reset();
    mState = BreathState::IDLE;
    mBreathStartSample = 0;
    mCurrentGainDb = 0.0f;
}

//==============================================================================
// Set parameters
//==============================================================================
void VCPluginDSP::setParams(const Params& p)
{
    mParams = p;
    updateBPCoefficients();

    if (mSampleRate > 0.0) {
        mLookahead.prepare(mParams.lookahead, mSampleRate);
    }
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
    ss << "VC-BreathControl Detection Report\n";
    ss << "=======================================================\n";
    ss << "Parameters:\n";
    ss << "  Threshold: " << mParams.threshold << " dBFS\n";
    ss << "  Reduction: " << mParams.reduction << " dB\n";
    ss << "  Attack: " << mParams.attack << " ms\n";
    ss << "  Release: " << mParams.release << " ms\n";
    ss << "  Auto Smooth: " << (mParams.autoSmooth ? "true" : "false") << "\n";
    if (!mParams.autoSmooth) {
        ss << "  Fade In: " << mParams.fadeIn << " ms\n";
        ss << "  Fade Out: " << mParams.fadeOut << " ms\n";
    }
    ss << "  Min Breath Duration: " << mParams.minBreathDuration << " ms\n";
    ss << "  Sensitivity: " << mParams.sensitivity << "\n";
    ss << "  Detection Band: " << mParams.freqLow << " - " << mParams.freqHigh << " Hz\n";
    ss << "  SF Threshold: " << mParams.sfThreshold << "\n";
    ss << "  Lookahead: " << mParams.lookahead << " ms\n";
    ss << "-------------------------------------------------------\n";
    ss << "Total breath regions: " << mBreathRegions.size() << "\n";

    if (!mBreathRegions.empty()) {
        float totalDurMs = 0.0f;
        float totalPeakDb = 0.0f;
        float totalAvgDb = 0.0f;
        float totalSf = 0.0f;
        float maxReduction = 0.0f;

        for (const auto& r : mBreathRegions) {
            float durMs = static_cast<float>(r.endSample - r.startSample) / static_cast<float>(mSampleRate) * 1000.0f;
            totalDurMs += durMs;
            totalPeakDb += r.peakDb;
            totalAvgDb += r.avgDb;
            totalSf += r.spectralFlatness;
            if (r.appliedReductionDb < maxReduction) maxReduction = r.appliedReductionDb;
        }

        float count = static_cast<float>(mBreathRegions.size());
        float totalDurationSec = static_cast<float>(mSampleRate) > 0 ?
            totalDurMs / 1000.0f : 0.0f;

        ss << "Total breath time: " << (int)totalDurMs << " ms";
        if (mSampleRate > 0) {
            // We don't have total file samples here, just report total time
            ss << "\n";
        }
        ss << "Average breath duration: " << (totalDurMs / count) << " ms\n";
        ss << "Average peak level: " << (totalPeakDb / count) << " dBFS\n";
        ss << "Average spectral flatness: " << (totalSf / count) << "\n";
        ss << "-------------------------------------------------------\n";
        ss << std::right;
        ss << std::setw(4) << "No." << " "
           << std::setw(10) << "Time(s)" << " "
           << std::setw(10) << "Dur(ms)" << " "
           << std::setw(10) << "Peak(dB)" << " "
           << std::setw(10) << "Avg(dB)" << " "
           << std::setw(8) << "SF" << " "
           << std::setw(10) << "Red(dB)" << "\n";

        int displayCount = std::min(static_cast<int>(mBreathRegions.size()), 50);
        for (int i = 0; i < displayCount; ++i) {
            const auto& r = mBreathRegions[i];
            float startSec = static_cast<float>(r.startSample) / static_cast<float>(mSampleRate);
            float durMs = static_cast<float>(r.endSample - r.startSample) / static_cast<float>(mSampleRate) * 1000.0f;
            ss << std::setw(4) << i << " "
               << std::setw(10) << std::fixed << std::setprecision(3) << startSec << " "
               << std::setw(10) << std::fixed << std::setprecision(1) << durMs << " "
               << std::setw(10) << std::fixed << std::setprecision(1) << r.peakDb << " "
               << std::setw(10) << std::fixed << std::setprecision(1) << r.avgDb << " "
               << std::setw(8) << std::fixed << std::setprecision(3) << r.spectralFlatness << " "
               << std::setw(10) << std::fixed << std::setprecision(1) << r.appliedReductionDb << "\n";
        }

        if (static_cast<int>(mBreathRegions.size()) > 50) {
            ss << "... " << mBreathRegions.size() << " total regions\n";
        }
    }
    ss << "=======================================================\n";

    return ss.str();
}
