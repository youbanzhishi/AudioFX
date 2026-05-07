#!/usr/bin/env python3
"""
AudioFX Preset Manager CLI
==========================
Unified preset management for all VC-Series audio plugins.

Usage:
  python preset_manager.py list <plugin>
  python preset_manager.py export <plugin> <preset_name> --output <file>
  python preset_manager.py import <plugin> --input <file>
  python preset_manager.py apply <plugin> <preset_name> --input <audio> --output <audio>
"""

import argparse
import json
import os
import shutil
import subprocess
import sys

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

AUDIOFX_ROOT = os.environ.get(
    "AUDIOFX_ROOT", os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
)
PRESETS_DIR = os.path.join(AUDIOFX_ROOT, "presets")
SCHEMA_VERSION = "1.0"


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _preset_dir(plugin: str) -> str:
    """Return the preset directory for a given plugin."""
    d = os.path.join(PRESETS_DIR, plugin)
    return d


def _preset_path(plugin: str, preset_name: str) -> str:
    """Return the full path for a preset JSON file."""
    safe_name = preset_name.replace(" ", "_").lower()
    return os.path.join(_preset_dir(plugin), f"{safe_name}.json")


def _cli_binary(plugin: str) -> str:
    """Find the CLI standalone binary for a plugin."""
    plugin_dir = os.path.join(AUDIOFX_ROOT, plugin)
    binary_name = f"{plugin}-CLI-Standalone"
    # Check plugin root dir first
    candidate = os.path.join(plugin_dir, binary_name)
    if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
        return candidate
    # Check build subdir
    candidate = os.path.join(plugin_dir, "build", "CLI_Standalone", binary_name)
    if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
        return candidate
    raise FileNotFoundError(f"CLI binary not found for {plugin}: tried {binary_name}")


def _validate_preset(data: dict) -> list:
    """Validate a preset dict. Returns list of errors (empty = valid)."""
    errors = []
    for field in ("schema_version", "plugin", "preset_name", "parameters"):
        if field not in data:
            errors.append(f"Missing required field: {field}")
    if "parameters" in data and not isinstance(data["parameters"], dict):
        errors.append("'parameters' must be an object")
    if "schema_version" in data and not isinstance(data["schema_version"], str):
        errors.append("'schema_version' must be a string")
    return errors


def _load_preset(plugin: str, preset_name: str) -> dict:
    """Load a preset file. Raises FileNotFoundError if not found."""
    path = _preset_path(plugin, preset_name)
    if not os.path.isfile(path):
        raise FileNotFoundError(f"Preset '{preset_name}' not found for {plugin} at {path}")
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)
    errors = _validate_preset(data)
    if errors:
        raise ValueError(f"Invalid preset file {path}: {'; '.join(errors)}")
    return data


def _params_to_cli_args(params: dict) -> list:
    """Convert preset parameters dict to CLI argument list.

    Keys become --key, values become the next argument.
    Array values are comma-joined.
    Note: bypass=0 is skipped because VC CLI treats --bypass 0 as "bypass ON".
    """
    args = []
    for key, value in params.items():
        # Skip bypass=0: VC CLI bug where --bypass 0 enables bypass
        if key == "bypass" and value == 0:
            continue
        args.append(f"--{key}")
        if isinstance(value, list):
            args.append(",".join(str(v) for v in value))
        else:
            args.append(str(value))
    return args


# ---------------------------------------------------------------------------
# Commands
# ---------------------------------------------------------------------------


def cmd_list(plugin: str) -> int:
    """List all presets for a plugin."""
    d = _preset_dir(plugin)
    if not os.path.isdir(d):
        print(f"No presets directory for {plugin} (expected: {d})")
        return 1
    presets = sorted(f for f in os.listdir(d) if f.endswith(".json"))
    if not presets:
        print(f"No presets found for {plugin}")
        return 0
    print(f"Presets for {plugin}:")
    for pf in presets:
        path = os.path.join(d, pf)
        try:
            with open(path, "r", encoding="utf-8") as f:
                data = json.load(f)
            name = data.get("preset_name", pf[:-5])
            desc = data.get("description", "")
            print(f"  {name:30s}  {desc}")
        except (json.JSONDecodeError, OSError) as e:
            print(f"  {pf:30s}  [error: {e}]")
    return 0


def cmd_export(plugin: str, preset_name: str, output: str) -> int:
    """Export a preset to a JSON file."""
    data = _load_preset(plugin, preset_name)
    out_dir = os.path.dirname(os.path.abspath(output))
    os.makedirs(out_dir, exist_ok=True)
    with open(output, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, ensure_ascii=False)
    print(f"Exported preset '{preset_name}' for {plugin} → {output}")
    return 0


def cmd_import(plugin: str, input_file: str) -> int:
    """Import a preset from a JSON file."""
    with open(input_file, "r", encoding="utf-8") as f:
        data = json.load(f)
    errors = _validate_preset(data)
    if errors:
        print(f"Invalid preset file: {'; '.join(errors)}")
        return 1
    # Override plugin name to match target
    data["plugin"] = plugin
    # Save to preset dir
    preset_name = data.get("preset_name", os.path.splitext(os.path.basename(input_file))[0])
    path = _preset_path(plugin, preset_name)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, ensure_ascii=False)
    print(f"Imported preset '{preset_name}' for {plugin} → {path}")
    return 0


def cmd_apply(plugin: str, preset_name: str, input_audio: str, output_audio: str) -> int:
    """Apply a preset to process an audio file."""
    data = _load_preset(plugin, preset_name)
    binary = _cli_binary(plugin)
    cli_args = _params_to_cli_args(data["parameters"])
    cmd = [binary, input_audio, output_audio] + cli_args
    print(f"Running: {' '.join(cmd)}")
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
    if result.returncode != 0:
        print(f"CLI error (exit {result.returncode}): {result.stderr}", file=sys.stderr)
        return result.returncode
    if result.stdout.strip():
        print(result.stdout)
    if os.path.isfile(output_audio):
        size = os.path.getsize(output_audio)
        print(f"Output: {output_audio} ({size} bytes)")
    else:
        print(f"Warning: output file not created: {output_audio}", file=sys.stderr)
        return 1
    return 0


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------


def main():
    parser = argparse.ArgumentParser(
        description="AudioFX Preset Manager - Unified preset management for VC plugins"
    )
    sub = parser.add_subparsers(dest="command", required=True)

    # list
    p_list = sub.add_parser("list", help="List presets for a plugin")
    p_list.add_argument("plugin", help="Plugin name, e.g. VC-Reverb")

    # export
    p_export = sub.add_parser("export", help="Export a preset to JSON file")
    p_export.add_argument("plugin", help="Plugin name")
    p_export.add_argument("preset_name", help="Preset name to export")
    p_export.add_argument("--output", "-o", required=True, help="Output JSON file path")

    # import
    p_import = sub.add_parser("import", help="Import a preset from JSON file")
    p_import.add_argument("plugin", help="Plugin name")
    p_import.add_argument("--input", "-i", required=True, help="Input JSON file path")

    # apply
    p_apply = sub.add_parser("apply", help="Apply a preset to process audio")
    p_apply.add_argument("plugin", help="Plugin name")
    p_apply.add_argument("preset_name", help="Preset name to apply")
    p_apply.add_argument("--input", "-i", required=True, help="Input audio file")
    p_apply.add_argument("--output", "-o", required=True, help="Output audio file")

    args = parser.parse_args()

    if args.command == "list":
        return cmd_list(args.plugin)
    elif args.command == "export":
        return cmd_export(args.plugin, args.preset_name, args.output)
    elif args.command == "import":
        return cmd_import(args.plugin, args.input)
    elif args.command == "apply":
        return cmd_apply(args.plugin, args.preset_name, args.input, args.output)
    return 1


if __name__ == "__main__":
    sys.exit(main())
