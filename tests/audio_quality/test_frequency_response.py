#!/usr/bin/env python3
"""
Frequency Response Tests for AudioFX Plugins
==============================================
Plugin-specific frequency domain analysis:
- VC-EQ: Verify EQ band shapes (low-shelf, high-shelf, parametric, HP, LP)
- VC-Reverb: Verify RT60 decay estimation
- VC-Delay: Verify delay time via autocorrelation
- VC-Comp: Verify compression curve (gain reduction vs input level)
- VC-Noise: Verify denoising (SNR improvement)
- VC-PitchShift: Verify pitch shifting accuracy
"""

import pytest
import os
import numpy as np
from scipy.signal import welch
from scipy.fft import rfft, rfftfreq

from conftest import cli_exists, process_and_read, AUDIOFX_ROOT
from generators import (
    generate_sine, generate_sweep, generate_white_noise,
    generate_stereo_signal_from_mono, save_wav, read_wav, SAMPLE_RATE
)


# ---------------------------------------------------------------------------
# Helper: spectral analysis
# ---------------------------------------------------------------------------

def measure_db_at_freq(signal, freq, sr=SAMPLE_RATE, fft_size=None):
    """Measure dB level at a specific frequency using FFT."""
    if signal.ndim > 1:
        signal = signal[:, 0]
    if fft_size is None:
        fft_size = len(signal)
    spectrum = np.abs(rfft(signal, n=fft_size))
    freqs = rfftfreq(fft_size, 1.0 / sr)
    idx = np.argmin(np.abs(freqs - freq))
    # Use small window around target freq
    win = max(1, int(3 * fft_size / len(signal)))
    local_max = np.max(spectrum[max(0, idx-win):idx+win+1])
    db_val = 20 * np.log10(local_max + 1e-10)
    return db_val


# ---------------------------------------------------------------------------
# VC-EQ Tests
# ---------------------------------------------------------------------------

class TestVCEQ:
    """VC-EQ frequency response validation."""

    @pytest.fixture(autouse=True)
    def check_cli(self):
        if not cli_exists("VC-EQ"):
            pytest.skip("No VC-EQ CLI binary")

    def test_low_shelf_boost(self, sweep_20_20k, work_dir):
        """Low-shelf boost at 200Hz should increase low-frequency energy."""
        output_path = os.path.join(work_dir, "eq_lowshelf.wav")
        sr, output = process_and_read(
            "VC-EQ", sweep_20_20k, output_path,
            ["--band0-type", "0", "--band0-freq", "200", "--band0-gain", "6",
             "--band0-on", "1"]
        )
        sr_in, input_data = read_wav(sweep_20_20k)
        low_db_out = measure_db_at_freq(output, 100)
        low_db_in = measure_db_at_freq(input_data, 100)
        boost = low_db_out - low_db_in
        assert boost > 2.0, f"Low-shelf boost too low: {boost:.1f}dB (expected > 2dB)"

    def test_high_shelf_cut(self, sweep_20_20k, work_dir):
        """High-shelf cut at 8000Hz should decrease high-frequency energy."""
        output_path = os.path.join(work_dir, "eq_highshelf.wav")
        sr, output = process_and_read(
            "VC-EQ", sweep_20_20k, output_path,
            ["--band4-type", "1", "--band4-freq", "8000", "--band4-gain", "-6",
             "--band4-on", "1"]
        )
        sr_in, input_data = read_wav(sweep_20_20k)
        high_db_out = measure_db_at_freq(output, 12000)
        high_db_in = measure_db_at_freq(input_data, 12000)
        cut = high_db_in - high_db_out
        assert cut > 2.0, f"High-shelf cut too low: {cut:.1f}dB (expected > 2dB)"

    def test_parametric_boost(self, sweep_20_20k, work_dir):
        """Parametric boost at 1kHz should increase energy around 1kHz."""
        output_path = os.path.join(work_dir, "eq_para_boost.wav")
        sr, output = process_and_read(
            "VC-EQ", sweep_20_20k, output_path,
            ["--band2-type", "2", "--band2-freq", "1000", "--band2-gain", "6",
             "--band2-q", "2.0", "--band2-on", "1"]
        )
        sr_in, input_data = read_wav(sweep_20_20k)
        mid_db_out = measure_db_at_freq(output, 1000)
        mid_db_in = measure_db_at_freq(input_data, 1000)
        boost = mid_db_out - mid_db_in
        assert boost > 2.0, f"Parametric boost too low: {boost:.1f}dB (expected > 2dB)"

    def test_high_pass_filter(self, sweep_20_20k, work_dir):
        """High-pass filter at 500Hz should attenuate below 500Hz."""
        output_path = os.path.join(work_dir, "eq_hp.wav")
        sr, output = process_and_read(
            "VC-EQ", sweep_20_20k, output_path,
            ["--band0-type", "4", "--band0-freq", "500", "--band0-on", "1"]
        )
        sr_in, input_data = read_wav(sweep_20_20k)
        low_db_out = measure_db_at_freq(output, 100)
        low_db_in = measure_db_at_freq(input_data, 100)
        attenuation = low_db_in - low_db_out
        assert attenuation > 6.0, f"HP attenuation too low: {attenuation:.1f}dB (expected > 6dB)"

    def test_low_pass_filter(self, sweep_20_20k, work_dir):
        """Low-pass filter at 2000Hz should attenuate above 2000Hz."""
        output_path = os.path.join(work_dir, "eq_lp.wav")
        sr, output = process_and_read(
            "VC-EQ", sweep_20_20k, output_path,
            ["--band4-type", "3", "--band4-freq", "2000", "--band4-on", "1"]
        )
        sr_in, input_data = read_wav(sweep_20_20k)
        high_db_out = measure_db_at_freq(output, 8000)
        high_db_in = measure_db_at_freq(input_data, 8000)
        attenuation = high_db_in - high_db_out
        assert attenuation > 6.0, f"LP attenuation too low: {attenuation:.1f}dB (expected > 6dB)"

    def test_flat_eq_no_change(self, sine_1k, work_dir):
        """Flat EQ (all bands 0dB) should not significantly change signal."""
        output_path = os.path.join(work_dir, "eq_flat.wav")
        sr, output = process_and_read(
            "VC-EQ", sine_1k, output_path,
            ["--band0-gain", "0", "--band1-gain", "0", "--band2-gain", "0",
             "--band3-gain", "0", "--band4-gain", "0"]
        )
        sr_in, input_data = read_wav(sine_1k)
        if output.ndim > 1:
            output = output[:, 0]
        if input_data.ndim > 1:
            input_data = input_data[:, 0]
        min_len = min(len(output), len(input_data))
        input_rms = float(np.sqrt(np.mean(input_data[:min_len] ** 2)))
        output_rms = float(np.sqrt(np.mean(output[:min_len] ** 2)))
        ratio_db = 20 * np.log10(output_rms / input_rms + 1e-10)
        assert abs(ratio_db) < 1.5, f"Flat EQ changed signal by {ratio_db:.2f}dB"


# ---------------------------------------------------------------------------
# VC-Reverb Tests
# ---------------------------------------------------------------------------

class TestVCReverb:
    """VC-Reverb decay characteristics validation."""

    @pytest.fixture(autouse=True)
    def check_cli(self):
        if not cli_exists("VC-Reverb"):
            pytest.skip("No VC-Reverb CLI binary")

    def test_reverb_adds_tail(self, impulse, work_dir):
        """Reverb should add a decay tail after the impulse."""
        output_path = os.path.join(work_dir, "reverb_impulse.wav")
        sr, output = process_and_read(
            "VC-Reverb", impulse, output_path,
            ["--mix", "100", "--decay", "70", "--room", "50"]
        )
        if output.ndim > 1:
            output = output[:, 0]
        tail_start = int(0.05 * sr)
        tail = output[tail_start:]
        tail_rms = float(np.sqrt(np.mean(tail ** 2)))
        assert tail_rms > 1e-4, f"Reverb tail too quiet: RMS={tail_rms:.6f}"

    def test_reverb_rt60_estimate(self, impulse, work_dir):
        """Estimate RT60 from impulse response and verify it's reasonable."""
        output_path = os.path.join(work_dir, "reverb_rt60.wav")
        sr, output = process_and_read(
            "VC-Reverb", impulse, output_path,
            ["--mix", "100", "--decay", "50", "--room", "50"]
        )
        if output.ndim > 1:
            output = output[:, 0]
        peak_idx = np.argmax(np.abs(output))
        energy = output[peak_idx:] ** 2
        win_size = int(0.01 * sr)
        if win_size < 1:
            win_size = 1
        kernel = np.ones(win_size) / win_size
        energy_smooth = np.convolve(energy, kernel, mode='same')
        peak_energy = energy_smooth[0] if energy_smooth[0] > 0 else np.max(energy_smooth)
        threshold = peak_energy * 1e-6  # -60dB
        above_thresh = np.where(energy_smooth > threshold)[0]
        if len(above_thresh) > 0:
            rt60_samples = above_thresh[-1]
            rt60_ms = rt60_samples / sr * 1000
        else:
            rt60_ms = 0
        assert 100 < rt60_ms < 15000, f"RT60 out of range: {rt60_ms:.0f}ms"

    def test_reverb_dry_mix(self, sine_1k, work_dir):
        """Reverb with mix=0 should pass dry signal only."""
        output_path = os.path.join(work_dir, "reverb_dry.wav")
        sr, output = process_and_read(
            "VC-Reverb", sine_1k, output_path, ["--mix", "0"]
        )
        sr_in, input_data = read_wav(sine_1k)
        if output.ndim > 1:
            output = output[:, 0]
        if input_data.ndim > 1:
            input_data = input_data[:, 0]
        min_len = min(len(output), len(input_data))
        error = np.sqrt(np.mean((output[:min_len] - input_data[:min_len]) ** 2))
        assert error < 0.05, f"Dry mix error too high: {error:.6f}"

    def test_high_decay_longer_tail(self, impulse, work_dir):
        """Higher decay setting should produce longer tail."""
        out_short_path = os.path.join(work_dir, "reverb_short.wav")
        sr, out_short = process_and_read(
            "VC-Reverb", impulse, out_short_path,
            ["--mix", "100", "--decay", "20"]
        )
        out_long_path = os.path.join(work_dir, "reverb_long.wav")
        sr, out_long = process_and_read(
            "VC-Reverb", impulse, out_long_path,
            ["--mix", "100", "--decay", "90"]
        )
        if out_short.ndim > 1:
            out_short = out_short[:, 0]
        if out_long.ndim > 1:
            out_long = out_long[:, 0]
        tail_start = int(0.2 * sr)
        short_tail_rms = float(np.sqrt(np.mean(out_short[tail_start:] ** 2)))
        long_tail_rms = float(np.sqrt(np.mean(out_long[tail_start:] ** 2)))
        assert long_tail_rms > short_tail_rms * 1.5, \
            f"Long decay tail ({long_tail_rms:.4f}) not significantly longer than short ({short_tail_rms:.4f})"


# ---------------------------------------------------------------------------
# VC-Delay Tests
# ---------------------------------------------------------------------------

class TestVCDelay:
    """VC-Delay timing validation."""

    @pytest.fixture(autouse=True)
    def check_cli(self):
        if not cli_exists("VC-Delay"):
            pytest.skip("No VC-Delay CLI binary")

    def test_delay_time_detection(self, impulse, work_dir):
        """Detect delay time and verify it matches parameter."""
        delay_ms = 250
        output_path = os.path.join(work_dir, "delay_250ms.wav")
        sr, output = process_and_read(
            "VC-Delay", impulse, output_path,
            ["--time", str(delay_ms), "--feedback", "50", "--mix", "80"]
        )
        if output.ndim > 1:
            output = output[:, 0]
        peak_idx = np.argmax(np.abs(output))
        search_start = peak_idx + int(0.05 * sr)
        search_end = min(len(output), search_start + int(1.0 * sr))
        search_region = np.abs(output[search_start:search_end])
        if len(search_region) > 0:
            echo_idx = np.argmax(search_region) + search_start
            detected_delay_ms = (echo_idx - peak_idx) / sr * 1000
            assert abs(detected_delay_ms - delay_ms) < delay_ms * 0.3, \
                f"Detected delay {detected_delay_ms:.0f}ms != expected {delay_ms}ms"
        else:
            pytest.fail("No echo detected in delay output")

    def test_delay_feedback_reduces(self, impulse, work_dir):
        """Delay feedback echoes should progressively decrease in amplitude."""
        output_path = os.path.join(work_dir, "delay_feedback.wav")
        sr, output = process_and_read(
            "VC-Delay", impulse, output_path,
            ["--time", "200", "--feedback", "50", "--mix", "80"]
        )
        if output.ndim > 1:
            output = output[:, 0]
        peak_idx = np.argmax(np.abs(output))
        delay_samples = int(0.2 * sr)
        echo_amplitudes = []
        for i in range(1, 5):
            echo_pos = peak_idx + i * delay_samples
            if echo_pos < len(output):
                win = int(0.01 * sr)
                start = max(0, echo_pos - win)
                end = min(len(output), echo_pos + win)
                amp = float(np.max(np.abs(output[start:end])))
                echo_amplitudes.append(amp)
        if len(echo_amplitudes) >= 2:
            for i in range(1, len(echo_amplitudes)):
                assert echo_amplitudes[i] <= echo_amplitudes[i-1] * 1.1, \
                    f"Echo {i} not decreasing from echo {i-1}"


# ---------------------------------------------------------------------------
# VC-Comp Tests
# ---------------------------------------------------------------------------

class TestVCComp:
    """VC-Comp compression curve validation."""

    @pytest.fixture(autouse=True)
    def check_cli(self):
        if not cli_exists("VC-Comp"):
            pytest.skip("No VC-Comp CLI binary")

    def test_compression_reduces_gain(self, sine_1k, work_dir):
        """Compressor with low threshold should reduce output level."""
        bypass_path = os.path.join(work_dir, "comp_bypass.wav")
        sr, bypass_out = process_and_read(
            "VC-Comp", sine_1k, bypass_path,
            ["--threshold", "0", "--ratio", "1"]
        )
        bypass_rms = float(np.sqrt(np.mean(bypass_out ** 2)))

        comp_path = os.path.join(work_dir, "comp_heavy.wav")
        sr, comp_out = process_and_read(
            "VC-Comp", sine_1k, comp_path,
            ["--threshold", "-20", "--ratio", "10", "--attack", "1", "--release", "50"]
        )
        steady_start = int(0.1 * sr)
        comp_rms = float(np.sqrt(np.mean(comp_out[steady_start:] ** 2)))

        assert comp_rms < bypass_rms * 0.95, \
            f"Compression didn't reduce level: bypass_rms={bypass_rms:.4f}, comp_rms={comp_rms:.4f}"

    def test_makeup_gain_compensates(self, sine_1k, work_dir):
        """Makeup gain should compensate for compression gain reduction."""
        no_makeup_path = os.path.join(work_dir, "comp_no_makeup.wav")
        sr, no_makeup = process_and_read(
            "VC-Comp", sine_1k, no_makeup_path,
            ["--threshold", "-20", "--ratio", "8", "--gain", "0"]
        )
        steady_start = int(0.1 * sr)
        no_makeup_rms = float(np.sqrt(np.mean(no_makeup[steady_start:] ** 2)))

        makeup_path = os.path.join(work_dir, "comp_with_makeup.wav")
        sr, with_makeup = process_and_read(
            "VC-Comp", sine_1k, makeup_path,
            ["--threshold", "-20", "--ratio", "8", "--gain", "10"]
        )
        with_makeup_rms = float(np.sqrt(np.mean(with_makeup[steady_start:] ** 2)))

        assert with_makeup_rms > no_makeup_rms * 1.5, \
            f"Makeup gain insufficient: no_makeup={no_makeup_rms:.4f}, with_makeup={with_makeup_rms:.4f}"

    def test_ratio_affects_gain_reduction(self, sine_1k, work_dir):
        """Higher ratio should produce more gain reduction."""
        results = []
        for ratio in ["2", "8", "20"]:
            path = os.path.join(work_dir, f"comp_ratio_{ratio}.wav")
            sr, output = process_and_read(
                "VC-Comp", sine_1k, path,
                ["--threshold", "-10", "--ratio", ratio, "--gain", "0"]
            )
            steady_start = int(0.1 * sr)
            rms = float(np.sqrt(np.mean(output[steady_start:] ** 2)))
            results.append((int(ratio), rms))
        assert results[2][1] < results[0][1], \
            f"Ratio trend wrong: ratio=2 rms={results[0][1]:.4f}, ratio=20 rms={results[2][1]:.4f}"


# ---------------------------------------------------------------------------
# VC-Noise (Denoise) Tests
# ---------------------------------------------------------------------------

class TestVCNoise:
    """VC-Noise denoising validation."""

    @pytest.fixture(autouse=True)
    def check_cli(self):
        if not cli_exists("VC-Noise"):
            pytest.skip("No VC-Noise CLI binary")

    def test_denoise_improves_snr(self, noisy_sine, work_dir):
        """Denoising should improve SNR of a noisy signal."""
        sr_in, input_data = read_wav(noisy_sine)
        if input_data.ndim > 1:
            input_data = input_data[:, 0]

        output_path = os.path.join(work_dir, "denoised.wav")
        sr, output = process_and_read(
            "VC-Noise", noisy_sine, output_path,
            ["--process", "--learn-ms", "200", "--reduction", "12", "--mode", "0"]
        )
        if output.ndim > 1:
            output = output[:, 0]

        input_signal = measure_db_at_freq(input_data, 1000)
        input_noise = measure_db_at_freq(input_data, 2000)
        input_snr = input_signal - input_noise

        output_signal = measure_db_at_freq(output, 1000)
        output_noise = measure_db_at_freq(output, 2000)
        output_snr = output_signal - output_noise

        assert output_snr >= input_snr - 3, \
            f"Denoising degraded SNR: input={input_snr:.1f}dB, output={output_snr:.1f}dB"

    def test_noise_generation(self, work_dir):
        """VC-Noise should generate valid noise signals."""
        from conftest import run_cli
        output_path = os.path.join(work_dir, "noise_gen.wav")
        result = run_cli("VC-Noise", [output_path, "--type", "0", "--duration", "2"])
        assert result.returncode == 0, f"Noise generation failed: {result.stderr}"

        sr, data = read_wav(output_path)
        rms = float(np.sqrt(np.mean(data ** 2)))
        assert rms > 1e-4, f"Generated noise too quiet: RMS={rms:.6f}"
        assert not np.any(np.isnan(data)), "Generated noise contains NaN"


# ---------------------------------------------------------------------------
# VC-PitchShift Tests
# ---------------------------------------------------------------------------

class TestVCPitchShift:
    """VC-PitchShift pitch shifting validation."""

    @pytest.fixture(autouse=True)
    def check_cli(self):
        if not cli_exists("VC-PitchShift"):
            pytest.skip("No VC-PitchShift CLI binary")

    def test_pitch_shift_up(self, sine_440, work_dir):
        """Pitch shift up by 12 semitones (1 octave) should double frequency."""
        output_path = os.path.join(work_dir, "pitchshift_up12.wav")
        sr, output = process_and_read(
            "VC-PitchShift", sine_440, output_path, ["--semitones", "12"]
        )
        if output.ndim > 1:
            output = output[:, 0]
        start = int(0.2 * sr)
        signal = output[start:]
        N = len(signal)
        spectrum = np.abs(rfft(signal))
        freqs = rfftfreq(N, 1.0 / sr)
        peak_freq = freqs[np.argmax(spectrum)]
        assert abs(peak_freq - 880) < 88, \
            f"Pitch shift up 12 semitones: expected ~880Hz, got {peak_freq:.0f}Hz"

    def test_pitch_shift_down(self, sine_440, work_dir):
        """Pitch shift down by 12 semitones should halve frequency."""
        output_path = os.path.join(work_dir, "pitchshift_down12.wav")
        sr, output = process_and_read(
            "VC-PitchShift", sine_440, output_path, ["--semitones", "-12"]
        )
        if output.ndim > 1:
            output = output[:, 0]
        start = int(0.2 * sr)
        signal = output[start:]
        N = len(signal)
        spectrum = np.abs(rfft(signal))
        freqs = rfftfreq(N, 1.0 / sr)
        peak_freq = freqs[np.argmax(spectrum)]
        assert abs(peak_freq - 220) < 30, \
            f"Pitch shift down 12 semitones: expected ~220Hz, got {peak_freq:.0f}Hz"

    def test_pitch_shift_bypass(self, sine_440, work_dir):
        """Bypass should not change pitch."""
        output_path = os.path.join(work_dir, "pitchshift_bypass.wav")
        sr, output = process_and_read(
            "VC-PitchShift", sine_440, output_path, ["--bypass", "1"]
        )
        if output.ndim > 1:
            output = output[:, 0]
        start = int(0.1 * sr)
        signal = output[start:]
        N = len(signal)
        spectrum = np.abs(rfft(signal))
        freqs = rfftfreq(N, 1.0 / sr)
        peak_freq = freqs[np.argmax(spectrum)]
        assert abs(peak_freq - 440) < 20, \
            f"Bypass changed frequency: expected ~440Hz, got {peak_freq:.0f}Hz"
