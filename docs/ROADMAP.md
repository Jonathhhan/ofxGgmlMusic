# Roadmap

## Current Milestone

- Seed the companion addon skeleton.
- Keep `ofxGgmlMusicAnalysisExample` as the first root-level smoke example.
- Keep `ofxGgmlCore` as the only required library dependency; examples may depend on `ofxImGui`.
- Add local validation and headless tests.
- Keep `ofxGgmlMusic` separate from `ofxGgmlAudio`; allow an optional future
  dependency on Audio for low-level stream/chunk/feature primitives.

## Next Milestones

- Connect the first real local backend or bridge adapter.
- Add music-domain request/result types for tempo, beat/downbeat, key/chord, and
  embedding workflows.
- Add one useful openFrameworks example that runs with user-provided assets.
- Add focused tests around request/result helpers.
- Document the `clone -> setup -> run` path from a new user's point of view.
