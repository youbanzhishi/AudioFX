# VC-Gain

A simple gain/volume control audio plugin with dry/wet mix support.

## Features

- **Gain Control**: -24dB to +24dB gain range
- **Dry/Wet Mix**: 0-100% blend between dry and processed signals
- **Bypass**: Toggle processing on/off
- **Presets**: Multiple factory presets for common gain settings

## Parameters

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| gainDB | -24 to +24 | 0.0 | Gain in decibels |
| mix | 0-100% | 100 | Dry/wet mix percentage |
| bypass | on/off | off | Bypass processing |

## Presets

- **bypass**: Unity gain, processing disabled
- **unity**: 0dB gain, full wet
- **boost-6db**: +6dB gain boost
- **boost-12db**: +12dB gain boost  
- **cut-6db**: -6dB cut
- **half-mix**: 0dB with 50% mix

## DSP Algorithm

Simple gain with dry/wet mixing:

```
output = dry × input + wet × (input × gain)
```

Where:
- `gain` = linear gain from dB value
- `dry` = 1 - mix/100
- `wet` = mix/100

## Building

### Prerequisites

- CMake 3.15+
- JUCE 6/7/8 (for JUCE CLI and VST3)
- C++17 compiler

### Build Commands

```bash
mkdir -p build && cd build
cmake .. -DJUCE_PATH=/path/to/JUCE -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

### Build Targets

- `VC-Gain-CLI-Standalone`: Standalone CLI (no JUCE dependency, uses dr_wav)
- `VC-Gain-CLI`: JUCE-based CLI
- `VC-Gain-VST3`: VST3 plugin

## CLI Usage

```bash
# Using preset
./VC-Gain-CLI-Standalone input.wav output.wav --preset boost-6db

# Direct parameters
./VC-Gain-CLI-Standalone input.wav output.wav --gain 6.0 --mix 75

# Bypass mode
./VC-Gain-CLI-Standalone input.wav output.wav --preset bypass
```

## License

MIT
