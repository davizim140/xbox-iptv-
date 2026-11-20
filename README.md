# Xbox IPTV

Homebrew IPTV player for Xbox 360 RGH/JTAG, built with [XSDK](https://github.com/xenosworks/xsdk) and GitHub Actions.

## Project goals

- Streaming-style interface navigable with the Xbox 360 controller.
- Built-in public playlist catalogue, with the initial catalogue URL maintained by iptv-org.
- Import a personal M3U/M3U8 playlist.
- Channel groups, search, favourites and EPG (XMLTV).
- Playback only for streams whose protocols, containers and codecs can be supported on Xbox 360.

## Current implementation status

**Milestone 1 — playlist import and network diagnostics (in progress):**
- XSDK application entry point, 720p presentation loop and controller navigation.
- M3U `#EXTINF` metadata parser with native host tests.
- USB playlist import from `usb:\\playlist.m3u`, with channel list navigation and reload.
- Network status screen using XSDK link and IPv4 APIs; X refreshes link/IP status.
- GitHub Actions workflow that builds XSDK, compiles the XEX and uploads it as an artifact.

The current build is an interface, USB playlist import and network-diagnostics foundation, **not yet a live TV player**. HTTP playlist retrieval, media demuxing/decoding and playback will be implemented and tested as separate milestones. No working stream or codec support is claimed until verified.

## Build

The GitHub Actions workflow uses the pinned XSDK commit in `.github/workflows/build.yml`. It builds the XSDK toolchain and SDK in the Ubuntu runner, then configures this project with XSDK's CMake toolchain. The generated `xbox-iptv.xex` is uploaded as a workflow artifact only when the build succeeds.

## Controller

- D-pad Up/Down: move through the menu.
- A: open the selected section.
- B: return to the home screen.
- Back: exit to Aurora.

## Playlist sources

The public catalogue entry point is `https://iptv-org.github.io/iptv/index.m3u`. It is a playlist catalogue, not a guarantee that every listed stream is available, authorized in every territory, or compatible with the Xbox 360. Check the rights and terms for each source. Personal lists are user-supplied; do not commit private URLs, credentials or tokens.

## Safety and honesty

- No premium/pay-TV channel links, credentials, or DRM circumvention are included.
- GitHub Actions/Xenia success does not prove playback on a real Xbox 360.
- A real RGH/JTAG console test is required for hardware, network and decoder claims.
