# Reframe

An unofficial Windows fork of [VideoLAN's VLC media player](https://github.com/videolan/vlc), based on VLC 3.0.24. Its native Qt interface implements [VLC Redesign by Juxtopposed](https://www.figma.com/community/file/1691465999134370980/vlc-redesign), licensed [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).

The player includes live Playback, Video, Audio and Subtitles panels; direct track and output-device selection; a queue with Chapters and Bookmarks; full and compact libraries; a movable mini player; seek thumbnails, subtitle timing and waveform displays; saved playlists and network collections; music metadata groups; pins and resume history. Global settings have thirteen categories, search and expandable advanced options. Onboarding, editable shortcuts, profiles and drag/drop customization use the supplied design's layouts and measured transitions.

Windows x64 installers and portable builds are published in [Releases](https://github.com/webmaster94/vlc-redesign/releases). The installer is unsigned. Reframe is not an official VideoLAN release.

## Build and verification

Development is on `redesign`, starting from upstream commit `6de05adcbaf2e8b85fe86aad4169393098628119`. The [Windows build workflow](.github/workflows/redesign-build.yml) uses VideoLAN's dated build image and matching dependency revision. It produces an NSIS installer, a portable player, corresponding fork and dependency source, notices and SHA-256 checksums.

Release checks exercise real media decoding, pause/resume/seek, equalizer persistence, subtitle timing, all thirteen settings pages and the local panels and creation dialogs. Fresh-profile checks cover privacy dialog ordering, centering, opt-out persistence and relaunch. Onboarding checks cover the appearance layout, exclusive accent selection, live theme changes and twelve shortcut rows with icons and keycaps. Generated Ogg and video fixtures cover creating a playlist from the library selection, saving and playing it, visible queue rows, fullscreen hiding and Escape, library navigation, mini-player placement, resize grips, dropdown placement and advanced-panel transitions. They also check layout overflow, the Waveform visualizer, additional startup options, paths with spaces and non-ASCII characters, installation, reinstallation, removal and coexistence with stock VLC. Artifacts are published as releases after these checks pass. These checks do not cover every codec, device or streaming service.

The installer follows VLC's NSIS wizard sequence: welcome, license, components, destination, installation and finish. Reframe has its own installation directory, preferences, IPC identity, audio-session identity and uninstall entry. Optional Open With registration preserves existing default associations. Uninstallation removes packaged files individually and retains user files and preferences.

## OpenSubtitles

Open **Add Subtitle Tracks**, choose **From OpenSubtitles**, and sign in with your own OpenSubtitles account. Search by title and language, select a result, then download and assign it to your media. Your password and session token are kept in memory and are not included in saved profiles.

Online search is unavailable in builds without an OpenSubtitles connection. You can still use **Open website** to download subtitles, then add the file or a subtitle link. Developers can find connection setup in the [OpenSubtitles integration guide](doc/redesign/opensubtitles-api.md).

Additional command-line options in General Settings apply on the next launch. Explicit command-line arguments take precedence. Start with `--ignore-config` to bypass saved additional options.

Shared profiles contain appearance and media settings. Startup arguments, scripting, remote-control services, file paths and credentials are excluded.

See the [security review](doc/redesign/security-audit.md) for fixed issues, adversarial tests and remaining limits.

## Licensing and credits

The combined Reframe release and new interface code use GPL-3.0-or-later. Existing VLC and dependency source licenses and notices remain in force. See [COPYING.GPL3](COPYING.GPL3), [COPYING](COPYING), [COPYING.LIB](COPYING.LIB) and individual file headers. Public modified-source forks and binary distribution are permitted subject to the applicable source and notice obligations; releases include corresponding source and build scripts.

Design: VLC Redesign by Juxtopposed, [original resource](https://www.figma.com/community/file/1691465999134370980/vlc-redesign), [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). Changes include the native implementation, functional media browsing, product identity and hand-drawn icons. Illustrative media artwork is not bundled.

Unmodified Inter 3.19 fonts are bundled under the SIL Open Font License 1.1, included in `fonts/LICENSE.txt`. Installed credits and About identify the upstream authors and design creator. Reframe is not endorsed by VideoLAN or Juxtopposed.
