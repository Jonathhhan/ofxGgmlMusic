# AGENTS.md

This repository is `ofxGgmlMusic`, the music companion addon for the ofxGgml family.

Codex should treat `ofxGgmlCore` as the backend-neutral foundation. This repo owns music analysis, beat/key/chord workflows, stems, procedural/music generation, manifests, CLI integration, and music-specific examples.

## Addon contract

Do:

- keep music-specific workflows in this addon
- depend on shared primitives from `ofxGgmlCore` where practical
- preserve openFrameworks addon layout and `addon_config.mk`
- keep examples projectGenerator-friendly
- document audio/model/runtime requirements clearly
- update scripts/docs/examples together with behavior changes

Do not:

- move backend-neutral Core primitives into this repo
- commit models, generated audio, stems, binaries, or caches
- hardcode local absolute paths
- silently break Windows/macOS/Linux script parity

## Codex workflow

1. Inspect README, docs, scripts, `addon_config.mk`, examples, and relevant `src/` files first.
2. Propose the smallest implementation plan before editing.
3. Keep diffs focused.
4. Preserve Core vs companion-addon boundaries.
5. Update examples/docs/scripts with code changes.
6. Summarize validation honestly.
