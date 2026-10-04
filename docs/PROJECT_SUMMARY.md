# JellyCube Project Summary

JellyCube is a GameCube homebrew Jellyfin browser and playback-test launcher.
It targets a GameCube Broadband Adapter, an SD Gecko, and a plain-HTTP Jellyfin server on the same LAN.

**Current version:** `0.2.3-dev`

## Current state

The project builds two PowerPC GameCube DOL files with devkitPPC and libogc:

- Hardware build: `jellycube-<version>+<commit>-gamecube.dol`
- Dolphin test build: `jellycube-<version>+<commit>-dolphin.dol`

`make release` creates versioned artifacts and a SHA-256 manifest under `dist/`. It removes generated DOL, ELF, and object files from the project root after packaging.

The normal hardware build does the following:

1. Mounts the first FAT-formatted SD device found: slot A, then slot B, then SD2SP2.
2. Loads `config.ini` from the launched DOL's directory, with `apps/jellycube/config.ini` as fallback. Path matching is case-insensitive.
3. Obtains a network configuration via BBA DHCP.
4. Authenticates to Jellyfin with `/Users/AuthenticateByName`.
5. Requests library and item data through the Jellyfin API.
6. Exports a one-item M3U playlist for MPlayer CE.

## Verified results

- The project compiles with devkitPPC r50, libogc 3.1.0, libfat-ogc 2.1.0, and libbba.
- A host-side build of the same Jellyfin HTTP/JSON code authenticates successfully against the configured Jellyfin server.
- Real GameCube BBA testing obtains DHCP successfully. Observed hardware lease: `10.0.0.13`, gateway `10.0.0.1`.
- Builds up to 0.2.1 failed TCP on hardware and Dolphin. Root cause, verified in libogc source: GameCube `net_fcntl()` is a stub that always returns -1. The client treated that as fatal and never called `net_connect()`. 0.2.2 uses blocking sockets. Host test passes with the new client. Hardware retest pending.

## Not verified

- A successful Jellyfin API request from real GameCube hardware.
- Library/item rendering from real hardware.
- M3U export on real hardware.
- MPlayer CE loading the generated playlist.
- Audio/video playback, codec profile, stability, or bandwidth limits.

## Network diagnostics

The current DOL reports the failing stage and socket errno:

```text
Network: <address|socket|TCP connect|HTTP send|HTTP receive|HTTP parse> failed (error <n>).
Network: server returned HTTP <status>.
JSON: response parse failed (<code>).
Jellyfin: authentication response omitted token or user ID.
```

The real BBA test should record the exact message. That message determines the next network change.

## Architecture

```text
source/main.c                     Console UI, SD mounting, controller input
source/config.c                   INI parser and IPv4 validation
source/network.c                  BBA DHCP initialization
source/http.c                     HTTP/1.1 TCP client and response parser
source/jellyfin.c                 Jellyfin authentication, browsing, M3U export
source/json.c                     JSMN JSON wrappers
source/dolphin_test.local.c       Ignored local Dolphin credentials
source/dolphin_test.local.c.example  Tracked Dolphin config template
third_party/jsmn.h                JSON tokenizer
```

## Constraints

- The Jellyfin server address must be a dotted IPv4 address.
- The client supports HTTP only. HTTPS and hostname lookup are not implemented.
- The generated M3U contains an access token. Delete the playlist after testing.
- `source/dolphin_test.local.c` contains local test credentials and is ignored by Git.
- The production build uses `config.ini` on SD Gecko. It does not embed credentials.

## Next gate

Obtain one successful API response from the real GameCube BBA. Do not work on playback integration until library browsing succeeds on hardware.
