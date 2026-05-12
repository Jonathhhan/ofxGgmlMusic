# Architecture

`ofxGgmlMusic` owns music-specific workflow code. It should use `ofxGgmlCore`
for stable runtime primitives, may use `ofxGgmlAudio` for generic audio stream
and feature primitives, and should keep model-family workflow details out of
core.

## Dependency Direction

```text
openFrameworks app
  -> ofxGgmlMusic
      -> ofxGgmlAudio (optional, low-level audio primitives)
      -> ofxGgmlCore
```

No dependency should point from `ofxGgmlCore` or `ofxGgmlAudio` back to
`ofxGgmlMusic`.

## Owned Here

- music-specific request/result helpers
- lightweight music media helpers such as PCM16 WAV write/read used by examples
  and generation smoke tests
- generation manifests that record prompt, backend, seed, tempo, key, audio
  stats, beat/downbeat markers, chord changes, and references next to rendered
  audio
- generation manifest loading for round-tripping generated metadata back into
  `ofxGgmlMusicGenerationResult`
- procedural stem export for named music components such as melody, bass, and
  pulse
- a tiny native procedural generation CLI used by scripts for no-IDE smoke runs
- an external generation backend contract test that launches a local generator
  executable through the same bridge model-backed tools will use
- shared generation presets used by both the CLI and openFrameworks example
- model-specific preprocessing and postprocessing
- beat, downbeat, tempo, key, chord, chroma, stem, and arrangement workflows
- prompt-to-music, loop, stem-targeted, and reference-audio generation workflows
- generation backend-family selection for diffusion, transformer, SampleRNN,
  and external bridge implementations
- `ofxGgmlMusicGenerationBackend` implementations for concrete generation
  runtimes
- `ofxGgmlMusicExternalGenerationBackend` as the explicit CLI boundary for
  MusicGen-style, audio diffusion, or custom GGML music generator executables
- the built-in `procedural-sketch` backend for deterministic model-free WAV
  generation smoke tests
- typed result fields for beats, chords, keys, embeddings, and stems
- focused root-level examples
- local media/model workflow documentation

## Not Owned Here

- ggml runtime setup and backend selection
- generic audio stream chunking, PCM, VAD, and low-level feature primitives
- generic tensor, graph, model metadata, and result types
- unrelated companion workflows
