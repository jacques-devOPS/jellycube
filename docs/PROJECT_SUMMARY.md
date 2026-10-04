# JellyCube Project Summary

A GameCube homebrew client for Jellyfin, built with devkitPPC and libogc.

## What works

- GameCube DOL/ELF build with devkitPPC r50 / libogc 3.1.0.
- Broadband Adapter DHCP initialization.
- Plain-HTTP Jellyfin API client with:
  - Username/password authentication
  - Access-token storage
  - MediaBrowser `X-Emby-Authorization` headers
  - Library and item listing
  - Conservative streaming URL generation
- SD-card `config.ini` for credentials and client identity.
- Controller-driven menu for browsing libraries and folders.
- M3U playlist export for playback with MPlayer CE r658.
- Dolphin test build that embeds credentials and skips the SD card requirement.

## What does not work yet

- Direct video/audio decoding inside JellyCube.
- HTTPS/TLS.
- Hostname DNS resolution.
- Resume position reporting to Jellyfin.
- Subtitles, audio track selection, and chapters.

## Architecture

```
source/main.c          Video setup, input loop, menu flow
source/config.c        INI parser and validation
source/dolphin_test.local.c  Embedded test config for Dolphin builds
source/jc_network.c    BBA DHCP setup
source/http.c          Non-blocking HTTP/1.1 client with chunked support
source/jellyfin.c      Jellyfin API calls and item parsing
source/json.c          JSMN-based JSON extraction
third_party/jsmn.h     JSON tokenizer
```

## Verified vs. unverified claims

- Verified by compilation and source inspection: build, network init, HTTP request/response parsing, JSON extraction.
- Not verified: actual video playback on GameCube hardware, Dolphin BBA throughput, MPlayer CE playlist loading, long-duration stability, Jellyfin transcode profiles.

## Known constraints

- The server must be HTTP and a literal IPv4 address.
- The generated playlist contains the API key and should be deleted after testing.
- MPlayer CE r658 for GameCube mounts SD cards via SD Gecko slots A/B; SD2SP2 is not guaranteed.
- Some Dolphin builds do not emulate the GameCube SD card slot; use the `jellycube-dolphin.dol` build for API testing.

## Roadmap

1. Hardware playback test with MPEG-2 TS 320×240 profile.
2. Hardware playback test with H.264 Baseline 512×480 profile (requires `lavdopts=fast=1:skiploopfilter=all`).
3. Choose integration model: embedded decoder, MPlayer CE subprocess, or playlist hand-off.
4. Add resume/progress reporting and subtitle support.
5. Add HTTPS termination via a local LAN proxy if needed.
