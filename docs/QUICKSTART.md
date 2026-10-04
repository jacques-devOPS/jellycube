# JellyCube Quick Start

This guide gets the GameCube Jellyfin client from source to a running test.

## 1. Install the toolchain

On Debian/Ubuntu:

```bash
curl -A 'dkp apt' -fsSL https://apt.devkitpro.org/install-devkitpro-pacman -o install-devkitpro-pacman
chmod +x install-devkitpro-pacman
sudo ./install-devkitpro-pacman
sudo dkp-pacman -S --needed gamecube-dev
```

Set environment variables (add to `~/.bashrc` or export each session):

```bash
export DEVKITPRO=/opt/devkitpro
export DEVKITPPC=/opt/devkitpro/devkitPPC
```

## 2. Build JellyCube

Normal build (requires SD card for `config.ini`):

```bash
cd /home/projects/jellyCube
make clean
make -j$(nproc)
```

Dolphin test build (no SD card needed, embeds test credentials):

```bash
make dolphin -j$(nproc)
```

You should get `jellycube.dol` and/or `jellycube-dolphin.dol` with no errors.

## 3. Prepare the Dolphin test build

Edit `source/dolphin_test.local.c` with your Jellyfin server's IPv4 address and a test account:

```c
const jellyfin_config_t dolphin_test_config = {
    .server_address = "192.168.1.100",
    .server_port    = 8096,
    .username       = "test_user",
    .password       = "test_pass",
    .device_id      = "jellycube_dolphin",
    .client_name    = "JellyCube-Dolphin",
    .client_version = "0.2-test"
};
```

Then rebuild:

```bash
make dolphin -j$(nproc)
```

This build is for emulator testing only. Do not use it on hardware or commit real credentials.

## 4. Test in Dolphin

1. Open Dolphin.
2. Config → GameCube → SP1 set to **Broadband Adapter (TAP)** and bridge it to your LAN adapter.
3. Load `jellycube-dolphin.dol` (File → Open, or set it as the default ISO).
4. Run the DOL.

Expected output:

```
JellyCube 0.2-test | API + playlist export
No FAT SD card. Insert SD Gecko in slot A or B.
Dolphin test mode: using embedded config. NOT for hardware release.
Broadband Adapter: DHCP...
IP: 192.168.1.x
Server: 192.168.1.100:8096
[library list appears]
```

If you see `Authentication failed`, log in through a browser with the same username and password first.

### Dolphin with SD card (optional)

If your Dolphin build shows an **SD Card** option in Config → GameCube → Slot A, you can use the normal `jellycube.dol` with a virtual SD card:

- Tools → Configure Emulated SD Card.
- Sync a folder containing `apps/jellycube/config.ini` and `mplayer/`.
- Set Slot A to **SD Card**.

Some Dolphin builds do not expose GameCube SD card emulation; use the Dolphin test build above instead.

## 5. Test on hardware

1. Copy `jellycube.dol` to `carda:/apps/jellycube/jellycube.dol`.
2. Copy `config.ini` to `carda:/apps/jellycube/config.ini`.
3. Boot from an SD-capable loader.
4. Select a movie/episode and press A. The app writes `carda:/mplayer/jellycube.m3u`.
5. Launch MPlayer CE r658 and load the playlist.

## 6. Configure the SD-card build

Place `config.ini` at `carda:/apps/jellycube/config.ini` (or `cardb:/apps/jellycube/config.ini`):

```ini
server   = 192.168.1.100
port     = 8096
username = test_user
password = test_pass
device_id = jellycube_01
client_name = JellyCube
```

Rules:

- `server` must be a dotted IPv4 address. Hostnames are not supported.
- Use plain HTTP only. HTTPS is not implemented.
- Use a test account. The playlist file written to the SD card will contain the API token.

## 7. Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| `No FAT SD card` | SD Gecko not detected or wrong filesystem | Use FAT32, try slot A or B, or use `make dolphin` |
| `DHCP failed` | No BBA, no DHCP server, or Dolphin BBA not bridged | Verify network setup |
| `Authentication failed` | Bad user/password, HTTP blocked, or Jellyfin unreachable | Test login in browser |
| `Request failed` | Server returned non-200, JSON too large, or connection dropped | Check Jellyfin logs |
| `Export failed` | `/mplayer` directory missing | Create `carda:/mplayer/` |

## 8. Next steps

- Verify which Jellyfin transcode profile plays reliably on GameCube (MPEG-2 TS low-bitrate, or H.264 Baseline 512×480).
- Decide whether to embed MPlayer CE as a library, launch it as a separate DOL, or keep the playlist hand-off model.
- Add resume reporting and playback progress back to Jellyfin.
