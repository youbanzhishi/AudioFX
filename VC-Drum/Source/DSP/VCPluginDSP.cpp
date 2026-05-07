#include "VCPluginDSP.h"

#ifdef VC_STANDALONE
#include <algorithm>
#include <cmath>
#include <cstdlib>
#endif

//==============================================================================
// VCKickSynth Implementation
//==============================================================================

void VCKickSynth::prepare(double sampleRate)
{
    mSampleRate = sampleRate;
    reset();
}

void VCKickSynth::noteOn(float velocity)
{
    mActive = true;
    mEnvelope = velocity;
    mPhase = 0.0;
}

float VCKickSynth::processSample()
{
    if (!mActive) return 0.0f;

    // Frequency sweep: freq(t) = freq_end + (freq_start - freq_end) * envelope
    // The envelope doubles as the sweep tracker (decays over time)
    float ampTau = mParams.decay / 1000.0f; // seconds
    float ampDecay = std::exp(-1.0f / (mSampleRate * ampTau));

    // Current frequency from envelope position
    float currentFreq = mParams.freqEnd + (mParams.freqStart - mParams.freqEnd) * mEnvelope;

    // Advance phase
    mPhase += currentFreq / mSampleRate;
    if (mPhase >= 1.0) mPhase -= 1.0;

    // Generate sine with optional drive (soft clip via tanh)
    float sample = std::sin(2.0 * VC_PI_D * mPhase);
    if (mParams.drive > 1.0f) {
        sample = std::tanh(sample * mParams.drive);
    }

    // Apply amplitude envelope
    float output = sample * mEnvelope;

    // Decay envelope
    mEnvelope *= ampDecay;

    if (mEnvelope < 0.001f) {
        mActive = false;
        mEnvelope = 0.0f;
    }

    return output;
}

void VCKickSynth::reset()
{
    mPhase = 0.0;
    mEnvelope = 0.0f;
    mActive = false;
}

//==============================================================================
// VCSnareSynth Implementation
//==============================================================================

void VCSnareSynth::prepare(double sampleRate)
{
    mSampleRate = sampleRate;
    updateBPFCoefficients();
    reset();
}

void VCSnareSynth::noteOn(float velocity)
{
    mActive = true;
    mEnvelope = velocity;
    mPhase = 0.0;
    mBPF = BiquadState();
    updateBPFCoefficients();
}

float VCSnareSynth::generateWhiteNoise()
{
    return (float)std::rand() / (float)RAND_MAX * 2.0f - 1.0f;
}

void VCSnareSynth::updateBPFCoefficients()
{
    float omega = 2.0f * VC_PI * mParams.noiseFreq / static_cast<float>(mSampleRate);
    float alpha = std::sin(omega) / (2.0f * mParams.noiseQ);
    float cosw = std::cos(omega);

    mBPF.b0 = alpha;
    mBPF.b1 = 0.0f;
    mBPF.b2 = -alpha;
    float a0 = 1.0f + alpha;
    mBPF.a1 = -2.0f * cosw / a0;
    mBPF.a2 = (1.0f - alpha) / a0;
    mBPF.b0 /= a0;
    mBPF.b2 /= a0;
}

float VCSnareSynth::processSample()
{
    if (!mActive) return 0.0f;

    float tau = mParams.decay / 1000.0f;
    float decayCoeff = std::exp(-1.0f / (mSampleRate * tau));

    // --- Body: triangle wave with frequency sweep ---
    float bodyFreq = mParams.freqEnd + (mParams.freqStart - mParams.freqEnd) * mEnvelope;
    mPhase += bodyFreq / mSampleRate;
    if (mPhase >= 1.0) mPhase -= 1.0;

    float triangle = 4.0f * std::abs(static_cast<float>(mPhase) - 0.5f) - 1.0f;
    float body = triangle * mEnvelope;

    // --- Noise: white noise through bandpass filter ---
    float noise = generateWhiteNoise();
    float filtered = mBPF.b0 * noise + mBPF.b1 * mBPF.x1 + mBPF.b2 * mBPF.x2
                   - mBPF.a1 * mBPF.y1 - mBPF.a2 * mBPF.y2;
    mBPF.x2 = mBPF.x1; mBPF.x1 = noise;
    mBPF.y2 = mBPF.y1; mBPF.y1 = filtered;

    float noiseEnv = std::min(mEnvelope * 1.2f, 1.0f);
    filtered *= noiseEnv;

    // --- Mix body and noise ---
    float tone = mParams.tone;
    float output = body * tone + filtered * (1.0f - tone);

    mEnvelope *= decayCoeff;

    if (mEnvelope < 0.001f) {
        mActive = false;
        mEnvelope = 0.0f;
    }

    return output;
}

void VCSnareSynth::reset()
{
    mPhase = 0.0;
    mEnvelope = 0.0f;
    mActive = false;
    mBPF = BiquadState();
}

//==============================================================================
// VCHiHatSynth Implementation
//==============================================================================

void VCHiHatSynth::prepare(double sampleRate)
{
    mSampleRate = sampleRate;
    updateBPFCoefficients();
    reset();
}

void VCHiHatSynth::noteOn(float velocity, Type type)
{
    mActive = true;
    mEnvelope = velocity;
    mCurrentType = type;
    for (int i = 0; i < kNumSquares; ++i) {
        mPhases[i] = 0.0;
    }
    mBPF = BiquadState();
    updateBPFCoefficients();
}

void VCHiHatSynth::updateBPFCoefficients()
{
    float omega = 2.0f * VC_PI * mParams.bpfFreq / static_cast<float>(mSampleRate);
    float alpha = std::sin(omega) / (2.0f * mParams.bpfQ);
    float cosw = std::cos(omega);

    mBPF.b0 = alpha;
    mBPF.b1 = 0.0f;
    mBPF.b2 = -alpha;
    float a0 = 1.0f + alpha;
    mBPF.a1 = -2.0f * cosw / a0;
    mBPF.a2 = (1.0f - alpha) / a0;
    mBPF.b0 /= a0;
    mBPF.b2 /= a0;
}

float VCHiHatSynth::processSample()
{
    if (!mActive) return 0.0f;

    float decayMs = (mCurrentType == Type::Open) ? mParams.decayOpen : mParams.decayClosed;
    float tau = decayMs / 1000.0f;
    float decayCoeff = std::exp(-1.0f / (mSampleRate * tau));

    float baseFreq = 2500.0f;
    float sum = 0.0f;
    for (int i = 0; i < kNumSquares; ++i) {
        float freq = baseFreq * kDetuneRatios[i];
        mPhases[i] += freq / mSampleRate;
        if (mPhases[i] >= 1.0) mPhases[i] -= 1.0;

        float sq = (mPhases[i] < 0.5) ? 1.0f : -1.0f;
        sum += sq;
    }
    sum /= static_cast<float>(kNumSquares);

    float filtered = mBPF.b0 * sum + mBPF.b1 * mBPF.x1 + mBPF.b2 * mBPF.x2
                   - mBPF.a1 * mBPF.y1 - mBPF.a2 * mBPF.y2;
    mBPF.x2 = mBPF.x1; mBPF.x1 = sum;
    mBPF.y2 = mBPF.y1; mBPF.y1 = filtered;

    float output = filtered * mEnvelope;

    mEnvelope *= decayCoeff;

    if (mEnvelope < 0.001f) {
        mActive = false;
        mEnvelope = 0.0f;
    }

    return output;
}

void VCHiHatSynth::reset()
{
    for (int i = 0; i < kNumSquares; ++i) mPhases[i] = 0.0;
    mEnvelope = 0.0f;
    mActive = false;
    mBPF = BiquadState();
}

//==============================================================================
// VCClapSynth Implementation
//==============================================================================

void VCClapSynth::prepare(double sampleRate)
{
    mSampleRate = sampleRate;
    updateBPFCoefficients();
    reset();
}

void VCClapSynth::noteOn(float velocity)
{
    mActive = true;
    mEnvelope = velocity;
    mVelocity = velocity;
    mCurrentBurst = 0;
    mBurstTimer = 0.0;
    mBurstInterval = mParams.spread / 1000.0 * mSampleRate;
    mBPF = BiquadState();
    updateBPFCoefficients();
}

float VCClapSynth::generateWhiteNoise()
{
    return (float)std::rand() / (float)RAND_MAX * 2.0f - 1.0f;
}

void VCClapSynth::updateBPFCoefficients()
{
    float omega = 2.0f * VC_PI * mParams.bpfFreq / static_cast<float>(mSampleRate);
    float alpha = std::sin(omega) / (2.0f * mParams.bpfQ);
    float cosw = std::cos(omega);

    mBPF.b0 = alpha;
    mBPF.b1 = 0.0f;
    mBPF.b2 = -alpha;
    float a0 = 1.0f + alpha;
    mBPF.a1 = -2.0f * cosw / a0;
    mBPF.a2 = (1.0f - alpha) / a0;
    mBPF.b0 /= a0;
    mBPF.b2 /= a0;
}

float VCClapSynth::processSample()
{
    if (!mActive) return 0.0f;

    mBurstTimer += 1.0;

    float burstDurationSamples = mSampleRate * 0.001f; // ~1ms per burst
    double burstStart = mCurrentBurst * mBurstInterval;
    double burstEnd = burstStart + burstDurationSamples;

    float burstEnv = 0.0f;
    if (mBurstTimer >= burstStart && mBurstTimer < burstEnd) {
        burstEnv = 1.0f;
    } else if (mBurstTimer >= burstEnd && mCurrentBurst < mParams.clapCount - 1) {
        if (mBurstTimer >= (mCurrentBurst + 1) * mBurstInterval) {
            mCurrentBurst++;
        }
    }

    float tau = mParams.decay / 1000.0f;
    float decayCoeff = std::exp(-1.0f / (mSampleRate * tau));

    float env = 0.0f;
    double lastBurstEnd = (mParams.clapCount - 1) * mBurstInterval + burstDurationSamples;

    if (mBurstTimer < lastBurstEnd) {
        env = burstEnv * mVelocity;
    } else {
        env = mEnvelope;
    }

    float noise = generateWhiteNoise();
    float filtered = mBPF.b0 * noise + mBPF.b1 * mBPF.x1 + mBPF.b2 * mBPF.x2
                   - mBPF.a1 * mBPF.y1 - mBPF.a2 * mBPF.y2;
    mBPF.x2 = mBPF.x1; mBPF.x1 = noise;
    mBPF.y2 = mBPF.y1; mBPF.y1 = filtered;

    float output = filtered * env;

    mEnvelope *= decayCoeff;

    if (mEnvelope < 0.001f && mBurstTimer > lastBurstEnd) {
        mActive = false;
        mEnvelope = 0.0f;
    }

    return output;
}

void VCClapSynth::reset()
{
    mEnvelope = 0.0f;
    mActive = false;
    mCurrentBurst = 0;
    mBurstTimer = 0.0;
    mBPF = BiquadState();
}

//==============================================================================
// VCDrumBusCompressor Implementation
//==============================================================================

void VCDrumBusCompressor::prepare(double sampleRate)
{
    mSampleRate = sampleRate;
    reset();
}

void VCDrumBusCompressor::setParams(const Params& p)
{
    mParams = p;
}

void VCDrumBusCompressor::process(float* left, float* right, int numSamples)
{
    if (!mParams.enabled) return;

    float attackCoeff = std::exp(-1.0f / (mSampleRate * mParams.attack / 1000.0f));
    float releaseCoeff = std::exp(-1.0f / (mSampleRate * mParams.release / 1000.0f));
    float thresholdLin = VCStandalone::decibelsToGain(mParams.threshold);
    float makeupLin = VCStandalone::decibelsToGain(mParams.makeupGain);

    for (int i = 0; i < numSamples; ++i) {
        float peak = std::max(std::abs(left[i]), std::abs(right[i]));

        if (peak > mEnvelopeFollower) {
            mEnvelopeFollower = attackCoeff * mEnvelopeFollower + (1.0f - attackCoeff) * peak;
        } else {
            mEnvelopeFollower = releaseCoeff * mEnvelopeFollower + (1.0f - releaseCoeff) * peak;
        }

        float gain = 1.0f;
        if (mEnvelopeFollower > thresholdLin) {
            float overDb = VCStandalone::gainToDecibels(mEnvelopeFollower / thresholdLin);
            float reductionDb = overDb * (1.0f - 1.0f / mParams.ratio);
            gain = VCStandalone::decibelsToGain(-reductionDb);
        }

        mGainSmooth = 0.999f * mGainSmooth + 0.001f * gain;

        float totalGain = mGainSmooth * makeupLin;
        left[i] *= totalGain;
        right[i] *= totalGain;
    }
}

void VCDrumBusCompressor::reset()
{
    mEnvelopeFollower = 0.0f;
    mGainSmooth = 1.0f;
}

//==============================================================================
// VCPatternSequencer Implementation
//==============================================================================

void VCPatternSequencer::prepare(double sampleRate, float bpm)
{
    mSampleRate = sampleRate;
    setBPM(bpm);
    reset();
}

void VCPatternSequencer::setBPM(float bpm)
{
    mBPM = bpm;
    mStepDuration = (60.0 / mBPM) / 4.0 * mSampleRate;
}

void VCPatternSequencer::setSwing(float percent)
{
    mSwing = percent / 100.0f;
}

void VCPatternSequencer::setHumanize(float percent)
{
    mHumanize = percent / 100.0f;
}

int VCPatternSequencer::processSample(float velocities[static_cast<int>(DrumType::NumTypes)])
{
    int triggered = 0;

    for (int d = 0; d < static_cast<int>(DrumType::NumTypes); ++d) {
        velocities[d] = 0.0f;
    }

    mSampleCounter += 1.0;

    double stepDur = mStepDuration;
    if (mCurrentStep % 2 == 0) {
        stepDur = mStepDuration * (1.0 + mSwing * 0.33);
    } else {
        stepDur = mStepDuration * (1.0 - mSwing * 0.33);
    }

    double humanizeOffset = mHumanizeOffset;

    if (mSampleCounter >= stepDur + humanizeOffset) {
        mSampleCounter = 0.0;
        mCurrentStep = (mCurrentStep + 1) % 16;

        if (mHumanize > 0.0f) {
            mHumanizeOffset = ((float)std::rand() / (float)RAND_MAX - 0.5f) * 2.0f
                            * mHumanize * mStepDuration * 0.1;
        } else {
            mHumanizeOffset = 0.0;
        }

        for (int d = 0; d < static_cast<int>(DrumType::NumTypes); ++d) {
            if (mPattern.drums[d][mCurrentStep].active) {
                triggered |= (1 << d);
                float vel = mPattern.drums[d][mCurrentStep].velocity;

                if (mHumanize > 0.0f) {
                    float velOffset = ((float)std::rand() / (float)RAND_MAX - 0.5f) * 2.0f
                                    * mHumanize * 0.3f;
                    vel = std::clamp(vel + velOffset, 0.1f, 1.0f);
                }
                velocities[d] = vel;
            }
        }
    }

    return triggered;
}

void VCPatternSequencer::reset()
{
    mCurrentStep = 0;
    mSampleCounter = 0.0;
    mHumanizeOffset = 0.0;
    for (int d = 0; d < static_cast<int>(DrumType::NumTypes); ++d) {
        mHumanizeVelocity[d] = 1.0f;
    }
    mStepTriggered = false;
}

//==============================================================================
// Pattern generators
//==============================================================================

VCPatternSequencer::Pattern VCPatternSequencer::createKickOnlyPattern()
{
    Pattern p;
    p.drums[0][0].active = true;  p.drums[0][0].velocity = 1.0f;
    p.drums[0][4].active = true;  p.drums[0][4].velocity = 1.0f;
    p.drums[0][8].active = true;  p.drums[0][8].velocity = 1.0f;
    p.drums[0][12].active = true; p.drums[0][12].velocity = 1.0f;
    return p;
}

VCPatternSequencer::Pattern VCPatternSequencer::createSnareOnlyPattern()
{
    Pattern p;
    p.drums[1][4].active = true;  p.drums[1][4].velocity = 1.0f;
    p.drums[1][12].active = true; p.drums[1][12].velocity = 1.0f;
    return p;
}

VCPatternSequencer::Pattern VCPatternSequencer::createHiHatOnlyPattern()
{
    Pattern p;
    for (int i = 0; i < 16; i += 2) {
        p.drums[2][i].active = true;
        p.drums[2][i].velocity = (i % 4 == 0) ? 1.0f : 0.7f;
    }
    return p;
}

VCPatternSequencer::Pattern VCPatternSequencer::createBasicBeatPattern()
{
    Pattern p;
    p.drums[0][0].active = true;  p.drums[0][0].velocity = 1.0f;
    p.drums[0][8].active = true;  p.drums[0][8].velocity = 1.0f;
    p.drums[1][4].active = true;  p.drums[1][4].velocity = 1.0f;
    p.drums[1][12].active = true; p.drums[1][12].velocity = 1.0f;
    for (int i = 0; i < 16; i += 2) {
        p.drums[2][i].active = true;
        p.drums[2][i].velocity = 0.8f;
    }
    return p;
}

VCPatternSequencer::Pattern VCPatternSequencer::createHousePattern()
{
    Pattern p;
    // Four on the floor
    for (int i = 0; i < 16; i += 4) {
        p.drums[0][i].active = true;
        p.drums[0][i].velocity = 1.0f;
    }
    // Clap on 2 and 4
    p.drums[3][4].active = true;  p.drums[3][4].velocity = 0.9f;
    p.drums[3][12].active = true; p.drums[3][12].velocity = 0.9f;
    // Open hi-hat on offbeats
    for (int i = 2; i < 16; i += 4) {
        p.drums[2][i].active = true;
        p.drums[2][i].velocity = 0.6f;
    }
    // Closed hi-hat on beats
    for (int i = 0; i < 16; i += 4) {
        if (!p.drums[2][i].active) {
            p.drums[2][i].active = true;
            p.drums[2][i].velocity = 0.5f;
        }
    }
    return p;
}

VCPatternSequencer::Pattern VCPatternSequencer::createTechnoPattern()
{
    Pattern p;
    for (int i = 0; i < 16; i += 4) {
        p.drums[0][i].active = true;
        p.drums[0][i].velocity = 1.0f;
    }
    // Closed hi-hat on 8ths
    for (int i = 0; i < 16; i += 2) {
        p.drums[2][i].active = true;
        p.drums[2][i].velocity = (i % 4 == 0) ? 0.7f : 0.4f;
    }
    // Open hi-hat
    p.drums[2][6].active = true;  p.drums[2][6].velocity = 0.6f;
    p.drums[2][14].active = true; p.drums[2][14].velocity = 0.6f;
    // Clap
    p.drums[3][4].active = true;  p.drums[3][4].velocity = 0.8f;
    p.drums[3][12].active = true; p.drums[3][12].velocity = 0.8f;
    return p;
}

VCPatternSequencer::Pattern VCPatternSequencer::createHipHopPattern()
{
    Pattern p;
    p.drums[0][0].active = true;  p.drums[0][0].velocity = 1.0f;
    p.drums[0][6].active = true;  p.drums[0][6].velocity = 0.8f;
    p.drums[0][8].active = true;  p.drums[0][8].velocity = 1.0f;
    p.drums[1][4].active = true;  p.drums[1][4].velocity = 1.0f;
    p.drums[1][12].active = true; p.drums[1][12].velocity = 1.0f;
    for (int i = 0; i < 16; i += 2) {
        p.drums[2][i].active = true;
        p.drums[2][i].velocity = (i % 4 == 0) ? 0.8f : 0.5f;
    }
    p.drums[2][3].active = true;  p.drums[2][3].velocity = 0.3f;
    p.drums[2][11].active = true; p.drums[2][11].velocity = 0.3f;
    return p;
}

VCPatternSequencer::Pattern VCPatternSequencer::createTrapPattern()
{
    Pattern p;
    p.drums[0][0].active = true;  p.drums[0][0].velocity = 1.0f;
    p.drums[0][6].active = true;  p.drums[0][6].velocity = 0.9f;
    p.drums[0][9].active = true;  p.drums[0][9].velocity = 0.85f;
    p.drums[1][4].active = true;  p.drums[1][4].velocity = 1.0f;
    p.drums[1][12].active = true; p.drums[1][12].velocity = 1.0f;
    for (int i = 0; i < 16; i++) {
        p.drums[2][i].active = true;
        p.drums[2][i].velocity = 0.5f;
    }
    p.drums[2][0].velocity = 0.8f;
    p.drums[2][4].velocity = 0.3f;
    p.drums[2][8].velocity = 0.8f;
    p.drums[2][12].velocity = 0.3f;
    p.drums[3][4].active = true;  p.drums[3][4].velocity = 0.7f;
    p.drums[3][12].active = true; p.drums[3][12].velocity = 0.7f;
    return p;
}

VCPatternSequencer::Pattern VCPatternSequencer::createDNBPattern()
{
    Pattern p;
    p.drums[0][0].active = true;  p.drums[0][0].velocity = 1.0f;
    p.drums[0][5].active = true;  p.drums[0][5].velocity = 0.8f;
    p.drums[0][8].active = true;  p.drums[0][8].velocity = 1.0f;
    p.drums[0][15].active = true; p.drums[0][15].velocity = 0.75f;
    p.drums[1][4].active = true;  p.drums[1][4].velocity = 1.0f;
    p.drums[1][12].active = true; p.drums[1][12].velocity = 1.0f;
    for (int i = 0; i < 16; i += 2) {
        p.drums[2][i].active = true;
        p.drums[2][i].velocity = 0.6f;
    }
    return p;
}

VCPatternSequencer::Pattern VCPatternSequencer::createFullPattern()
{
    Pattern p = createBasicBeatPattern();
    p.drums[3][4].active = true;  p.drums[3][4].velocity = 0.8f;
    p.drums[3][12].active = true; p.drums[3][12].velocity = 0.8f;
    return p;
}

//==============================================================================
// VCPluginDSP (VCDrumEngine) Implementation
//==============================================================================

VCPluginDSP::VCPluginDSP()
{
}

VCPluginDSP::~VCPluginDSP()
{
}

void VCPluginDSP::prepare(double sampleRate, int blockSize)
{
    mSampleRate = sampleRate;
    mBlockSize = blockSize;

    mKick.prepare(sampleRate);
    mSnare.prepare(sampleRate);
    mHiHat.prepare(sampleRate);
    mClap.prepare(sampleRate);
    mCompressor.prepare(sampleRate);
    mSequencer.prepare(sampleRate, mParams.bpm);

    mInternalBuffer.resize(blockSize * 2);
    mInternalPtrs.resize(2);

    setParams(mParams);
}

void VCPluginDSP::process(float* left, float* right, int numSamples)
{
    if (!mEnabled) return;

    float masterLin = dBToLinear(mParams.masterGain);
    float velocities[static_cast<int>(VCPatternSequencer::DrumType::NumTypes)];

    for (int i = 0; i < numSamples; ++i) {
        // Run sequencer
        int triggered = mSequencer.processSample(velocities);

        // Trigger drum synths
        if (triggered & 1) mKick.noteOn(velocities[0]);
        if (triggered & 2) mSnare.noteOn(velocities[1]);
        if (triggered & 4) {
            // Hi-hat: open on offbeats (steps 2,6,10,14), closed otherwise
            VCHiHatSynth::Type type = VCHiHatSynth::Type::Closed;
            mHiHat.noteOn(velocities[2], type);
        }
        if (triggered & 8) mClap.noteOn(velocities[3]);

        // Render drum synths
        float kickSample  = mKick.processSample();
        float snareSample = mSnare.processSample();
        float hihatSample = mHiHat.processSample();
        float clapSample  = mClap.processSample();

        // Mix to stereo: kick/snare center, hihat/clap slightly wide
        // Scale individual levels to prevent clipping when multiple drums hit
        float kickLevel  = kickSample * 0.7f;
        float snareLevel = snareSample * 0.5f;
        float hihatLevel = hihatSample * 0.5f;
        float clapLevel  = clapSample * 0.5f;

        float center = kickLevel + snareLevel;
        float sides  = hihatLevel + clapLevel;

        left[i]  = (center + sides * 0.7f) * masterLin;
        right[i] = (center + sides * 0.3f) * masterLin;
    }

    // Bus compression
    mCompressor.process(left, right, numSamples);
}

#ifndef VC_STANDALONE
void VCPluginDSP::process(juce::dsp::AudioBlock<float>& block)
{
    if (!mEnabled) return;

    if ((int)mInternalBuffer.size() < (int)block.getNumSamples() * 2)
        mInternalBuffer.resize(block.getNumSamples() * 2);

    float* leftBuf = mInternalBuffer.data();
    float* rightBuf = mInternalBuffer.data() + block.getNumSamples();

    for (size_t i = 0; i < block.getNumSamples(); ++i) {
        leftBuf[i] = block.getChannelPointer(0)[i];
        rightBuf[i] = block.getChannelPointer(1)[i];
    }

    process(leftBuf, rightBuf, static_cast<int>(block.getNumSamples()));

    for (size_t i = 0; i < block.getNumSamples(); ++i) {
        block.getChannelPointer(0)[i] = leftBuf[i];
        block.getChannelPointer(1)[i] = rightBuf[i];
    }
}
#endif

void VCPluginDSP::reset()
{
    mKick.reset();
    mSnare.reset();
    mHiHat.reset();
    mClap.reset();
    mCompressor.reset();
    mSequencer.reset();
}

void VCPluginDSP::setParams(const Params& p)
{
    mEnabled = p.enabled;
    mParams = p;

    mKick.setParams(p.kick);
    mSnare.setParams(p.snare);
    mHiHat.setParams(p.hihat);
    mClap.setParams(p.clap);
    mCompressor.setParams(p.compressor);

    mSequencer.setBPM(p.bpm);
    mSequencer.setSwing(p.swing);
    mSequencer.setHumanize(p.humanize);

    VCPatternSequencer::Pattern pattern;
    switch (p.pattern) {
        case 1:  pattern = VCPatternSequencer::createKickOnlyPattern(); break;
        case 2:  pattern = VCPatternSequencer::createSnareOnlyPattern(); break;
        case 3:  pattern = VCPatternSequencer::createHiHatOnlyPattern(); break;
        case 4:  pattern = VCPatternSequencer::createBasicBeatPattern(); break;
        case 5:  pattern = VCPatternSequencer::createHousePattern(); break;
        case 6:  pattern = VCPatternSequencer::createTechnoPattern(); break;
        case 7:  pattern = VCPatternSequencer::createHipHopPattern(); break;
        case 8:  pattern = VCPatternSequencer::createTrapPattern(); break;
        case 9:  pattern = VCPatternSequencer::createDNBPattern(); break;
        default: pattern = VCPatternSequencer::createFullPattern(); break;
    }
    mSequencer.setPattern(pattern);
}

VCPluginDSP::Params VCPluginDSP::getParams() const
{
    return mParams;
}

void VCPluginDSP::setEnabled(bool enabled)
{
    mEnabled = enabled;
}

//==============================================================================
// MIDI-triggered drum methods (VST3 instrument mode)
//==============================================================================

void VCPluginDSP::triggerDrum(int drumType, float velocity, bool openHiHat)
{
    if (!mEnabled) return;
    mMidiMode = true;

    switch (drumType)
    {
        case 0: // Kick
            mKick.noteOn(velocity);
            break;
        case 1: // Snare
            mSnare.noteOn(velocity);
            break;
        case 2: // HiHat
            {
                VCHiHatSynth::Type type = openHiHat
                    ? VCHiHatSynth::Type::Open
                    : VCHiHatSynth::Type::Closed;
                mHiHat.noteOn(velocity, type);
            }
            break;
        case 3: // Clap
            mClap.noteOn(velocity);
            break;
        default:
            break;
    }
}

void VCPluginDSP::render(float* left, float* right, int numSamples)
{
    if (!mEnabled) return;

    float masterLin = dBToLinear(mParams.masterGain);

    for (int i = 0; i < numSamples; ++i)
    {
        // Render drum synths (no sequencer — drums triggered via triggerDrum)
        float kickSample  = mKick.processSample();
        float snareSample = mSnare.processSample();
        float hihatSample = mHiHat.processSample();
        float clapSample  = mClap.processSample();

        // Mix to stereo: kick/snare center, hihat/clap slightly wide
        float kickLevel  = kickSample * 0.7f;
        float snareLevel = snareSample * 0.5f;
        float hihatLevel = hihatSample * 0.5f;
        float clapLevel  = clapSample * 0.5f;

        float center = kickLevel + snareLevel;
        float sides  = hihatLevel + clapLevel;

        left[i]  = (center + sides * 0.7f) * masterLin;
        right[i] = (center + sides * 0.3f) * masterLin;
    }

    // Bus compression
    mCompressor.process(left, right, numSamples);
}

void VCPluginDSP::renderBars(int bars, std::vector<float>& outLeft, std::vector<float>& outRight)
{
    double samplesPerBar = 4.0 * (60.0 / mParams.bpm) * mSampleRate;
    int totalSamples = static_cast<int>(samplesPerBar * bars);

    outLeft.resize(totalSamples, 0.0f);
    outRight.resize(totalSamples, 0.0f);

    process(outLeft.data(), outRight.data(), totalSamples);
}
