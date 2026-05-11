# Roadmap

## Current Milestone

- Seed the companion addon skeleton.
- Keep `ofxGgmlMusicAnalysisExample` as the first root-level smoke example.
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
- Add a deterministic `procedural-sketch` backend and root-level generation
  example so prompt-to-music writes an actual WAV before model bridges land.

## Next Milestones

- Connect the first real local backend or bridge adapter.
- Add MusicGen-style transformer and diffusion bridge research notes with a
  clear runtime/setup decision.
- Keep image and video GAN work in `ofxGgmlDiffusion` and `ofxGgmlVideo`; this
  addon should focus on audio-producing music generators.
- Add one useful openFrameworks example that runs with user-provided assets.
- Add focused tests around request/result helpers.
- Document the `clone -> setup -> run` path from a new user's point of view.
