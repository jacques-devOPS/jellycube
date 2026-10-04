# JellyCube

A native Jellyfin client for the Nintendo GameCube Broadband Adapter.

## Current status

Builds and runs as a GameCube DOL. It authenticates to a Jellyfin server over plain HTTP, browses libraries and items, and exports an M3U playlist for playback testing with [MPlayer CE](https://github.com/ExtremsCorner/mplayer-ce).

Actual playback through MPlayer CE is the next gate and is **not yet verified**.

## What it does

- Reads server credentials from `config.ini` on the SD card.
- Brings up the GameCube Broadband Adapter with DHCP.
- Authenticates to Jellyfin with username/password and stores the access token.
- Lists libraries, then folders/items inside each library.
- Generates a conservative streaming URL (MPEG-2 TS, 320×240, MP2 audio).
- Writes a one-item M3U playlist to the SD card that MPlayer CE can load.

## What it does NOT do yet

- Play video directly. MPlayer CE or another GameCube player is still required.
- HTTPS/TLS. Only plain HTTP on a trusted LAN is supported.
- Hostname lookup. The server address must be a dotted IPv4 address.
- Resume, subtitles, audio track selection, or progress reporting.

## Requirements

- GameCube with Broadband Adapter (BBA) or Dolphin with emulated BBA.
- SD Gecko in slot A or B, formatted FAT32, for `config.ini` and the exported playlist.
- devkitPPC / libogc toolchain.
- Jellyfin server reachable on the same LAN, with at least one test user.

## Build

Normal build:

```bash
export DEVKITPRO=/opt/devkitpro
export DEVKITPPC=/opt/devkitpro/devkitPPC
make
```

Dolphin test build (embeds test credentials, no SD card required):

```bash
make dolphin
```

Output:

- `jellycube.dol` — GameCube executable for hardware
- `jellycube-dolphin.dol` — GameCube executable for Dolphin API/network testing

## Configure

For hardware, copy `config.ini.example` to the SD card at:

```
carda:/apps/jellycube/config.ini
```

or `cardb:/apps/jellycube/config.ini`.

Edit it:

```ini
server   = 192.168.1.100
port     = 8096
username = jellyfin_test_user
password = jellyfin_test_pass
device_id = jellycube_01
client_name = JellyCube
```

The address must be a literal IPv4 address. HTTPS is not supported.

For the Dolphin test build, edit `source/dolphin_test.local.c` and rebuild with `make dolphin`.

## Test

### Dolphin (recommended first step)

Some Dolphin builds do not expose a GameCube SD card slot. If yours does not, use `jellycube-dolphin.dol`.

1. Configure emulated BBA and bridge it to your LAN.
2. Point Dolphin at `jellycube-dolphin.dol`.
3. Verify that the app obtains an IP, authenticates, and shows libraries.

If your Dolphin has an SD Card option in Slot A, you can instead use `jellycube.dol` with a virtual SD card.

### Real hardware

1. Copy `jellycube.dol` to `carda:/apps/jellycube/jellycube.dol`.
2. Copy `config.ini` to `carda:/apps/jellycube/config.ini`.
3. Boot from an SD-capable loader.
4. Select a movie/episode and press A. The app writes `carda:/mplayer/jellycube.m3u`.
5. Launch MPlayer CE r658 and load the playlist.

## Project layout

```
include/        Public headers
source/         C implementation
third_party/    jsmn JSON parser
backups/        Snapshot of the original prototype
research/       MPlayer CE source and release binaries
build/          Object files (generated)
```

## License

JellyCube code is provided as-is for homelab experimentation. Third-party files retain their own licenses:

- jsmn — MIT
- libogc / devkitPPC — see devkitPro
- MPlayer CE — GPLv2

## Credits

- MPlayer CE by Extrems
- libogc by shagkur and the devkitPro team
- Jellyfin by the Jellyfin contributors
