# Reframe

An unofficial Windows fork of [VideoLAN's VLC media player](https://github.com/videolan/vlc), based on VLC 3.0.24.

Its native Qt interface adapts [VLC Redesign by Juxtopposed](https://www.figma.com/community/file/1691465999134370980/vlc-redesign), published under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).

## Current status

The interface includes a dark player, a collapsible sidebar, a single transport bar, folder-based media browsing with generated video thumbnails, recent media, playlists, settings, and selectable themes and accent colors. VLC's playback engine, hotkeys, media dialogs, and advanced preferences remain available.

Build artifacts are development builds until verification passes. The installer is unsigned. This fork is not an official VideoLAN release.

## Development

Development is on the `redesign` branch. The upstream 3.0.24 commit is `6de05adcbaf2e8b85fe86aad4169393098628119`.

The **Windows build and installer** workflow uses VideoLAN's Windows x64 build image and matching prebuilt dependencies. It packages an NSIS executable, a portable player, the exact fork source, dependency source, and SHA-256 checksums. A Windows job checks rendering, media decoding, installation, reinstallation, removal, and coexistence with stock VLC.

The installer has welcome, license, component, destination, install, and finish pages. Reframe uses its own installation directory, preferences, IPC identity, audio-session identity, and uninstall entry. Optional Open With registration preserves existing file-association defaults. The uninstaller deletes packaged files individually and retains user files and preferences.

See [the implementation notes](doc/redesign/implementation.md) and [the licensing findings](doc/redesign/licensing.md).

## Licensing and credits

VLC's existing source licenses and notices remain in force. See [COPYING](COPYING) and [COPYING.LIB](COPYING.LIB), and the notices in individual files. Distributing a modified executable requires access to the corresponding source, including modifications and build/installation scripts.

Design reference: **VLC Redesign**, by **Juxtopposed**, [original resource](https://www.figma.com/community/file/1691465999134370980/vlc-redesign), [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). Changes include the native implementation, functional media browsing, product identity, and hand-drawn icons. Illustrative media artwork is not bundled.

The unmodified Inter 3.19 fonts are bundled under the SIL Open Font License 1.1. Their copyright and license are included in the installed `fonts/LICENSE.txt`.

Reframe is not endorsed by VideoLAN or Juxtopposed. Installed credits and the About page identify the upstream authors and design creator.
