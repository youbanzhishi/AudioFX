# Changelog

## [1.0.0] — 2025-05-07

### 🎉 VocalChain Gen 1 Complete — 16 Plugins

**Full plugin list:**
1. VC-EQ — Parametric EQ (low cut, high shelf, peak)
2. VC-Comp — Compressor (RMS envelope, soft knee)
3. VC-Smooth — Spectral smoother
4. VC-DeEsser — Sibilance reducer
5. VC-Gain — Gain/trim
6. VC-Saturator — Saturation/harmonic exciter
7. VC-Limiter — Brick-wall limiter (true peak)
8. VC-Delay — Delay (BPM sync, note values)
9. VC-Reverb — Algorithmic reverb (Schroeder/FDN)
10. VC-DynamicEQ — Dynamic EQ (frequency-dependent compression)
11. VC-Distortion — Waveshaping distortion
12. VC-Noise — Noise generator (white/pink/brown)
13. VC-SurgicalDeEsser — Multi-band sibilance reducer
14. VC-Tune — Pitch correction (YIN detection + PSOLA + auto key detection)
15. VC-Gate — Noise gate/expander
16. VC-Chorus — Multi-voice chorus (LFO modulated delay)

### Features
- All 16 plugins have VST3 (JUCE 8) + CLI Standalone builds
- CLI supports batch processing via dr_wav
- BPM note value auto-conversion (1/4, 1/8, 1/8d, 1/8t, 1/16)
- VC-Tune: YIN pitch detection + PSOLA correction + Krumhansl-Schmuckler key detection (24 keys)
- VC-Gate: RMS envelope follower + gate state machine (attack/hold/release)
- VC-Chorus: Multi-voice LFO modulated delay + stereo width + feedback

### Bug Fixes
- VC-EQ IIR coefficient calculation corrected
- VC-Comp envelope detector attack coefficient bug fixed
- VC-Reverb damping range + comb normalization + wet filter fixed
- VC-DeEsser reduction parameter (was using compression ratio logic)
- CLI negative dB parameter parsing (stof instead of stoi)
- DeEsser/DynamicEQ/EQ/Smooth stof crash on non-numeric input
- JUCE 8 compatibility (inputBuses, createWriterFor 6-arg, BusesProperties)

### CI
- GitHub Actions CI matrix for all 16 plugins
- Dynamic DSP filename detection (VCDistortionDSP/VCNoiseDSP/VCSurgicalDeEsserDSP)

## [0.2.0] — 2025-05-07

### Added
- VC-Distortion, VC-Noise, VC-SurgicalDeEsser plugins
- CHANGELOG.md

## [0.1.0] — 2025-05-06

### Added
- Initial 10 VC plugins (EQ, Comp, Smooth, DeEsser, Gain, Saturator, Limiter, Delay, Reverb, DynamicEQ)
- VC-Plugin-Template
- CI/CD with GitHub Actions

## [1.1.0] — 2026-05-07

### Gen 2 Upgrades

**VC-Reverb FDN** — Complete architecture upgrade
- 8-delay-line Feedback Delay Network with Householder matrix
- Per-delay-line 1-pole LP for frequency-dependent decay
- Simplified image-source early reflections
- 6 new presets: small-room, large-hall, plate, ambient, cathedral
- Gen1 Schroeder code backed up as .gen1 files

**VC-Stereo** — New plugin (Gen 2)
- Stereo width control via M/S encoding (0-200%)
- Constant-power pan (cos/sin panning law)
- Mono bass via Linkwitz-Riley crossover (optional)
- 6 presets: bypass/mono/wide/extra-wide/bass-mono/center-pan

**VC-PitchShift** — New plugin (Gen 2)
- Phase Vocoder pitch shifting (STFT → phase accumulation → ISTFT)
- Semitone (-12 to +12) + cents micro-tuning
- Formant preservation (placeholder)
- 8 presets: bypass/up1/down1/up3/down3/octave-up/octave-down/formant-shift

### Bug Fixes
- VC-PitchShift: inline pitch ratio computation (was referencing undefined function)

## [1.2.0] — 2026-05-07

### Gen 2 P0 Complete

**VC-Comp Multiband** — 4-band multiband compression upgrade
- LR4 (4th-order Linkwitz-Riley, 24dB/oct) crossover
- 3 crossover points: 120Hz / 1kHz / 8kHz → 4 frequency bands
- 4 independent compressors with per-band threshold/ratio/makeup
- Single-band mode fully backward compatible
- Gen1 code backed up as .gen1 files

All Gen 2 P0 upgrades complete: FDN Reverb + Multiband Comp + Stereo + PitchShift
