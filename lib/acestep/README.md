## ofxGgmlMusic AceStep Runtime Slot

This folder is the preferred local location for an AceStep-compatible C++ server
built from [ServeurpersoCom/acestep.cpp](https://github.com/ServeurpersoCom/acestep.cpp).

Place one of:

- `bin/ace-server(.exe)` (preferred auto-discovered path from launcher)
- Any compatible executable that exposes the same endpoints (override with
  `OFXGGML_ACESTEP_SERVER_EXE` or `-ServerExecutable`)

Expected server endpoints used by the addon:

- `GET /health`
- `POST /lm`
- `POST /synth`

Notes:

- `lib/acestep` is intentionally **optional**.
- Model files are intentionally not tracked in this repository.
- If you place the server as `lib/acestep/bin/ace-server(.exe)`, launch defaults
  work without extra parameters.
- The launcher preference is:
  1. `OFXGGML_ACESTEP_SERVER_EXE`
  2. `lib/acestep/bin/ace-server(.exe)`
  3. explicit `-ServerExecutable`
