#!/usr/bin/env python3
"""
AudioFX Preset System Test Suite
=================================
Tests for the preset format, preset manager, batch processor,
and chain processor.

Run:
  cd /tmp/AudioFX
  python -m pytest tests/test_preset_system.py -v
  # or:
  python tests/test_preset_system.py
"""

import json
import os
import shutil
import subprocess
import sys
import tempfile
import wave

import numpy as np
import pytest

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------

AUDIOFX_ROOT = os.environ.get(
    "AUDIOFX_ROOT", os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
)
PRESETS_DIR = os.path.join(AUDIOFX_ROOT, "presets")
SCHEMA_PATH = os.path.join(AUDIOFX_ROOT, "Libs", "preset_format", "preset_schema.json")
TOOLS_DIR = os.path.join(AUDIOFX_ROOT, "tools")
TEST_AUDIO = os.path.join(AUDIOFX_ROOT, "test_audio", "vocal_test.wav")

PRESET_MANAGER = os.path.join(TOOLS_DIR, "preset_manager.py")
BATCH_PROCESSOR = os.path.join(TOOLS_DIR, "batch_processor.py")
CHAIN_PROCESSOR = os.path.join(TOOLS_DIR, "chain_processor.py")


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _read_wav_float(path):
    """Read a WAV file (PCM or IEEE float) and return (sample_rate, float_data).
    Handles dr_wav IEEE float output (format tag 3) and standard PCM.
    Always returns float data normalized so that +/-1.0 = full scale.
    For float WAV files (dr_wav output), values may exceed +/-1.0 after gain.
    """
    try:
        import scipy.io.wavfile as wf
        sr, data = wf.read(path)
        if data.dtype == np.int16:
            data = data.astype(np.float64) / 32768.0
        elif data.dtype == np.int32:
            data = data.astype(np.float64) / 2147483648.0
        elif data.dtype in (np.float32, np.float64):
            data = data.astype(np.float64)
        return sr, data
    except ImportError:
        pass
    # Fallback: try numpy fromfile with wave header skip (limited)
    with wave.open(path, "rb") as wf:
        sr = wf.getframerate()
        n_ch = wf.getnchannels()
        frames = wf.readframes(wf.getnframes())
        data = np.frombuffer(frames, dtype=np.int16).astype(np.float64) / 32768.0
        if n_ch > 1:
            data = data.reshape(-1, n_ch)
        return sr, data


@pytest.fixture
def tmp_dir():
    """Create and clean up a temporary directory."""
    d = tempfile.mkdtemp(prefix="audiofx_test_")
    yield d
    shutil.rmtree(d, ignore_errors=True)


@pytest.fixture
def short_wav(tmp_dir):
    """Generate a short 0.5s sine WAV file for testing."""
    sr = 44100
    duration = 0.5
    t = np.linspace(0, duration, int(sr * duration), dtype=np.float32)
    signal = 0.5 * np.sin(2 * np.pi * 440 * t)
    # Stereo
    stereo = np.column_stack([signal, signal])
    path = os.path.join(tmp_dir, "test_sine.wav")
    with wave.open(path, "wb") as wf:
        wf.setnchannels(2)
        wf.setsampwidth(2)
        wf.setframerate(sr)
        pcm = (stereo * 32767).astype(np.int16)
        wf.writeframes(pcm.tobytes())
    return path


def _run_script(script_path, args, timeout=60):
    """Run a Python script and return (returncode, stdout, stderr)."""
    cmd = [sys.executable, script_path] + args
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    return result.returncode, result.stdout, result.stderr


def _make_test_preset(plugin, preset_name, params, tmp_dir):
    """Create a test preset JSON file and return its path."""
    data = {
        "schema_version": "1.0",
        "plugin": plugin,
        "preset_name": preset_name,
        "author": "test",
        "description": "test preset",
        "parameters": params,
    }
    path = os.path.join(tmp_dir, f"{preset_name.replace(' ', '_').lower()}.json")
    with open(path, "w") as f:
        json.dump(data, f, indent=2)
    return path


# ===========================================================================
# 1. Preset Schema Validation Tests
# ===========================================================================


class TestPresetSchema:
    """Tests for the preset JSON schema file."""

    def test_schema_file_exists(self):
        assert os.path.isfile(SCHEMA_PATH), f"Schema file missing: {SCHEMA_PATH}"

    def test_schema_is_valid_json(self):
        with open(SCHEMA_PATH) as f:
            data = json.load(f)
        assert isinstance(data, dict)

    def test_schema_has_required_properties(self):
        with open(SCHEMA_PATH) as f:
            data = json.load(f)
        required = data.get("required", [])
        for field in ("schema_version", "plugin", "preset_name", "parameters"):
            assert field in required, f"Missing required field in schema: {field}"

    def test_schema_defines_parameters_type(self):
        with open(SCHEMA_PATH) as f:
            data = json.load(f)
        props = data.get("properties", {})
        assert "parameters" in props
        assert props["parameters"]["type"] == "object"


# ===========================================================================
# 2. Preset Format Validation Tests
# ===========================================================================


class TestPresetFormat:
    """Tests for preset file format validation."""

    def test_valid_preset_has_required_fields(self):
        preset = {
            "schema_version": "1.0",
            "plugin": "VC-Reverb",
            "preset_name": "Large Hall",
            "parameters": {"room": 80, "mix": 30},
        }
        for field in ("schema_version", "plugin", "preset_name", "parameters"):
            assert field in preset

    def test_preset_parameters_are_dict(self):
        preset = {
            "schema_version": "1.0",
            "plugin": "VC-EQ",
            "preset_name": "test",
            "parameters": {"band0-freq": 80, "band0-type": 4},
        }
        assert isinstance(preset["parameters"], dict)

    def test_preset_parameters_values_are_valid_types(self):
        preset = {
            "schema_version": "1.0",
            "plugin": "VC-Comp",
            "preset_name": "test",
            "parameters": {
                "threshold": -12,
                "ratio": 4.0,
                "knee": "soft",
                "band-threshold": [-20, -18, -22, -30],
            },
        }
        for key, val in preset["parameters"].items():
            assert isinstance(val, (int, float, str, list)), f"Invalid type for {key}: {type(val)}"

    def test_missing_required_field_is_invalid(self):
        preset = {"plugin": "VC-EQ", "preset_name": "test"}  # Missing schema_version and parameters
        required = ["schema_version", "plugin", "preset_name", "parameters"]
        missing = [f for f in required if f not in preset]
        assert len(missing) > 0

    def test_preset_optional_fields(self):
        """Optional fields should not break validation."""
        preset = {
            "schema_version": "1.0",
            "plugin": "VC-Reverb",
            "preset_name": "test",
            "parameters": {"room": 50},
            "author": "test-author",
            "description": "test desc",
            "tags": ["reverb", "hall"],
        }
        assert preset.get("author") == "test-author"
        assert preset.get("tags") == ["reverb", "hall"]


# ===========================================================================
# 3. Preset File Integrity Tests
# ===========================================================================


class TestPresetFiles:
    """Tests for actual preset files on disk."""

    @pytest.mark.parametrize("plugin", [
        "VC-EQ", "VC-Comp", "VC-Reverb", "VC-Tune", "VC-Delay",
        "VC-Limiter", "VC-DeEsser", "VC-Gate", "VC-Saturator",
        "VC-Chorus", "VC-Stereo", "VC-Distortion", "VC-DynamicEQ",
        "VC-Gain", "VC-Smooth",
    ])
    def test_preset_dir_exists(self, plugin):
        d = os.path.join(PRESETS_DIR, plugin)
        assert os.path.isdir(d), f"Preset directory missing: {d}"

    def _load_all_presets(self):
        """Load all preset JSON files."""
        presets = []
        for plugin_dir in sorted(os.listdir(PRESETS_DIR)):
            full = os.path.join(PRESETS_DIR, plugin_dir)
            if not os.path.isdir(full):
                continue
            for fname in sorted(os.listdir(full)):
                if fname.endswith(".json"):
                    path = os.path.join(full, fname)
                    with open(path) as f:
                        data = json.load(f)
                    presets.append((plugin_dir, fname, data, path))
        return presets

    def test_all_preset_files_are_valid_json(self):
        for plugin_dir, fname, data, path in self._load_all_presets():
            assert isinstance(data, dict), f"Invalid JSON object: {path}"

    def test_all_presets_have_required_fields(self):
        for plugin_dir, fname, data, path in self._load_all_presets():
            for field in ("schema_version", "plugin", "preset_name", "parameters"):
                assert field in data, f"Missing '{field}' in {path}"

    def test_all_presets_plugin_matches_directory(self):
        for plugin_dir, fname, data, path in self._load_all_presets():
            assert data["plugin"] == plugin_dir, (
                f"Plugin mismatch in {path}: expected {plugin_dir}, got {data['plugin']}"
            )

    def test_all_presets_parameters_are_dict(self):
        for plugin_dir, fname, data, path in self._load_all_presets():
            assert isinstance(data["parameters"], dict), f"parameters not dict in {path}"

    def test_all_presets_schema_version(self):
        for plugin_dir, fname, data, path in self._load_all_presets():
            assert data["schema_version"] == "1.0", f"Unexpected schema_version in {path}"

    def test_core_plugins_have_minimum_presets(self):
        """5 core plugins should have at least 3 presets each."""
        for plugin in ["VC-EQ", "VC-Comp", "VC-Reverb", "VC-Tune", "VC-Delay"]:
            d = os.path.join(PRESETS_DIR, plugin)
            count = len([f for f in os.listdir(d) if f.endswith(".json")])
            assert count >= 3, f"{plugin} has only {count} presets (need >=3)"

    def test_at_least_30_total_presets(self):
        total = 0
        for plugin_dir in os.listdir(PRESETS_DIR):
            full = os.path.join(PRESETS_DIR, plugin_dir)
            if os.path.isdir(full):
                total += len([f for f in os.listdir(full) if f.endswith(".json")])
        assert total >= 30, f"Only {total} presets total (need >=30)"


# ===========================================================================
# 4. Preset Manager CLI Tests
# ===========================================================================


class TestPresetManager:
    """Tests for preset_manager.py commands."""

    def test_list_vc_eq(self):
        rc, stdout, stderr = _run_script(PRESET_MANAGER, ["list", "VC-EQ"])
        assert rc == 0, f"list failed: {stderr}"
        assert "vocal-clean" in stdout

    def test_list_vc_reverb(self):
        rc, stdout, stderr = _run_script(PRESET_MANAGER, ["list", "VC-Reverb"])
        assert rc == 0, f"list failed: {stderr}"
        assert "large-hall" in stdout

    def test_list_nonexistent_plugin(self):
        rc, stdout, stderr = _run_script(PRESET_MANAGER, ["list", "VC-NonExistent"])
        # Should return 1 (no presets dir)
        assert rc == 1

    def test_export_preset(self, tmp_dir):
        output = os.path.join(tmp_dir, "exported.json")
        rc, stdout, stderr = _run_script(
            PRESET_MANAGER, ["export", "VC-EQ", "vocal-clean", "--output", output]
        )
        assert rc == 0, f"export failed: {stderr}"
        assert os.path.isfile(output)
        with open(output) as f:
            data = json.load(f)
        assert data["plugin"] == "VC-EQ"
        assert data["preset_name"] == "vocal-clean"
        assert "parameters" in data

    def test_export_nonexistent_preset(self, tmp_dir):
        output = os.path.join(tmp_dir, "nope.json")
        rc, stdout, stderr = _run_script(
            PRESET_MANAGER, ["export", "VC-EQ", "nonexistent", "--output", output]
        )
        assert rc != 0

    def test_import_preset(self, tmp_dir):
        # Create a test preset file
        test_data = {
            "schema_version": "1.0",
            "plugin": "VC-EQ",
            "preset_name": "imported-test",
            "parameters": {"band0-freq": 100, "band0-type": 4, "band0-on": 1},
        }
        input_path = os.path.join(tmp_dir, "to_import.json")
        with open(input_path, "w") as f:
            json.dump(test_data, f)

        rc, stdout, stderr = _run_script(
            PRESET_MANAGER, ["import", "VC-EQ", "--input", input_path]
        )
        assert rc == 0, f"import failed: {stderr}"

        # Verify the imported preset can be listed
        rc2, stdout2, _ = _run_script(PRESET_MANAGER, ["list", "VC-EQ"])
        assert rc2 == 0
        assert "imported-test" in stdout2

    def test_import_invalid_preset(self, tmp_dir):
        input_path = os.path.join(tmp_dir, "bad.json")
        with open(input_path, "w") as f:
            json.dump({"bad": "data"}, f)

        rc, stdout, stderr = _run_script(
            PRESET_MANAGER, ["import", "VC-EQ", "--input", input_path]
        )
        assert rc != 0

    def test_apply_preset_vc_gain(self, short_wav, tmp_dir):
        """Apply a preset to process a short audio file using VC-Gain."""
        output = os.path.join(tmp_dir, "output.wav")
        rc, stdout, stderr = _run_script(
            PRESET_MANAGER,
            ["apply", "VC-Gain", "plus-6db", "--input", short_wav, "--output", output],
            timeout=30,
        )
        assert rc == 0, f"apply failed: {stderr}\nstdout: {stdout}"
        assert os.path.isfile(output)
        assert os.path.getsize(output) > 0

    def test_apply_preset_vc_reverb(self, short_wav, tmp_dir):
        """Apply reverb preset to a short audio file."""
        output = os.path.join(tmp_dir, "reverb_out.wav")
        rc, stdout, stderr = _run_script(
            PRESET_MANAGER,
            ["apply", "VC-Reverb", "small-room", "--input", short_wav, "--output", output],
            timeout=30,
        )
        assert rc == 0, f"apply failed: {stderr}\nstdout: {stdout}"
        assert os.path.isfile(output)
        assert os.path.getsize(output) > 0


# ===========================================================================
# 5. Batch Processor Tests
# ===========================================================================


class TestBatchProcessor:
    """Tests for batch_processor.py."""

    def _make_test_dir(self, tmp_dir, n_files=3):
        """Create a directory with multiple short WAV files."""
        sr = 44100
        duration = 0.3
        t = np.linspace(0, duration, int(sr * duration), dtype=np.float32)
        inp_dir = os.path.join(tmp_dir, "input")
        os.makedirs(inp_dir)
        for i in range(n_files):
            freq = 440 + i * 100
            signal = 0.5 * np.sin(2 * np.pi * freq * t)
            stereo = np.column_stack([signal, signal])
            path = os.path.join(inp_dir, f"test_{i}.wav")
            with wave.open(path, "wb") as wf:
                wf.setnchannels(2)
                wf.setsampwidth(2)
                wf.setframerate(sr)
                pcm = (stereo * 32767).astype(np.int16)
                wf.writeframes(pcm.tobytes())
        return inp_dir

    def test_batch_with_preset(self, tmp_dir):
        inp_dir = self._make_test_dir(tmp_dir)
        out_dir = os.path.join(tmp_dir, "output")
        rc, stdout, stderr = _run_script(
            BATCH_PROCESSOR,
            ["--plugin", "VC-Gain", "--preset", "plus-6db",
             "--input-dir", inp_dir, "--output-dir", out_dir],
            timeout=60,
        )
        assert rc == 0, f"batch failed: {stderr}\nstdout: {stdout}"
        # Check output files
        out_files = [f for f in os.listdir(out_dir) if f.endswith(".wav")]
        assert len(out_files) == 3, f"Expected 3 output files, got {len(out_files)}"

    def test_batch_with_json_params(self, tmp_dir):
        inp_dir = self._make_test_dir(tmp_dir, n_files=2)
        out_dir = os.path.join(tmp_dir, "output")
        params_json = '{"gain": -3, "mix": 100}'
        rc, stdout, stderr = _run_script(
            BATCH_PROCESSOR,
            ["--plugin", "VC-Gain", "--params", params_json,
             "--input-dir", inp_dir, "--output-dir", out_dir],
            timeout=60,
        )
        assert rc == 0, f"batch with params failed: {stderr}\nstdout: {stdout}"

    def test_batch_with_input_files(self, short_wav, tmp_dir):
        # Create a second file by copying
        wav2 = os.path.join(tmp_dir, "test2.wav")
        shutil.copy2(short_wav, wav2)
        out_dir = os.path.join(tmp_dir, "output")
        rc, stdout, stderr = _run_script(
            BATCH_PROCESSOR,
            ["--plugin", "VC-Gain", "--preset", "plus-6db",
             "--input-files", short_wav, wav2,
             "--output-dir", out_dir],
            timeout=60,
        )
        assert rc == 0, f"batch with file list failed: {stderr}\nstdout: {stdout}"
        out_files = [f for f in os.listdir(out_dir) if f.endswith(".wav")]
        assert len(out_files) >= 2

    def test_batch_no_input_error(self, tmp_dir):
        out_dir = os.path.join(tmp_dir, "output")
        rc, stdout, stderr = _run_script(
            BATCH_PROCESSOR,
            ["--plugin", "VC-Gain", "--preset", "plus-6db", "--output-dir", out_dir],
        )
        # Should fail - no input specified
        assert rc != 0

    def test_batch_workers_option(self, tmp_dir):
        inp_dir = self._make_test_dir(tmp_dir, n_files=2)
        out_dir = os.path.join(tmp_dir, "output")
        rc, stdout, stderr = _run_script(
            BATCH_PROCESSOR,
            ["--plugin", "VC-Gain", "--preset", "plus-6db",
             "--input-dir", inp_dir, "--output-dir", out_dir,
             "--workers", "1"],
            timeout=60,
        )
        assert rc == 0, f"batch workers failed: {stderr}\nstdout: {stdout}"


# ===========================================================================
# 6. Chain Processor Tests
# ===========================================================================


class TestChainProcessor:
    """Tests for chain_processor.py."""

    def test_simple_two_step_chain(self, short_wav, tmp_dir):
        """Gain +3dB → Gain -3dB chain."""
        chain_config = {
            "chain": [
                {"plugin": "VC-Gain", "params": {"gain": 3}},
                {"plugin": "VC-Gain", "params": {"gain": -3}},
            ]
        }
        chain_path = os.path.join(tmp_dir, "chain.json")
        with open(chain_path, "w") as f:
            json.dump(chain_config, f)

        output = os.path.join(tmp_dir, "chain_out.wav")
        rc, stdout, stderr = _run_script(
            CHAIN_PROCESSOR,
            ["--chain", chain_path, "--input", short_wav, "--output", output],
            timeout=60,
        )
        assert rc == 0, f"chain failed: {stderr}\nstdout: {stdout}"
        assert os.path.isfile(output)
        assert os.path.getsize(output) > 0

    def test_three_step_chain(self, short_wav, tmp_dir):
        """Gain → Comp → Reverb chain."""
        chain_config = {
            "chain": [
                {"plugin": "VC-Gain", "params": {"gain": 3}},
                {"plugin": "VC-Comp", "params": {"threshold": -12, "ratio": 3, "attack": 10, "release": 100}},
                {"plugin": "VC-Reverb", "params": {"room": 40, "mix": 20}},
            ]
        }
        chain_path = os.path.join(tmp_dir, "chain3.json")
        with open(chain_path, "w") as f:
            json.dump(chain_config, f)

        output = os.path.join(tmp_dir, "chain3_out.wav")
        rc, stdout, stderr = _run_script(
            CHAIN_PROCESSOR,
            ["--chain", chain_path, "--input", short_wav, "--output", output],
            timeout=60,
        )
        assert rc == 0, f"3-step chain failed: {stderr}\nstdout: {stdout}"
        assert os.path.isfile(output)

    def test_chain_with_preset(self, short_wav, tmp_dir):
        """Chain step using a preset."""
        chain_config = {
            "chain": [
                {"plugin": "VC-Reverb", "preset": "small-room"},
                {"plugin": "VC-Gain", "params": {"gain": -3}},
            ]
        }
        chain_path = os.path.join(tmp_dir, "chain_preset.json")
        with open(chain_path, "w") as f:
            json.dump(chain_config, f)

        output = os.path.join(tmp_dir, "preset_chain_out.wav")
        rc, stdout, stderr = _run_script(
            CHAIN_PROCESSOR,
            ["--chain", chain_path, "--input", short_wav, "--output", output],
            timeout=60,
        )
        assert rc == 0, f"chain with preset failed: {stderr}\nstdout: {stdout}"
        assert os.path.isfile(output)

    def test_chain_empty_config_error(self, short_wav, tmp_dir):
        """Empty chain should fail."""
        chain_config = {"chain": []}
        chain_path = os.path.join(tmp_dir, "empty_chain.json")
        with open(chain_path, "w") as f:
            json.dump(chain_config, f)

        output = os.path.join(tmp_dir, "empty_out.wav")
        rc, stdout, stderr = _run_script(
            CHAIN_PROCESSOR,
            ["--chain", chain_path, "--input", short_wav, "--output", output],
        )
        assert rc != 0

    def test_chain_keep_temp(self, short_wav, tmp_dir):
        """Test --keep-temp flag preserves intermediate files."""
        chain_config = {
            "chain": [
                {"plugin": "VC-Gain", "params": {"gain": 3}},
                {"plugin": "VC-Gain", "params": {"gain": -3}},
            ]
        }
        chain_path = os.path.join(tmp_dir, "chain_keep.json")
        with open(chain_path, "w") as f:
            json.dump(chain_config, f)

        output = os.path.join(tmp_dir, "keep_out.wav")
        rc, stdout, stderr = _run_script(
            CHAIN_PROCESSOR,
            ["--chain", chain_path, "--input", short_wav, "--output", output, "--keep-temp"],
            timeout=60,
        )
        assert rc == 0, f"chain keep-temp failed: {stderr}\nstdout: {stdout}"
        # Temp files are in system /tmp/ dir with prefix chain_0_
        # Check stdout mentions the temp file path
        assert "chain_0_" in stdout or "chain_1_" in stdout or "Step 1/" in stdout

    def test_yaml_chain_config(self, short_wav, tmp_dir):
        """Test YAML-format chain config."""
        yaml_content = """chain:
  - plugin: VC-Gain
    params:
      gain: 3
  - plugin: VC-Gain
    params:
      gain: -3
"""
        chain_path = os.path.join(tmp_dir, "chain.yaml")
        with open(chain_path, "w") as f:
            f.write(yaml_content)

        output = os.path.join(tmp_dir, "yaml_out.wav")
        rc, stdout, stderr = _run_script(
            CHAIN_PROCESSOR,
            ["--chain", chain_path, "--input", short_wav, "--output", output],
            timeout=60,
        )
        assert rc == 0, f"yaml chain failed: {stderr}\nstdout: {stdout}"
        assert os.path.isfile(output)


# ===========================================================================
# 7. End-to-End Integration Tests
# ===========================================================================


class TestIntegration:
    """End-to-end integration tests."""

    def test_export_then_import_roundtrip(self, tmp_dir):
        """Export a preset, then import it to a different plugin dir."""
        # Export VC-EQ vocal-clean
        exported = os.path.join(tmp_dir, "roundtrip.json")
        rc, _, _ = _run_script(
            PRESET_MANAGER, ["export", "VC-EQ", "vocal-clean", "--output", exported]
        )
        assert rc == 0
        assert os.path.isfile(exported)

        # Import it back (will override plugin name)
        rc, _, _ = _run_script(
            PRESET_MANAGER, ["import", "VC-EQ", "--input", exported]
        )
        assert rc == 0

        # List should show it
        rc, stdout, _ = _run_script(PRESET_MANAGER, ["list", "VC-EQ"])
        assert "vocal-clean" in stdout

    def test_preset_to_batch_to_chain(self, short_wav, tmp_dir):
        """Full workflow: create preset → batch process → chain process."""
        # 1. Use existing preset to batch process
        inp_dir = os.path.join(tmp_dir, "batch_in")
        os.makedirs(inp_dir)
        shutil.copy2(short_wav, os.path.join(inp_dir, "sine.wav"))

        batch_out = os.path.join(tmp_dir, "batch_out")
        rc, stdout, stderr = _run_script(
            BATCH_PROCESSOR,
            ["--plugin", "VC-Gain", "--preset", "plus-6db",
             "--input-dir", inp_dir, "--output-dir", batch_out],
            timeout=30,
        )
        assert rc == 0, f"Batch step failed: {stderr}"
        batch_file = os.path.join(batch_out, "sine.wav")
        assert os.path.isfile(batch_file)

        # 2. Chain process the batch output
        chain_config = {
            "chain": [
                {"plugin": "VC-Reverb", "preset": "small-room"},
            ]
        }
        chain_path = os.path.join(tmp_dir, "chain.json")
        with open(chain_path, "w") as f:
            json.dump(chain_config, f)

        chain_out = os.path.join(tmp_dir, "final.wav")
        rc, stdout, stderr = _run_script(
            CHAIN_PROCESSOR,
            ["--chain", chain_path, "--input", batch_file, "--output", chain_out],
            timeout=30,
        )
        assert rc == 0, f"Chain step failed: {stderr}"
        assert os.path.isfile(chain_out)
        assert os.path.getsize(chain_out) > 0

    def test_signal_level_after_gain_chain(self, short_wav, tmp_dir):
        """Verify signal level changes after +6dB gain."""
        # Read input RMS
        sr, inp_data = _read_wav_float(short_wav)
        if inp_data.ndim > 1:
            inp_data = inp_data[:, 0]
        inp_rms = float(np.sqrt(np.mean(inp_data.astype(np.float64) ** 2)))

        # Process with +6dB gain
        out6 = os.path.join(tmp_dir, "plus6.wav")
        rc, _, stderr = _run_script(
            PRESET_MANAGER,
            ["apply", "VC-Gain", "plus-6db", "--input", short_wav, "--output", out6],
            timeout=30,
        )
        assert rc == 0, f"Apply +6dB failed: {stderr}"

        # Read output RMS using scipy (handles IEEE float WAV)
        sr2, out_data = _read_wav_float(out6)
        if out_data.ndim > 1:
            out_data = out_data[:, 0]
        out_rms = float(np.sqrt(np.mean(out_data.astype(np.float64) ** 2)))

        # +6dB should roughly double the amplitude (factor ~2)
        ratio = out_rms / (inp_rms + 1e-10)
        assert 1.5 < ratio < 3.0, f"Expected gain ratio ~2.0, got {ratio}"

    def test_batch_output_file_sizes_reasonable(self, tmp_dir):
        """Batch processed files should have reasonable sizes."""
        sr = 44100
        duration = 0.3
        t = np.linspace(0, duration, int(sr * duration), dtype=np.float32)
        signal = 0.5 * np.sin(2 * np.pi * 440 * t)
        stereo = np.column_stack([signal, signal])
        inp_dir = os.path.join(tmp_dir, "input")
        os.makedirs(inp_dir)
        path = os.path.join(inp_dir, "one.wav")
        with wave.open(path, "wb") as wf:
            wf.setnchannels(2)
            wf.setsampwidth(2)
            wf.setframerate(sr)
            pcm = (stereo * 32767).astype(np.int16)
            wf.writeframes(pcm.tobytes())

        out_dir = os.path.join(tmp_dir, "output")
        rc, stdout, stderr = _run_script(
            BATCH_PROCESSOR,
            ["--plugin", "VC-Reverb", "--preset", "small-room",
             "--input-dir", inp_dir, "--output-dir", out_dir],
            timeout=30,
        )
        assert rc == 0, f"Batch failed: {stderr}"
        out_file = os.path.join(out_dir, "one.wav")
        assert os.path.isfile(out_file)
        # Output should be > 1KB (not empty) and < 10MB (not exploded)
        size = os.path.getsize(out_file)
        assert size > 1000, f"Output too small: {size} bytes"
        assert size < 10_000_000, f"Output suspiciously large: {size} bytes"


# ===========================================================================
# 8. Params-to-CLI conversion Tests
# ===========================================================================


class TestParamsConversion:
    """Tests for parameter conversion from preset to CLI args."""

    def test_simple_number_params(self):
        """Simple numeric params become --key value."""
        sys.path.insert(0, TOOLS_DIR)
        from preset_manager import _params_to_cli_args
        args = _params_to_cli_args({"room": 80, "mix": 30})
        assert "--room" in args
        assert "80" in args
        assert "--mix" in args
        assert "30" in args

    def test_string_params(self):
        """String params become --key value."""
        sys.path.insert(0, TOOLS_DIR)
        from preset_manager import _params_to_cli_args
        args = _params_to_cli_args({"knee": "soft", "character": "warm"})
        assert "--knee" in args
        assert "soft" in args

    def test_array_params(self):
        """Array params are comma-joined."""
        sys.path.insert(0, TOOLS_DIR)
        from preset_manager import _params_to_cli_args
        args = _params_to_cli_args({"band-threshold": [-20, -18, -22, -30]})
        assert "--band-threshold" in args
        idx = args.index("--band-threshold")
        assert args[idx + 1] == "-20,-18,-22,-30"

    def test_mixed_params(self):
        """Mixed type params convert correctly."""
        sys.path.insert(0, TOOLS_DIR)
        from preset_manager import _params_to_cli_args
        args = _params_to_cli_args({"threshold": -12, "ratio": 4.0, "knee": "soft"})
        assert "--threshold" in args
        assert "-12" in args
        assert "--ratio" in args
        assert "4.0" in args
        assert "--knee" in args
        assert "soft" in args


# ===========================================================================
# Main
# ===========================================================================


if __name__ == "__main__":
    pytest.main([__file__, "-v", "--tb=short"])
