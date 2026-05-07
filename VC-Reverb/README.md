# VC-Reverb

**Schroeder Algorithmic Reverb** - A classic reverb implementation based on the Schroeder reverberator algorithm, featuring parallel Comb filters and series Allpass filters for natural room simulation.

## Overview

VC-Reverb implements the classic Schroeder reverberator algorithm with:

- **4 Parallel Comb Filters**: Create room reflections with adjustable size
- **2 Series Allpass Filters**: Add diffusion and coloration
- **Pre-Delay**: Control early reflection timing
- **High-Frequency Damping**: Simulate air absorption
- **Stereo Processing**: Independent L/R channel processing with slight offset for width

## Algorithm Details

### Signal Flow
```
Input → Pre-Delay → [Comb1] ─┐
                   [Comb2] ─┤
                   [Comb3] ─┼→ Sum → Allpass1 → Allpass2 → Wet Output
                   [Comb4] ─┘

Final = Dry × (1 - mix) + Wet × mix
```

### Comb Filter Structure
Each Comb filter includes a lowpass filter in its feedback path to simulate high-frequency damping in real rooms.

### Stereo Implementation
Left and right channels use slightly different delay line lengths to create stereo spread:
- Left Comb delays: 1557, 1617, 1491, 1422 samples
- Right Comb delays: 1568, 1630, 1498, 1431 samples

## Parameters

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| Room Size | 0-100% | 50% | Scales delay line lengths |
| Decay | 0-100% | 50% | Feedback amount (longer tail) |
| Damping | 0-100% | 50% | High-frequency absorption |
| Pre-Delay | 0-100 ms | 20 ms | Initial reflection delay |
| Mix | 0-100% | 30% | Dry/Wet balance |
| Bypass | On/Off | Off | Enable/disable processing |

## Presets

| Preset | Room | Decay | Damping | Pre-Delay | Mix |
|--------|------|-------|---------|-----------|-----|
| bypass | 50 | 50 | 50 | 20 | 30 |
| small-room | 30 | 40 | 60 | 10 | 25 |
| large-hall | 80 | 70 | 40 | 30 | 35 |
| plate | 60 | 55 | 30 | 5 | 40 |
| ambient | 90 | 80 | 70 | 50 | 20 |

## Build

### Prerequisites
- CMake 3.22+
- JUCE 8.0+ (for VST3 and JUCE CLI)
- C++17 compatible compiler

### Build All Targets
```bash
mkdir build && cd build
cmake .. -DJUCE_PATH=/opt/JUCE -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

### Build Specific Targets

**Standalone CLI (recommended, no JUCE dependency):**
```bash
cmake --build . --target VC-Reverb-CLI-Standalone
```

**VST3 Plugin:**
```bash
cmake --build . --target VC-Reverb
```

## Usage

### CLI

#### Standalone CLI (No JUCE)
```bash
./build/CLI_Standalone/VC-Reverb-CLI-Standalone input.wav output.wav --preset large-hall
./build/CLI_Standalone/VC-Reverb-CLI-Standalone input.wav output.wav --room 80 --decay 70 --mix 35
```

#### JUCE CLI
```bash
./build/CLI/VC-Reverb-CLI input.wav output.wav --room 75 --damping 40
```

### VST3 Plugin

Copy the built VST3 bundle to your VST3 directory:
- macOS: `~/Library/Audio/Plug-Ins/VST3/`
- Linux: `~/.vst3/`
- Windows: `C:\Program Files\VST3\`

## Architecture

VC-Reverb follows the three-layer architecture:

1. **DSP Core** (`Source/DSP/`): Platform-agnostic algorithm implementation
2. **CLI Layer** (`Source/CLI/`, `Source/CLI_Standalone/`): Command-line interface
3. **VST3 Layer** (`Source/PluginProcessor.cpp`, `Source/PluginEditor.cpp`): JUCE integration

## Related Plugins

- [VC-EQ](https://github.com/youbanzhishi/AudioFX) - 5-band parametric equalizer
- [VC-Comp](https://github.com/youbanzhishi/AudioFX) - Sidechain compressor
- [VC-Smooth](https://github.com/youbanzhishi/AudioFX) - Spectral resonance suppressor
- [VC-Gain](https://github.com/youbanzhishi/AudioFX) - Gain utility
- [VC-Saturator](https://github.com/youbanzhishi/AudioFX) - Waveshaper saturation
- [VC-DeEsser](https://github.com/youbanzhishi/AudioFX) - De-esser

## License

MIT License

## References

- Schroeder, M. R. (1962). Natural-sounding artificial reverberation. Journal of the Audio Engineering Society, 10(3), 219-223.
- Schroeder, M. R., & Logan, B. F. (1961). Colorless artificial reverberation. IRE Transactions on Audio, AU-9(6), 209-214.
