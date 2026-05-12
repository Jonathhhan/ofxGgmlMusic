# Release Checklist

Use this before tagging or announcing an `ofxGgmlMusic` release. The goal is to
prove the procedural generation path and keep generated media local.

## Fresh Clone Layout

From the openFrameworks `addons` folder:

```powershell
git clone https://github.com/Jonathhhan/ofxGgmlCore.git
git clone https://github.com/Jonathhhan/ofxGgmlMusic.git
cd ofxGgmlMusic
```

Expected layout:

```text
addons/
  ofxGgmlCore/
  ofxGgmlMusic/
  ofxImGui/
```

## Procedural Generation Smoke

Run the no-IDE helper:

```powershell
scripts\generate-procedural-music.bat -Preset lofi -Output C:\temp\music.wav -Loop
```

macOS/Linux:

```sh
./scripts/generate-procedural-music.sh -Preset lofi -Output /tmp/music.wav -Loop
```

Expected generated files:

```text
music.wav
music.wav.json
music-melody.mid
music-chords.mid
music-arrangement.mid
ofxGgmlMusic-history.json
```

Generated WAV, MIDI, manifest, and history outputs must not be staged for
release.

## Local Validation

Run:

```powershell
scripts\validate-local.bat
```

macOS/Linux:

```sh
./scripts/validate-local.sh
```

For a pre-tag release candidate gate:

```powershell
scripts\release-candidate.bat
```

macOS/Linux:

```sh
./scripts/release-candidate.sh
```

## Before Tagging

- `git status --short --ignored` shows no unexpected generated outputs
- no generated WAV, MIDI, manifest, model, IDE, or build files are staged
- `CHANGELOG.md` has an entry for the release
- `docs/releases/vX.Y.Z.md` matches the release scope
- `README.md` is clear that procedural generation is built in, the external
  bridge is available, and Hugging Face MusicGen remains an optional Python
  runner profile
- `docs/ROADMAP.md` keeps image/video GAN work outside the Music addon
