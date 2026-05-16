# ofxGgmlMusic

`ofxGgmlMusic` is the companion addon for music generation, music embeddings,
beat/downbeat, onset/chroma helpers, tempo, key/chord workflows, stem-aware
analysis, and arrangement tools on top of `ofxGgmlCore`.

`ofxGgmlCore` stays the required dependency. `ofxGgmlAudio` may become an
optional dependency for shared stream, chunking, PCM, and lightweight feature
primitives. This addon owns music-specific workflow code so core and audio can
stay focused.

Family map: https://jonathhhan.github.io/ofxGgmlCore/

Current addon API version: `1.0.1`.

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
- `ofxGgmlMusicSection` for generated arrangement regions such as loop halves,
  intro/body/outro, or model-provided song sections
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
simple PCM16 files through one path. Generation results also carry a manifest
path; backends can write a `.wav.json` sidecar with prompt, backend, seed, tempo,
key, duration, sample rate, peak level, beat/downbeat markers, chords, generated
sections, stems, MIDI sidecars, and references. Backends also update an
`ofxGgmlMusic-history.json` index next to generated audio so tools and examples
can find recent manifests without guessing output filenames.

`ofxGgmlMusicExternalGenerationBackend` is the first explicit bridge boundary
for real local music models. Configure `request.external.executablePath`,
optional `modelPath`, and the expected CLI flags; the backend calls that local
generator, expects it to write `request.outputPath`, and then loads or writes
the standard `.wav.json` manifest. This keeps MusicGen-style transformer tools,
audio diffusion tools, or custom GGML generators outside the addon until one is
chosen deliberately. If a generator does not take a model argument, set
`request.external.modelFlag.clear()`. The external bridge is contract-tested
against the local procedural CLI so future model-backed generators have a clear
minimum interface to match.

For model ids such as `facebook/musicgen-small`, set
`request.external.requireModelPathExists = false`. Local model files keep the
default file-existence check.

For music-lane planning and backend boundaries, see
[docs/MUSIC_WORKFLOWS.md](docs/MUSIC_WORKFLOWS.md).

## Example

`ofxGgmlMusicAnalysisExample` is a root-level audio analysis request smoke test.
`ofxGgmlMusicGenerationExample` is a root-level prompt-to-music sketch that
writes a WAV file with the built-in `procedural-sketch` backend and draws a
waveform preview after generation. It also writes a `.wav.json` manifest next to
the audio file, writes editable melody, chord, and combined arrangement `.mid`
files, can export shared melody, bass, pulse, and mix stems, and overlays
sections plus beat/chord timing on the waveform. Each Generate press writes a
timestamped WAV so the history index can track multiple renders. The generation
example reloads recent renders from that index on startup when available,
falling back to the standard render manifest. Generate these examples with the
openFrameworks projectGenerator using addons `ofxGgmlMusic`, `ofxGgmlCore`, and
`ofxImGui`.

`ofxGgmlMusicAceStepExample` is the real local music generation example ported
from the legacy GUI lane. It connects to an AceStep-compatible server, checks
`/health`, runs the `/lm` prompt-enrichment stage, forwards that result to
`/synth`, writes the returned audio into `bin/data/generated/acestep`, and plays
the generated track back with a waveform preview for WAV output. The heavy
server, models, and generated audio remain local artifacts outside git.

For a no-IDE smoke run, use the procedural CLI helper:

```powershell
scripts\generate-procedural-music.bat -Preset lofi -Output C:\temp\music.wav -Loop
```

The helper builds `tools/ofxGgmlMusicGenerate`, writes the WAV, writes the
`.wav.json` manifest, writes editable melody/chord/arrangement `.mid` files, and
writes requested stem WAVs next to the mix. Built-in presets are `ambient`,
`lofi`, and `pulse`; list them from the CLI with:

```powershell
tools\ofxGgmlMusicGenerate\build\ofxGgmlMusicGenerate.exe --list-presets
```

Inspect a preset before rendering with:

```powershell
tools\ofxGgmlMusicGenerate\build\ofxGgmlMusicGenerate.exe --describe-preset lofi
```

List canonical stem export names with:

```powershell
tools\ofxGgmlMusicGenerate\build\ofxGgmlMusicGenerate.exe --list-stems
```

List supported key tonics and modes with:

```powershell
tools\ofxGgmlMusicGenerate\build\ofxGgmlMusicGenerate.exe --list-keys
```

Explicit prompt, tempo, key, duration, seed, and stem flags override the preset
defaults. Use
`ofxGgmlMusicUtils::loadGenerationManifest()` to load the sidecar back into an
`ofxGgmlMusicGenerationResult`; the CLI also supports
`--inspect C:\temp\music.wav.json` and
`--history C:\temp\ofxGgmlMusic-history.json`. To keep a render folder from
growing indefinitely, prune older history entries and their generated artifacts:

```powershell
tools\ofxGgmlMusicGenerate\build\ofxGgmlMusicGenerate.exe --prune-history C:\temp\ofxGgmlMusic-history.json --keep 8
```

Add `--json` to render, list-presets, describe-preset, list-stems, list-keys,
inspect, history, or prune commands when another tool needs machine-readable
output.

To verify that the external bridge can drive a local generator executable, run:

```powershell
scripts\test-external-generation-contract.bat -Clean
```

That script builds `tools\ofxGgmlMusicGenerate`, builds the external backend
contract test, launches the generator through `ofxGgmlMusicExternalGenerationBackend`,
and checks the generated WAV, manifest, history, MIDI, and stem artifacts.

To drive any local generator through the same backend from the command line:

```powershell
scripts\generate-external-music.bat -Executable C:\tools\music-generator.bat -Model C:\models\music-model.bin -Prompt "warm lofi loop" -Output C:\temp\music.wav
scripts\generate-external-music.bat -DryRun
```

The first concrete opt-in runner profile targets Hugging Face Transformers
MusicGen. It uses a small Python wrapper around `MusicgenForConditionalGeneration`
and writes the standard `.wav.json` manifest so the C++ backend can read the
result. Install a local Python environment with `torch`, `transformers`, and
`numpy`, then run:

```powershell
scripts\generate-musicgen-hf.bat -Prompt "warm lofi loop with soft keys" -Duration 8 -Output C:\temp\musicgen.wav
scripts\generate-musicgen-hf.bat -DryRun
```

Set `OFXGGML_MUSIC_PYTHON` when the desired Python executable is not first on
`PATH`. This profile is intentionally optional and does not make PyTorch a
dependency of the addon.

## Dependencies

- openFrameworks
- `ofxGgmlCore`
- optional later: `ofxGgmlAudio` for reusable low-level audio stream/features
- `ofxImGui` for examples

## Validate

```powershell
scripts\doctor-music.bat
scripts\run-music-runtime-smoke.bat -Json -SummaryOnly -Clean
scripts\validate-local.bat
```

On macOS/Linux:

```sh
./scripts/doctor-music.sh
./scripts/run-music-runtime-smoke.sh -Json -SummaryOnly -Clean
./scripts/validate-local.sh
```

`scripts\run-music-runtime-smoke.*` is the lane-owned runtime-smoke entrypoint
for ecosystem planning and CI rollouts. It runs the deterministic helper tests,
checks doctor readiness, builds the no-IDE procedural generator, writes a short
model-free `procedural-sketch` WAV into a temp directory, and verifies the
manifest, history, MIDI, and stem sidecars. It does not claim model-backed
MusicGen, audio diffusion, SampleRNN, or custom GGML generator coverage.

## Boundary

Keep music-specific preprocessing, postprocessing, model launch, media handling,
music terminology, and examples here. Reuse `ofxGgmlAudio` for generic audio
stream/chunk/feature plumbing when needed. Move code down into `ofxGgmlCore`
only when it becomes a stable, domain-neutral primitive with focused tests.
