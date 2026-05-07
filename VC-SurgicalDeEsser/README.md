# VC-SurgicalDeEsser

Surgical de-esser with two-pass detection for VocalChain.
Unlike traditional de-essers that apply continuous compression, this plugin
detects individual sibilance regions and applies targeted gain reduction
with crossfade to avoid artifacts.

## Algorithm
1. Bandpass filter (5-9kHz default) extracts sibilance frequencies
2. RMS envelope tracking with configurable threshold
3. Regions above threshold marked as sibilance
4. Per-region: `attenuation = min((envelope - threshold) * 0.8, reduction)`
5. Crossfade in/out to prevent clicks

## Parameters
- **threshold** (-60~0 dBFS): Detection threshold (default -30)
- **reduction** (0~20 dB): Maximum attenuation (default 6)
- **min-duration** (5~100 ms): Minimum sibilance duration (default 20)
- **fade** (0.5~10 ms): Crossfade time (default 5)
- **freq-low** (2000~8000 Hz): Detection band low (default 5000)
- **freq-high** (5000~14000 Hz): Detection band high (default 9000)
- **mode** (0/1): gain(0, recommended) or dynEQ(1)

## CLI Usage
```
./VC-SurgicalDeEsser-CLI-Standalone input.wav output.wav --threshold -30 --reduction 6 --report
./VC-SurgicalDeEsser-CLI-Standalone input.wav output.wav --preset aggressive --export-sibilances
```

## Special Features
- `--report`: Print detection report with per-region details
- `--report-file <f>`: Save report to file
- `--export-sibilances`: Export each sibilance clip as separate WAV

## Presets
- `gentle`: Light de-essing (-25dB threshold, 3dB reduction)
- `moderate`: Standard de-essing (-30dB, 6dB)
- `aggressive`: Heavy de-essing (-35dB, 12dB, wider band)
- `broadcast`: Broadcast-safe (-28dB, 8dB, tight fade)
- `high-freq`: Target only high frequencies (7-12kHz)
- `dyneq`: Dynamic EQ mode (vs default gain mode)
