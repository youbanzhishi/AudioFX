# VC-Delay

Stereo Delay VST3 Plugin with Feedback. Based on JUCE 8 + CMake.

## Features

- Stereo delay with circular buffer
- Adjustable delay time (10-2000ms)
- Feedback control (0-90%)
- Dry/Wet mix
- Bypass switch

## Parameters

| Parameter | Range | Default | Description |
|-----------|-------|--------|-------------|
| Delay Time | 10 ~ 2000 ms | 250 ms | Delay time |
| Feedback | 0 ~ 90% | 30% | Feedback amount |
| Mix | 0 ~ 100% | 50% | Dry/Wet mix |
| Bypass | 0 / 1 | 0 | Bypass switch |

## Presets

| Preset | Time | Feedback | Mix |
|--------|------|----------|-----|
| bypass | 250 ms | 30% | 50% |
| slapback | 80 ms | 10% | 40% |
| short | 150 ms | 25% | 50% |
| medium | 350 ms | 40% | 45% |
| long | 700 ms | 55% | 40% |

## Build

```bash
mkdir build && cd build
cmake .. -DJUCE_PATH=/opt/JUCE -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

## CLI Usage

```bash
./build/CLI/VC-Delay-CLI input.wav output.wav --preset medium
./build/CLI/VC-Delay-CLI input.wav output.wav --time 350 --feedback 40
```

## License

MIT License
