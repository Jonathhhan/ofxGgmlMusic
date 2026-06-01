# ofxGgmlMusicAnalysisExample

Root-level request-building example for `ofxGgmlMusic` analysis workflows.

The example does not run a trained analysis backend yet. It demonstrates the
typed request surface that future tempo, beat tracking, key detection, chord
recognition, embedding, and stem-separation backends will receive.
It also draws a deterministic preview result so the common output shapes are
visible before a model-backed analyzer is installed.

Use the UI to edit:

- audio path
- analysis task
- prompt or note for the backend
- comma-separated tags
- preview duration for the mock result timeline

Press `Log request` to emit the current request with `ofLogNotice`.
Press `Refresh preview` after changing the task to update the mock result shape.
Use `Next task` or `T` to cycle through the available task types, and expand
`Current request` to inspect the request shape. Expand `Last result` to inspect
which preview result fields are populated for the selected task.

Useful shortcuts:

- `T`: cycle task
- `L`: log request
