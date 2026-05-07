#!/usr/bin/env python3
"""
Shared pytest fixtures for AudioFX DSP validation tests.
"""

import pytest
import os
import tempfile
import shutil
import subprocess
import numpy as np

# Import generators from same directory
import sys
sys.path.insert(0, os.path.dirname(__file__))
from generators import (
    generate_sine, generate_sweep, generate_white_noise, generate_pink_noise,
    generate_impulse, generate_multitone, generate_stereo_sine,
    generate_stereo_signal_from_mono, save_wav, read_wav, SAMPLE_RATE
)

# Base path for AudioFX repo
AUDIOFX_ROOT = os.environ.get("AUDIOFX_ROOT", "/tmp/AudioFX")

# All 23 plugins: 20 effects + 3 instruments
ALL_EFFECTS = [
    "VC-EQ", "VC-Comp", "VC-Reverb", "VC-Delay", "VC-Chorus",
    "VC-Saturator", "VC-Gate", "VC-Limiter", "VC-DeEsser", "VC-PitchShift",
    "VC-Harmonizer", "VC-MultiBand", "VC-DynamicEQ", "VC-Smooth",
    "VC-SurgicalDeEsser", "VC-Tune", "VC-Gain", "VC-Noise",
    "VC-Stereo", "VC-Distortion",
]

INSTRUMENTS = ["VC-Synth", "VC-Drum", "VC-Arp"]

ALL_PLUGINS = ALL_EFFECTS + INSTRUMENTS

# Plugins that take input WAV (effects) vs output-only (instruments/generators)
GENERATOR_PLUGINS = {"VC-Synth", "VC-Drum", "VC-Arp", "VC-Noise"}


def get_cli_path(plugin_name: str) -> str:
    """Get the CLI Standalone binary path for a plugin."""
    return os.path.join(AUDIOFX_ROOT, plugin_name, f"{plugin_name}-CLI-Standalone")


def cli_exists(plugin_name: str) -> bool:
    """Check if CLI binary exists for a plugin."""
    return os.path.isfile(get_cli_path(plugin_name))


def run_cli(plugin_name: str, args: list, timeout: int = 30) -> subprocess.CompletedProcess:
    """Run a plugin CLI with given arguments.

    Args:
        plugin_name: e.g. "VC-Gain"
        args: list of CLI arguments
        timeout: process timeout in seconds

    Returns:
        CompletedProcess instance
    """
    cli = get_cli_path(plugin_name)
    cmd = [cli] + args
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    return result


def process_effect(plugin_name: str, input_path: str, output_path: str,
                   extra_args: list = None, timeout: int = 30) -> subprocess.CompletedProcess:
    """Process a WAV file through an effect plugin CLI.

    Args:
        plugin_name: e.g. "VC-Gain"
        input_path: path to input WAV
        output_path: path to output WAV
        extra_args: additional CLI arguments
        timeout: process timeout in seconds

    Returns:
        CompletedProcess instance
    """
    args = [input_path, output_path]
    if extra_args:
        args.extend(extra_args)
    return run_cli(plugin_name, args, timeout)


@pytest.fixture(scope="session")
def tmp_dir():
    """Create a temporary directory for test audio files (session-scoped)."""
    d = tempfile.mkdtemp(prefix="audiofx_test_")
    yield d
    shutil.rmtree(d, ignore_errors=True)


@pytest.fixture
def work_dir(tmp_path):
    """Per-test temporary directory."""
    return tmp_path


@pytest.fixture(scope="session")
def sine_1k(tmp_dir):
    """1kHz sine wave, 2 seconds, mono."""
    path = os.path.join(tmp_dir, "sine_1k.wav")
    sig = generate_sine(1000, 2.0, amplitude=0.8)
    save_wav(sig, path)
    return path


@pytest.fixture(scope="session")
def sine_440(tmp_dir):
    """440Hz sine wave, 2 seconds, mono."""
    path = os.path.join(tmp_dir, "sine_440.wav")
    sig = generate_sine(440, 2.0, amplitude=0.8)
    save_wav(sig, path)
    return path


@pytest.fixture(scope="session")
def sweep_20_20k(tmp_dir):
    """Log sweep 20Hz to 20kHz, 3 seconds, mono."""
    path = os.path.join(tmp_dir, "sweep_20_20k.wav")
    sig = generate_sweep(20, 20000, 3.0, amplitude=0.8)
    save_wav(sig, path)
    return path


@pytest.fixture(scope="session")
def white_noise(tmp_dir):
    """White noise, 2 seconds, mono."""
    path = os.path.join(tmp_dir, "white_noise.wav")
    sig = generate_white_noise(2.0, amplitude=0.5)
    save_wav(sig, path)
    return path


@pytest.fixture(scope="session")
def pink_noise(tmp_dir):
    """Pink noise, 2 seconds, mono."""
    path = os.path.join(tmp_dir, "pink_noise.wav")
    sig = generate_pink_noise(2.0, amplitude=0.5)
    save_wav(sig, path)
    return path


@pytest.fixture(scope="session")
def impulse(tmp_dir):
    """Impulse response, 2 seconds, mono."""
    path = os.path.join(tmp_dir, "impulse.wav")
    sig = generate_impulse(2.0, amplitude=1.0)
    save_wav(sig, path)
    return path


@pytest.fixture(scope="session")
def multitone(tmp_dir):
    """Multi-tone (100, 500, 1000, 5000 Hz), 2 seconds, mono."""
    path = os.path.join(tmp_dir, "multitone.wav")
    sig = generate_multitone([100, 500, 1000, 5000], 2.0, amplitude=0.5)
    save_wav(sig, path)
    return path


@pytest.fixture(scope="session")
def stereo_sine(tmp_dir):
    """Stereo sine (440Hz left, 880Hz right), 2 seconds."""
    path = os.path.join(tmp_dir, "stereo_sine.wav")
    sig = generate_stereo_sine(440, 880, 2.0, amplitude=0.8)
    save_wav(sig, path)
    return path


@pytest.fixture(scope="session")
def sine_1k_stereo(tmp_dir):
    """Stereo 1kHz sine (both channels identical), 2 seconds."""
    path = os.path.join(tmp_dir, "sine_1k_stereo.wav")
    mono = generate_sine(1000, 2.0, amplitude=0.8)
    stereo = generate_stereo_signal_from_mono(mono)
    save_wav(stereo, path)
    return path


@pytest.fixture(scope="session")
def sweep_stereo(tmp_dir):
    """Stereo log sweep 20Hz-20kHz, 3 seconds."""
    path = os.path.join(tmp_dir, "sweep_stereo.wav")
    mono = generate_sweep(20, 20000, 3.0, amplitude=0.8)
    stereo = generate_stereo_signal_from_mono(mono)
    save_wav(stereo, path)
    return path


@pytest.fixture(scope="session")
def noisy_sine(tmp_dir):
    """1kHz sine + white noise (SNR ~12dB), 2 seconds, mono."""
    path = os.path.join(tmp_dir, "noisy_sine.wav")
    sine = generate_sine(1000, 2.0, amplitude=0.8)
    noise = generate_white_noise(2.0, amplitude=0.2)
    sig = sine + noise
    # Clip to prevent exceeding 1.0
    sig = np.clip(sig, -1.0, 1.0)
    save_wav(sig.astype(np.float32), path)
    return path


def process_and_read(plugin_name, input_path, output_path, extra_args=None, timeout=30):
    """Helper: process effect and read output WAV. Returns (sr, data) or raises."""
    result = process_effect(plugin_name, input_path, output_path, extra_args, timeout)
    if result.returncode != 0:
        raise RuntimeError(f"CLI failed: {result.stderr}")
    return read_wav(output_path)
