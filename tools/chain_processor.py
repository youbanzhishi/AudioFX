#!/usr/bin/env python3
"""
AudioFX Effect Chain Processor
================================
Process a single audio file through a chain of effects.

Usage:
  python chain_processor.py --chain chain_config.json --input vocal.wav --output processed.wav
  python chain_processor.py --chain chain_config.yaml --input vocal.wav --output processed.wav

Chain config format (JSON or YAML):
{
  "chain": [
    {"plugin": "VC-EQ", "params": {"band0-freq": 80, "band0-type": 4, "band0-on": 1}},
    {"plugin": "VC-Comp", "params": {"threshold": -12, "ratio": 4, "attack": 10, "release": 100}},
    {"plugin": "VC-Reverb", "params": {"room": 60, "mix": 30}}
  ]
}
"""

import argparse
import json
import os
import subprocess
import sys
import tempfile

AUDIOFX_ROOT = os.environ.get(
    "AUDIOFX_ROOT", os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
)
PRESETS_DIR = os.path.join(AUDIOFX_ROOT, "presets")


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _cli_binary(plugin: str) -> str:
    plugin_dir = os.path.join(AUDIOFX_ROOT, plugin)
    binary_name = f"{plugin}-CLI-Standalone"
    candidate = os.path.join(plugin_dir, binary_name)
    if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
        return candidate
    candidate = os.path.join(plugin_dir, "build", "CLI_Standalone", binary_name)
    if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
        return candidate
    raise FileNotFoundError(f"CLI binary not found for {plugin}")


def _load_chain_config(path: str) -> dict:
    """Load chain config from JSON or YAML file."""
    with open(path, "r", encoding="utf-8") as f:
        text = f.read()

    # Try JSON first
    try:
        return json.loads(text)
    except json.JSONDecodeError:
        pass

    # Try YAML (simple parser, no pyyaml dependency needed for basic cases)
    try:
        import yaml
        return yaml.safe_load(text)
    except ImportError:
        pass

    # Fallback: minimal YAML-like parser for chain configs
    return _parse_simple_yaml(text)


def _parse_simple_yaml(text: str) -> dict:
    """Minimal YAML parser for chain config format. Supports basic structure only."""
    result = {"chain": []}
    current_step = None
    in_params = False

    for line in text.split("\n"):
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue

        if stripped.startswith("- plugin:"):
            if current_step:
                result["chain"].append(current_step)
            plugin_name = stripped.split(":", 1)[1].strip()
            current_step = {"plugin": plugin_name, "params": {}}
            in_params = False
        elif stripped.startswith("params:"):
            in_params = True
        elif current_step and in_params and ":" in stripped:
            key_val = stripped.lstrip("- ").split(":", 1)
            key = key_val[0].strip()
            val_str = key_val[1].strip()
            # Try to parse value
            try:
                val = json.loads(val_str)
            except (json.JSONDecodeError, ValueError):
                val = val_str
            current_step["params"][key] = val

    if current_step:
        result["chain"].append(current_step)

    return result


def _params_to_cli_args(params: dict) -> list:
    args = []
    for key, value in params.items():
        args.append(f"--{key}")
        if isinstance(value, list):
            args.append(",".join(str(v) for v in value))
        else:
            args.append(str(value))
    return args


def _resolve_params(step: dict) -> dict:
    """Resolve params for a chain step, loading preset if specified."""
    params = {}
    if "preset" in step:
        plugin = step["plugin"]
        safe_name = step["preset"].replace(" ", "_").lower()
        path = os.path.join(PRESETS_DIR, plugin, f"{safe_name}.json")
        if os.path.isfile(path):
            with open(path, "r", encoding="utf-8") as f:
                data = json.load(f)
            params.update(data.get("parameters", {}))
        else:
            print(f"Warning: preset '{step['preset']}' not found for {plugin}", file=sys.stderr)
    if "params" in step:
        params.update(step["params"])
    return params


def _process_chain_step(binary: str, input_path: str, output_path: str, cli_args: list) -> bool:
    """Run a single chain step. Returns True on success."""
    cmd = [binary, input_path, output_path] + cli_args
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
    if result.returncode != 0:
        print(f"  Error: {result.stderr[:300]}", file=sys.stderr)
        return False
    return os.path.isfile(output_path)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------


def main():
    parser = argparse.ArgumentParser(description="AudioFX Effect Chain Processor")
    parser.add_argument("--chain", "-c", required=True, help="Chain config file (JSON or YAML)")
    parser.add_argument("--input", "-i", required=True, help="Input audio file")
    parser.add_argument("--output", "-o", required=True, help="Output audio file")
    parser.add_argument(
        "--keep-temp", action="store_true", help="Keep intermediate temp files for debugging"
    )

    args = parser.parse_args()

    # Load chain config
    config = _load_chain_config(args.chain)
    chain = config.get("chain", [])
    if not chain:
        print("Error: chain config has no steps", file=sys.stderr)
        return 1

    print(f"Effect Chain: {len(chain)} step(s)")
    for idx, step in enumerate(chain):
        plugin = step.get("plugin", "?")
        preset = step.get("preset", "")
        params = step.get("params", {})
        desc = f"  {idx + 1}. {plugin}"
        if preset:
            desc += f" (preset: {preset})"
        if params:
            desc += f" params={json.dumps(params)}"
        print(desc)
    print()

    # Process chain
    current_input = args.input
    temp_files = []

    try:
        for idx, step in enumerate(chain):
            plugin = step["plugin"]
            params = _resolve_params(step)
            cli_args = _params_to_cli_args(params)
            binary = _cli_binary(plugin)

            is_last = idx == len(chain) - 1
            if is_last:
                current_output = args.output
            else:
                # Create temp file for intermediate output
                fd, temp_path = tempfile.mkstemp(suffix=".wav", prefix=f"chain_{idx}_")
                os.close(fd)
                current_output = temp_path
                temp_files.append(temp_path)

            print(f"Step {idx + 1}/{len(chain)}: {plugin}...")
            success = _process_chain_step(binary, current_input, current_output, cli_args)
            if not success:
                print(f"Failed at step {idx + 1} ({plugin})", file=sys.stderr)
                return 1

            # Intermediate output becomes next input
            current_input = current_output

            if os.path.isfile(current_output):
                size = os.path.getsize(current_output)
                print(f"  → {current_output} ({size} bytes)")
            else:
                print(f"  → output not created!", file=sys.stderr)
                return 1

    finally:
        # Clean up temp files
        if not args.keep_temp:
            for tf in temp_files:
                if os.path.isfile(tf):
                    os.remove(tf)

    if os.path.isfile(args.output):
        size = os.path.getsize(args.output)
        print(f"\nChain complete: {args.output} ({size} bytes)")
        return 0
    else:
        print(f"\nChain failed: output not created", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
