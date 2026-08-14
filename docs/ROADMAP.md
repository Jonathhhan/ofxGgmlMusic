# Roadmap

## Current Milestone

- Seed the companion addon skeleton.
- Keep `ofxGgmlMusicAceStepExample` as the single canonical generation example;
  retain the model-free procedural backend only for CLI use and deterministic tests.
- Keep `ofxGgmlCore` as the only required library dependency; examples may depend on `ofxImGui`.
- Add local validation and headless tests.
- Keep `ofxGgmlMusic` separate from `ofxGgmlAudio`; allow an optional future
  dependency on Audio for low-level stream/chunk/feature primitives.
- Add typed music request/result shapes for tempo, beat/downbeat, key/chord,
  embeddings, stems, and generation.
- Add explicit prompt-to-music generation request/result shapes.
- Add a generation backend-family hint, including diffusion, transformer,
  SampleRNN, and external bridge lanes.
- Add the `ofxGgmlMusicGenerationBackend` interface and unavailable fallback
  backend.
- Keep the deterministic `procedural-sketch` backend as a CLI and test fixture
  for writing real WAV artifacts without presenting it as model inference.
- Add shared PCM16 WAV utilities for CLI, tests, and model-backed example playback.
- Add generation manifests so rendered WAVs keep prompt/backend/seed/audio
  provenance beside the file.
- Add generated beat/downbeat and chord metadata to music generation results,
  manifests, and tests.
- Add optional procedural stem exports for melody, bass, and pulse components.
- Add a native procedural generation CLI plus Windows/macOS/Linux wrapper
  scripts for no-IDE smoke runs.
- Add an explicit external generation backend boundary for local model-backed
  music generator executables.
- Add shared generation presets for ambient, lofi, and pulse workflows.
- Add generation manifest loading for metadata round trips.
- Add independent addon version metadata and release-candidate docs.
- Add an external generation contract test that drives the local procedural CLI
  through `ofxGgmlMusicExternalGenerationBackend`.
- Add a user-facing external generation CLI and an opt-in Hugging Face
  Transformers MusicGen runner profile.
- Add a smoke test mode for installed MusicGen Python environments without
  making PyTorch a required addon dependency.

## Next Milestones

- Add audio diffusion bridge research notes with a clear runtime/setup decision.
- Keep image and video GAN work in `ofxGgmlDiffusion` and `ofxGgmlVideo`; this
  addon should focus on audio-producing music generators.
- Keep one useful model-backed openFrameworks example that runs with
  user-provided local assets.
- Add focused tests around request/result helpers.
- Document the `clone -> setup -> run` path from a new user's point of view.
