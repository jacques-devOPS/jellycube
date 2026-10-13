# JellyCube GameCube Client

Native Jellyfin client for Nintendo GameCube. Uses Broadband Adapter for
API access and exports playlists for MPlayer CE GameCube playback.

## Verified functions (hardware)

- Builds with devkitPPC r50, GCC 16.1, libogc 3.1.
- Mounts FAT32 storage via SD2SP2 (`sd2:`) and SD Gecko (`carda:`, `cardb:`).
- Uses launch-device preference from `argv[0]` without silent fallback.
- Initializes BBA with DHCP.
- Authenticates to Jellyfin, validates `token.cache` via `/Users/Me`, and
  re-authenticates only when needed.
- Browses libraries/items and exports playlist files.
- Supports per-item audio and subtitle stream selection at export time.
- Supports named transcode profiles from `config.ini`.

## Filesystem requirement

SD2SP2 hardware can be used with multiple filesystems, but the
JellyCube/libfat path requires FAT32. Swiss exFAT support does not imply
JellyCube mount support.

## Configuration

Place `config.ini` beside `jellycube.dol`.

```ini
server = 10.0.0.179
port = 8096
username = gamecube
password = REPLACE_ME
device_id = jellycube_01
client_name = JellyCube

video_codec = mpeg2video
audio_codec = mp2
max_width = 320
max_height = 240
max_framerate = 25
video_bitrate = 1000000
audio_bitrate = 128000
audio_channels = 2
audio_sample_rate = 48000

profile_count = 3
profile1_name = GC_Safe
profile1_video_codec = mpeg2video
profile1_audio_codec = mp2
profile1_max_width = 320
profile1_max_height = 240
profile1_max_framerate = 25
profile1_video_bitrate = 1000000
profile1_audio_bitrate = 128000
profile1_audio_channels = 2
profile1_audio_sample_rate = 48000

profile2_name = GC_Balanced
profile2_video_codec = mpeg2video
profile2_audio_codec = mp2
profile2_max_width = 480
profile2_max_height = 360
profile2_max_framerate = 25
profile2_video_bitrate = 1500000
profile2_audio_bitrate = 160000
profile2_audio_channels = 2
profile2_audio_sample_rate = 48000

profile3_name = GC_High
profile3_video_codec = mpeg2video
profile3_audio_codec = mp2
profile3_max_width = 640
profile3_max_height = 480
profile3_max_framerate = 25
profile3_video_bitrate = 2200000
profile3_audio_bitrate = 192000
profile3_audio_channels = 2
profile3_audio_sample_rate = 48000
```

## Playlist output

- File path: `<volume>:/mplayer/<sanitized_title>.m3u`
- Episode naming uses series/season/episode when available.
- Movie naming uses title and year when available.

Keep `/mplayer` at card root.

## Token cache

`token.cache` is stored beside `config.ini`. Exported playlists embed the
current token. Token rotation can invalidate older playlists.

## Runtime UX notes

- Storage diagnostics at startup are removed for faster launch.
- After export/cancel/fail, JellyCube now shows an explicit confirmation
  screen with `A: back` and `START: exit`.

## MPlayer CE dependency note

Current GameCube MPlayer CE path requires `nocache=yes` for reliable HTTP
playback in this workflow.
