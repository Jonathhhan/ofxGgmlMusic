#!/usr/bin/env python3
import argparse
import importlib.util
import json
import math
import os
from pathlib import Path
import platform
import sys
import wave


def _peak_abs(samples):
    if len(samples) == 0:
        return 0.0
    return float(max(abs(float(value)) for value in samples))


def _write_wav16(path, sample_rate, samples):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    peak = _peak_abs(samples)
    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(int(sample_rate))
        frames = bytearray()
        for sample in samples:
            value = max(-1.0, min(1.0, float(sample)))
            frames.extend(int(round(value * 32767.0)).to_bytes(2, "little", signed=True))
        output.writeframes(bytes(frames))
    return peak


def _write_manifest(args, sample_rate, duration_seconds, peak_abs):
    output_path = Path(args.output)
    manifest_path = Path(str(output_path) + ".json")
    history_path = output_path.parent / "ofxGgmlMusic-history.json"
    manifest = {
        "backend": "huggingface-musicgen",
        "backendFamily": "transformer",
        "prompt": args.prompt,
        "negativePrompt": args.negative_prompt,
        "style": "musicgen",
        "referenceAudioPath": "",
        "outputPath": str(output_path),
        "manifestPath": str(manifest_path),
        "historyPath": str(history_path),
        "midiPath": "",
        "chordMidiPath": "",
        "arrangementMidiPath": "",
        "seed": args.seed,
        "durationSeconds": duration_seconds,
        "sampleRate": int(sample_rate),
        "channels": 1,
        "peakAbs": peak_abs,
        "tempo": { "bpm": args.tempo, "confidence": 1 if args.tempo > 0 else 0 },
        "key": {
            "tonic": args.key,
            "mode": args.mode,
            "confidence": 1 if args.key or args.mode else 0
        },
        "beats": [],
        "chords": [],
        "sections": [
            {
                "name": "generated",
                "startSeconds": 0,
                "durationSeconds": duration_seconds,
                "confidence": 1
            }
        ],
        "loop": False,
        "targetStems": [],
        "stems": [],
        "references": [
            f"huggingface model: {args.model}",
            "runner: tools/musicgen_hf_runner.py"
        ]
    }
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    history = { "manifests": [] }
    if history_path.exists():
        try:
            history = json.loads(history_path.read_text(encoding="utf-8"))
        except json.JSONDecodeError:
            history = { "manifests": [] }
    manifests = [entry for entry in history.get("manifests", []) if entry != str(manifest_path)]
    manifests.insert(0, str(manifest_path))
    history_path.write_text(json.dumps({ "manifests": manifests[:32] }, indent=2) + "\n", encoding="utf-8")


def _load_dependencies():
    try:
        import numpy as np
        import torch
        from transformers import AutoProcessor, MusicgenForConditionalGeneration
        return np, torch, AutoProcessor, MusicgenForConditionalGeneration
    except ImportError as error:
        print(
            "Missing Python dependency for Hugging Face MusicGen. Install a local "
            "PyTorch/Transformers environment, for example: python -m pip install "
            "torch transformers numpy",
            file=sys.stderr,
        )
        raise error


def _dependency_probe():
    dependencies = {}
    for name in ["numpy", "torch", "transformers"]:
        dependencies[name] = importlib.util.find_spec(name) is not None
    return dependencies


def _write_smoke_status(args, status):
    if args.json:
        print(json.dumps(status, indent=2))
        return

    print("Hugging Face MusicGen smoke")
    print(f"passed: {str(status['passed']).lower()}")
    print(f"python: {status['pythonExecutable']}")
    print(f"model: {status['model']}")
    print(f"device: {status['device']}")
    print(f"loadModel: {str(status['loadModel']).lower()}")
    for name, present in status["dependencies"].items():
        print(f"{name}: {str(present).lower()}")
    if status["error"]:
        print(f"error: {status['error']}")


def _smoke_test(args):
    dependencies = _dependency_probe()
    missing = [name for name, present in dependencies.items() if not present]
    status = {
        "name": "ofxGgmlMusic Hugging Face MusicGen smoke",
        "passed": len(missing) == 0,
        "pythonExecutable": sys.executable,
        "pythonVersion": platform.python_version(),
        "model": args.model,
        "device": args.device,
        "loadModel": bool(args.load_model),
        "dependencies": dependencies,
        "missingDependencies": missing,
        "error": "",
    }

    if missing:
        status["error"] = "missing Python dependencies: " + ", ".join(missing)
        _write_smoke_status(args, status)
        return 0 if args.allow_missing_deps else 1

    if args.load_model:
        try:
            np, torch, AutoProcessor, MusicgenForConditionalGeneration = _load_dependencies()
            device = args.device
            if device == "auto":
                device = "cuda" if torch.cuda.is_available() else "cpu"
            status["device"] = device
            AutoProcessor.from_pretrained(args.model)
            MusicgenForConditionalGeneration.from_pretrained(args.model)
        except Exception as error:
            status["passed"] = False
            status["error"] = str(error)
            _write_smoke_status(args, status)
            return 1

    _write_smoke_status(args, status)
    return 0


def main():
    parser = argparse.ArgumentParser(description="Generate a WAV with Hugging Face Transformers MusicGen.")
    parser.add_argument("--prompt")
    parser.add_argument("--output")
    parser.add_argument("--model", default="facebook/musicgen-small")
    parser.add_argument("--negative-prompt", default="")
    parser.add_argument("--duration", type=float, default=8.0)
    parser.add_argument("--tempo", type=float, default=0.0)
    parser.add_argument("--key", default="")
    parser.add_argument("--mode", default="")
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--guidance", type=float, default=3.0)
    parser.add_argument("--max-new-tokens", type=int, default=0)
    parser.add_argument("--device", default="auto")
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--smoke-test", action="store_true")
    parser.add_argument("--load-model", action="store_true")
    parser.add_argument("--allow-missing-deps", action="store_true")
    args = parser.parse_args()

    if args.smoke_test:
        return _smoke_test(args)

    if not args.prompt:
        print("--prompt is required unless --smoke-test is set", file=sys.stderr)
        return 2
    if not args.output:
        print("--output is required unless --smoke-test is set", file=sys.stderr)
        return 2
    if args.duration <= 0:
        print("duration must be greater than zero", file=sys.stderr)
        return 2

    np, torch, AutoProcessor, MusicgenForConditionalGeneration = _load_dependencies()
    if args.seed >= 0:
        torch.manual_seed(args.seed)

    device = args.device
    if device == "auto":
        device = "cuda" if torch.cuda.is_available() else "cpu"

    processor = AutoProcessor.from_pretrained(args.model)
    model = MusicgenForConditionalGeneration.from_pretrained(args.model)
    model.to(device)

    inputs = processor(text=[args.prompt], padding=True, return_tensors="pt")
    inputs = { key: value.to(device) for key, value in inputs.items() }
    max_new_tokens = args.max_new_tokens
    if max_new_tokens <= 0:
        max_new_tokens = max(1, min(1503, int(math.ceil(args.duration * 50.0))))

    with torch.no_grad():
        audio_values = model.generate(
            **inputs,
            do_sample=True,
            guidance_scale=args.guidance,
            max_new_tokens=max_new_tokens,
        )

    sample_rate = int(model.config.audio_encoder.sampling_rate)
    audio = audio_values[0, 0].detach().cpu().float().numpy()
    target_samples = int(args.duration * sample_rate)
    if target_samples > 0 and len(audio) > target_samples:
        audio = audio[:target_samples]
    peak = _peak_abs(audio)
    if peak > 1.0:
        audio = audio / peak
        peak = _peak_abs(audio)
    peak = _write_wav16(args.output, sample_rate, audio)
    duration_seconds = float(len(audio)) / float(sample_rate) if sample_rate > 0 else 0.0
    _write_manifest(args, sample_rate, duration_seconds, peak)
    print(f"output: {args.output}")
    print(f"model: {args.model}")
    print(f"duration: {duration_seconds:.3f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
