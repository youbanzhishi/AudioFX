# Changelog

## [1.6.0] - 2026-05-08

### Added
- VC-Tune Gen2: PSOLA+LPC共振峰保留+formant-preserve/shift+vibrato-preserve+transition-smooth
- VC-MultiBand: 4频段LR4分频路由器+每段增益/压缩+solo/mute
- VC-Harmonizer: 智能和声生成(YIN+K-S+4声部+LPC共振峰+gain/pan)
- VC-NoiseProfile: 谱减降噪(radix-2 FFT+噪声指纹+谱减法+噪声门)
- CI gen2-verify job扩展到8个Gen2插件

### Fixed
- VC-Comp Gen2: envelope加sqrt()转线性+每sample调用setReleaseTime
- VC-EQ IIR: 6项安全钳位(freq/Q/gain/a0/极点/denormal)

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

## v1.3.0 — VC-Comp CLI Multiband + VC-PitchShift Fixes (2025-05-07)

### VC-Comp CLI Multiband Parameters
- `--multiband 0|1` — Enable 4-band multiband mode
- `--band-threshold dB4` — Per-band threshold (comma-separated)
- `--band-ratio r4` — Per-band ratio (comma-separated)
- `--band-makeup dB4` — Per-band makeup gain (comma-separated)
- `--solo-band 0-4` — Solo a specific band
- `--xover Hz3` — Crossover frequencies (default: 120,1000,8000)
- New preset: `multiband-master`

### VC-PitchShift Fixes
- Inline pitch ratio calculation (removed undefined `updatePitchRatio()` call)
- Fixed duplicate line in `setParams()`

## v1.4.0 — VC-Tune PSOLA + VC-MultiBand (2025-05-08)

### VC-Tune Gen2: Complete PSOLA + LPC Formant Preservation
- Full PSOLA engine: analysis markers → pitch modification → OLA synthesis
- LPC formant extractor (12th-order Linear Predictive Coding)
- Formant preservation: `--formant-preserve 0-100` (default: 100 = full)
- Formant shifting: `--formant-shift -12 to +12` semitones
- Vibrato preservation: `--vibrato-preserve 0-100`
- Transition smoothing: `--transition-smooth 0-100`
- New presets: chipmunk, deep, vibrato

### VC-MultiBand: 4-band LR4 Crossover
- 4-band Linkwitz-Riley crossover (24dB/oct, phase-coherent)
- Adjustable crossover: --xover1/2/3 (default: 120/1k/8kHz)
- Per-band gain: --band-gain
- Per-band compression: --band-threshold + --band-ratio
- Solo/Mute per band
- Perfect reconstruction

### VC-EQ IIR Stability Fix
- Frequency clamp [20Hz, Nyquist-1Hz]
- Q clamp [0.1, 100.0]
- Gain clamp [-60, +60] dB
- a0 zero-division guard
- Pole stability check (|a2| >= 1 → passthrough)
- Denormal prevention

### VC-PitchShift Fix
- Correct Phase Vocoder: time-stretch then resample algorithm
- Fix frequency smearing artifacts

## [1.5.0] - 2026-05-08

### Added
- VC-Harmonizer: Intelligent harmony generator (YIN+K-S+4 voices+LPC formant+gain/pan)
- VC-NoiseProfile: Spectral subtraction noise reduction (radix-2 FFT+noise fingerprint+spectral subtraction+noise gate)
- VC-MultiBand CLI presets
- AudioFX CI: VC-MultiBand+VC-Harmonizer+VC-Tune Gen2 in gen2-verify

### Fixed
- VC-Comp Gen2: envelope sqrt()+setReleaseTime per sample
- VC-EQ: IIR coefficient stability (6 safety clamps)
- VC-PitchShift: Phase Vocoder time-stretch+resample fix
