# ofxGgmlMusic

`ofxGgmlMusic` is the companion addon for music generation, music embeddings,
beat/downbeat, onset/chroma helpers, tempo, key/chord workflows, stem-aware
analysis, and arrangement tools on top of `ofxGgmlCore`.

`ofxGgmlCore` stays the required dependency. `ofxGgmlAudio` may become an
optional dependency for shared stream, chunking, PCM, and lightweight feature
primitives. This addon owns music-specific workflow code so core and audio can
stay focused.

Family map: https://jonathhhan.github.io/ofxGgmlCore/

## First Milestone

- define small music-specific request/result types
- keep one root-level smoke example
- keep generated models, media, builds, and IDE files out of git
- validate the addon with local headless tests

## Music Scope

The public API starts with typed music workflow shapes:

- `ofxGgmlMusicTask` for analysis, tempo, beat tracking, key detection, chord
  recognition, embeddings, stem separation, and generation
- `ofxGgmlMusicTempo`, `ofxGgmlMusicBeat`, `ofxGgmlMusicKey`, and
  `ofxGgmlMusicChord` for common analysis output
- `ofxGgmlMusicStem` for stem-aware workflows
- `ofxGgmlMusicResult` fields for beats, chords, embeddings, and stems
- `ofxGgmlMusicGenerationRequest` and `ofxGgmlMusicGenerationResult` for
  prompt-to-music, arrangement, loop, stem-targeted, and reference-audio
  generation workflows
- `ofxGgmlMusicGenerationBackendFamily` to describe likely backend families such as
  diffusion, transformer, SampleRNN, or an external bridge

Concrete backends can fill these plain C++ types without pulling low-level audio
plumbing into the Music addon.

Image and video GAN workflows belong in `ofxGgmlDiffusion` and `ofxGgmlVideo`.
This addon keeps its generation boundary focused on music backends that produce
audio: diffusion, transformer, SampleRNN, or explicit external bridges.
Generation backends should implement `ofxGgmlMusicGenerationBackend`; the addon
ships an unavailable stub so examples can fail clearly before a diffusion,
transformer, SampleRNN, or external bridge runtime is installed. It also ships a
small `procedural-sketch` backend that writes deterministic prompt-conditioned
WAV files. That backend is model-free and exists to make the generation workflow
testable before a real model bridge is selected. Shared WAV helpers live in
`ofxGgmlMusicAudioUtils` so examples and future backends can write and inspect
simple PCM16 files through one path.

## Example

`ofxGgmlMusicAnalysisExample` is a root-level audio analysis request smoke test.
`ofxGgmlMusicGenerationExample` is a root-level prompt-to-music sketch that
writes a WAV file with the built-in `procedural-sketch` backend and draws a
waveform preview after generation. Generate either example with the
openFrameworks projectGenerator using addons `ofxGgmlMusic`, `ofxGgmlCore`, and
`ofxImGui`.

## Dependencies

- openFrameworks
- `ofxGgmlCore`
- optional later: `ofxGgmlAudio` for reusable low-level audio stream/features
- `ofxImGui` for examples

## Validate

```powershell
scripts\validate-local.bat
```

On macOS/Linux:

```sh
./scripts/validate-local.sh
```

## Boundary

Keep music-specific preprocessing, postprocessing, model launch, media handling,
music terminology, and examples here. Reuse `ofxGgmlAudio` for generic audio
stream/chunk/feature plumbing when needed. Move code down into `ofxGgmlCore`
only when it becomes a stable, domain-neutral primitive with focused tests.
