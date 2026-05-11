# ofxGgmlMusicGenerationExample

Root-level prompt-to-music sketch example for `ofxGgmlMusic`.

This example uses the built-in `procedural-sketch` backend. It is deterministic,
model-free, and intended as a workflow proof: edit a prompt, tempo, key, style,
duration, and seed, then generate a small WAV file into `bin/data/outputs`.
The example loads the rendered WAV back into a waveform preview and writes a
`.wav.json` manifest next to the audio file.

The backend is not a trained music model. It gives the addon a real generation
path while transformer, diffusion, SampleRNN, or external model bridges remain
explicit future backends.
