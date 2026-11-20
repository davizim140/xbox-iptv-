# Xbox IPTV

Homebrew IPTV player for Xbox 360 RGH/JTAG, built with [XSDK](https://github.com/xenosworks/xsdk) and GitHub Actions.

## Project goals

- Streaming-style interface navigable with the Xbox 360 controller.
- Built-in public playlist catalogue, with the initial catalogue URL maintained by iptv-org.
- Import a personal M3U/M3U8 playlist.
- Channel groups, search, favourites and EPG (XMLTV).
- Playback only for streams whose protocols, containers and codecs can be supported on Xbox 360.

## Current implementation status

**Milestone 1 — playlist import and network diagnostics:**
- XSDK application entry point, 720p presentation loop and controller navigation.
- M3U `#EXTINF` metadata parser with native host tests.
- Bounded remote M3U downloader built on pinned curl-xenon + mbedtls-xenon. Supports HTTP/HTTPS, redirects, chunked transfer handled by libcurl, 15-second connect timeout, 45-second total timeout, 2 MiB response cap and certificate/hostname validation using the bundled Mozilla CA file.
- HLS master/media manifest parser with variant bandwidth/resolution, segment duration/title, media sequence, target duration, end-list and encryption detection; resolves absolute and relative HTTP(S) segment URIs. AES-128 decryption and media segment playback are not implemented.
- XMLTV programme parser extracts channel IDs, start/stop timestamps, titles and descriptions with basic XML entity decoding. It is parser groundwork only; remote EPG download, time-zone conversion, channel matching and on-screen schedule are not yet wired.
- Automatic external-storage detection: scans USB mount points (`usb:\\`, `usb0:\\`, `usb1:\\`, `usb2:\\`), showing only roots that can be opened.
- Controller-driven folder browser for detected external devices: navigate folders, list `.m3u` and `.m3u8` files, select one, then browse its channels. Internal HDD and DVD are intentionally excluded.
- The browser scans up to 96 folders/M3U files per directory; device detection is based on XSDK filesystem mount availability and still requires testing on a physical RGH/JTAG console.
- Network status screen using XSDK link and IPv4 APIs; X refreshes link/IP status.
- Live catalogue screen: press X to download the public iptv-org Brazil M3U over validated HTTPS to the first detected external USB root (xbox-iptv-online.m3u), then load up to 64 channels. The previous downloaded file is retained if a new download fails.
- The XEX artifact includes cacert.pem; keep it in the same folder as xbox-iptv.xex so certificate validation can find the trust store.
- Channel favourites: Y toggles the selected channel; favourites are saved to `xbox-iptv-fav.dat` on the currently selected USB root and loaded again when that device is selected. Without a selected writable USB device, changes remain only in memory.
- GitHub Actions workflow that builds XSDK, compiles the XEX and uploads it as an artifact.

The application now downloads a remote playlist, but it is **still not a live TV player**. HLS manifest and XMLTV parsers are present, but HLS segment retrieval, adaptive bitrate selection, AES-128 HLS decryption, remote EPG download/channel matching/on-screen schedule, media demuxing, video decoding, audio decoding, audio/video synchronization and playback are not implemented. The app currently displays channel names and stores stream URLs; it does not render video or play audio. No working stream or codec support is claimed.

## Build

The GitHub Actions workflow uses the pinned XSDK commit in `.github/workflows/build.yml`. It builds the XSDK toolchain and SDK in the Ubuntu runner, then configures this project with XSDK's CMake toolchain. The generated xbox-iptv.xex and cacert.pem are uploaded as the xbox-iptv-xex artifact only when the build succeeds. Copy both files into the same folder on an external USB device before launching the XEX from Aurora.

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
