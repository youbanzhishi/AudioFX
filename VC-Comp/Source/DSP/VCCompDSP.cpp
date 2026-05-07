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
    
    // Initialize 2 channels
    mChannels.resize(2);
    mScHPFs.resize(2);
    
    // Reset all states
    reset();
}

void VCCompDSP::reset()
{
    for (auto& ch : mChannels)
        ch.reset();
    mLimiter.reset();
    mGainReductionDB = 0.0f;
}

void VCCompDSP::setParams(const Params& p)
{
    mParams = p;
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
            // Auto knee: wider when GR is small, narrower when heavily compressing
            return 6.0f + currentGR * 0.5f;
    }
}

void VCCompDSP::process(float* left, float* right, int numSamples)
{
    if (!mEnabled)
        return;
    
    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Get input samples
        float inputL = left[sample];
        float inputR = right[sample];
        float dryL = inputL;
        float dryR = inputR;
        
        // Detection signal (use left channel for simplicity in basic DSP)
        float detectionL = inputL;
        float detectionR = inputR;
        
        // Process each channel
        float outputL, outputR;
        float grL, grR;
        
        // Channel 0
        {
            auto& proc = mChannels[0];
            
            proc.envelopeFollower.setAttackTime(mParams.attack, mSampleRate);
            proc.envelopeFollower.setReleaseTime(mParams.release, mSampleRate);
            
            float effectiveRelease = mParams.release;
            
            // Calculate envelope
            float envelopeL = proc.envelopeFollower.processSample(detectionL);
            float envelopeR = proc.envelopeFollower.processSample(detectionR);
            float envelope = (envelopeL + envelopeR) * 0.5f;
            envelope = std::sqrt(envelope);
            
            float envelopeDb = linearToDb(envelope + 1e-10f);
            
            // Calculate GR
            float kneeWidth = getKneeWidth(mParams.kneeMode, proc.gainReduction);
            float targetGR = mCompressor.computeGainReduction(
                envelopeDb, mParams.threshold, mParams.ratio, kneeWidth);
            
            // Smooth GR
            float grCoef = std::exp(-1.0f / (effectiveRelease * 0.001f * mSampleRate * 0.05f));
            proc.gainReduction = proc.gainReduction * grCoef + targetGR * (1.0f - grCoef);
            
            grL = proc.gainReduction;
            
            // Apply gain reduction
            float grLinear = dBToLinear(-proc.gainReduction);
            outputL = inputL * grLinear;
            
            // Character processing
            if (mParams.character == 1) // Warm
            {
                proc.warmCharacter.processSample(outputL, outputR, proc.gainReduction);
            }
        }
        
        // Channel 1
        {
            auto& proc = mChannels[1];
            proc.envelopeFollower.setAttackTime(mParams.attack, mSampleRate);
            proc.envelopeFollower.setReleaseTime(mParams.release, mSampleRate);
            
            float effectiveRelease = mParams.release;
            
            float grCoef = std::exp(-1.0f / (effectiveRelease * 0.001f * mSampleRate * 0.05f));
            
            // Calculate envelope
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
        
        // Output
        left[sample] = outputL;
        right[sample] = outputR;
        
        // Store GR (use average)
        mGainReductionDB = (grL + grR) * 0.5f;
    }
}
