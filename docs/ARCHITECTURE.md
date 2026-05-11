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
- model-specific preprocessing and postprocessing
- beat, downbeat, tempo, key, chord, chroma, stem, and arrangement workflows
- focused root-level examples
- local media/model workflow documentation

## Not Owned Here

- ggml runtime setup and backend selection
- generic audio stream chunking, PCM, VAD, and low-level feature primitives
- generic tensor, graph, model metadata, and result types
- unrelated companion workflows
