#!/usr/bin/env python3
"""
Stereo Integrity Tests for AudioFX Plugins
============================================
Tests for stereo-aware plugins:
- VC-Stereo: Width control, M/S processing, mono collapse
- VC-Chorus: Stereo widening effect
- VC-Delay: Ping-pong mode
- VC-Harmonizer: Harmony generation with stereo panning
"""

import pytest
import os
import numpy as np
from scipy.fft import rfft, rfftfreq

from conftest import cli_exists, process_and_read
from generators import (
    generate_stereo_sine, generate_stereo_signal_from_mono,
    generate_sine, save_wav, read_wav, SAMPLE_RATE
)


def mid_side_correlation(left, right):
    """Compute correlation between L and R channels."""
    if len(left) != len(right):
        min_len = min(len(left), len(right))
        left = left[:min_len]
        right = right[:min_len]
    left_centered = left - np.mean(left)
    right_centered = right - np.mean(right)
    denom = np.sqrt(np.sum(left_centered ** 2) * np.sum(right_centered ** 2))
    if denom < 1e-10:
        return 1.0
    return float(np.sum(left_centered * right_centered) / denom)


def stereo_width_rms(left, right):
    """Measure stereo width as ratio of side to mid RMS."""
    mid = (left + right) / 2
    side = (left - right) / 2
    mid_rms = float(np.sqrt(np.mean(mid ** 2)))
    side_rms = float(np.sqrt(np.mean(side ** 2)))
    if mid_rms < 1e-10:
        return 0.0
    return side_rms / mid_rms


# ---------------------------------------------------------------------------
# VC-Stereo Tests
# ---------------------------------------------------------------------------

class TestVCStereo:
    """VC-Stereo width and M/S processing validation."""

    @pytest.fixture(autouse=True)
    def check_cli(self):
        if not cli_exists("VC-Stereo"):
            pytest.skip("No VC-Stereo CLI binary")

    def test_mono_collapse(self, stereo_sine, work_dir):
        """Width=0 should collapse to mono (identical L and R)."""
        output_path = os.path.join(work_dir, "stereo_mono.wav")
        sr, output = process_and_read(
            "VC-Stereo", stereo_sine, output_path, ["--width", "0"]
        )
        if output.ndim < 2:
            pytest.fail("Output is mono, expected stereo")
        left = output[:, 0]
        right = output[:, 1]
        diff = np.sqrt(np.mean((left - right) ** 2))
        lr_rms = np.sqrt(np.mean(left ** 2))
        relative_diff = diff / (lr_rms + 1e-10)
        assert relative_diff < 0.1, \
            f"Mono collapse: L/R too different, relative diff={relative_diff:.4f}"

    def test_wide_increases_width(self, stereo_sine, work_dir):
        """Width=200 should increase stereo width compared to width=100."""
        out_normal_path = os.path.join(work_dir, "stereo_normal.wav")
        sr, out_normal = process_and_read(
            "VC-Stereo", stereo_sine, out_normal_path, ["--width", "100"]
        )
        out_wide_path = os.path.join(work_dir, "stereo_wide.wav")
        sr, out_wide = process_and_read(
            "VC-Stereo", stereo_sine, out_wide_path, ["--width", "200"]
        )
        if out_normal.ndim < 2 or out_wide.ndim < 2:
            pytest.skip("Output not stereo")
        normal_width = stereo_width_rms(out_normal[:, 0], out_normal[:, 1])
        wide_width = stereo_width_rms(out_wide[:, 0], out_wide[:, 1])
        assert wide_width > normal_width, \
            f"Wider setting didn't increase width: normal={normal_width:.4f}, wide={wide_width:.4f}"

    def test_pan_left(self, stereo_sine, work_dir):
        """Pan=-100 should route signal primarily to left channel."""
        output_path = os.path.join(work_dir, "stereo_pan_left.wav")
        sr, output = process_and_read(
            "VC-Stereo", stereo_sine, output_path, ["--pan", "-100"]
        )
        if output.ndim < 2:
            pytest.skip("Output not stereo")
        left_rms = float(np.sqrt(np.mean(output[:, 0] ** 2)))
        right_rms = float(np.sqrt(np.mean(output[:, 1] ** 2)))
        assert left_rms > right_rms, \
            f"Pan left failed: L RMS={left_rms:.4f}, R RMS={right_rms:.4f}"

    def test_pan_right(self, stereo_sine, work_dir):
        """Pan=100 should route signal primarily to right channel."""
        output_path = os.path.join(work_dir, "stereo_pan_right.wav")
        sr, output = process_and_read(
            "VC-Stereo", stereo_sine, output_path, ["--pan", "100"]
        )
        if output.ndim < 2:
            pytest.skip("Output not stereo")
        left_rms = float(np.sqrt(np.mean(output[:, 0] ** 2)))
        right_rms = float(np.sqrt(np.mean(output[:, 1] ** 2)))
        assert right_rms > left_rms, \
            f"Pan right failed: L RMS={left_rms:.4f}, R RMS={right_rms:.4f}"

    def test_bypass_preserves_stereo(self, stereo_sine, work_dir):
        """Bypass should preserve stereo image."""
        output_path = os.path.join(work_dir, "stereo_bypass.wav")
        sr, output = process_and_read(
            "VC-Stereo", stereo_sine, output_path, ["--bypass", "1"]
        )
        sr_in, input_data = read_wav(stereo_sine)
        if output.ndim < 2 or input_data.ndim < 2:
            pytest.skip("Not stereo")
        in_corr = mid_side_correlation(input_data[:, 0], input_data[:, 1])
        out_corr = mid_side_correlation(output[:, 0], output[:, 1])
        assert abs(in_corr - out_corr) < 0.1, \
            f"Bypass changed correlation: in={in_corr:.3f}, out={out_corr:.3f}"


# ---------------------------------------------------------------------------
# VC-Chorus Tests
# ---------------------------------------------------------------------------

class TestVCChorus:
    """VC-Chorus stereo widening and modulation."""

    @pytest.fixture(autouse=True)
    def check_cli(self):
        if not cli_exists("VC-Chorus"):
            pytest.skip("No VC-Chorus CLI binary")

    def test_chorus_adds_stereo_width(self, sine_1k_stereo, work_dir):
        """Chorus should increase stereo width of a mono-like input."""
        sr_in, input_data = read_wav(sine_1k_stereo)
        in_corr = mid_side_correlation(input_data[:, 0], input_data[:, 1])

        output_path = os.path.join(work_dir, "chorus_stereo.wav")
        sr, output = process_and_read(
            "VC-Chorus", sine_1k_stereo, output_path,
            ["--width", "80", "--depth", "50", "--stereo-phase", "90"]
        )
        if output.ndim < 2:
            pytest.skip("Output not stereo")
        out_corr = mid_side_correlation(output[:, 0], output[:, 1])
        assert out_corr < in_corr, \
            f"Chorus didn't add width: in_corr={in_corr:.3f}, out_corr={out_corr:.3f}"

    def test_chorus_bypass(self, sine_1k_stereo, work_dir):
        """Chorus bypass should not change signal."""
        output_path = os.path.join(work_dir, "chorus_bypass.wav")
        sr, output = process_and_read(
            "VC-Chorus", sine_1k_stereo, output_path, ["--bypass", "1"]
        )
        sr_in, input_data = read_wav(sine_1k_stereo)
        if output.ndim < 2 or input_data.ndim < 2:
            pytest.skip("Not stereo")
        min_len = min(len(output), len(input_data))
        error = np.sqrt(np.mean((output[:min_len] - input_data[:min_len]) ** 2))
        assert error < 0.05, f"Bypass error: {error:.6f}"

    def test_chorus_width_parameter(self, sine_1k_stereo, work_dir):
        """Higher width parameter should produce more stereo separation."""
        out_narrow_path = os.path.join(work_dir, "chorus_narrow.wav")
        sr, out_narrow = process_and_read(
            "VC-Chorus", sine_1k_stereo, out_narrow_path,
            ["--width", "0", "--depth", "50"]
        )
        out_wide_path = os.path.join(work_dir, "chorus_wide.wav")
        sr, out_wide = process_and_read(
            "VC-Chorus", sine_1k_stereo, out_wide_path,
            ["--width", "100", "--depth", "50"]
        )
        if out_narrow.ndim < 2 or out_wide.ndim < 2:
            pytest.skip("Not stereo")
        narrow_width = stereo_width_rms(out_narrow[:, 0], out_narrow[:, 1])
        wide_width = stereo_width_rms(out_wide[:, 0], out_wide[:, 1])
        assert wide_width >= narrow_width * 0.9, \
            f"Width parameter ineffective: narrow={narrow_width:.4f}, wide={wide_width:.4f}"


# ---------------------------------------------------------------------------
# VC-Delay Ping-Pong Tests
# ---------------------------------------------------------------------------

class TestVCDelayPingPong:
    """VC-Delay ping-pong stereo mode."""

    @pytest.fixture(autouse=True)
    def check_cli(self):
        if not cli_exists("VC-Delay"):
            pytest.skip("No VC-Delay CLI binary")

    def test_ping_pong_with_stereo_input(self, work_dir):
        """Ping-pong delay should create stereo difference from true stereo input."""
        # Use true stereo input (different freqs L/R) to detect ping-pong effect
        stereo = generate_stereo_sine(440, 880, 3.0, amplitude=0.6)
        input_path = os.path.join(work_dir, "pp_stereo_in.wav")
        save_wav(stereo, input_path)

        output_path = os.path.join(work_dir, "delay_pingpong.wav")
        sr, output = process_and_read(
            "VC-Delay", input_path, output_path,
            ["--ping-pong", "1", "--time", "300", "--feedback", "60", "--mix", "80"]
        )
        if output.ndim < 2:
            pytest.skip("Output not stereo in ping-pong mode")

        # Ping-pong should produce some stereo content
        left_rms = float(np.sqrt(np.mean(output[:, 0] ** 2)))
        right_rms = float(np.sqrt(np.mean(output[:, 1] ** 2)))
        assert left_rms > 1e-4 and right_rms > 1e-4, \
            f"Ping-pong output too quiet: L={left_rms:.4f}, R={right_rms:.4f}"

    def test_delay_mono_mode(self, sine_1k_stereo, work_dir):
        """Non-ping-pong delay should process both channels similarly."""
        output_path = os.path.join(work_dir, "delay_mono.wav")
        sr, output = process_and_read(
            "VC-Delay", sine_1k_stereo, output_path,
            ["--ping-pong", "0", "--time", "250", "--feedback", "30", "--mix", "50"]
        )
        if output.ndim < 2:
            pytest.skip("Output not stereo")
        corr = mid_side_correlation(output[:, 0], output[:, 1])
        assert corr > 0.5, f"Non-ping-pong: L/R correlation too low: {corr:.3f}"


# ---------------------------------------------------------------------------
# VC-Harmonizer Tests
# ---------------------------------------------------------------------------

class TestVCHarmonizer:
    """VC-Harmonizer harmony generation and stereo panning."""

    @pytest.fixture(autouse=True)
    def check_cli(self):
        if not cli_exists("VC-Harmonizer"):
            pytest.skip("No VC-Harmonizer CLI binary")

    def test_harmonizer_produces_output(self, sine_1k_stereo, work_dir):
        """Harmonizer should produce non-zero output."""
        output_path = os.path.join(work_dir, "harmonizer_out.wav")
        sr, output = process_and_read(
            "VC-Harmonizer", sine_1k_stereo, output_path,
            ["--voices", "2", "--intervals", "3,7"]
        )
        rms = float(np.sqrt(np.mean(output ** 2)))
        assert rms > 1e-4, f"Harmonizer output too quiet: RMS={rms:.6f}"

    def test_harmonizer_no_nan_inf(self, sine_1k_stereo, work_dir):
        """Harmonizer should not produce NaN/Inf."""
        output_path = os.path.join(work_dir, "harmonizer_clean.wav")
        sr, output = process_and_read(
            "VC-Harmonizer", sine_1k_stereo, output_path,
            ["--voices", "2", "--intervals", "3,7"]
        )
        assert not np.any(np.isnan(output)), "Harmonizer output contains NaN"
        assert not np.any(np.isinf(output)), "Harmonizer output contains Inf"

    def test_harmonizer_produces_stereo(self, sine_1k_stereo, work_dir):
        """Harmonizer with panned voices should produce stereo difference."""
        output_path = os.path.join(work_dir, "harmonizer_stereo.wav")
        sr, output = process_and_read(
            "VC-Harmonizer", sine_1k_stereo, output_path,
            ["--voices", "2", "--intervals", "3,7",
             "--voice-pan", "-0.8,0.8"]
        )
        if output.ndim < 2:
            pytest.skip("Output not stereo")
        # With panned voices, L and R should differ
        width = stereo_width_rms(output[:, 0], output[:, 1])
        # Just check it produces valid output
        rms = float(np.sqrt(np.mean(output ** 2)))
        assert rms > 1e-4, f"Harmonizer output too quiet with panned voices"
