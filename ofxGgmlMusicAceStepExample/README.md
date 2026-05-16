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

Useful controls:

- `Health`: checks `/health`
- `Generate`: runs `/lm` and `/synth`
- `Play`: plays the generated output
- `H`: keyboard shortcut for health
- `G`: keyboard shortcut for generation
- `Space`: toggles playback

Generated audio remains local under `bin/data/generated/acestep` and is not
tracked by git.
