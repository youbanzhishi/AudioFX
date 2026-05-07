# VC-Limiter

Peak Limiter VST3 Plugin with Ceiling Control. Based on JUCE 8 + CMake.

## Features

- Peak limiter with envelope follower
- Output ceiling control
- Fast attack, adjustable release
- Dry/Wet mix
- Bypass switch

## Parameters

| Parameter | Range | Default | Description |
|-----------|-------|--------|-------------|
| Threshold | -24 ~ 0 dB | -6 dB | Detection threshold |
| Ceiling | -6 ~ 0 dB | -0.3 dB | Output level ceiling |
| Release | 10 ~ 500 ms | 50 ms | Release time |
| Mix | 0 ~ 100% | 100% | Dry/Wet mix |
| Bypass | 0 / 1 | 0 | Bypass switch |

## Presets

| Preset | Threshold | Ceiling | Release | Mix |
|--------|-----------|---------|---------|-----|
| bypass | -6 dB | -0.3 dB | 50 ms | 100% |
| gentle | -3 dB | -0.3 dB | 100 ms | 100% |
| brick-wall | -1 dB | -0.1 dB | 30 ms | 100% |
| mastering | -6 dB | -0.3 dB | 50 ms | 100% |

## Build

```bash
mkdir build && cd build
cmake .. -DJUCE_PATH=/opt/JUCE -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

## CLI Usage

```bash
./build/CLI/VC-Limiter-CLI input.wav output.wav --preset brick-wall
./build/CLI/VC-Limiter-CLI input.wav output.wav --threshold -3 --release 80
```

## License

MIT License
