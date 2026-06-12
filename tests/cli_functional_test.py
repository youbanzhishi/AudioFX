#!/usr/bin/env python3
"""
VC Plugin CLI Functional Test Suite
====================================
Three-tier testing for each VC plugin's standalone CLI:

  Tier 1 (Runnable): CLI executes, produces valid WAV, reasonable file size
  Tier 2 (Functional): DSP correctness - plugin-specific signal analysis
  Tier 3 (Effect): Signal chain sanity - RMS/peak in reasonable range

Usage:
  python3 tests/cli_functional_test.py <plugin_name> <cli_binary_path>

Example:
  python3 tests/cli_functional_test.py VC-Gain ./VC-Gain/standalone-CLI
"""

import sys
import os
import struct
import wave
import tempfile
import subprocess
import shutil
import numpy as np

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def db(x):
    """Convert linear amplitude to dBFS."""
    return 20.0 * np.log10(np.abs(x) + 1e-30)

def rms_db(data):
    """RMS in dBFS of a numpy array."""
    return db(np.sqrt(np.mean(data ** 2)))

def peak_db(data):
    """Peak in dBFS."""
    return db(np.max(np.abs(data)))

def peak_linear(data):
    """Peak linear amplitude."""
    return float(np.max(np.abs(data)))

def read_wav_float(path):
    """Read a WAV file and return (sample_rate, float_data).
    Handles IEEE float WAV (dr_wav output) and PCM WAV (sox output).
    Falls back to sox conversion if scipy can't read it."""
    try:
        from scipy.io import wavfile
        sr, data = wavfile.read(path)
        if data.dtype == np.int16:
            data = data.astype(np.float32) / 32768.0
        elif data.dtype == np.int32:
            data = data.astype(np.float32) / 2147483648.0
        elif data.dtype == np.float64:
            data = data.astype(np.float32)
        return sr, data
    except Exception:
        pcm_path = path + ".pcm.wav"
        try:
            subprocess.run(
                ["sox", path, "-b", "16", pcm_path],
                check=True, capture_output=True, timeout=10
            )
            with wave.open(pcm_path, "rb") as wf:
                sr = wf.getframerate()
                nch = wf.getnchannels()
                nframes = wf.getnframes()
                raw = wf.readframes(nframes)
            arr = np.frombuffer(raw, dtype=np.int16).reshape(-1, nch)
            data = arr.astype(np.float32) / 32768.0
            return sr, data
        finally:
            if os.path.exists(pcm_path):
                os.remove(pcm_path)


def generate_impulse_wav(path, sr=44100, duration_samples=8192):
    """Generate a 2-channel WAV with a single-sample impulse at sample 0."""
    with wave.open(path, "w") as f:
        f.setnchannels(2)
        f.setsampwidth(2)
        f.setframerate(sr)
        data = struct.pack("<hh", 32767, 32767)
        data += b"\x00\x00\x00\x00" * (duration_samples - 1)
        f.writeframes(data)


def generate_sine_wav(path, freq=440.0, sr=44100, duration=0.5, amplitude=0.5):
    """Generate a 2-channel sine WAV (16-bit PCM)."""
    n_samples = int(sr * duration)
    t = np.arange(n_samples, dtype=np.float64) / sr
    sig = (amplitude * np.sin(2.0 * np.pi * freq * t)).astype(np.float32)
    stereo = np.column_stack([sig, sig])
    pcm = (stereo * 32767).astype(np.int16)
    with wave.open(path, "w") as f:
        f.setnchannels(2)
        f.setsampwidth(2)
        f.setframerate(sr)
        f.writeframes(pcm.tobytes())


def generate_multitone_wav(path, sr=44100, duration=1.0, amplitude=0.5):
    """Generate a 2-channel WAV with multiple sine tones."""
    n_samples = int(sr * duration)
    t = np.arange(n_samples, dtype=np.float64) / sr
    freqs = [100, 500, 1000, 5000, 10000]
    sig = np.zeros(n_samples, dtype=np.float64)
    for freq in freqs:
        sig += np.sin(2.0 * np.pi * freq * t)
    sig = sig / len(freqs)
    sig = (sig * amplitude).astype(np.float32)
    stereo = np.column_stack([sig, sig])
    pcm = (stereo * 32767).astype(np.int16)
    with wave.open(path, "w") as f:
        f.setnchannels(2)
        f.setsampwidth(2)
        f.setframerate(sr)
        f.writeframes(pcm.tobytes())



# ---------------------------------------------------------------------------
# Synth (instrument) plugins - no input WAV, only output WAV
# ---------------------------------------------------------------------------

SYNTH_PLUGINS = {"VC-Drum"}

def run_cli_synth(cli_path, output_wav, extra_args=None, timeout=60):
    """Run a synth CLI (no input WAV), return (returncode, stdout, stderr)."""
    cmd = [cli_path, output_wav]
    if extra_args:
        cmd.extend(extra_args)
    result = subprocess.run(
        cmd, capture_output=True, text=True, timeout=timeout
    )
    return result.returncode, result.stdout, result.stderr


def run_cli(cli_path, input_wav, output_wav, extra_args=None, timeout=60):
    """Run the CLI, return (returncode, stdout, stderr)."""
    cmd = [cli_path, input_wav, output_wav]
    if extra_args:
        cmd.extend(extra_args)
    result = subprocess.run(
        cmd, capture_output=True, text=True, timeout=timeout
    )
    return result.returncode, result.stdout, result.stderr


# ---------------------------------------------------------------------------
# Test result tracking
# ---------------------------------------------------------------------------

class TestResults:
    def __init__(self):
        self.passed = []
        self.failed = []

    def ok(self, name, detail=""):
        self.passed.append(name)
        tag = "\033[92mPASS\033[0m"
        print(f"  {tag}: {name}" + (f" - {detail}" if detail else ""))

    def fail(self, name, detail=""):
        self.failed.append(name)
        tag = "\033[91mFAIL\033[0m"
        print(f"  {tag}: {name}" + (f" - {detail}" if detail else ""))

    def summary(self):
        total = len(self.passed) + len(self.failed)
        print(f"\n{'='*60}")
        print(f"Results: {len(self.passed)}/{total} passed, {len(self.failed)}/{total} failed")
        if self.failed:
            print("Failed tests:")
            for n in self.failed:
                print(f"  - {n}")
            return False
        return True


# ---------------------------------------------------------------------------
# Tier 1: Runnable tests
# ---------------------------------------------------------------------------

def tier1_runnable(results, cli_path, tmpdir):
    plugin_name = os.path.basename(os.path.dirname(cli_path))
    is_synth = plugin_name in SYNTH_PLUGINS

    if is_synth:
        # Synth plugins: no input WAV needed
        output_path = os.path.join(tmpdir, "output_t1.wav")
        if plugin_name == "VC-Drum":
            rc, out, err = run_cli_synth(cli_path, output_path, ["--preset", "basic-beat", "--bars", "1"])
        else:
            rc, out, err = run_cli_synth(cli_path, output_path)
        if rc != 0:
            results.fail("CLI executes without crash", f"exit code {rc}, stderr: {err[:200]}")
            return False
        results.ok("CLI executes without crash")
    else:
        # Effect plugins: input WAV → output WAV
        impulse_path = os.path.join(tmpdir, "impulse_t1.wav")
        output_path = os.path.join(tmpdir, "output_t1.wav")
        generate_impulse_wav(impulse_path)

        rc, out, err = run_cli(cli_path, impulse_path, output_path)
        if rc != 0:
            results.fail("CLI executes without crash", f"exit code {rc}, stderr: {err[:200]}")
            return False
        results.ok("CLI executes without crash")

    if not os.path.isfile(output_path):
        results.fail("Output WAV file exists", "File not found")
        return False
    results.ok("Output WAV file exists")

    fsize = os.path.getsize(output_path)
    if fsize < 1000:
        results.fail("Output file size reasonable", f"Size: {fsize} bytes (expected >1000)")
        return False
    results.ok("Output file size reasonable", f"{fsize} bytes")

    try:
        sr, data = read_wav_float(output_path)
    except Exception as e:
        results.fail("Output is valid WAV", str(e)[:200])
        return False
    results.ok("Output is valid WAV", f"sr={sr}, shape={data.shape}")

    if data.ndim != 2 or data.shape[1] != 2:
        results.fail("Output is stereo", f"shape={data.shape}")
        return False
    results.ok("Output is stereo")

    return True
# ---------------------------------------------------------------------------
# Tier 2: Functional tests (plugin-specific)
# ---------------------------------------------------------------------------

def tier2_functional(results, plugin_name, cli_path, tmpdir):
    impulse_path = os.path.join(tmpdir, "impulse_t2.wav")
    sine_path = os.path.join(tmpdir, "sine440_t2.wav")
    multitone_path = os.path.join(tmpdir, "multitone_t2.wav")
    output_path = os.path.join(tmpdir, "output_t2.wav")

    generate_impulse_wav(impulse_path)
    generate_sine_wav(sine_path, freq=440.0, duration=0.5, amplitude=0.5)
    generate_multitone_wav(multitone_path, duration=1.0, amplitude=0.5)

    dispatch = {
        "VC-Gain": test_vc_gain,
        "VC-EQ": test_vc_eq,
        "VC-Comp": test_vc_comp,
        "VC-Smooth": test_vc_smooth,
        "VC-DeEsser": test_vc_deesser,
        "VC-Saturator": test_vc_saturator,
        "VC-Limiter": test_vc_limiter,
        "VC-Delay": test_vc_delay,
        "VC-Reverb": test_vc_reverb,
        "VC-DynamicEQ": test_vc_dynamiceq,
        "VC-Tune": test_vc_tune,
    }

    tester = dispatch.get(plugin_name)
    if tester:
        tester(results, cli_path, impulse_path, sine_path, multitone_path, output_path, tmpdir)
    else:
        results.fail(f"Unknown plugin: {plugin_name}", "No functional test defined")


def _safe_read(path):
    return read_wav_float(path)


def _safe_remove(*paths):
    for p in paths:
        try:
            if os.path.exists(p):
                os.remove(p)
        except OSError:
            pass


# -- VC-Gain ----------------------------------------------------------------

def test_vc_gain(results, cli_path, impulse_path, sine_path, multitone_path, output_path, tmpdir):
    """VC-Gain: --gain 6 -> output amplitude ~2x input"""
    ref_path = output_path + ".ref.wav"
    rc, _, _ = run_cli(cli_path, impulse_path, ref_path, ["--gain", "0"])
    if rc != 0:
        results.fail("VC-Gain: 0dB reference run", f"exit={rc}")
        return
    _, ref_data = _safe_read(ref_path)

    rc, _, _ = run_cli(cli_path, impulse_path, output_path, ["--gain", "6"])
    if rc != 0:
        results.fail("VC-Gain: +6dB run", f"exit={rc}")
        _safe_remove(ref_path)
        return
    _, out_data = _safe_read(output_path)

    ref_peak = peak_linear(ref_data)
    out_peak = peak_linear(out_data)

    if ref_peak < 1e-6:
        results.fail("VC-Gain: reference peak too low", f"peak={ref_peak}")
        _safe_remove(ref_path)
        return

    ratio = out_peak / ref_peak
    expected_ratio = 2.0
    if abs(ratio - expected_ratio) / expected_ratio < 0.15:
        results.ok("VC-Gain: +6dB doubles amplitude",
                    f"ratio={ratio:.3f} (expected ~2.0)")
    else:
        results.fail("VC-Gain: +6dB doubles amplitude",
                      f"ratio={ratio:.3f} (expected ~2.0, tolerance +/-15%)")

    _safe_remove(ref_path)


# -- VC-EQ ------------------------------------------------------------------

def test_vc_eq(results, cli_path, impulse_path, sine_path, multitone_path, output_path, tmpdir):
    """VC-EQ: flat -> unity; band2 +12dB -> measurable gain on 1kHz sine."""
    # Test 1: flat preset on impulse -> unity gain
    flat_path = output_path + ".flat.wav"
    rc, _, _ = run_cli(cli_path, impulse_path, flat_path, ["--preset", "flat"])
    if rc != 0:
        results.fail("VC-EQ: flat preset run", f"exit={rc}")
        return
    _, flat_data = _safe_read(flat_path)

    flat_peak = peak_linear(flat_data)
    flat_peak_db = db(flat_peak)
    if abs(flat_peak_db) < 1.0:
        results.ok("VC-EQ: flat preset -> unity gain",
                    f"peak={flat_peak_db:.2f} dBFS (expected +-1dB)")
    else:
        results.fail("VC-EQ: flat preset -> unity gain",
                      f"peak={flat_peak_db:.2f} dBFS (expected +-1dB)")

    # Test 2: band2 +12dB on 1kHz sine -> measurable gain
    sine1k_path = os.path.join(tmpdir, "sine1k_eq.wav")
    generate_sine_wav(sine1k_path, freq=1000.0, duration=1.0, amplitude=0.5)

    ref_eq_path = output_path + ".eqref.wav"
    rc, _, _ = run_cli(cli_path, sine1k_path, ref_eq_path, ["--preset", "flat"])
    if rc != 0:
        results.fail("VC-EQ: flat 1kHz reference", f"exit={rc}")
        _safe_remove(flat_path, sine1k_path)
        return
    _, ref_eq = _safe_read(ref_eq_path)

    boost_path = output_path + ".boost.wav"
    rc, _, _ = run_cli(cli_path, sine1k_path, boost_path,
                       ["--band2-gain", "12", "--band2-freq", "1000", "--band2-q", "1.0"])
    if rc != 0:
        results.fail("VC-EQ: band2 +12dB run", f"exit={rc}")
        _safe_remove(flat_path, sine1k_path, ref_eq_path)
        return
    _, boost_data = _safe_read(boost_path)

    # Compare RMS of second half (after filter settling)
    half = len(ref_eq) // 2
    ref_rms = rms_db(ref_eq[half:])
    boost_rms = rms_db(boost_data[half:])
    gain_increase = boost_rms - ref_rms

    # +12dB on a narrow band should produce at least a few dB RMS gain
    if gain_increase >= 3.0:
        results.ok("VC-EQ: band2 +12dB -> gain increase on 1kHz",
                    f"RMS gain={gain_increase:.2f} dB (expected >=3dB)")
    else:
        results.fail("VC-EQ: band2 +12dB -> gain increase on 1kHz",
                      f"RMS gain={gain_increase:.2f} dB (expected >=3dB)")

    _safe_remove(flat_path, sine1k_path, ref_eq_path, boost_path)


# -- VC-Comp ----------------------------------------------------------------

def test_vc_comp(results, cli_path, impulse_path, sine_path, multitone_path, output_path, tmpdir):
    """VC-Comp: loud signal + low threshold -> measurable gain reduction."""
    # Use a loud 1kHz sine with threshold=-30dB, fast attack=1ms
    # Note: The compressor envelope uses squared values which underestimates level,
    # so we need a very low threshold AND fast attack to ensure detection.
    loud_sine_path = os.path.join(tmpdir, "sine1k_loud_comp.wav")
    generate_sine_wav(loud_sine_path, freq=1000.0, duration=1.0, amplitude=0.9)

    # Reference: no compression (ratio=1)
    ref_path = output_path + ".ref.wav"
    rc, _, _ = run_cli(cli_path, loud_sine_path, ref_path,
                       ["--threshold", "0", "--ratio", "1", "--attack", "1", "--character", "clean"])
    if rc != 0:
        results.fail("VC-Comp: reference run", f"exit={rc}")
        _safe_remove(loud_sine_path)
        return
    _, ref_data = _safe_read(ref_path)

    # Compressed: threshold=-30dB, ratio=10, attack=1ms
    comp_path = output_path + ".comp.wav"
    rc, _, _ = run_cli(cli_path, loud_sine_path, comp_path,
                       ["--threshold", "-30", "--ratio", "10", "--attack", "1", "--character", "clean"])
    if rc != 0:
        results.fail("VC-Comp: compressed run", f"exit={rc}")
        _safe_remove(loud_sine_path, ref_path)
        return
    _, comp_data = _safe_read(comp_path)

    # Compare RMS of second half (after settling)
    half = len(ref_data) // 2
    ref_rms = rms_db(ref_data[half:])
    comp_rms = rms_db(comp_data[half:])
    rms_diff = comp_rms - ref_rms

    if rms_diff < -3.0:
        results.ok("VC-Comp: threshold -30dB -> gain reduction",
                    f"RMS change={rms_diff:.2f} dB (expected < -3dB)")
    else:
        results.fail("VC-Comp: threshold -30dB -> gain reduction",
                      f"RMS change={rms_diff:.2f} dB (expected < -3dB)")

    # Verify DSP is actually processing (not bypass)
    if np.allclose(ref_data, comp_data, atol=1e-5):
        results.fail("VC-Comp: output differs from bypass",
                      "Output identical to bypass - DSP may not be running")
    else:
        results.ok("VC-Comp: output differs from bypass")

    _safe_remove(loud_sine_path, ref_path, comp_path)


# -- VC-Smooth ---------------------------------------------------------------

def test_vc_smooth(results, cli_path, impulse_path, sine_path, multitone_path, output_path, tmpdir):
    """VC-Smooth: default params -> output signal amplitude reasonable."""
    rc, _, _ = run_cli(cli_path, sine_path, output_path)
    if rc != 0:
        results.fail("VC-Smooth: default run", f"exit={rc}")
        return
    _, data = _safe_read(output_path)

    peak = peak_linear(data)
    peak_db_val = peak_db(data)
    rms_val = rms_db(data)

    if peak <= 1.0:
        results.ok("VC-Smooth: peak <= 0dBFS", f"peak={peak_db_val:.2f} dBFS")
    else:
        results.fail("VC-Smooth: peak <= 0dBFS",
                      f"peak={peak_db_val:.2f} dBFS (exceeds 0dBFS)")

    if rms_val > -60.0:
        results.ok("VC-Smooth: output not silent", f"RMS={rms_val:.2f} dBFS")
    else:
        results.fail("VC-Smooth: output not silent",
                      f"RMS={rms_val:.2f} dBFS (too low)")


# -- VC-DeEsser --------------------------------------------------------------

def test_vc_deesser(results, cli_path, impulse_path, sine_path, multitone_path, output_path, tmpdir):
    """VC-DeEsser: --threshold -24 -> doesn't crash, output is valid."""
    rc, _, _ = run_cli(cli_path, sine_path, output_path, ["--threshold", "-24"])
    if rc != 0:
        results.fail("VC-DeEsser: --threshold -24 doesn't crash", f"exit={rc}")
        return
    results.ok("VC-DeEsser: --threshold -24 doesn't crash")

    _, data = _safe_read(output_path)
    peak = peak_linear(data)
    rms_val = rms_db(data)

    if peak > 0 and rms_val > -60:
        results.ok("VC-DeEsser: output has signal",
                    f"peak={db(peak):.2f} dBFS, RMS={rms_val:.2f} dBFS")
    else:
        results.fail("VC-DeEsser: output has signal",
                      f"peak={db(peak):.2f} dBFS, RMS={rms_val:.2f} dBFS")


# -- VC-Saturator ------------------------------------------------------------

def test_vc_saturator(results, cli_path, impulse_path, sine_path, multitone_path, output_path, tmpdir):
    """VC-Saturator: --drive 6 -> output soft-clipped."""
    sine_loud_path = os.path.join(tmpdir, "sine_loud_sat.wav")
    generate_sine_wav(sine_loud_path, freq=440.0, duration=0.5, amplitude=0.9)

    ref_path = output_path + ".ref.wav"
    rc, _, _ = run_cli(cli_path, sine_loud_path, ref_path, ["--drive", "0"])
    if rc != 0:
        results.fail("VC-Saturator: drive=0 reference", f"exit={rc}")
        _safe_remove(sine_loud_path)
        return
    _, ref_data = _safe_read(ref_path)

    sat_path = output_path + ".sat.wav"
    rc, _, _ = run_cli(cli_path, sine_loud_path, sat_path, ["--drive", "6"])
    if rc != 0:
        results.fail("VC-Saturator: drive=6 run", f"exit={rc}")
        _safe_remove(sine_loud_path, ref_path)
        return
    _, sat_data = _safe_read(sat_path)

    input_peak = peak_linear(ref_data)
    sat_peak = peak_linear(sat_data)
    theoretical_linear_peak = input_peak * (10 ** (6.0 / 20.0))

    if sat_peak < theoretical_linear_peak * 0.95:
        results.ok("VC-Saturator: drive=6 -> soft limiting",
                    f"sat_peak={db(sat_peak):.2f} dBFS, linear_peak={db(theoretical_linear_peak):.2f} dBFS")
    else:
        results.fail("VC-Saturator: drive=6 -> soft limiting",
                      f"sat_peak={db(sat_peak):.2f} dBFS, linear_peak={db(theoretical_linear_peak):.2f} dBFS")

    _safe_remove(sine_loud_path, ref_path, sat_path)


# -- VC-Limiter --------------------------------------------------------------

def test_vc_limiter(results, cli_path, impulse_path, sine_path, multitone_path, output_path, tmpdir):
    """VC-Limiter: --ceiling -3 -> output peak <= -3dBFS."""
    rc, _, _ = run_cli(cli_path, impulse_path, output_path, ["--ceiling", "-3"])
    if rc != 0:
        results.fail("VC-Limiter: --ceiling -3 run", f"exit={rc}")
        return
    _, data = _safe_read(output_path)

    peak_db_val = peak_db(data)

    if peak_db_val <= -2.8:
        results.ok("VC-Limiter: --ceiling -3 -> peak <= -3dBFS",
                    f"peak={peak_db_val:.2f} dBFS")
    else:
        results.fail("VC-Limiter: --ceiling -3 -> peak <= -3dBFS",
                      f"peak={peak_db_val:.2f} dBFS (expected <= -3dBFS)")


# -- VC-Delay ----------------------------------------------------------------

def test_vc_delay(results, cli_path, impulse_path, sine_path, multitone_path, output_path, tmpdir):
    """VC-Delay: --time 100 --mix 50 -> output has delayed component."""
    long_impulse_path = os.path.join(tmpdir, "impulse_long_delay.wav")
    generate_impulse_wav(long_impulse_path, duration_samples=22050)

    rc, _, _ = run_cli(cli_path, long_impulse_path, output_path,
                       ["--time", "100", "--mix", "50"])
    if rc != 0:
        results.fail("VC-Delay: delay run", f"exit={rc}")
        _safe_remove(long_impulse_path)
        return
    _, data = _safe_read(output_path)

    delay_samples = 4410  # 100ms at 44100Hz
    if delay_samples < len(data):
        window_start = max(0, delay_samples - 200)
        window_end = min(len(data), delay_samples + 200)
        delay_region = data[window_start:window_end, 0]
        delay_rms = np.sqrt(np.mean(delay_region ** 2))

        if delay_rms > 1e-4:
            results.ok("VC-Delay: delayed signal present at ~100ms",
                        f"delay_region_RMS={db(delay_rms):.2f} dBFS")
        else:
            results.fail("VC-Delay: delayed signal present at ~100ms",
                          f"delay_region_RMS={db(delay_rms):.2f} dBFS (too low)")
    else:
        rms_val = rms_db(data)
        if rms_val > -60:
            results.ok("VC-Delay: output has signal", f"RMS={rms_val:.2f} dBFS")
        else:
            results.fail("VC-Delay: output has signal",
                          f"RMS={rms_val:.2f} dBFS (too low)")

    _safe_remove(long_impulse_path)


# -- VC-Reverb ---------------------------------------------------------------

def test_vc_reverb(results, cli_path, impulse_path, sine_path, multitone_path, output_path, tmpdir):
    """VC-Reverb: --mix 50 -> output has reverb tail."""
    long_impulse_path = os.path.join(tmpdir, "impulse_long_reverb.wav")
    generate_impulse_wav(long_impulse_path, duration_samples=44100)

    rc, _, _ = run_cli(cli_path, long_impulse_path, output_path, ["--mix", "50"])
    if rc != 0:
        results.fail("VC-Reverb: reverb run", f"exit={rc}")
        _safe_remove(long_impulse_path)
        return
    _, data = _safe_read(output_path)

    tail_start = min(len(data), 22050)
    if tail_start < len(data):
        tail = data[tail_start:, 0]
        tail_rms = np.sqrt(np.mean(tail ** 2))

        if tail_rms > 1e-5:
            results.ok("VC-Reverb: reverb tail present",
                        f"tail_RMS={db(tail_rms):.2f} dBFS (from 500ms onward)")
        else:
            results.fail("VC-Reverb: reverb tail present",
                          f"tail_RMS={db(tail_rms):.2f} dBFS (too low)")
    else:
        results.fail("VC-Reverb: reverb tail present", "Output too short")

    _safe_remove(long_impulse_path)


# -- VC-DynamicEQ ------------------------------------------------------------

def test_vc_dynamiceq(results, cli_path, impulse_path, sine_path, multitone_path, output_path, tmpdir):
    """VC-DynamicEQ: --threshold -12 -> doesn't crash, output valid."""
    rc, _, _ = run_cli(cli_path, sine_path, output_path, ["--threshold", "-12"])
    if rc != 0:
        results.fail("VC-DynamicEQ: --threshold -12 doesn't crash", f"exit={rc}")
        return
    results.ok("VC-DynamicEQ: --threshold -12 doesn't crash")

    _, data = _safe_read(output_path)
    peak = peak_linear(data)
    rms_val = rms_db(data)

    if peak > 0 and rms_val > -60:
        results.ok("VC-DynamicEQ: output has signal",
                    f"peak={db(peak):.2f} dBFS, RMS={rms_val:.2f} dBFS")
    else:
        results.fail("VC-DynamicEQ: output has signal",
                      f"peak={db(peak):.2f} dBFS, RMS={rms_val:.2f} dBFS")



# -- VC-Tune ----------------------------------------------------------------

def _detect_dominant_freq(data, sr):
    """FFT-based dominant frequency detection in 50-2000 Hz range."""
    n = len(data)
    fft = np.fft.rfft(data)
    freqs = np.fft.rfftfreq(n, 1.0 / sr)
    mag = np.abs(fft)
    mask = (freqs >= 50) & (freqs <= 2000)
    mag_masked = mag.copy()
    mag_masked[~mask] = 0
    peak_idx = np.argmax(mag_masked)
    return freqs[peak_idx]


def generate_multifreq_sequence_wav(path, freqs, sr=44100, duration=4.0, amplitude=0.5):
    """Generate a 2-channel WAV with sequential sine tones (melody)."""
    n_samples = int(sr * duration)
    seg_len = n_samples // len(freqs)
    sig = np.zeros(n_samples, dtype=np.float64)
    t = np.arange(n_samples, dtype=np.float64) / sr
    for i, freq in enumerate(freqs):
        start = i * seg_len
        end = min(start + seg_len, n_samples)
        sig[start:end] = np.sin(2.0 * np.pi * freq * t[start:end])
    sig = (sig * amplitude).astype(np.float32)
    stereo = np.column_stack([sig, sig])
    pcm = (stereo * 32767).astype(np.int16)
    with wave.open(path, "w") as f:
        f.setnchannels(2)
        f.setsampwidth(2)
        f.setframerate(sr)
        f.writeframes(pcm.tobytes())


def test_vc_tune(results, cli_path, impulse_path, sine_path, multitone_path, output_path, tmpdir):
    """VC-Tune: Pitch correction + auto key detection tests."""

    # --- Pitch Correction: 440Hz Chromatic -> A4 (no change) ---
    rc, _, _ = run_cli(cli_path, sine_path, output_path, ["--speed", "100", "--scale", "0"])
    if rc != 0:
        results.fail("VC-Tune: basic pitch correction run", f"exit={rc}")
        return
    sr, data = _safe_read(output_path)
    freq = _detect_dominant_freq(data[:, 0], sr)
    if abs(freq - 440.0) < 5.0:
        results.ok("VC-Tune: 440Hz Chromatic -> A4", f"detected {freq:.1f}Hz")
    else:
        results.fail("VC-Tune: 440Hz Chromatic -> A4", f"detected {freq:.1f}Hz, expected ≈440Hz")

    # --- Pitch Correction: 440Hz Minor -> Ab4 ---
    sine440_minor = os.path.join(tmpdir, "sine440_minor.wav")
    generate_sine_wav(sine440_minor, freq=440.0, duration=1.0, amplitude=0.5)
    rc, _, _ = run_cli(cli_path, sine440_minor, output_path, ["--speed", "100", "--scale", "2"])
    if rc != 0:
        results.fail("VC-Tune: Minor scale run", f"exit={rc}")
    else:
        sr, data = _safe_read(output_path)
        freq = _detect_dominant_freq(data[:, 0], sr)
        if abs(freq - 415.3) < 15.0:
            results.ok("VC-Tune: 440Hz Minor -> Ab4", f"detected {freq:.1f}Hz")
        else:
            results.fail("VC-Tune: 440Hz Minor -> Ab4", f"detected {freq:.1f}Hz, expected ≈415Hz")

    # --- Bypass: no change ---
    sine445 = os.path.join(tmpdir, "sine445.wav")
    generate_sine_wav(sine445, freq=445.0, duration=1.0, amplitude=0.5)
    rc, _, _ = run_cli(cli_path, sine445, output_path, ["--bypass", "1"])
    if rc != 0:
        results.fail("VC-Tune: bypass run", f"exit={rc}")
    else:
        sr, data = _safe_read(output_path)
        freq = _detect_dominant_freq(data[:, 0], sr)
        if abs(freq - 445.0) < 5.0:
            results.ok("VC-Tune: bypass no change", f"detected {freq:.1f}Hz")
        else:
            results.fail("VC-Tune: bypass no change", f"detected {freq:.1f}Hz, expected ≈445Hz")

    # --- Speed=0: no correction ---
    rc, _, _ = run_cli(cli_path, sine445, output_path, ["--speed", "0", "--scale", "0"])
    if rc != 0:
        results.fail("VC-Tune: speed=0 run", f"exit={rc}")
    else:
        sr, data = _safe_read(output_path)
        freq = _detect_dominant_freq(data[:, 0], sr)
        if abs(freq - 445.0) < 5.0:
            results.ok("VC-Tune: speed=0 no correction", f"detected {freq:.1f}Hz")
        else:
            results.fail("VC-Tune: speed=0 no correction", f"detected {freq:.1f}Hz, expected ≈445Hz")

    # --- Transpose +2: 440Hz -> B4 ---
    rc, _, _ = run_cli(cli_path, sine_path, output_path, ["--speed", "100", "--scale", "0", "--transpose", "2"])
    if rc != 0:
        results.fail("VC-Tune: transpose +2 run", f"exit={rc}")
    else:
        sr, data = _safe_read(output_path)
        freq = _detect_dominant_freq(data[:, 0], sr)
        if abs(freq - 493.88) < 15.0:
            results.ok("VC-Tune: transpose +2 -> B4", f"detected {freq:.1f}Hz")
        else:
            results.fail("VC-Tune: transpose +2 -> B4", f"detected {freq:.1f}Hz, expected ≈494Hz")

    # --- Auto Key Detection: C Major melody ---
    c_major_notes = [261.63, 293.66, 329.63, 349.23, 392.00, 440.00, 493.88]
    c_major_wav = os.path.join(tmpdir, "c_major_melody.wav")
    generate_multifreq_sequence_wav(c_major_wav, c_major_notes, sr=44100, duration=4.0, amplitude=0.5)
    rc, stdout, _ = run_cli(cli_path, c_major_wav, output_path, ["--autokey", "1", "--speed", "100"])
    if rc != 0:
        results.fail("VC-Tune: autokey C Major run", f"exit={rc}")
    else:
        if "C Major" in stdout:
            results.ok("VC-Tune: autokey detects C Major", "Found in output")
        else:
            results.fail("VC-Tune: autokey detects C Major", f"stdout: {stdout[-200:]}")

    # --- Report mode ---
    rc, stdout, _ = run_cli(cli_path, sine_path, output_path, ["--report", "--speed", "100", "--scale", "0"])
    if rc != 0:
        results.fail("VC-Tune: report mode run", f"exit={rc}")
    else:
        if "Time(ms)" in stdout:
            results.ok("VC-Tune: report mode produces report", "Found header")
        else:
            results.fail("VC-Tune: report mode produces report", "Header not found")

    # --- Presets: T-Pain ---
    rc, _, _ = run_cli(cli_path, sine_path, output_path, ["--preset", "tpain"])
    if rc != 0:
        results.fail("VC-Tune: T-Pain preset", f"exit={rc}")
    else:
        sr, data = _safe_read(output_path)
        if len(data) > 0 and np.max(np.abs(data)) > 0.01:
            results.ok("VC-Tune: T-Pain preset produces output")
        else:
            results.fail("VC-Tune: T-Pain preset produces output", "Silent output")

    _safe_remove(sine440_minor, sine445, c_major_wav)

# ---------------------------------------------------------------------------
# Tier 3: Effect tests (signal chain sanity)
# ---------------------------------------------------------------------------


def test_vc_drum(results, cli_path, impulse_path, sine_path, multitone_path, output_path, tmpdir):
    """VC-Drum: synth plugin - verify basic-beat preset generates audio."""
    # VC-Drum is a synth, uses run_cli_synth
    beat_path = os.path.join(tmpdir, "drum_beat.wav")
    rc, stdout, _ = run_cli_synth(cli_path, beat_path, ["--preset", "basic-beat", "--bars", "1"])
    if rc != 0:
        results.fail("VC-Drum: basic-beat preset run", f"exit={rc}")
        return
    results.ok("VC-Drum: basic-beat preset executes", "exit=0")

    _, beat_data = _safe_read(beat_path)
    beat_rms = rms_db(beat_data)
    beat_peak = peak_db(beat_data)

    if beat_rms > -40:
        results.ok("VC-Drum: basic-beat produces audio", f"RMS={beat_rms:.2f} dBFS")
    else:
        results.fail("VC-Drum: basic-beat produces audio", f"RMS={beat_rms:.2f} dBFS (too quiet)")

    if beat_peak > -6:
        results.ok("VC-Drum: peak level reasonable", f"peak={beat_peak:.2f} dBFS")
    else:
        results.fail("VC-Drum: peak level reasonable", f"peak={beat_peak:.2f} dBFS (too low)")

    # Test different presets
    kick_path = os.path.join(tmpdir, "drum_kick.wav")
    rc, _, _ = run_cli_synth(cli_path, kick_path, ["--preset", "kick-only", "--bars", "1"])
    if rc != 0:
        results.fail("VC-Drum: kick-only preset", f"exit={rc}")
    else:
        results.ok("VC-Drum: kick-only preset executes")


def tier3_effect(results, plugin_name, cli_path, tmpdir):
    """Verify signal-level sanity: RMS not too low, peak not too high."""
    sine_path = os.path.join(tmpdir, "sine_t3.wav")
    output_path = os.path.join(tmpdir, "output_t3.wav")
    generate_sine_wav(sine_path, freq=440.0, duration=1.0, amplitude=0.5)

    plugin_name = os.path.basename(os.path.dirname(cli_path))
    is_synth = plugin_name in SYNTH_PLUGINS
    if is_synth:
        rc, _, _ = run_cli_synth(cli_path, output_path, ["--preset", "basic-beat", "--bars", "1"] if plugin_name == "VC-Drum" else [])
    else:
        rc, _, _ = run_cli(cli_path, sine_path, output_path)
    if rc != 0:
        results.fail("Tier3: default parameter run", f"exit={rc}")
        return

    _, data = _safe_read(output_path)

    rms_val = rms_db(data)
    peak_val = peak_db(data)

    if rms_val > -60:
        results.ok("Tier3: RMS > -60dBFS (not silent)", f"RMS={rms_val:.2f} dBFS")
    else:
        results.fail("Tier3: RMS > -60dBFS (not silent)", f"RMS={rms_val:.2f} dBFS")

    if plugin_name == "VC-Limiter":
        if peak_val <= 0.0:
            results.ok("Tier3: peak <= 0dBFS (Limiter)", f"peak={peak_val:.2f} dBFS")
        else:
            results.fail("Tier3: peak <= 0dBFS (Limiter)",
                          f"peak={peak_val:.2f} dBFS (exceeds 0dBFS)")
    else:
        if peak_val < 12.0:
            results.ok("Tier3: peak in reasonable range", f"peak={peak_val:.2f} dBFS")
        else:
            results.fail("Tier3: peak in reasonable range",
                          f"peak={peak_val:.2f} dBFS (too high)")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <plugin_name> <cli_binary_path>")
        print(f"Example: {sys.argv[0]} VC-Gain ./VC-Gain/standalone-CLI")
        sys.exit(2)

    plugin_name = sys.argv[1]
    cli_path = os.path.abspath(sys.argv[2])

    if not os.path.isfile(cli_path):
        print(f"ERROR: CLI binary not found: {cli_path}")
        sys.exit(2)

    if not os.access(cli_path, os.X_OK):
        print(f"ERROR: CLI binary not executable: {cli_path}")
        sys.exit(2)

    print(f"{'='*60}")
    print(f"Testing: {plugin_name}")
    print(f"CLI:     {cli_path}")
    print(f"{'='*60}")

    results = TestResults()
    tmpdir = tempfile.mkdtemp(prefix=f"vc_test_{plugin_name}_")

    try:
        print(f"\n--- Tier 1: Runnable ---")
        tier1_ok = tier1_runnable(results, cli_path, tmpdir)

        if tier1_ok:
            print(f"\n--- Tier 2: Functional ---")
            tier2_functional(results, plugin_name, cli_path, tmpdir)

            print(f"\n--- Tier 3: Effect ---")
            tier3_effect(results, plugin_name, cli_path, tmpdir)
        else:
            print("\nTier 1 failed - skipping Tier 2 and 3")

        success = results.summary()
        sys.exit(0 if success else 1)

    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)


if __name__ == "__main__":
    main()