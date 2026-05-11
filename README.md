# ofxGgmlMusic

`ofxGgmlMusic` is the companion addon for music embeddings, beat/downbeat,
onset/chroma helpers, tempo, key/chord workflows, stem-aware analysis, and
generation-oriented music workflows on top of `ofxGgmlCore`.

`ofxGgmlCore` stays the required dependency. `ofxGgmlAudio` may become an
optional dependency for shared stream, chunking, PCM, and lightweight feature
primitives. This addon owns music-specific workflow code so core and audio can
stay focused.

Family map: https://jonathhhan.github.io/ofxGgmlCore/

## First Milestone

- define small request/result types
- keep one root-level smoke example
- keep generated models, media, builds, and IDE files out of git
- validate the addon with local headless tests

## Example

`ofxGgmlMusicAnalysisExample` is a root-level audio analysis request smoke test. Generate it with the openFrameworks projectGenerator using addons `ofxGgmlMusic`, `ofxGgmlCore`, and `ofxImGui`.

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
