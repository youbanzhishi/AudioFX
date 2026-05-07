#!/usr/bin/env python3
"""
Test Signal Generators for AudioFX DSP Validation
===================================================
Generates various test audio signals as WAV files for feeding into
VC plugin CLI Standalone binaries.
"""

import numpy as np
import soundfile as sf
import os
import tempfile


SAMPLE_RATE = 44100


def generate_sine(freq: float, duration: float, sample_rate: int = SAMPLE_RATE,
                  amplitude: float = 0.8) -> np.ndarray:
    """Generate a mono sine wave signal."""
    t = np.linspace(0, duration, int(sample_rate * duration), endpoint=False, dtype=np.float64)
    signal = amplitude * np.sin(2 * np.pi * freq * t)
    return signal.astype(np.float32)


def generate_sweep(f_start: float, f_end: float, duration: float,
                   sample_rate: int = SAMPLE_RATE, amplitude: float = 0.8,
                   logarithmic: bool = True) -> np.ndarray:
    """Generate a frequency sweep (chirp) signal."""
    t = np.linspace(0, duration, int(sample_rate * duration), endpoint=False, dtype=np.float64)
    if logarithmic:
        k = f_end / f_start
        phase = 2 * np.pi * f_start * duration / np.log(k) * (np.power(k, t / duration) - 1)
    else:
        phase = 2 * np.pi * (f_start * t + (f_end - f_start) / (2 * duration) * t ** 2)
    signal = amplitude * np.sin(phase)
    return signal.astype(np.float32)


def generate_white_noise(duration: float, sample_rate: int = SAMPLE_RATE,
                         amplitude: float = 0.5) -> np.ndarray:
    """Generate white noise signal (fixed seed for reproducibility)."""
    n_samples = int(sample_rate * duration)
    rng = np.random.default_rng(42)
    noise = rng.standard_normal(n_samples).astype(np.float32)
    current_rms = np.sqrt(np.mean(noise ** 2))
    if current_rms > 0:
        noise = noise * (amplitude / current_rms)
    return noise


def generate_pink_noise(duration: float, sample_rate: int = SAMPLE_RATE,
                        amplitude: float = 0.5) -> np.ndarray:
    """Generate pink noise (1/f noise) using filtered white noise."""
    n_samples = int(sample_rate * duration)
    rng = np.random.default_rng(42)
    white = rng.standard_normal(n_samples).astype(np.float64)
    b = [0.049922035, -0.095993537, 0.050612699, -0.004709510]
    a = [1.0, -2.494956002, 2.017265875, -0.522189400]
    from scipy.signal import lfilter
    pink = lfilter(b, a, white)
    current_rms = np.sqrt(np.mean(pink ** 2))
    if current_rms > 0:
        pink = pink * (amplitude / current_rms)
    return pink.astype(np.float32)


def generate_impulse(duration: float, sample_rate: int = SAMPLE_RATE,
                     amplitude: float = 1.0) -> np.ndarray:
    """Generate an impulse (Dirac delta) signal."""
    n_samples = int(sample_rate * duration)
    signal = np.zeros(n_samples, dtype=np.float32)
    signal[0] = amplitude
    return signal


def generate_multitone(freqs: list, duration: float, sample_rate: int = SAMPLE_RATE,
                       amplitude: float = 0.5) -> np.ndarray:
    """Generate a multi-frequency signal (sum of sines)."""
    t = np.linspace(0, duration, int(sample_rate * duration), endpoint=False, dtype=np.float64)
    signal = np.zeros_like(t)
    per_amp = amplitude / len(freqs)
    for f in freqs:
        signal += per_amp * np.sin(2 * np.pi * f * t)
    return signal.astype(np.float32)


def generate_stereo_sine(freq_l: float, freq_r: float, duration: float,
                         sample_rate: int = SAMPLE_RATE,
                         amplitude: float = 0.8) -> np.ndarray:
    """Generate a stereo sine wave with different frequencies per channel."""
    left = generate_sine(freq_l, duration, sample_rate, amplitude)
    right = generate_sine(freq_r, duration, sample_rate, amplitude)
    return np.column_stack([left, right])


def generate_stereo_signal_from_mono(mono: np.ndarray) -> np.ndarray:
    """Convert mono signal to stereo by duplicating."""
    return np.column_stack([mono, mono])


def save_wav(signal: np.ndarray, path: str, sample_rate: int = SAMPLE_RATE) -> str:
    """Save a signal to WAV file (FLOAT subtype for precision)."""
    sf.write(path, signal, sample_rate, subtype='FLOAT')
    return path


def read_wav(path: str):
    """Read a WAV file and return (sample_rate, data).

    Handles both float and integer WAV formats.
    Returns float32 data normalized to [-1, 1].
    """
    data, sr = sf.read(path, dtype='float32')
    return sr, data
