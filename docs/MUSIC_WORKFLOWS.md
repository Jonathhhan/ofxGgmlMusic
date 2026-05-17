# Music Workflow Boundaries

`ofxGgmlMusic` owns music generation, music analysis, arrangement, stems, and
music-specific model handoff workflows for the ofxGgml ecosystem. This document
is for Codex, GitHub Copilot, Hermes Agent, and human contributors planning
music-lane work before changing runtime behavior.

This guide follows the split rule from the legacy/reference `ofxGgml` docs:
domain workflows, model-specific preprocessing, generated media, and heavy
optional dependencies belong in companion addons. Shared code should move down
only when it is stable, domain-neutral, dependency-light, and covered by
focused tests.

## Owned workflow surface

This addon may define:

- music-specific request/result shapes for tempo, beat, downbeat, key, chord,
  stems, embeddings, arrangement, and generation
- prompt-to-music, loop, stem-targeted, and reference-audio generation plans
- procedural-sketch generation used for deterministic smoke tests
- external generator bridge contracts for local MusicGen-style, audio
  diffusion, transformer, SampleRNN, or custom GGML tools
- Hugging Face MusicGen runner profiles as opt-in local tooling
- music generation manifests, history indexes, MIDI sidecars, and stem exports
- focused music analysis and generation examples

## Not owned here

Keep these responsibilities out of `ofxGgmlMusic`:

- ggml setup, backend selection, and runtime discovery owned by `ofxGgmlCore`
- generic audio stream chunking, VAD, PCM plumbing, or low-level feature
  primitives owned by `ofxGgmlAudio`
- image/video GAN or visual diffusion workflows owned by `ofxGgmlDiffusion`
  and `ofxGgmlVideo`
- agent orchestration loops owned by `ofxGgmlAgents`
- committed generated WAVs, stems, MIDI files, manifests, model files, Python
  environments, build output, or generated openFrameworks project files
- reusable GitHub Actions policy owned by `ofxGgmlWorkflows`

## Planning handoff

Before changing music behavior, write down:

```text
Workflow:
Backend family:
Input assets:
Generated local artifacts:
External executable or model:
User-visible output:
Out of scope:
Validation:
```

Runtime changes should name whether the path uses procedural-sketch,
external-generator, MusicGen, audio diffusion, transformer, SampleRNN, or custom
GGML tooling, and should identify which generated artifacts are expected.

## Validation ladder

Use the smallest command that proves the changed layer:

| Change type | Suggested validation |
| --- | --- |
| Docs or planning only | `scripts\validate-local.bat` |
| Procedural generation path | `scripts\generate-procedural-music.bat` |
| Ecosystem runtime smoke evidence | `scripts\run-music-runtime-smoke.bat -Json -SummaryOnly -Clean` |
| AceStep runtime setup | `scripts\test-acestep-setup-dry-run.ps1` |
| AceStep server launch contract | `scripts\test-acestep-server-dry-run.ps1` |
| External generator bridge | `scripts\test-external-generation-contract.bat -Clean` |
| MusicGen runner profile | `scripts\generate-musicgen-hf.bat -DryRun` |
| Local setup diagnosis | `scripts\doctor-music.bat` |
| Request/result/helper changes | `scripts\test-addon.bat` |

`scripts\run-music-runtime-smoke.*` is intentionally model-free but generation
backed. It validates helper tests and doctor readiness, then runs the
`procedural-sketch` backend through the no-IDE generator and verifies WAV,
manifest, history, MIDI, and stem artifacts in a temp directory. Model-backed
MusicGen, diffusion, SampleRNN, and custom GGML generator checks should be
added only after their model paths, executables, outputs, and cleanup rules are
explicit.

## Safe first tasks

Good early music-lane tasks are:

- documenting backend and artifact assumptions
- adding dry-run validation for optional external runners
- improving manifest/history/stem handoff docs
- clarifying when low-level audio helpers should move to `ofxGgmlAudio`
- keeping visual GAN work out of this lane

Avoid broadening runtime behavior until input assets, generated artifacts,
external executable or model expectations, and validation commands are explicit.
