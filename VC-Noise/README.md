# VC-NoiseProfile (Gen2)

Noise Profile Analysis + Adaptive Spectral Subtraction + Noise Gate for VocalChain.
Gen1 signal generator mode preserved.

## Gen2 New Features

### Noise Profile Learning
- Learn noise fingerprint from the first N ms of audio (default 500ms)
- Outputs 64-band spectral energy profile
- Mel-spaced frequency bands for perceptual accuracy

### Spectral Subtraction Denoising
- Based on learned noise profile, applies spectral subtraction
- `--reduction 0-30 dB`: over-subtraction factor
- `--floor 1-20`: spectral floor to prevent musical noise artifacts

### Noise Gate (Gen1 preserved, enhanced)
- `--threshold -80~0 dB`: gate threshold
- `--attack 0.1-100 ms`: gate attack time
- `--release 1-1000 ms`: gate release time

### Processing Modes
| Mode | Description |
|------|-------------|
| 0: Denoise | Spectral subtraction only |
| 1: Gate | Noise gate only |
| 2: Both | Denoise + Gate (default) |
| 3: Analyze | Print noise profile, pass through audio |

## Signal Types (Gen1 Generate Mode)
| Type | Algorithm | Character |
|------|-----------|-----------|
| 0: White | Uniform random [-1,1] | Full spectrum noise |
| 1: Pink | Voss-McCartney | -3dB/octave, natural sounding |
| 2: Brown | White noise integration | -6dB/octave, deep rumble |
| 3: Sine | `sin(2π*f*phase)` | Pure tone |
| 4: Sweep | Linear/log frequency sweep | Calibration/test signal |
| 5: Impulse | Single/periodic impulse | IR measurement |

## Architecture
```
Input → [Learn Noise Profile (first N ms)] → [Spectral Subtraction] → [Noise Gate] → Output
                         │                          │
                         └→ noise spectrum           └→ clean spectrum
```

## Parameters

### Generate Mode
- **type** (0-5): Signal type
- **freq** (20-20000 Hz): Sine frequency / sweep start
- **end-freq** (20-20000 Hz): Sweep end frequency
- **sweep-dur** (1-60 s): Sweep duration
- **sweep-log** (0/1): Log(1) or linear(0) sweep
- **volume** (-60~0 dBFS): Output volume
- **channel** (0-3): Stereo(0), Left(1), Right(2), Anti-phase(3)
- **pulse-period** (0-10 s): Impulse period (0=single)
- **duration** (s): Output duration (default 10s)
- **sample-rate** (Hz): Sample rate (default 44100)

### Process Mode
- **learn-ms** (100-5000 ms): Learn noise from first N ms (default: 500)
- **reduction** (0-30 dB): Spectral subtraction amount (default: 10)
- **floor** (1-20): Spectral floor ratio (default: 5, prevents musical noise)
- **threshold** (-80~0 dB): Noise gate threshold (default: -40)
- **attack** (0.1-100 ms): Gate attack (default: 5)
- **release** (1-1000 ms): Gate release (default: 50)
- **mode** (0-3): 0=denoise 1=gate 2=both 3=analyze (default: 2)

## CLI Usage

### Generate Mode (Gen1)
```
./VC-Noise-CLI-Standalone output.wav --type 3 --freq 440 --duration 5 --volume -6
```

### Process Mode (Gen2)
```
# Denoise audio with 12dB reduction
./VC-Noise-CLI-Standalone noisy.wav clean.wav --process --learn-ms 500 --reduction 12

# Analyze noise profile
./VC-Noise-CLI-Standalone noisy.wav analysis.wav --process --mode 3 --learn-ms 1000

# Noise gate only
./VC-Noise-CLI-Standalone input.wav output.wav --process --mode 1 --threshold -30

# Denoise + gate
./VC-Noise-CLI-Standalone input.wav output.wav --process --mode 2 --reduction 10 --threshold -30
```

## Presets

### Gen1 Signal Generator
- `white-0db`, `pink-6db`, `brown-12db`, `sine-1k`, `sine-440`, `sweep-log`, `sweep-linear`, `impulse`, `impulse-1s`

### Gen2 Noise Processing
- `denoise-mild`: 6dB reduction, floor 5
- `denoise-moderate`: 12dB reduction, floor 5
- `denoise-aggressive`: 20dB reduction, floor 3
- `gate-only`: Gate at -30dB threshold
- `denoise-gate`: 12dB reduction + gate at -35dB
- `analyze-only`: Profile analysis mode

## DSP Implementation

### FFT
- Radix-2 Cooley-Tukey, 512-point
- Bit-reversal permutation + butterfly stages
- Hann window with 50% overlap (256-sample hop)

### Noise Profile
- 64 frequency bands (mel-spaced)
- Accumulates spectral energy during learning phase
- Finalizes to average per-band energy

### Spectral Subtraction
```
For each FFT frame:
  |S(ω)|² = max(|X(ω)|² - α·|N(ω)|², β·|X(ω)|²)
  where α = 10^(reduction/20), β = floor * 0.01
  gain = sqrt(|S(ω)|² / |X(ω)|²)
  Output = Input × gain (preserves original phase)
```

## Build
```bash
g++ -std=c++17 -DVC_STANDALONE -O2 -I/tmp/AudioFX/Libs/dr_wav \
    Source/CLI_Standalone/main.cpp Source/DSP/VCNoiseDSP.cpp \
    -o VC-Noise-CLI-Standalone -lm
```

## Changelog

### v2.0.0 (Gen2)
- Added noise profile learning from first N ms
- Added spectral subtraction denoising (64-band)
- Added 4 processing modes: denoise/gate/both/analyze
- Added `--learn-ms`, `--reduction`, `--floor`, `--threshold`, `--attack`, `--release`, `--mode` parameters
- Added Gen2 presets (denoise-mild/moderate/aggressive, gate-only, denoise-gate, analyze-only)
- Preserved Gen1 signal generator with full backward compatibility

### v1.0.0 (Gen1)
- Signal/noise generator: White, Pink, Brown, Sine, Sweep, Impulse
- Stereo output with channel modes
- CLI with presets and parameter control
