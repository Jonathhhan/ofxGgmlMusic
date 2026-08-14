# Changelog

## Unreleased

- Made the ACE-Step ggml fork exclusively Music-owned. The setup script now
  always uses the `acestep.cpp` ggml submodule and removes the redundant
  `-UseCoreGgml`, `-BundledGgml`, and `-OfxGgmlCorePath` variants.
- Added a MusicGen/ACE-Step workflow planner for no-side-effect readiness,
  launch, generation, artifact, and environment command discovery.
- Added a soft generation readiness checker for optional MusicGen Python deps
  and ACE-Step server health.
- Added a combined generation workflow report writer for planning/readiness
  handoff and release evidence.
- Advertised generation workflow report and script entrypoints through the
  addon manifest for ecosystem tooling.
- Added `-UseManifestReportPath` so the workflow report writer can use the
  addon manifest's report path without repeating it at the command line.
- Included reproducible next commands in generation workflow report summaries.
- Preserved MusicGen HF tempo, key, mode, and negative-prompt metadata in the
  generated manifest while keeping the runner optional.

## 1.0.2 - 2026-06-03

- Added `ofxGgmlMusicExternalGenerationBackend` as the first explicit bridge
  boundary for local model-backed music generator executables.
- Added an external generation contract test that proves the bridge can launch
  a local generator and round-trip WAV, manifest, history, MIDI, and stem outputs.
- Added a user-facing external generation CLI plus an opt-in Hugging Face
  Transformers MusicGen runner profile.
- Allowed external model values to be model ids instead of only existing files
  when `requireModelPathExists` is disabled.
- Added draggable waveform scrubbing to the generation and AceStep examples.
- Made procedural backend sample narrowing explicit so example builds stay
  warning-clean on MSVC.

## 1.0.1 - 2026-05-12

- Added independent Music addon version metadata.
- Exposed version metadata through the public umbrella header.
- Documented the release checklist, release policy, and `v1.0.1` scope.
- Kept procedural prompt-to-music generation as the first testable generation
  path while model-backed bridges remain future work.

## 1.0.0

- Started `ofxGgmlMusic` as the companion addon for music generation, music
  embeddings, tempo, beat/downbeat, key/chord workflows, stems, and arrangement
  tools on top of `ofxGgmlCore`.
- Added music request/result types, generation request/result types, a
  procedural prompt-conditioned WAV backend, manifests, MIDI sidecars, stem
  exports, root-level examples, and a no-IDE procedural generation CLI.
