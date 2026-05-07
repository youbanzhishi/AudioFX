#!/usr/bin/env python3
"""
AudioFX Batch Processor
========================
Process multiple audio files with a single plugin + preset/parameters.

Usage:
  python batch_processor.py --plugin VC-Comp --preset vocal-compress --input-dir ./vocals/ --output-dir ./processed/
  python batch_processor.py --plugin VC-Reverb --params '{"room":80,"mix":40}' --input-dir ./input/ --output-dir ./output/
  python batch_processor.py --plugin VC-EQ --preset vocal-clean --input-files a.wav b.wav c.wav --output-dir ./out/
"""

import argparse
import json
import os
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed

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


def _load_preset_params(plugin: str, preset_name: str) -> dict:
    safe_name = preset_name.replace(" ", "_").lower()
    path = os.path.join(PRESETS_DIR, plugin, f"{safe_name}.json")
    if not os.path.isfile(path):
        raise FileNotFoundError(f"Preset '{preset_name}' not found for {plugin} at {path}")
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)
    return data.get("parameters", {})


def _params_to_cli_args(params: dict) -> list:
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


def _collect_input_files(input_dir=None, input_files=None) -> list:
    """Collect list of input WAV files."""
    files = []
    if input_dir:
        for f in sorted(os.listdir(input_dir)):
            if f.lower().endswith((".wav", ".wave")):
                files.append(os.path.join(input_dir, f))
    if input_files:
        files.extend(input_files)
    return files


def _process_one(binary: str, input_path: str, output_path: str, cli_args: list) -> dict:
    """Process a single file. Returns dict with status info."""
    cmd = [binary, input_path, output_path] + cli_args
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
        if result.returncode != 0:
            return {"file": input_path, "status": "error", "detail": result.stderr[:200]}
        if not os.path.isfile(output_path):
            return {"file": input_path, "status": "error", "detail": "Output file not created"}
        size = os.path.getsize(output_path)
        return {"file": input_path, "status": "ok", "output": output_path, "size": size}
    except subprocess.TimeoutExpired:
        return {"file": input_path, "status": "error", "detail": "Timeout (120s)"}
    except Exception as e:
        return {"file": input_path, "status": "error", "detail": str(e)}


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------


def main():
    parser = argparse.ArgumentParser(description="AudioFX Batch Processor")
    parser.add_argument("--plugin", required=True, help="Plugin name, e.g. VC-Comp")
    parser.add_argument("--preset", default=None, help="Preset name to use")
    parser.add_argument("--params", default=None, help="JSON string of parameters")
    parser.add_argument("--input-dir", default=None, help="Input directory with WAV files")
    parser.add_argument("--input-files", nargs="+", default=None, help="Input file paths")
    parser.add_argument("--output-dir", required=True, help="Output directory")
    parser.add_argument("--workers", type=int, default=4, help="Parallel workers (default: 4)")

    args = parser.parse_args()

    if not args.input_dir and not args.input_files:
        parser.error("Must specify --input-dir or --input-files")

    # Resolve parameters
    params = {}
    if args.preset:
        params.update(_load_preset_params(args.plugin, args.preset))
    if args.params:
        params.update(json.loads(args.params))

    cli_args = _params_to_cli_args(params)
    binary = _cli_binary(args.plugin)
    input_files = _collect_input_files(args.input_dir, args.input_files)

    if not input_files:
        print("No input files found.")
        return 1

    os.makedirs(args.output_dir, exist_ok=True)

    print(f"Batch processing {len(input_files)} file(s) with {args.plugin}")
    print(f"  Parameters: {json.dumps(params)}")
    print(f"  Workers: {args.workers}")
    print()

    # Build task list
    tasks = []
    for inp in input_files:
        out_name = os.path.splitext(os.path.basename(inp))[0] + ".wav"
        out_path = os.path.join(args.output_dir, out_name)
        tasks.append((inp, out_path))

    # Process in parallel
    ok_count = 0
    err_count = 0
    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        futures = {
            pool.submit(_process_one, binary, inp, outp, cli_args): (inp, outp)
            for inp, outp in tasks
        }
        done_count = 0
        for fut in as_completed(futures):
            done_count += 1
            result = fut.result()
            status = result["status"]
            if status == "ok":
                ok_count += 1
                print(f"  [{done_count}/{len(tasks)}] ✓ {os.path.basename(result['file'])} → {result['output']} ({result['size']} bytes)")
            else:
                err_count += 1
                print(f"  [{done_count}/{len(tasks)}] ✗ {os.path.basename(result['file'])}: {result['detail']}")

    print()
    print(f"Done: {ok_count} succeeded, {err_count} failed out of {len(tasks)}")
    return 1 if err_count > 0 else 0


if __name__ == "__main__":
    sys.exit(main())
