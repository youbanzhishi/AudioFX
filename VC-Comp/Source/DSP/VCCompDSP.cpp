// ============================================================================
// VCCompDSP.cpp - VC-Compressor DSP Engine Implementation
// Gen2: Added 4-band multiband compression with LR4 crossover
// Original Gen1 code preserved in VCCompDSP.cpp.gen1
// ============================================================================

#include "VCCompDSP.h"

VCCompDSP::VCCompDSP()
{
}

VCCompDSP::~VCCompDSP()
{
}

void VCCompDSP::prepare(double sampleRate, int blockSize)
{
    mSampleRate = sampleRate;
    mBlockSize = blockSize;
    
    // Initialize 2 channels (Gen1 single-band state)
    mChannels.resize(2);
    mScHPFs.resize(2);
    
    // Initialize Gen2 multiband state
    for (int ch = 0; ch < 2; ++ch) {
        for (int xover = 0; xover < 3; ++xover) {
            mCrossover[xover][ch].reset();
        }
        for (int band = 0; band < NUM_BANDS; ++band) {
            mBandState[band][ch].reset();
        }
    }
    mCrossoverDirty = true;
    
    // Reset all states
    reset();
}

void VCCompDSP::reset()
{
    for (auto& ch : mChannels)
        ch.reset();
    mLimiter.reset();
    mGainReductionDB = 0.0f;
    
    // Reset multiband state
    for (int ch = 0; ch < 2; ++ch) {
        for (int xover = 0; xover < 3; ++xover) {
            mCrossover[xover][ch].reset();
        }
        for (int band = 0; band < NUM_BANDS; ++band) {
            mBandState[band][ch].reset();
        }
    }
}

void VCCompDSP::setParams(const Params& p)
{
    // Check if crossover frequencies changed
    for (int i = 0; i < 3; ++i) {
        if (mParams.xoverFreqs[i] != p.xoverFreqs[i]) {
            mCrossoverDirty = true;
        }
    }
    mParams = p;
}

void VCCompDSP::updateCrossoverFilters()
{
    for (int ch = 0; ch < 2; ++ch) {
        for (int xover = 0; xover < 3; ++xover) {
            setLR4LP(mCrossover[xover][ch].lp, mParams.xoverFreqs[xover], static_cast<float>(mSampleRate));
            setLR4HP(mCrossover[xover][ch].hp, mParams.xoverFreqs[xover], static_cast<float>(mSampleRate));
        }
    }
    mCrossoverDirty = false;
}

float VCCompDSP::getKneeWidth(int kneeMode, float currentGR)
{
    switch (kneeMode)
    {
        case 0: // Hard
            return 0.0f;
        case 1: // Soft
            return 6.0f;
        case 2: // Auto
        default:
            return 6.0f + currentGR * 0.5f;
    }
}

// ============================================================================
// Compress a single sample for one band (used in multiband mode)
// Returns the compressed output sample
// ============================================================================
float VCCompDSP::compressOneBand(float input, BandCompressorState& state,
                                  float threshold, float ratio, float attack, float release)
{
    state.envelopeFollower.setAttackTime(attack, static_cast<float>(mSampleRate));
    state.envelopeFollower.setReleaseTime(release, static_cast<float>(mSampleRate));
    
    // RMS envelope detection (same as Gen1)
    float envelope = state.envelopeFollower.processSample(input);
    envelope = std::sqrt(envelope);  // RMS -> amplitude
    float envelopeDb = linearToDb(envelope + 1e-10f);
    
    // Compute gain reduction
    float kneeWidth = getKneeWidth(mParams.kneeMode, state.gainReduction);
    float targetGR = mCompressor.computeGainReduction(
        envelopeDb, threshold, ratio, kneeWidth);
    
    // Smooth GR (same as Gen1)
    float grCoef = std::exp(-1.0f / (release * 0.001f * mSampleRate * 0.05f));
    state.gainReduction = state.gainReduction * grCoef + targetGR * (1.0f - grCoef);
    
    // Apply gain reduction
    float grLinear = dBToLinear(-state.gainReduction);
    return input * grLinear;
}

// ============================================================================
// Gen1 single-band processing (unchanged logic)
// ============================================================================
void VCCompDSP::processSingleBand(float* left, float* right, int numSamples)
{
    for (int sample = 0; sample < numSamples; ++sample)
    {
        float inputL = left[sample];
        float inputR = right[sample];
        float dryL = inputL;
        float dryR = inputR;
        
        float detectionL = inputL;
        float detectionR = inputR;
        
        float outputL, outputR;
        float grL, grR;
        
        // Channel 0 (Left)
        {
            auto& proc = mChannels[0];
            
            proc.envelopeFollower.setAttackTime(mParams.attack, mSampleRate);
            proc.envelopeFollower.setReleaseTime(mParams.release, mSampleRate);
            
            float effectiveRelease = mParams.release;
            
            float envelopeL = proc.envelopeFollower.processSample(detectionL);
            float envelopeR = proc.envelopeFollower.processSample(detectionR);
            float envelope = (envelopeL + envelopeR) * 0.5f;
            envelope = std::sqrt(envelope);
            
            float envelopeDb = linearToDb(envelope + 1e-10f);
            
            float kneeWidth = getKneeWidth(mParams.kneeMode, proc.gainReduction);
            float targetGR = mCompressor.computeGainReduction(
                envelopeDb, mParams.threshold, mParams.ratio, kneeWidth);
            
            float grCoef = std::exp(-1.0f / (effectiveRelease * 0.001f * mSampleRate * 0.05f));
            proc.gainReduction = proc.gainReduction * grCoef + targetGR * (1.0f - grCoef);
            
            grL = proc.gainReduction;
            
            float grLinear = dBToLinear(-proc.gainReduction);
            outputL = inputL * grLinear;
            
            if (mParams.character == 1)
            {
                proc.warmCharacter.processSample(outputL, outputR, proc.gainReduction);
            }
        }
        
        // Channel 1 (Right)
        {
            auto& proc = mChannels[1];
            proc.envelopeFollower.setAttackTime(mParams.attack, mSampleRate);
            proc.envelopeFollower.setReleaseTime(mParams.release, mSampleRate);
            
            float effectiveRelease = mParams.release;
            
            float grCoef = std::exp(-1.0f / (effectiveRelease * 0.001f * mSampleRate * 0.05f));
            
            float envelopeL = mChannels[0].envelopeFollower.getEnvelope();
            float envelopeR = proc.envelopeFollower.processSample(detectionR);
            float envelope = (envelopeL + envelopeR) * 0.5f;
            envelope = std::sqrt(envelope);
            float envelopeDb = linearToDb(envelope + 1e-10f);
            
            float kneeWidth = getKneeWidth(mParams.kneeMode, proc.gainReduction);
            float targetGR = mCompressor.computeGainReduction(
                envelopeDb, mParams.threshold, mParams.ratio, kneeWidth);
            
            proc.gainReduction = proc.gainReduction * grCoef + targetGR * (1.0f - grCoef);
            
            grR = proc.gainReduction;
            
            float grLinear = dBToLinear(-proc.gainReduction);
            outputR = inputR * grLinear;
            
            if (mParams.character == 1)
            {
                proc.warmCharacter.processSample(outputL, outputR, proc.gainReduction);
            }
        }
        
        // Apply makeup gain
        outputL *= dBToLinear(mParams.gain);
        outputR *= dBToLinear(mParams.gain);
        
        // Wet/Dry mix
        float mixFactor = mParams.mix / 100.0f;
        outputL = dryL * (1.0f - mixFactor) + outputL * mixFactor;
        outputR = dryR * (1.0f - mixFactor) + outputR * mixFactor;
        
        // Apply trim
        outputL *= dBToLinear(mParams.trim);
        outputR *= dBToLinear(mParams.trim);
        
        // Limiter
        outputL = mLimiter.processSample(outputL);
        outputR = mLimiter.processSample(outputR);
        
        left[sample] = outputL;
        right[sample] = outputR;
        
        mGainReductionDB = (grL + grR) * 0.5f;
    }
}

// ============================================================================
// Gen2 multiband processing
// Signal flow: Input -> LR4 Crossover x3 -> 4 bands -> compress each -> merge -> output
// ============================================================================
void VCCompDSP::processMultiband(float* left, float* right, int numSamples)
{
    if (mCrossoverDirty) {
        updateCrossoverFilters();
    }
    
    float totalGR = 0.0f;
    
    for (int sample = 0; sample < numSamples; ++sample)
    {
        float dryL = left[sample];
        float dryR = right[sample];
        
        // Per-channel crossover splitting
        // We produce 4 band signals per channel
        // Band 0: < xoverFreqs[0] (low)
        // Band 1: xoverFreqs[0] ~ xoverFreqs[1] (mid-low)
        // Band 2: xoverFreqs[1] ~ xoverFreqs[2] (mid-high)
        // Band 3: > xoverFreqs[2] (high)
        
        float bandL[NUM_BANDS];
        float bandR[NUM_BANDS];
        
        for (int ch = 0; ch < 2; ++ch) {
            float input = (ch == 0) ? left[sample] : right[sample];
            float bands[NUM_BANDS];
            
            // Crossover cascade:
            // Crossover 0 at freq[0]: splits input into low(<f0) and high(>f0)
            // Crossover 1 at freq[1]: splits high into mid-low(f0~f1) and high(>f1)
            // Crossover 2 at freq[2]: splits remaining high into mid-high(f1~f2) and high(>f2)
            
            float lo0  = mCrossover[0][ch].lp.process(input);
            float hi0  = mCrossover[0][ch].hp.process(input);
            
            float lo1  = mCrossover[1][ch].lp.process(hi0);  // mid-low
            float hi1  = mCrossover[1][ch].hp.process(hi0);
            
            float lo2  = mCrossover[2][ch].lp.process(hi1);  // mid-high
            float hi2  = mCrossover[2][ch].hp.process(hi1);  // high
            
            bands[0] = lo0;   // low: < 120Hz
            bands[1] = lo1;   // mid-low: 120Hz ~ 1kHz
            bands[2] = lo2;   // mid-high: 1kHz ~ 8kHz
            bands[3] = hi2;   // high: > 8kHz
            
            if (ch == 0) {
                for (int b = 0; b < NUM_BANDS; ++b) bandL[b] = bands[b];
            } else {
                for (int b = 0; b < NUM_BANDS; ++b) bandR[b] = bands[b];
            }
        }
        
        // Compress each band independently
        float outL = 0.0f, outR = 0.0f;
        float maxGR = 0.0f;
        
        for (int b = 0; b < NUM_BANDS; ++b) {
            // Solo mode: mute other bands
            if (mParams.soloBand > 0 && mParams.soloBand != (b + 1)) {
                continue;
            }
            
            float compL = compressOneBand(bandL[b], mBandState[b][0],
                                           mParams.bandThreshold[b], mParams.bandRatio[b],
                                           mParams.attack, mParams.release);
            float compR = compressOneBand(bandR[b], mBandState[b][1],
                                           mParams.bandThreshold[b], mParams.bandRatio[b],
                                           mParams.attack, mParams.release);
            
            // Apply per-band makeup gain
            compL *= dBToLinear(mParams.bandMakeup[b]);
            compR *= dBToLinear(mParams.bandMakeup[b]);
            
            // Sum bands back together
            outL += compL;
            outR += compR;
            
            // Track max GR for metering
            float avgGR = (mBandState[b][0].gainReduction + mBandState[b][1].gainReduction) * 0.5f;
            if (avgGR > maxGR) maxGR = avgGR;
        }
        
        // Apply global makeup gain
        outL *= dBToLinear(mParams.gain);
        outR *= dBToLinear(mParams.gain);
        
        // Wet/Dry mix
        float mixFactor = mParams.mix / 100.0f;
        outL = dryL * (1.0f - mixFactor) + outL * mixFactor;
        outR = dryR * (1.0f - mixFactor) + outR * mixFactor;
        
        // Apply trim
        outL *= dBToLinear(mParams.trim);
        outR *= dBToLinear(mParams.trim);
        
        // Limiter
        outL = mLimiter.processSample(outL);
        outR = mLimiter.processSample(outR);
        
        left[sample] = outL;
        right[sample] = outR;
        
        totalGR += maxGR;
    }
    
    mGainReductionDB = totalGR / numSamples;
}

// ============================================================================
// Main process entry - routes to single-band or multiband
// ============================================================================
void VCCompDSP::process(float* left, float* right, int numSamples)
{
    if (!mEnabled)
        return;
    
    if (mParams.multiband == 0) {
        processSingleBand(left, right, numSamples);
    } else {
        processMultiband(left, right, numSamples);
    }
}
