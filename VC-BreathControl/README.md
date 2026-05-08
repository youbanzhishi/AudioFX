# VC-BreathControl

VocalChain Plugin Series #26 — Automatic Breath Detection and Gain Control

## Overview

VC-BreathControl automatically detects breath regions in vocal recordings and applies controlled gain adjustment (reduction or enhancement). Unlike a noise gate (hard on/off) or a de-esser (targets sibilance), this plugin specifically targets breath sounds using dual-criteria detection: short-time energy + spectral flatness estimation.

## Features

- **Dual-criteria detection**: Energy (bandpass 200Hz-4kHz) + spectral flatness (bandpass/full-band energy ratio)
- **State machine with hysteresis**: 3dB gap between enter/exit thresholds prevents chattering
- **Minimum duration filtering**: Ignores short false positives (<50ms default)
- **Two smoothing modes**: 
  - Auto (attack/release envelope) — natural, simulates hand-drawn volume automation
  - Step + micro-fade — precise, avoids click artifacts in some scenarios
- **Bidirectional gain control**: -18dB (nearly remove) to +12dB (enhance breaths)
- **Lookahead buffer**: Up to 10ms lookahead for early response to breath onset
- **Two-pass CLI processing**: First detect all breaths, then process with perfect boundaries
- **Detection report**: Detailed output of every detected breath region

## Parameters

| Parameter | Range | Default | Unit | Description |
|-----------|-------|---------|------|-------------|
| threshold | -60 ~ -10 | -40 | dBFS | Energy threshold for breath detection |
| reduction | -18 ~ +12 | -8 | dB | Gain adjustment for breath regions |
| attack | 1 ~ 100 | 10 | ms | Gain decrease time (auto_smooth=true) |
| release | 10 ~ 500 | 50 | ms | Gain recovery time (auto_smooth=true) |
| auto_smooth | — | true | — | Auto smoothing mode |
| fade_in | 0 ~ 20 | 2 | ms | Micro-fade at breath start (auto_smooth=false) |
| fade_out | 0 ~ 20 | 5 | ms | Micro-fade at breath end (auto_smooth=false) |
| min_duration | 10 ~ 500 | 50 | ms | Minimum breath duration |
| sensitivity | 0.1 ~ 1.0 | 0.5 | — | Spectral flatness weight |
| lookahead | 0 ~ 10 | 5 | ms | Lookahead buffer time |

## Presets

| Preset | Threshold | Reduction | Description |
|--------|-----------|-----------|-------------|
| gentle | -35 | -4 | Subtle reduction |
| moderate | -40 | -8 | Standard processing |
| aggressive | -45 | -14 | Heavy reduction |
| broadcast | -38 | -10 | Broadcast-grade |
| solo-vocal | -40 | -6 | Preserves natural feel |
| enhance | -40 | +6 | Enhance breaths |
| detect-only | -40 | 0 | Detection only (no processing) |
| step-fade | -40 | -8 | Step + micro-fade mode |

## CLI Usage

```bash
# Compile
g++ -std=c++17 -DVC_STANDALONE -O2 \
    -I/tmp/AudioFX/Libs/dr_wav \
    Source/CLI_Standalone/main.cpp \
    Source/DSP/VCPluginDSP.cpp \
    -o VC-BreathControl-CLI-Standalone -lm

# Basic processing
./VC-BreathControl-CLI-Standalone input.wav output.wav --threshold -40 --reduction -8

# With detection report
./VC-BreathControl-CLI-Standalone input.wav output.wav --report

# Enhance breaths
./VC-BreathControl-CLI-Standalone input.wav output.wav --reduction 6

# Detect only (no processing)
./VC-BreathControl-CLI-Standalone input.wav output.wav --reduction 0 --report
```

## Algorithm

1. Mono downmix → Bandpass filter (200Hz-4kHz) → Envelope follower
2. Full-band envelope → Spectral flatness estimation (bandpass ratio)
3. Dual-criteria: energy < threshold AND spectral flatness > sfThreshold
4. State machine with hysteresis (3dB gap)
5. Minimum duration filtering
6. Lookahead buffer for early response
7. Gain curve: auto_smooth or step+fade
8. Apply gain to delayed audio signal
