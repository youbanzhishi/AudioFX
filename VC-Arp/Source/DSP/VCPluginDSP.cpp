#include "VCPluginDSP.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <set>

//==============================================================================
// VCArpPattern Implementation
//==============================================================================

void VCArpPattern::setNotes(const std::vector<int>& notes)
{
    // Store and deduplicate + sort input notes
    std::set<int> uniqueNotes;
    for (int n : notes) {
        int transposed = n + mTranspose;
        if (transposed >= 0 && transposed <= 127)
            uniqueNotes.insert(transposed);
    }
    mInputNotes.assign(uniqueNotes.begin(), uniqueNotes.end());
    rebuildPattern();
}

void VCArpPattern::rebuildPattern()
{
    mPattern.clear();

    if (mInputNotes.empty()) {
        mPattern.push_back(60); // default to middle C
        return;
    }

    // Build octave-expanded note set
    // For each octave (0 to octaveRange-1), shift notes up by 12*octave
    std::vector<int> expanded;
    for (int oct = 0; oct < mOctaveRange; ++oct) {
        for (int note : mInputNotes) {
            int shifted = note + 12 * oct;
            if (shifted >= 0 && shifted <= 127) {
                expanded.push_back(shifted);
            }
        }
    }

    if (expanded.empty()) {
        mPattern = expanded;
        return;
    }

    // Sort for up/down patterns
    std::vector<int> sorted = expanded;
    std::sort(sorted.begin(), sorted.end());

    switch (mMode) {
    case VC_ARP_UP:
        mPattern = sorted;
        break;

    case VC_ARP_DOWN:
        mPattern = sorted;
        std::reverse(mPattern.begin(), mPattern.end());
        break;

    case VC_ARP_UP_DOWN: {
        // Up then down, excluding first and last to avoid duplicates
        mPattern = sorted;
        if (sorted.size() > 1) {
            for (int i = (int)sorted.size() - 2; i >= 1; --i) {
                mPattern.push_back(sorted[i]);
            }
        }
        break;
    }

    case VC_ARP_DOWN_UP: {
        // Down then up, excluding first and last to avoid duplicates
        std::vector<int> revSorted = sorted;
        std::reverse(revSorted.begin(), revSorted.end());
        mPattern = revSorted;
        if (revSorted.size() > 1) {
            for (int i = 1; i < (int)revSorted.size() - 1; ++i) {
                mPattern.push_back(revSorted[i]);
            }
        }
        break;
    }

    case VC_ARP_RANDOM: {
        mPattern = sorted;
        // Shuffle using simple LCG seeded from note data
        unsigned int seed = 12345;
        for (int n : mInputNotes) seed ^= (unsigned int)n * 2654435761u;
        for (size_t i = mPattern.size() - 1; i > 0; --i) {
            seed = seed * 1103515245u + 12345u;
            size_t j = (size_t)((seed >> 16) % (i + 1));
            std::swap(mPattern[i], mPattern[j]);
        }
        break;
    }

    case VC_ARP_AS_PLAYED:
        // Use input order (not sorted)
        mPattern.clear();
        for (int oct = 0; oct < mOctaveRange; ++oct) {
            for (int note : mInputNotes) {
                int shifted = note + 12 * oct + mTranspose;
                if (shifted >= 0 && shifted <= 127) {
                    mPattern.push_back(shifted);
                }
            }
        }
        break;

    case VC_ARP_CHORD:
        // All notes at once (pattern has all notes, sequencer plays them simultaneously)
        mPattern = sorted;
        break;
    }
}

//==============================================================================
// VCArpSequencer Implementation
//==============================================================================

void VCArpSequencer::prepare(double sampleRate, float bpm)
{
    mSampleRate = sampleRate;
    mBPM = bpm;
    updateStepDuration();
}

void VCArpSequencer::setBPM(float bpm)
{
    mBPM = std::clamp(bpm, 20.0f, 300.0f);
    updateStepDuration();
}

void VCArpSequencer::setRate(VCArpRate rate)
{
    mRate = rate;
    updateStepDuration();
}

void VCArpSequencer::setGate(float percent)
{
    mGate = std::clamp(percent, 1.0f, 200.0f);
}

void VCArpSequencer::setSwing(float percent)
{
    mSwing = std::clamp(percent, 0.0f, 100.0f);
}

void VCArpSequencer::setHumanize(float percent)
{
    mHumanize = std::clamp(percent, 0.0f, 100.0f);
}

void VCArpSequencer::setVelocityMode(VCVelocityMode mode)
{
    mVelocityMode = mode;
}

void VCArpSequencer::setPattern(const VCArpPattern& pattern)
{
    mPatternNotes = pattern.getPattern();
    mPatternLength = pattern.getPatternLength();
    if (mPatternLength == 0) {
        mPatternNotes = {60};
        mPatternLength = 1;
    }
}

void VCArpSequencer::reset()
{
    mCurrentStep = 0;
    mSampleCounter = 0.0;
    mNoteActive = false;
    mNoteOffSample = 0.0;
    mForward = true;
    mHumanizeOffset = 0.0;
    mHumanizeVelocityScale = 1.0f;
    mCurrentNote = 60;
    mCurrentVelocity = 1.0f;
    updateStepDuration();
}

double VCArpSequencer::calcBaseStepDuration() const
{
    // Duration of one beat in samples
    double beatDuration = 60.0 / (double)mBPM * mSampleRate;

    // Rate subdivisions
    switch (mRate) {
    case VC_RATE_1_1:  return beatDuration * 4.0;   // whole
    case VC_RATE_1_2:  return beatDuration * 2.0;   // half
    case VC_RATE_1_4:  return beatDuration;          // quarter
    case VC_RATE_1_8:  return beatDuration * 0.5;    // eighth
    case VC_RATE_1_16: return beatDuration * 0.25;   // sixteenth
    case VC_RATE_1_32: return beatDuration * 0.125;  // thirty-second
    default:           return beatDuration * 0.5;
    }
}

double VCArpSequencer::calcStepDuration(int step) const
{
    double base = calcBaseStepDuration();

    if (mSwing <= 0.0f) return base;

    // Swing: even steps longer, odd steps shorter (or vice versa)
    // Typical swing: even=67%, odd=33% at max swing
    float swingAmount = mSwing / 100.0f;  // 0..1

    if (step % 2 == 0) {
        // Even step: longer
        double ratio = 1.0 + swingAmount * 0.33;  // up to 1.33
        return base * ratio;
    } else {
        // Odd step: shorter
        double ratio = 1.0 - swingAmount * 0.33;  // down to 0.67
        return base * ratio;
    }
}

float VCArpSequencer::calcVelocity(int step) const
{
    if (mPatternLength == 0) return 1.0f;

    switch (mVelocityMode) {
    case VC_VEL_ORIGINAL:
        return 1.0f;

    case VC_VEL_ASCENDING: {
        float t = (float)(step % mPatternLength) / (float)mPatternLength;
        return 0.3f + 0.7f * t;
    }

    case VC_VEL_DESCENDING: {
        float t = (float)(step % mPatternLength) / (float)mPatternLength;
        return 1.0f - 0.7f * t;
    }

    case VC_VEL_RANDOM:
        // Will be humanized with random offset
        return 0.5f + 0.5f * (float)(rand() % 1000) / 1000.0f;

    default:
        return 1.0f;
    }
}

void VCArpSequencer::updateStepDuration()
{
    mCurrentStepDuration = calcStepDuration(mCurrentStep);
}

float VCArpSequencer::randomFloat()
{
    return (float)(rand() % 10000) / 10000.0f;
}

int VCArpSequencer::randomInt(int min, int max)
{
    if (min >= max) return min;
    return min + (rand() % (max - min + 1));
}

void VCArpSequencer::triggerCurrentStep(std::vector<ArpEvent>& events)
{
    // Update step duration for current step (swing)
    mCurrentStepDuration = calcStepDuration(mCurrentStep);

    // Get note for this step
    int noteIdx = mCurrentStep % mPatternLength;
    mCurrentNote = mPatternNotes[noteIdx];
    mCurrentVelocity = calcVelocity(mCurrentStep) * mHumanizeVelocityScale;
    mCurrentVelocity = std::clamp(mCurrentVelocity, 0.0f, 1.0f);

    // Calculate note-off time based on gate
    double gateDuration = mCurrentStepDuration * (double)(mGate / 100.0f);
    // Cap gate so note off doesn't exceed step duration significantly
    if (gateDuration > mCurrentStepDuration * 2.0) {
        gateDuration = mCurrentStepDuration * 2.0;
    }
    mNoteOffSample = gateDuration;

    // Send note-on event(s)
    if (mCurrentNote >= 0 && mCurrentNote <= 127) {
        ArpEvent on;
        on.noteNumber = mCurrentNote;
        on.velocity = mCurrentVelocity;
        on.noteOn = true;
        events.push_back(on);
        mNoteActive = true;
    }
}

void VCArpSequencer::processSample(std::vector<ArpEvent>& events)
{
    events.clear();

    if (mPatternLength == 0) return;

    // Trigger first note immediately if sequencer just started
    if (!mNoteActive && mCurrentStep == 0 && mSampleCounter == 0.0) {
        triggerCurrentStep(events);
    }

    // Check if we need to send note-off for current note
    if (mNoteActive && mSampleCounter >= mNoteOffSample) {
        ArpEvent off;
        off.noteNumber = mCurrentNote;
        off.velocity = 0.0f;
        off.noteOn = false;
        events.push_back(off);
        mNoteActive = false;
    }

    // Check if it's time for the next step
    double effectiveCounter = mSampleCounter;
    if (mHumanize > 0.0f) {
        effectiveCounter -= mHumanizeOffset;
    }

    if (effectiveCounter >= mCurrentStepDuration) {
        // Advance to next step
        mSampleCounter = 0.0;
        mCurrentStep = (mCurrentStep + 1) % mPatternLength;

        // Apply humanize
        if (mHumanize > 0.0f) {
            float humanizeAmount = mHumanize / 100.0f;
            mHumanizeOffset = (double)(randomFloat() * humanizeAmount * calcBaseStepDuration() * 0.5);
            mHumanizeVelocityScale = 1.0f - humanizeAmount * randomFloat() * 0.5f;
        } else {
            mHumanizeOffset = 0.0;
            mHumanizeVelocityScale = 1.0f;
        }

        // Trigger the new step
        triggerCurrentStep(events);
    }

    mSampleCounter += 1.0;
}

//==============================================================================
// VCArpSynth Implementation — Simple synth for CLI mode
//==============================================================================

void VCArpSynth::prepare(double sampleRate)
{
    mSampleRate = sampleRate;
    // Default decay: ~500ms for nice arpeggio sound
    mDecayCoeff = std::exp(-1.0 / (0.5 * sampleRate));
}

void VCArpSynth::setWaveform(Waveform wf)
{
    mWaveform = wf;
}

void VCArpSynth::noteOn(int noteNumber, float velocity)
{
    mFreq = 440.0f * std::pow(2.0f, (noteNumber - 69) / 12.0f);
    mPhaseInc = (double)mFreq / mSampleRate;
    mVelocity = std::clamp(velocity, 0.0f, 1.0f);
    mEnvelope = 1.0f;
    mActive = true;
}

void VCArpSynth::noteOff()
{
    // Let natural decay handle it
    mActive = false;
}

float VCArpSynth::processSample()
{
    if (mEnvelope < 0.001f) {
        mActive = false;
        return 0.0f;
    }

    // Generate waveform
    float sample = 0.0f;
    switch (mWaveform) {
    case SINE:
        sample = (float)std::sin(2.0 * VC_PI_D * mPhase);
        break;
    case SAW:
        sample = (float)(2.0 * mPhase - 1.0);
        break;
    case SQUARE:
        sample = (mPhase < 0.5) ? 1.0f : -1.0f;
        break;
    }

    // Advance phase
    mPhase += mPhaseInc;
    while (mPhase >= 1.0) mPhase -= 1.0;

    // Apply envelope (exponential decay)
    sample *= mEnvelope * mVelocity;

    // Decay envelope
    mEnvelope *= mDecayCoeff;

    return sample;
}

void VCArpSynth::reset()
{
    mPhase = 0.0;
    mPhaseInc = 0.0;
    mEnvelope = 0.0f;
    mVelocity = 0.0f;
    mActive = false;
}

//==============================================================================
// Presets
//==============================================================================
struct ArpPreset {
    const char* name;
    VCPluginDSP::Params params;
};

static const ArpPreset arpPresets[] = {
    { "bypass",       [](){ VCPluginDSP::Params p; p.enabled = false; return p; }() },
    { "up-8th",       [](){ VCPluginDSP::Params p; p.mode = VC_ARP_UP;
                            p.rate = VC_RATE_1_8; p.gate = 100.0f; return p; }() },
    { "down-8th",     [](){ VCPluginDSP::Params p; p.mode = VC_ARP_DOWN;
                            p.rate = VC_RATE_1_8; p.gate = 100.0f; return p; }() },
    { "up-down-16th", [](){ VCPluginDSP::Params p; p.mode = VC_ARP_UP_DOWN;
                            p.rate = VC_RATE_1_16; p.gate = 80.0f; return p; }() },
    { "trance-gate",  [](){ VCPluginDSP::Params p; p.mode = VC_ARP_UP;
                            p.rate = VC_RATE_1_16; p.gate = 30.0f;
                            p.velocityMode = VC_VEL_ASCENDING;
                            p.waveform = VCArpSynth::SAW; return p; }() },
    { "random-bells", [](){ VCPluginDSP::Params p; p.mode = VC_ARP_RANDOM;
                            p.rate = VC_RATE_1_8; p.gate = 50.0f;
                            p.velocityMode = VC_VEL_RANDOM;
                            p.waveform = VCArpSynth::SINE; return p; }() },
    { "chord-pad",    [](){ VCPluginDSP::Params p; p.mode = VC_ARP_CHORD;
                            p.rate = VC_RATE_1_4; p.gate = 200.0f;
                            p.waveform = VCArpSynth::SINE; return p; }() },
    { "octave-run",   [](){ VCPluginDSP::Params p; p.mode = VC_ARP_UP;
                            p.rate = VC_RATE_1_16; p.octaveRange = 2;
                            p.gate = 70.0f;
                            p.waveform = VCArpSynth::SAW; return p; }() },
    { "ping-pong",    [](){ VCPluginDSP::Params p; p.mode = VC_ARP_UP_DOWN;
                            p.rate = VC_RATE_1_8; p.gate = 90.0f;
                            p.swing = 30.0f; return p; }() },
};

static constexpr int NUM_ARP_PRESETS = (int)(sizeof(arpPresets) / sizeof(arpPresets[0]));

//==============================================================================
// VCPluginDSP Implementation
//==============================================================================

VCPluginDSP::VCPluginDSP()
{
    srand((unsigned int)time(nullptr));
}

VCPluginDSP::~VCPluginDSP()
{
}

void VCPluginDSP::prepare(double sampleRate, int blockSize)
{
    mSampleRate = sampleRate;
    mBlockSize = blockSize;

    mSynth.prepare(sampleRate);
    mSequencer.prepare(sampleRate, mParams.bpm);
    mPattern.setMode(mParams.mode);
    mPattern.setOctaveRange(mParams.octaveRange);
    mPattern.setTranspose(mParams.transpose);
    mSequencer.setRate(mParams.rate);
    mSequencer.setGate(mParams.gate);
    mSequencer.setSwing(mParams.swing);
    mSequencer.setHumanize(mParams.humanize);
    mSequencer.setVelocityMode(mParams.velocityMode);
}

void VCPluginDSP::render(float* left, float* right, int numSamples)
{
    if (!mEnabled) {
        std::memset(left, 0, numSamples * sizeof(float));
        std::memset(right, 0, numSamples * sizeof(float));
        return;
    }

    std::vector<VCArpSequencer::ArpEvent> events;

    for (int s = 0; s < numSamples; ++s) {
        mSequencer.processSample(events);

        // Handle events
        for (const auto& ev : events) {
            if (ev.noteOn) {
                mSynth.noteOn(ev.noteNumber, ev.velocity);
            } else {
                mSynth.noteOff();
            }
        }

        // Generate audio
        float sample = mSynth.processSample() * mVolumeLinear;

        // Soft clip
        auto softClip = [](float x) -> float {
            if (x > 1.0f) return 1.0f - 0.5f * std::exp(-(x - 1.0f) * 4.0f);
            if (x < -1.0f) return -1.0f + 0.5f * std::exp((x + 1.0f) * 4.0f);
            return x;
        };

        float out = softClip(sample);
        left[s] = out;
        right[s] = out;
    }
}

#ifndef VC_STANDALONE
void VCPluginDSP::process(juce::dsp::AudioBlock<float>& block)
{
    if (!mEnabled) {
        block.clear();
        return;
    }

    int numSamples = (int)block.getNumSamples();

    if ((int)mInternalBuffer.size() < numSamples * 2)
        mInternalBuffer.resize(numSamples * 2);

    float* leftBuf = mInternalBuffer.data();
    float* rightBuf = mInternalBuffer.data() + numSamples;

    std::memset(leftBuf, 0, numSamples * sizeof(float));
    std::memset(rightBuf, 0, numSamples * sizeof(float));

    render(leftBuf, rightBuf, numSamples);

    for (size_t ch = 0; ch < block.getNumChannels() && ch < 2; ++ch) {
        auto* data = block.getChannelPointer(ch);
        float* src = (ch == 0) ? leftBuf : rightBuf;
        std::memcpy(data, src, numSamples * sizeof(float));
    }
}
#endif

void VCPluginDSP::reset()
{
    mSynth.reset();
    mSequencer.reset();
}

void VCPluginDSP::noteOn(int noteNumber, float velocity)
{
    // Add to held notes
    auto it = std::find(mHeldNotes.begin(), mHeldNotes.end(), noteNumber);
    if (it == mHeldNotes.end()) {
        mHeldNotes.push_back(noteNumber);
    }
    mPattern.setNotes(mHeldNotes);
    mSequencer.setPattern(mPattern);
}

void VCPluginDSP::noteOff(int noteNumber)
{
    auto it = std::find(mHeldNotes.begin(), mHeldNotes.end(), noteNumber);
    if (it != mHeldNotes.end()) {
        mHeldNotes.erase(it);
    }
    mPattern.setNotes(mHeldNotes);
    mSequencer.setPattern(mPattern);
}

void VCPluginDSP::setChordNotes(const std::vector<int>& notes)
{
    mHeldNotes = notes;
    mPattern.setNotes(notes);
    mSequencer.setPattern(mPattern);
}

void VCPluginDSP::renderBars(int bars, std::vector<float>& outLeft, std::vector<float>& outRight)
{
    // Calculate total samples for N bars
    // 1 bar = 4 beats at given BPM
    double beatDuration = 60.0 / (double)mParams.bpm;
    double barDuration = beatDuration * 4.0;
    int totalSamples = (int)(barDuration * bars * mSampleRate);

    outLeft.resize(totalSamples, 0.0f);
    outRight.resize(totalSamples, 0.0f);

    // Reset sequencer
    mSequencer.reset();
    mSynth.reset();

    render(outLeft.data(), outRight.data(), totalSamples);
}

void VCPluginDSP::setParams(const Params& p)
{
    mParams = p;
    mVolumeLinear = dBToLinear(mParams.volumeDB);

    mPattern.setMode(mParams.mode);
    mPattern.setOctaveRange(mParams.octaveRange);
    mPattern.setTranspose(mParams.transpose);
    mPattern.setNotes(mHeldNotes);
    mSequencer.setPattern(mPattern);

    mSequencer.setBPM(mParams.bpm);
    mSequencer.setRate(mParams.rate);
    mSequencer.setGate(mParams.gate);
    mSequencer.setSwing(mParams.swing);
    mSequencer.setHumanize(mParams.humanize);
    mSequencer.setVelocityMode(mParams.velocityMode);

    mSynth.setWaveform(mParams.waveform);
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
// Preset management
//==============================================================================

const char* VCPluginDSP::getPresetName(int index)
{
    if (index < 0 || index >= NUM_ARP_PRESETS) return nullptr;
    return arpPresets[index].name;
}

int VCPluginDSP::getNumPresets()
{
    return NUM_ARP_PRESETS;
}

bool VCPluginDSP::getPreset(int index, Params& p)
{
    if (index < 0 || index >= NUM_ARP_PRESETS) return false;
    p = arpPresets[index].params;
    return true;
}

bool VCPluginDSP::getPresetByName(const char* name, Params& p)
{
    for (int i = 0; i < NUM_ARP_PRESETS; ++i) {
        if (strcmp(name, arpPresets[i].name) == 0) {
            p = arpPresets[i].params;
            return true;
        }
    }
    return false;
}
