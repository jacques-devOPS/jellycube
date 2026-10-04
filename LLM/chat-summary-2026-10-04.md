# JellyCube Chat Summary — 2026-10-04

## Objective
Build a native Jellyfin client for the Nintendo GameCube (homebrew), starting from the existing `jellyCube` prototype.

## Project location
`/home/projects/jellyCube/`

## What exists now
- Refactored GameCube client that builds with devkitPPC/libogc.
- Produces:
  - `jellycub.dol` — hardware build (reads `config.ini` from SD Gecko)
  - `jellycub-dolphin.dol` — Dolphin emulator test build (embedded config, no SD needed)
- HTTP client with non-blocking sockets, chunked transfer, status checks.
- JSMN-based JSON parser (token limit raised to 8192).
- Jellyfin API: authenticate, list libraries, list items, generate stream URL.
- Controller-driven UI; exports an `.m3u` playlist for MPlayer CE r658.
- Host auth test at `extras/tests/host_auth_test.c` succeeds against `<JELLYFIN_IP>:8096` with user `<JELLYFIN_USERNAME>` / `<JELLYFIN_PASSWORD>`.

## Configuration
- Dolphin build: edit `source/dolphin_test.c`, then `make dolphin`.
- Hardware build: place `config.ini` on SD card (`carda:/apps/jellycub/config.ini`).
- Server must be a literal IPv4 address; only plain HTTP is supported.

## Current blocker
Dolphin HLE Broadband Adapter obtains an IP (`10.0.0.139`) but cannot reach the Jellyfin server at `<JELLYFIN_IP>:8096`. Debug output shows:

```text
DEBUG: request failed (-1). Response preview:
```

The host-side `curl` and `host_auth_test` both work, so the code path is correct. The failure is Dolphin networking.

## Options to proceed
1. Run Jellyfin on the same PC as Dolphin and point `dolphin_test.c` at that PC's LAN IP.
2. Set up a TAP adapter bridged to the LAN (Dolphin Config → GameCube → SP1 → Broadband Adapter (TAP)).
3. Move testing to real GameCube hardware with a Broadband Adapter.

## Documentation
- `README.md` — overview
- `docs/QUICKSTART.md` — build/test instructions
- `docs/PROJECT_SUMMARY.md` — status and roadmap
- `docs/STRUCTURE.txt` — folder layout
- `config.ini.example` — hardware config template

## Backup
`extras/backups/original-prototype.tar.gz` contains the pre-refactor source.

## Next step (pending)
Choose one of the three network options and verify the GameCube can reach Jellyfin.
