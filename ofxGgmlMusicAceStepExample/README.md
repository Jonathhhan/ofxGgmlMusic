# ofxGgmlMusicAceStepExample

Root-level GUI example for real text-to-music generation through an
AceStep-compatible local server.

The example keeps the heavy model runtime outside the addon. It sends the
request to the server, calls the model-side `/lm` stage, forwards that result to
`/synth`, writes the returned audio into `bin/data/generated/acestep`, and plays
the result back in openFrameworks. When WAV output is requested it also draws a
waveform preview.

## Requirements

- `ofxGgmlCore`
- `ofxGgmlMusic`
- `ofxImGui`
- an AceStep-compatible local server on `http://127.0.0.1:8085`

## Run

Generate the project with the openFrameworks projectGenerator using the addons
listed in `addons.make`, start your AceStep server, then run the example.
If Visual Studio reports missing headers such as `ofxGgmlMusic.h`,
`ofxGgmlMusic/ofxGgmlMusicAceStepBridge.h`, or `ofxImGui.h`, repair the
generated project metadata from the addon root:

```powershell
scripts\build-music-example.ps1 -Example ofxGgmlMusicAceStepExample -RepairOnly
```

To repair and build the example in one step:

```powershell
scripts\build-music-example.ps1 -Example ofxGgmlMusicAceStepExample -Jobs 0
```

Start the server with the new helper:

```powershell
From the addon root:
..\scripts\setup-acestep-server.ps1
..\scripts\start-acestep-server.ps1 -ServerExecutable C:\path\to\ace-server.exe -ModelPath C:\models\...
```

Or place `ace-server(.exe)` at:

```powershell
ofxGgmlMusic\libs\acestep\bin\ace-server(.exe)
```
and launch with:

```powershell
..\scripts\start-acestep-server.ps1 -ModelPath C:\models\...
```

Optional environment variables:

- `OFXGGML_ACESTEP_SERVER_EXE`
- `OFXGGML_ACESTEP_SERVER_URL` (defaults to `http://127.0.0.1:8085`)
- `OFXGGML_ACESTEP_MODEL_PATH`
- `OFXGGML_ACESTEP_SERVER_ARGS` (optional extra flags)

Useful controls:

- `Health`: checks `/health`
- `Generate`: runs `/lm` and `/synth`
- `Play`: plays the generated output
- `H`: keyboard shortcut for health
- `G`: keyboard shortcut for generation
- `Space`: toggles playback

Generated audio remains local under `bin/data/generated/acestep` and is not
tracked by git.
