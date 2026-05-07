# VC-Distortion

5-mode distortion/saturation effect for VocalChain.

## Distortion Types
| Type | Algorithm | Character |
|------|-----------|-----------|
| 0: Tube | `tanh(drive * x)` | Warm, musical soft clipping |
| 1: Tape | Soft clip + one-pole LP | Magnetic tape saturation |
| 2: Transistor | `clamp(x, -thresh, thresh)` | Hard clipping, aggressive |
| 3: Fuzz | `sign(x) * pow(abs(x), 1/drive)` | Extreme distortion |
| 4: BitCrush | Quantize + downsample | Lo-fi digital degradation |

## Parameters
- **type** (0-4): Distortion type
- **drive** (0-100): Drive amount
- **mix** (0-100): Dry/wet mix %
- **tone** (0-100): Tone filter (0=dark, 100=bright)
- **makeup** (-30~+30 dB): Gain compensation

## CLI Usage
```
./VC-Distortion-CLI-Standalone input.wav output.wav --type 0 --drive 70 --mix 80 --tone 50 --makeup 0
```

## Presets
- `tube-light`: Gentle tube warmth
- `tube-drive`: Heavy tube overdrive
- `tape-saturate`: Tape saturation
- `transistor`: Hard transistor clipping
- `fuzz-heavy`: Extreme fuzz with -6dB makeup
- `bitcrush-lofi`: Lo-fi bitcrush
