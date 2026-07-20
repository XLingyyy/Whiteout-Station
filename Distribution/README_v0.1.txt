WHITEOUT STATION: BEFORE THE BLACKOUT — v0.1

Run: Windows\WhiteoutStation.exe

Controls
  WASD       Move
  Mouse      Look
  F          Interact
  Q          Cycle dialogue act / promise
  E          Evidence board
  C          Continue autosave
  Enter      Resolve run
  R          Start a new run

Goal
  Stabilize the generator, calibrate the antenna, and send the rescue signal
  within 8 AP while managing crew, supplies, evidence, and promises.

The game works fully offline. Saves and event logs are stored under:
  %LOCALAPPDATA%\WhiteoutStation\Saved

Optional local expression endpoint:
  WhiteoutStation.exe -WhiteoutAgentEndpoint=http://127.0.0.1:8765

The model may rewrite dialogue only. Deterministic C++ rules remain authoritative,
the per-run model budget is capped at 10, and any failure falls back locally.

Free asset licenses and sources are recorded in the repository at:
  SourceAssets\ASSET_LICENSES.md
