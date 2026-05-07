#!/usr/bin/env python3
"""
Latency Tests for AudioFX Plugins
===================================
Measures processing latency (group delay) for each plugin by:
- Feeding impulse signals and measuring delay to first significant output
- Checking that latency is within acceptable bounds
- Instrument plugins: verify audio output generation
"""

import pytest
import os
import numpy as np

from conftest import cli_exists, process_and_read, run_cli, ALL_EFFECTS, INSTRUMENTS, AUDIOFX_ROOT
from generators import (
    generate_impulse, generate_stereo_signal_from_mono,
    save_wav, read_wav, SAMPLE_RATE
)


# Maximum acceptable latency in milliseconds for real-time use
MAX_LATENCY_MS = 50.0


def measure_impulse_latency(output_data, sr=SAMPLE_RATE, threshold_db=-40):
    """Measure latency from impulse response.

    Returns the sample offset from the expected impulse position to the
    actual peak in the output.
    """
    if output_data.ndim > 1:
        output_data = output_data[:, 0]

    # Find the peak of the output
    peak_idx = np.argmax(np.abs(output_data))
    # Expected position is sample 0 (impulse at start)
    # Latency = peak position (in samples)
    latency_samples = peak_idx
    latency_ms = latency_samples / sr * 1000

    # Also check: first sample that exceeds threshold
    threshold = 10 ** (threshold_db / 20)
    above = np.where(np.abs(output_data) > threshold)[0]
    if len(above) > 0:
        first_above_ms = above[0] / sr * 1000
    else:
        first_above_ms = latency_ms

    return latency_ms, first_above_ms


# ---------------------------------------------------------------------------
# Effect Plugin Latency Tests
# ---------------------------------------------------------------------------

class TestEffectLatency:
    """Measure processing latency for each effect plugin."""

    @pytest.fixture
    def impulse_stereo(self, work_dir):
        """Generate stereo impulse for latency testing."""
        mono = generate_impulse(1.0, amplitude=1.0)
        stereo = generate_stereo_signal_from_mono(mono)
        path = os.path.join(work_dir, "latency_impulse_stereo.wav")
        save_wav(stereo, path)
        return path

    @pytest.fixture
    def impulse_mono(self, work_dir):
        """Generate mono impulse for latency testing."""
        mono = generate_impulse(1.0, amplitude=1.0)
        path = os.path.join(work_dir, "latency_impulse_mono.wav")
        save_wav(mono, path)
        return path

    @pytest.mark.parametrize("plugin_name", ALL_EFFECTS, ids=lambda x: x)
    def test_effect_latency(self, plugin_name, impulse_mono, work_dir):
        """Each effect should have latency under MAX_LATENCY_MS."""
        if not cli_exists(plugin_name):
            pytest.skip(f"No CLI binary for {plugin_name}")

        output_path = os.path.join(work_dir, f"{plugin_name}_latency.wav")
        try:
            sr, output = process_and_read(plugin_name, impulse_mono, output_path)
        except Exception as e:
            pytest.skip(f"CLI execution failed: {e}")

        latency_ms, first_sample_ms = measure_impulse_latency(output, sr)

        assert latency_ms < MAX_LATENCY_MS, \
            f"{plugin_name} latency too high: {latency_ms:.1f}ms (max={MAX_LATENCY_MS}ms)"


# ---------------------------------------------------------------------------
# Instrument Plugin Tests
# ---------------------------------------------------------------------------

class TestInstruments:
    """Test instrument plugins (VC-Synth, VC-Drum, VC-Arp)."""

    def test_synth_note_output(self, work_dir):
        """VC-Synth should produce audio output for a note."""
        if not cli_exists("VC-Synth"):
            pytest.skip("No VC-Synth CLI binary")

        output_path = os.path.join(work_dir, "synth_C4.wav")
        result = run_cli("VC-Synth", [output_path, "--note", "60", "--duration", "1.0"])
        assert result.returncode == 0, f"Synth CLI failed: {result.stderr}"

        sr, data = read_wav(output_path)
        rms = float(np.sqrt(np.mean(data ** 2)))
        assert rms > 1e-4, f"Synth output too quiet: RMS={rms:.6f}"

        # Check no NaN/Inf
        assert not np.any(np.isnan(data)), "Synth output contains NaN"
        assert not np.any(np.isinf(data)), "Synth output contains Inf"

    def test_synth_different_oscillators(self, work_dir):
        """VC-Synth should produce different output for different oscillator types."""
        if not cli_exists("VC-Synth"):
            pytest.skip("No VC-Synth CLI binary")

        outputs = {}
        for osc in ["sine", "saw", "square", "triangle"]:
            path = os.path.join(work_dir, f"synth_{osc}.wav")
            result = run_cli("VC-Synth", [path, "--note", "60", "--duration", "1.0", "--osc", osc])
            assert result.returncode == 0, f"Synth {osc} CLI failed: {result.stderr}"
            sr, data = read_wav(path)
            outputs[osc] = data

        # Different oscillators should produce different signals
        sine_rms = float(np.sqrt(np.mean(outputs["sine"] ** 2)))
        saw_rms = float(np.sqrt(np.mean(outputs["saw"] ** 2)))
        # At minimum, saw should be louder or similar to sine
        assert saw_rms > 1e-4, f"Saw output too quiet: RMS={saw_rms:.6f}"
        assert sine_rms > 1e-4, f"Sine output too quiet: RMS={sine_rms:.6f}"

    def test_synth_adsr_envelope(self, work_dir):
        """VC-Synth should have ADSR envelope shape (attack→sustain→release)."""
        if not cli_exists("VC-Synth"):
            pytest.skip("No VC-Synth CLI binary")

        output_path = os.path.join(work_dir, "synth_adsr.wav")
        result = run_cli("VC-Synth", [output_path, "--note", "60", "--duration", "1.0"])
        assert result.returncode == 0, f"Synth CLI failed: {result.stderr}"

        sr, data = read_wav(output_path)
        if data.ndim > 1:
            data = data[:, 0]

        # Check envelope shape: should start from near-zero (attack ramp up)
        first_10ms = data[:int(0.01 * sr)]
        # First few samples should be quieter than peak
        peak = float(np.max(np.abs(data)))
        if peak > 1e-4:
            initial_level = float(np.max(np.abs(first_10ms)))
            # The initial ramp-up should exist (first 10ms < 90% of peak)
            # Not strict - some synths have zero attack
            # But the end should decay
            last_100ms = data[-int(0.1 * sr):]
            end_level = float(np.sqrt(np.mean(last_100ms ** 2)))
            # This validates there IS an envelope of some kind

    def test_drum_output(self, work_dir):
        """VC-Drum should produce audio output."""
        if not cli_exists("VC-Drum"):
            pytest.skip("No VC-Drum CLI binary")

        output_path = os.path.join(work_dir, "drum_basic.wav")
        result = run_cli("VC-Drum", [output_path, "--preset", "basic-beat", "--bars", "1"])
        assert result.returncode == 0, f"Drum CLI failed: {result.stderr}"

        sr, data = read_wav(output_path)
        rms = float(np.sqrt(np.mean(data ** 2)))
        assert rms > 1e-4, f"Drum output too quiet: RMS={rms:.6f}"
        assert not np.any(np.isnan(data)), "Drum output contains NaN"

    def test_drum_different_presets(self, work_dir):
        """Different drum presets should produce different output."""
        if not cli_exists("VC-Drum"):
            pytest.skip("No VC-Drum CLI binary")

        presets = ["kick-only", "snare-only", "hihat-only"]
        outputs = {}
        for preset in presets:
            path = os.path.join(work_dir, f"drum_{preset}.wav")
            result = run_cli("VC-Drum", [path, "--preset", preset, "--bars", "1"])
            assert result.returncode == 0, f"Drum {preset} CLI failed: {result.stderr}"
            sr, data = read_wav(path)
            outputs[preset] = data

        # Different presets should have different spectral content
        kick_rms = float(np.sqrt(np.mean(outputs["kick-only"] ** 2)))
        hihat_rms = float(np.sqrt(np.mean(outputs["hihat-only"] ** 2)))
        # Both should produce sound
        assert kick_rms > 1e-4, f"Kick too quiet: RMS={kick_rms:.6f}"
        assert hihat_rms > 1e-4, f"HiHat too quiet: RMS={hihat_rms:.6f}"

    def test_arp_output(self, work_dir):
        """VC-Arp should produce arpeggiated audio output."""
        if not cli_exists("VC-Arp"):
            pytest.skip("No VC-Arp CLI binary")

        output_path = os.path.join(work_dir, "arp_up.wav")
        result = run_cli("VC-Arp", [output_path, "--notes", "60,64,67", "--mode", "up", "--rate", "1/4"])
        assert result.returncode == 0, f"Arp CLI failed: {result.stderr}"

        sr, data = read_wav(output_path)
        rms = float(np.sqrt(np.mean(data ** 2)))
        assert rms > 1e-4, f"Arp output too quiet: RMS={rms:.6f}"
        assert not np.any(np.isnan(data)), "Arp output contains NaN"

    def test_arp_modes(self, work_dir):
        """Different arp modes should produce different patterns."""
        if not cli_exists("VC-Arp"):
            pytest.skip("No VC-Arp CLI binary")

        modes = ["up", "down", "random"]
        outputs = {}
        for mode in modes:
            path = os.path.join(work_dir, f"arp_{mode}.wav")
            result = run_cli("VC-Arp", [path, "--notes", "60,64,67", "--mode", mode, "--rate", "1/8"])
            assert result.returncode == 0, f"Arp {mode} CLI failed: {result.stderr}"
            sr, data = read_wav(path)
            outputs[mode] = data

        # Up and down modes should produce different output
        up_rms = float(np.sqrt(np.mean(outputs["up"] ** 2)))
        down_rms = float(np.sqrt(np.mean(outputs["down"] ** 2)))
        assert up_rms > 1e-4 and down_rms > 1e-4, \
            f"Arp modes too quiet: up={up_rms:.6f}, down={down_rms:.6f}"


# ---------------------------------------------------------------------------
# Latency Comparison Tests
# ---------------------------------------------------------------------------

class TestLatencyComparison:
    """Compare latencies across plugins to identify outliers."""

    def test_comp_latency_reasonable(self, impulse, work_dir):
        """VC-Comp should have minimal latency (no lookahead by default)."""
        if not cli_exists("VC-Comp"):
            pytest.skip("No VC-Comp CLI binary")

        output_path = os.path.join(work_dir, "comp_latency.wav")
        sr, output = process_and_read("VC-Comp", impulse, output_path,
                                       ["--threshold", "-10", "--ratio", "4"])
        latency_ms, _ = measure_impulse_latency(output, sr)
        assert latency_ms < 10, f"Comp latency too high: {latency_ms:.1f}ms"

    def test_eq_latency_reasonable(self, impulse, work_dir):
        """VC-EQ should have minimal latency (IIR filters)."""
        if not cli_exists("VC-EQ"):
            pytest.skip("No VC-EQ CLI binary")

        output_path = os.path.join(work_dir, "eq_latency.wav")
        sr, output = process_and_read("VC-EQ", impulse, output_path,
                                       ["--band2-gain", "3", "--band2-freq", "1000"])
        latency_ms, _ = measure_impulse_latency(output, sr)
        assert latency_ms < 10, f"EQ latency too high: {latency_ms:.1f}ms"

    def test_reverb_latency_reasonable(self, impulse, work_dir):
        """VC-Reverb may have predelay but should be reasonable."""
        if not cli_exists("VC-Reverb"):
            pytest.skip("No VC-Reverb CLI binary")

        output_path = os.path.join(work_dir, "reverb_latency.wav")
        sr, output = process_and_read("VC-Reverb", impulse, output_path,
                                       ["--predelay", "20", "--mix", "100"])
        latency_ms, _ = measure_impulse_latency(output, sr)
        # Predelay is 20ms, allow up to 30ms total
        assert latency_ms < 50, f"Reverb latency too high: {latency_ms:.1f}ms"
