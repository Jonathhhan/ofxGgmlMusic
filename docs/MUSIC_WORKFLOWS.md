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
| MusicGen or ACE-Step command planning | `scripts\plan-generation-workflow.bat` |
| MusicGen or ACE-Step readiness | `scripts\check-generation-readiness.bat` |
| Planning plus readiness report | `scripts\write-generation-workflow-report.bat` |
| MusicGen runner profile | `scripts\generate-musicgen-hf.bat -DryRun` |
| Installed MusicGen Python probe | `scripts\generate-musicgen-hf.bat -SmokeTest -Json` |
| Optional MusicGen JSON readiness report | `scripts\generate-musicgen-hf.bat -SmokeTest -Json -AllowMissingDeps` |
| Installed MusicGen model-load probe | `scripts\generate-musicgen-hf.bat -SmokeTest -LoadModel -Json` |
| Local setup diagnosis | `scripts\doctor-music.bat` |
| Request/result/helper changes | `scripts\test-addon.bat` |

Use `scripts\plan-generation-workflow.*` as the first stop when comparing
MusicGen HF and ACE-Step on a machine. It emits no-side-effect text or JSON with
the relevant readiness probes, server setup/start commands, generation command,
environment variables, and local artifact paths. The planner is intentionally a
coordination helper; it does not download models, start ACE-Step, load
Transformers, or write generated audio.

Use `scripts\check-generation-readiness.*` after planning when you want a live
status snapshot. It probes the optional MusicGen Python package stack with
`-AllowMissingDeps` semantics and checks the ACE-Step `/health` endpoint. By
default it reports warnings without failing so development machines can lack
optional model stacks; pass `-Strict` when CI or release evidence should fail on
warnings.

Use `scripts\write-generation-workflow-report.*` when a handoff or release note
needs one artifact that contains both the no-side-effect plan and the current
readiness snapshot. It can write JSON to `-ReportPath`, emit JSON to stdout, or
fail under `-Strict` when readiness warnings are not acceptable.

`scripts\run-music-runtime-smoke.*` is intentionally model-free but generation
backed. It validates helper tests and doctor readiness, then runs the
`procedural-sketch` backend through the no-IDE generator and verifies WAV,
manifest, history, MIDI, and stem artifacts in a temp directory. Model-backed
MusicGen, diffusion, SampleRNN, and custom GGML generator checks should be
added only after their model paths, executables, outputs, and cleanup rules are
explicit.

The Hugging Face MusicGen probe is intentionally separate from generation. The
plain `-SmokeTest` path checks Python package availability for `numpy`, `torch`,
and `transformers` without loading a model or writing audio. Add `-LoadModel`
only when model downloads or local cache access are acceptable for that machine.
Use `-AllowMissingDeps` when automation should capture the JSON report without
failing on machines that intentionally lack the optional MusicGen stack.
Validation uses `scripts\test-musicgen-hf-smoke.ps1`, which exercises the JSON
contract when Python is available but does not make PyTorch or Transformers
required for this addon. The MusicGen generation wrapper forwards tempo, key,
mode, and negative-prompt metadata into the runner manifest so history,
inspection, and example reloads keep the full request context even when the
underlying model only consumes the positive prompt.

## Safe first tasks

Good early music-lane tasks are:

- documenting backend and artifact assumptions
- adding dry-run validation for optional external runners
- improving manifest/history/stem handoff docs
- clarifying when low-level audio helpers should move to `ofxGgmlAudio`
- keeping visual GAN work out of this lane

Avoid broadening runtime behavior until input assets, generated artifacts,
external executable or model expectations, and validation commands are explicit.
