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
- HTTP URL parser and bounded HTTP/1.1 GET transport over XSDK sockets, with DNS resolution and 10-second socket timeouts; native URL parsing tests. HTTPS URLs are recognized but network fetch rejects TLS until certificate-validated mbedTLS is integrated. Chunked transfer and redirect handling are not yet implemented.
- HLS master/media manifest parser with variant bandwidth/resolution, segment duration/title, media sequence, target duration, end-list and encryption detection; resolves absolute and relative HTTP(S) segment URIs. Native parser tests pass in CI when the workflow completes. AES-128 decryption and media segment playback are not implemented.
- Automatic external-storage detection: scans USB mount points (`usb:\\`, `usb0:\\`, `usb1:\\`, `usb2:\\`), showing only roots that can be opened.
- Controller-driven folder browser for detected external devices: navigate folders, list `.m3u` and `.m3u8` files, select one, then browse its channels. Internal HDD and DVD are intentionally excluded.
- The browser scans up to 96 folders/M3U files per directory; device detection is based on XSDK filesystem mount availability and still requires testing on a physical RGH/JTAG console.
- Network status screen using XSDK link and IPv4 APIs; X refreshes link/IP status.
- Channel favourites: Y toggles the selected channel; favourites are saved to `xbox-iptv-fav.dat` on the currently selected USB root and loaded again when that device is selected. Without a selected writable USB device, changes remain only in memory.
- GitHub Actions workflow that builds XSDK, compiles the XEX and uploads it as an artifact.

The current build is an interface, USB playlist import and network-diagnostics foundation, **not yet a live TV player**. HTTP/HTTPS playlist retrieval, XMLTV EPG parsing, media demuxing/decoding and playback remain separate milestones. The current XSDK tree has sockets and PCM audio output, but no integrated video demuxer/decoder; no working stream or codec support is claimed until implemented and verified.

## Build

The GitHub Actions workflow uses the pinned XSDK commit in `.github/workflows/build.yml`. It builds the XSDK toolchain and SDK in the Ubuntu runner, then configures this project with XSDK's CMake toolchain. The generated `xbox-iptv.xex` is uploaded as a workflow artifact only when the build succeeds.

## Controller

- D-pad Up/Down: move through the menu.
- A: open the selected section.
- B: return to the previous screen (channel list to folder browser; browser to home).
- Back: exit to Aurora.

## Playlist sources

The public catalogue entry point is `https://iptv-org.github.io/iptv/index.m3u`. It is a playlist catalogue, not a guarantee that every listed stream is available, authorized in every territory, or compatible with the Xbox 360. Check the rights and terms for each source. Personal lists are user-supplied; do not commit private URLs, credentials or tokens.

## Safety and honesty

- No premium/pay-TV channel links, credentials, or DRM circumvention are included.
- GitHub Actions/Xenia success does not prove playback on a real Xbox 360.
- A real RGH/JTAG console test is required for hardware, network and decoder claims.
