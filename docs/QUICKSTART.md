# JellyCube Quick Start

This guide builds a versioned GameCube DOL and tests Jellyfin connectivity with a real Broadband Adapter.

## 1. Toolchain

Install the GameCube development group on Debian-derived Linux:

```bash
curl -A 'dkp apt' -fsSL https://apt.devkitpro.org/install-devkitpro-pacman -o install-devkitpro-pacman
chmod +x install-devkitpro-pacman
sudo ./install-devkitpro-pacman
sudo dkp-pacman -S --needed gamecube-dev
```

Set the required environment variables:

```bash
export DEVKITPRO=/opt/devkitpro
export DEVKITPPC=/opt/devkitpro/devkitPPC
```

## 2. Configure the hardware build

JellyCube reads this file from an SD Gecko:

```text
carda:/apps/jellycube/config.ini
```

Card slot B also works:

```text
cardb:/apps/jellycube/config.ini
```

Create this SD-card layout:

```text
SD card
└── apps
    └── jellycube
        ├── config.ini
        └── jellycube-<version>+<commit>-gamecube.dol
```

Copy `config.ini.example` as `config.ini`, then edit it:

```ini
server = <JELLYFIN_IP>
port = 8096
username = test_user
password = test_password
device_id = jellycube_01
client_name = JellyCube
```

Rules:

- `server` must be a literal IPv4 address.
- Use Jellyfin HTTP, normally port `8096`.
- Use a dedicated test account.
- The client does not support HTTPS or DNS names.

## 3. Build a release

From the project root:

```bash
cd /home/projects/jellyCube
export DEVKITPRO=/opt/devkitpro
export DEVKITPPC=/opt/devkitpro/devkitPPC
make release -j$(nproc)
```

This command:

1. Runs a clean build for hardware and Dolphin targets.
2. Packages versioned DOL files in `dist/`.
3. Writes a manifest containing SHA-256 checksums.
4. Removes transient DOL, ELF, `build/`, and `build-dolphin/` output from the project root.

Example artifact names for version `0.2.1-dev`:

```text
dist/jellycube-v0.2.1-dev+<commit>-gamecube.dol
dist/jellycube-v0.2.1-dev+<commit>-dolphin.dol
dist/jellycube-v0.2.1-dev+<commit>-manifest.txt
```

Use the `-gamecube.dol` artifact on real hardware.

## 4. Test on real hardware

Requirements:

- GameCube Broadband Adapter.
- DHCP service on the LAN.
- FAT-formatted SD Gecko in slot A or B.
- Jellyfin reachable through HTTP from the GameCube subnet.
- A GameCube homebrew loader that can launch DOL files from SD.

Steps:

1. Copy the versioned `-gamecube.dol` artifact to `carda:/apps/jellycube/`.
2. Copy the edited `config.ini` to `carda:/apps/jellycube/config.ini`.
3. Boot the DOL with the homebrew loader.
4. Record the console output exactly.

Expected network sequence:

```text
JellyCube 0.2-test | API + playlist export
Server: <JELLYFIN_IP>:8096
Broadband Adapter: DHCP...
IP: <gamecube-address>  Gateway: <gateway-address>
```

A successful API test continues with a library list. If it fails, record the first diagnostic line after DHCP.

## 5. Diagnose network failure

| Console output | Meaning | Next check |
|---|---|---|
| `DHCP failed` | The BBA did not obtain a lease. | Cable, adapter, router DHCP, VLAN configuration. |
| `Network: TCP connect ... failed` | The GameCube has an IP but cannot establish TCP with Jellyfin. | Server IP, port 8096 listener, firewall, router ACL, VLAN routing. |
| `Network: HTTP request transmission failed` | TCP connected but request transmission failed. | Ethernet stability, BBA driver behavior, server connection log. |
| `Network: HTTP response reception failed` | Request sent but no valid HTTP response arrived. | Jellyfin access log, reverse proxy, server response headers. |
| `JSON: response parse failed` | Server response arrived but parser rejected it. | Save Jellyfin response and inspect server/proxy output. |
| `Jellyfin: authentication response omitted token or user ID` | Jellyfin returned a response without expected login values. | Credentials and Jellyfin authentication log. |

## 6. Dolphin test build

Dolphin does not replace the real-BBA test. It is useful for console/UI checks.

1. Copy the template:

```bash
cp source/dolphin_test.local.c.example source/dolphin_test.local.c
```

2. Edit `source/dolphin_test.local.c` with a test server and account.
3. Build:

```bash
make dolphin -j$(nproc)
```

The local file is ignored by Git. The Dolphin DOL has embedded credentials and must not be treated as a hardware release.

## 7. Playback status

Library browsing is the current gate. MPlayer CE playback is not verified. Do not treat M3U export or the selected transcode profile as working until a real GameCube plays one test item.
