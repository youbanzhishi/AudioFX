# VC-Synth — VCMix Subtractive Synthesizer (VSTi)

The first virtual instrument plugin in the VCMix ecosystem. A lightweight subtractive synthesizer with polyblep anti-aliased oscillators, Moog-style ladder filter, ADSR envelope, and built-in effects.

## Signal Flow

```
Oscillator(s) → Moog Ladder Filter → ADSR Amplifier → Reverb + Delay → Output
```

## Features

- **5 Oscillator Types**: sine, saw, square, triangle, noise
- **Unison**: 1-7 voices with detune spread and stereo panning
- **Polyblep Anti-Aliasing**: Band-limited saw and square waves without aliasing artifacts
- **Moog Ladder Filter**: Huovilainen model with LP/BP/HP modes
- **ADSR Envelope**: Exponential curves with configurable attack, decay, sustain, release
- **Built-in Effects**: Schroeder reverb + simple delay
- **10 Presets**: bypass, init, pad, lead, bass, pluck, strings, organ, synth-brass, supersaw
- **Voice Management**: 16-voice polyphony with round-robin allocation

## CLI Usage

```bash
# Render a C4 sine wave
./VC-Synth-CLI-Standalone output.wav --note 60 --duration 2.0 --osc sine

# Render a saw lead
./VC-Synth-CLI-Standalone lead.wav --preset lead --note 60 --duration 2.0

# Render a supersaw chord
./VC-Synth-CLI-Standalone chord.wav --preset supersaw --note 48,60,64 --duration 4.0

# Render a chromatic scale
./VC-Synth-CLI-Standalone scale.wav --scale C4:C5 --step 1 --preset pad --duration 0.5
```

## Parameters

| Parameter | Flag | Range | Default |
|-----------|------|-------|---------|
| Oscillator Type | `--osc` | sine/saw/square/triangle/noise | saw |
| Unison | `--unison` | 1-7 | 1 |
| Detune | `--detune` | cents | 10 |
| Filter Cutoff | `--cutoff` | 20-20000 Hz | 8000 |
| Filter Resonance | `--resonance` | 0-1 | 0.5 |
| Filter Type | `--filter-type` | lp/bp/hp | lp |
| Attack | `--attack` | ms | 10 |
| Decay | `--decay` | ms | 100 |
| Sustain | `--sustain` | 0-1 | 0.7 |
| Release | `--release` | ms | 200 |
| Reverb Mix | `--reverb-mix` | 0-1 | 0 |
| Delay Mix | `--delay-mix` | 0-1 | 0 |
| Delay Time | `--delay-time` | ms | 375 |
| Volume | `--volume` | dB | 0 |

## Presets

| Preset | Description |
|--------|-------------|
| bypass | Silent output |
| init | Basic saw with moderate settings |
| pad | Slow attack, long release, 3 unison, filtered |
| lead | Snappy, bright saw with 2 unison |
| bass | Low cutoff, short decay, 2 unison |
| pluck | Fast attack, short decay, no sustain |
| strings | Slow attack, high sustain, 3 unison triangle |
| organ | Instant attack, full sustain, sine |
| synth-brass | Medium attack, resonant filter, 2 unison |
| supersaw | 7 unison voices, wide detune |

## Build

### Standalone CLI (no JUCE dependency)
```bash
g++ -std=c++17 -DVC_STANDALONE -O2 -I../Libs/dr_wav \
    Source/CLI_Standalone/main.cpp Source/DSP/VCPluginDSP.cpp \
    -o VC-Synth-CLI-Standalone -lm
```

### VST3 Plugin (requires JUCE)
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
```

## VSTi Notes

As a virtual instrument (VSTi), VC-Synth differs from effect plugins:
- Accepts MIDI input (`NEEDS_MIDI_INPUT=TRUE`)
- No audio input (`IS_SYNTH=TRUE`)
- Produces audio output from synthesized sound
