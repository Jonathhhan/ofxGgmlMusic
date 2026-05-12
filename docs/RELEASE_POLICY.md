# Release Policy

`ofxGgmlMusic` releases are tagged independently from `ofxGgmlCore` and the
other companion addons.

## Tagging

Use per-addon semantic version tags:

```text
v1.0.0
v1.0.1
v1.1.0
```

Do not mirror tags across the whole addon family unless this addon changed and
passed its own release checklist.

## Compatibility

Document the minimum compatible `ofxGgmlCore` version in each release note.

For normal development:

- patch releases should not require Core API changes
- minor releases may require a newer Core minor version
- breaking API changes should use a major version bump

## Runtime Scope

Procedural prompt-to-music generation is allowed as a built-in smoke backend.
Model-backed MusicGen, diffusion, SampleRNN, or external bridge integrations
must be documented explicitly when they land.

Generated audio, MIDI, manifests, histories, model binaries, and build outputs
stay out of git.

## Pre-Release Gate

Before tagging:

1. Run `scripts\release-candidate.bat` on Windows.
2. Run `./scripts/release-candidate.sh` on macOS or Linux when available.
3. Complete `docs/RELEASE_CHECKLIST.md`.
4. Update `CHANGELOG.md`.
5. Confirm no generated artifacts, model binaries, or generated media are staged.
