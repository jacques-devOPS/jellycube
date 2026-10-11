# JellyCube GameCube Client

A native Jellyfin client for the Nintendo GameCube Broadband Adapter.
Derives from the JellyCube project. Pairs with the MPlayer CE GameCube
port (jacques-devOPS/mplayer-ce-gc) for playback.

## Verified functions (hardware)

- Builds with devkitPPC r50 / GCC 16.1 / libogc 3.1.
- Mounts FAT32 storage through SD2SP2 (`sd2:`) and SD Gecko
  (`carda:`, `cardb:`) with per-device startup/inserted/mounted
  diagnostics.
- Uses the launcher's `argv[0]` physical device preference; no silent
  different-card fallback for a known launch device.
- Brings up the BBA with DHCP.
- Authenticates to Jellyfin (username/password), validates cached
  tokens via `/Users/Me`, re-authenticates only on rejection, and
  persists the token beside `config.ini` in `token.cache`.
- Lists libraries and items; selects an item and exports an M3U.
- Requests a progressive MPEG-TS transcode (MPEG-2 + MP2).
- End-to-end verified with the MPlayer CE GameCube port using
  `nocache=yes`.

## Filesystem requirement (application path, not hardware)

SD2SP2 hardware is not limited to FAT32, and Swiss reads exFAT fine.
The **JellyCube / libfat path requires FAT32**: an exFAT card mounts
as `0` in the boot diagnostics. Format the card used by JellyCube as
FAT32. exFAT support is future work.

## Configuration

`config.ini` beside `jellycube.dol` (never commit real credentials):

```ini
server = 10.0.0.179
port = 8096
username = gamecube
password = REPLACE_ME
device_id = jellycube_01
client_name = JellyCube
```

## Token cache

Jellyfin revokes the previous access token when the same device
authenticates again, which invalidated previously exported playlists.
`token.cache` beside `config.ini` holds the current token; export is
only valid while that token lives. Playlists embed the token: delete
them after testing.

## Playlist output

JellyCube writes `<volume>:/mplayer/jellycube.m3u` (e.g.
`sd2:/mplayer/jellycube.m3u`). Keep a `mplayer/` folder at the card
root. MPlayer CE loads it via Open → Playlist → SD2SP2.

## Transcode request

```
Static=false
VideoCodec=mpeg2video&AudioCodec=mp2
VideoBitRate=1000000&AudioBitRate=128000
MaxWidth=320&MaxHeight=240&MaxFramerate=25
AudioChannels=2&AudioSampleRate=48000
EnableAutoStreamCopy=false
AllowVideoStreamCopy=false&AllowAudioStreamCopy=false
```

Jellyfin transcodes server-side; JellyCube does not decode media.

## Known MPlayer CE integration constraint

HTTP playback through the MPlayer CE GameCube stream-cache path fails;
the same content plays with the cache disabled. Until that port
repairs its cache path, `mplayer.conf` must contain `nocache=yes`.
