# VC-NoiseProfile

Noise Profile Analysis + Adaptive Spectral Subtraction + Noise Gate

Part of the **VocalChain** audio plugin suite.

## Features

- **Noise Profile Learning**: Analyzes the first N milliseconds of audio to build a 64-band spectral noise profile
- **Spectral Subtraction**: Reduces noise using adaptive spectral subtraction with configurable reduction amount and floor
- **Noise Gate**: Configurable gate with threshold, attack, and release
- **Multiple Modes**: Denoise only, Gate only, Both, or Analyze mode

## Parameters

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| Process Mode | Denoise/Gate/Both/Analyze | Both | Processing mode |
| Reduction | 0–30 dB | 10 | Spectral subtraction amount |
| Noise Floor | 1–20 | 5 | Spectral floor ratio (prevents musical noise) |
| Attack | 0.1–100 ms | 5 | Gate attack time |
| Release | 1–1000 ms | 50 | Gate release time |
| FFT Size | 256/512/1024/2048 | 512 | FFT analysis window size |
| Noise Learn Time | 100–5000 ms | 500 | Duration to learn noise profile |
| Gate Threshold | -80–0 dB | -40 | Noise gate threshold |
| Gate Depth | 0–100% | 100 | Gate attenuation amount |

## Build

```bash
mkdir build && cd build
cmake .. -G Ninja -DJUCE_PATH=/opt/JUCE -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

## CLI Usage

### JUCE CLI
```bash
./VC-NoiseProfile-CLI input.wav output.wav --mode 2 --reduction 12 --learn-ms 500
```

### Standalone CLI (zero dependency)
```bash
./VC-NoiseProfile-CLI-Standalone input.wav output.wav --process --reduction 12 --learn-ms 500
```

## Architecture

```
Input → STFT Analysis → Noise Profile Learning
                        ↓
         Spectral Subtraction → IFFT → Noise Gate → Output
```

## License

Part of VocalChain - All rights reserved.
