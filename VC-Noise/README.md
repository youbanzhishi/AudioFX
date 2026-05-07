# VC-Noise

Signal/noise generator for VocalChain. Generates audio without input files.

## Signal Types
| Type | Algorithm | Character |
|------|-----------|-----------|
| 0: White | Uniform random [-1,1] | Full spectrum noise |
| 1: Pink | Voss-McCartney | -3dB/octave, natural sounding |
| 2: Brown | White noise integration | -6dB/octave, deep rumble |
| 3: Sine | `sin(2π*f*phase)` | Pure tone |
| 4: Sweep | Linear/log frequency sweep | Calibration/test signal |
| 5: Impulse | Single/periodic impulse | IR measurement |

## Parameters
- **type** (0-5): Signal type
- **freq** (20-20000 Hz): Sine frequency / sweep start
- **end-freq** (20-20000 Hz): Sweep end frequency
- **sweep-dur** (1-60 s): Sweep duration
- **sweep-log** (0/1): Log(1) or linear(0) sweep
- **volume** (-60~0 dBFS): Output volume
- **channel** (0-3): Stereo(0), Left(1), Right(2), Anti-phase(3)
- **pulse-period** (0-10 s): Impulse period (0=single)
- **duration** (s): Output duration (default 10s)

## CLI Usage
```
./VC-Noise-CLI-Standalone output.wav --type 3 --freq 440 --duration 5 --volume -6
```

## Presets
- `white-0db`, `pink-6db`, `brown-12db`, `sine-1k`, `sine-440`, `sweep-log`, `sweep-linear`, `impulse`, `impulse-1s`
