# ofxGgmlMusic

`ofxGgmlMusic` is the companion addon for music generation, music embeddings,
beat/downbeat, onset/chroma helpers, tempo, key/chord workflows, stem-aware
analysis, and arrangement tools on top of `ofxGgmlCore`.

`ofxGgmlCore` stays the required dependency. `ofxGgmlAudio` may become an
optional dependency for shared stream, chunking, PCM, and lightweight feature
primitives. This addon owns music-specific workflow code so core and audio can
stay focused.

Family map: https://jonathhhan.github.io/ofxGgmlCore/

Current addon API version: `1.0.2`.

## Features

- music analysis helpers
- beat and timing workflows
- key/chord workflow boundaries
- stem and generation lane planning
- runtime smoke validation entrypoint

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
WAV files with style/negative-prompt-sensitive progressions, chord-anchored
melody, chord-root bass, pulse, pad texture, section-aware layer energy, and
smoothed loop boundaries. That backend is model-free and exists to make the
generation workflow testable before a real model bridge is selected. Shared WAV helpers live in
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
The addon manifest also advertises the generated workflow report path and the
plan/readiness/report scripts for ecosystem tooling in `ofxggml-addon.json`.

## Example

`ofxGgmlMusicAnalysisExample` is a root-level audio analysis request builder.
It lets you edit the audio path, task, prompt, and tags, then draws a
deterministic preview result timeline so tempo, beat, key, chord, embedding,
and stem result shapes are visible before a trained analysis backend is wired in.
Its preview duration, current request summary, logging button, and task cycling
shortcut make the analysis request shape easy to inspect.
`ofxGgmlMusicGenerationExample` is a root-level prompt-to-music sketch that
writes a WAV file with the built-in `procedural-sketch` backend and draws a
waveform preview after generation. It includes prompt and negative-prompt fields
so procedural layers can be encouraged or damped before rendering. It also writes
a `.wav.json` manifest next to the audio file, writes editable melody, chord, and
combined arrangement `.mid` files, can export shared melody, bass, pulse, and mix stems,
overlays sections plus beat/chord timing on the waveform, and shows playback
position while audio is playing or while dragging the waveform. The arrangement MIDI includes melody,
chords, bass, and pulse tracks for quick remixing. Use `Current request` to inspect the render shape, `Last result` to
inspect rendered timing and artifact metadata, `P` to cycle presets, `D` to log
the request summary, `New seed` to audition deterministic variations quickly,
and `Reload` or `L` to pull the latest history entry back into the preview.
Each Generate press writes a
timestamped WAV so the history index can track multiple renders. The generation
example reloads recent renders from that index on startup when available,
falling back to the standard render manifest. Generate
these examples with the openFrameworks projectGenerator using addons
`ofxGgmlMusic`, `ofxGgmlCore`, and `ofxImGui`.

On Windows, if an existing generated Visual Studio project reports missing
addon headers such as `ofxGgmlMusic.h` or `ofxImGui.h`, repair the generated
metadata from the addon root:

```powershell
scripts\build-music-example.ps1 -Example ofxGgmlMusicAceStepExample -RepairOnly
```

`ofxGgmlMusicAceStepExample` is the real local music generation example ported
from the legacy GUI lane. It connects to an AceStep-compatible server, checks
`/health`, runs the `/lm` prompt-enrichment stage, forwards that result to
`/synth`, writes the returned audio into `bin/data/generated/acestep`, and plays
the generated track back with a waveform preview for WAV output. The heavy
server, models, and generated audio remain local artifacts outside git.
The GUI includes prompt presets, quick seed variation, batch-output preview
selection, current-request and last-result inspection, and a playback cursor
over the draggable waveform.

To run it, start an AceStep server first. The launcher prefers:

1) `OFXGGML_ACESTEP_SERVER_EXE`
2) `libs/acestep/bin/ace-server(.exe)` (built from [ServeurpersoCom/acestep.cpp](https://github.com/ServeurpersoCom/acestep.cpp))
3) explicit `-ServerExecutable`

```powershell
scripts\setup-acestep-server.ps1 -DryRun
scripts\setup-acestep-server.ps1 -Clean -Cuda
scripts\start-acestep-server.ps1 -ServerExecutable "C:\path\to\ace-server.exe" -ModelPath "C:\models\..."
```

By default the setup script clones the remote default branch of `acestep.cpp`.
Pass `-Revision <branch-or-tag>` only when you intentionally want to pin an
upstream branch or tag.

AceStep follows the ecosystem rule: prefer `ofxGgmlCore` ggml when Core exposes
the ACE-Step fork ops such as `ggml_col2im_1d`, then fall back to the bundled
`acestep.cpp` ggml only when Core is missing or incompatible. Pass
`-BundledGgml` only when you intentionally want the upstream bundled source.
Use `-Clean -Cuda` for NVIDIA GPU builds; explicit `-Cuda` is strict and fails
if the final CMake cache or installed backend artifacts are CPU-only. Auto setup
may fall back to CPU-only, but reports that result in the setup summary. Use
`-Blas` only when a system BLAS installation is configured.

If you keep `ace-server(.exe)` at `ofxGgmlMusic/libs/acestep/bin`, you can just run:

```powershell
scripts\start-acestep-server.ps1 -ModelPath "C:\models\..."
```

`setup-acestep-server.ps1` keeps downloaded ACE-Step source, build output, and
installed server binaries under `libs/acestep`, which remains local artifact
space outside git.

The example reads `OFXGGML_ACESTEP_SERVER_URL` automatically at startup.
You can also set these optional environment variables:

- `OFXGGML_ACESTEP_SERVER_EXE`
- `OFXGGML_ACESTEP_SERVER_URL` (defaults to `http://127.0.0.1:8085`)
- `OFXGGML_ACESTEP_MODEL_PATH`
- `OFXGGML_ACESTEP_SERVER_ARGS` (optional extra flags passed to the server)

For a no-IDE smoke run, use the procedural CLI helper:

```powershell
scripts\generate-procedural-music.bat -Preset lofi -Output C:\temp\music.wav -Loop
```

The helper builds `tools/ofxGgmlMusicGenerate`, writes the WAV, writes the
`.wav.json` manifest, writes editable melody/chord/arrangement `.mid` files, and
writes requested stem WAVs next to the mix. The arrangement MIDI carries melody,
chords, bass, and pulse tracks. Built-in presets are `ambient`,
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

Explicit prompt, negative prompt, guidance, tempo, key, duration, seed, and
stem flags override the preset defaults. Use
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

When choosing between the optional MusicGen and ACE-Step workflows, start with
the model-free planner. It prints the relevant readiness, setup, start, and
generation commands without building, downloading, starting a server, or writing
audio:

```powershell
scripts\plan-generation-workflow.bat
scripts\plan-generation-workflow.bat -Backend MusicGenHf -Tempo 92 -Key C -Mode major -Json
scripts\plan-generation-workflow.bat -Backend AceStep -ServerExecutable C:\tools\ace-server.exe -ModelPath C:\models\acestep
```

After planning, run the readiness checker to probe optional MusicGen Python
dependencies and ACE-Step `/health` status without writing audio:

```powershell
scripts\check-generation-readiness.bat
scripts\check-generation-readiness.bat -Backend MusicGenHf -Json
scripts\check-generation-readiness.bat -Backend AceStep -ServerUrl http://127.0.0.1:8085
```

Readiness warnings are expected on machines without the optional Python stack or
without a running ACE-Step server. Add `-Strict` when automation should fail on
warnings.

To capture a combined planning and readiness snapshot for handoff or release
evidence. The JSON summary includes reproducible next commands and the
manifest-advertised report path:

```powershell
scripts\write-generation-workflow-report.bat -Backend all -ReportPath generation-workflow-report.json
scripts\write-generation-workflow-report.bat -UseManifestReportPath
scripts\write-generation-workflow-report.bat -Backend MusicGenHf -Json -SummaryOnly
```

The first concrete opt-in runner profile targets Hugging Face Transformers
MusicGen. It uses a small Python wrapper around `MusicgenForConditionalGeneration`
and writes the standard `.wav.json` manifest so the C++ backend can read the
result. Install a local Python environment with `torch`, `transformers`, and
`numpy`, then run:

```powershell
scripts\generate-musicgen-hf.bat -Prompt "warm lofi loop with soft keys" -Duration 8 -Output C:\temp\musicgen.wav
scripts\generate-musicgen-hf.bat -Prompt "warm lofi loop with soft keys" -NegativePrompt "muddy drums" -Tempo 92 -Key C -Mode major -Output C:\temp\musicgen.wav
scripts\generate-musicgen-hf.bat -DryRun
```

Probe an installed MusicGen Python environment without generating audio:

```powershell
scripts\generate-musicgen-hf.bat -SmokeTest -Json
scripts\generate-musicgen-hf.bat -SmokeTest -Json -AllowMissingDeps
scripts\generate-musicgen-hf.bat -SmokeTest -LoadModel -Json
```

The first command checks Python package readiness only. `-LoadModel` also tries
to load the configured Hugging Face model and may download model files through
the local Transformers cache. `-AllowMissingDeps` is useful for automation that
wants the JSON report without failing when the optional Python stack is absent.

Set `OFXGGML_MUSIC_PYTHON` when the desired Python executable is not first on
`PATH`. `-Tempo`, `-Key`, `-Mode`, and `-NegativePrompt` are recorded in the
standard manifest for downstream tools even though the Hugging Face MusicGen
model consumes only the positive text prompt. This profile is intentionally
optional and does not make PyTorch a dependency of the addon.

## Dependencies

- openFrameworks
- `ofxGgmlCore`
- optional later: `ofxGgmlAudio` for reusable low-level audio stream/features
- `ofxImGui` for examples

## Validate

```powershell
scripts\doctor-music.bat
scripts\run-music-runtime-smoke.bat -Json -SummaryOnly -Clean
scripts\setup-acestep-server.ps1 -DryRun
scripts\plan-generation-workflow.bat
scripts\check-generation-readiness.bat
scripts\write-generation-workflow-report.bat
scripts\test-acestep-server-dry-run.bat
scripts\test-musicgen-hf-smoke.bat
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
