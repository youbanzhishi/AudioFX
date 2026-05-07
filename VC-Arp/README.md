# VC-Arp — Arpeggiator Plugin

Part of the VCMix audio plugin suite. Receives MIDI note input and generates arpeggio patterns automatically.

## Features

### 7 Arpeggio Modes
- **Up**: Notes played low to high
- **Down**: Notes played high to low
- **Up-Down**: Ascending then descending (no duplicate peak notes)
- **Down-Up**: Descending then ascending (no duplicate valley notes)
- **Random**: Random note order
- **As-Played**: Notes in input order
- **Chord**: All notes played simultaneously

### Parameters
| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| `--mode` | up/down/up-down/down-up/random/as-played/chord | up | Arpeggio pattern |
| `--rate` | 1/1, 1/2, 1/4, 1/8, 1/16, 1/32 | 1/8 | Note subdivision |
| `--octave-range` | 1-4 | 1 | Octave expansion range |
| `--gate` | 1-200% | 100% | Note length as % of step |
| `--swing` | 0-100% | 0% | Swing feel (even steps longer) |
| `--velocity-mode` | original/ascending/descending/random | original | Velocity pattern |
| `--bpm` | 20-300 | 120 | Tempo |
| `--transpose` | semitones | 0 | Pitch transpose |
| `--humanize` | 0-100% | 0% | Timing/velocity randomization |

### 9 Presets
- **bypass**: Disabled
- **up-8th**: Classic upward eighth notes
- **down-8th**: Classic downward eighth notes
- **up-down-16th**: Bouncing 16th notes
- **trance-gate**: Short gated 16ths with ascending velocity
- **random-bells**: Random order, sine, short gate
- **chord-pad**: All notes together, long sustain
- **octave-run**: 2-octave expansion, fast 16ths, saw
- **ping-pong**: Up-down with swing

## CLI Standalone

```bash
# Build
cd VC-Arp
g++ -std=c++17 -DVC_STANDALONE -O2 -I../Libs/dr_wav \
    Source/CLI_Standalone/main.cpp Source/DSP/VCPluginDSP.cpp \
    -o VC-Arp-CLI-Standalone -lm

# Basic usage
./VC-Arp-CLI-Standalone output.wav --notes 60,64,67 --mode up --rate 1/8 --bpm 120 --bars 4

# Note names supported
./VC-Arp-CLI-Standalone output.wav --notes C4,E4,G4 --mode up-down --rate 1/16

# Presets
./VC-Arp-CLI-Standalone output.wav --preset trance-gate --notes 60,64,67,72 --bpm 128 --bars 8

# 2-octave run with saw wave
./VC-Arp-CLI-Standalone output.wav --notes 48,60,72 --mode up --rate 1/16 --octave-range 2 --waveform saw
```

## Architecture

```
VCArpPattern    — Pattern generator (notes → ordered sequence with octave expansion)
VCArpSequencer  — Timing engine (triggers notes at rate, applies gate/swing/humanize)
VCArpSynth      — Simple synth for CLI mode (sine/saw/square with decay envelope)
VCPluginDSP     — Main class (coordinates pattern + sequencer + synth)
```

## Electronic Music Trinity

VC-Arp completes the VCMix electronic music trio:
1. **VC-Synth** — Subtractive synthesizer
2. **VC-Drum** — Drum synthesizer + pattern sequencer
3. **VC-Arp** — Arpeggiator

## VST3 (Future)

In VST3 mode, VC-Arp will output MIDI notes instead of audio, acting as a MIDI effect that can drive other instruments.
