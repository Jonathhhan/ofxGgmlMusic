# Changelog

## Unreleased

- Added `ofxGgmlMusicExternalGenerationBackend` as the first explicit bridge
  boundary for local model-backed music generator executables.
- Added an external generation contract test that proves the bridge can launch
  a local generator and round-trip WAV, manifest, history, MIDI, and stem outputs.
- Added a user-facing external generation CLI plus an opt-in Hugging Face
  Transformers MusicGen runner profile.
- Allowed external model values to be model ids instead of only existing files
  when `requireModelPathExists` is disabled.

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
