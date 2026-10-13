# Implementation notes

Checked 12 October 2026.

## Baseline

- Upstream source: VideoLAN VLC, tag `3.0.24`, commit `6de05adcbaf2e8b85fe86aad4169393098628119`.
- Existing installed player: Windows VLC 3.0.20, confirmed from `vlc.exe` file version and the uninstall record.
- Latest stable Windows x64 release: [3.0.24 in VideoLAN's download directory](https://download.videolan.org/pub/videolan/vlc/last/win64/).
- Public fork: [webmaster94/vlc-redesign](https://github.com/webmaster94/vlc-redesign).
- Development branch: `redesign`.

## Design access

The [Figma Community page](https://www.figma.com/community/file/1691465999134370980/vlc-redesign) identifies Juxtopposed as the creator and displays CC BY 4.0. The initial canvas did not load. The user then supplied 25 exported PNGs and a view-only copy link. Its prototype was readable, and local raw `.fig` data was decoded for measurements and transition settings. That resolved the access issue.

Implemented views include the player, folder library, recent media, playlist access, global settings, audio equalizer, subtitle synchronization, and dark/light/system themes with nine accent choices. The sidebar uses a 300 ms cubic transition measured from the prototype. Advanced VLC dialogs retain their established controls with the new palette. Media cards use real file names and locally generated thumbnails; example media artwork is not copied.

## Interface approach

`modules/gui/qt/redesign.cpp` adds a native shell around VLC's video widget and playlist. Transport controls connect to `InputManager`, `MainInputManager`, and `ActionsManager`; opening media uses the existing `Open` helper. The library scans selected folders and recent media, and generates thumbnails through an isolated dummy-interface VLC subprocess. The existing equalizer supplies live gain and preset controls.

The `master` branch is a development version and has a different interface architecture. Starting from a stable tag avoids making the installable fork depend on unreleased VLC changes. This choice can be revisited if the design requires library capabilities absent from 3.0.

## Windows build

Use VideoLAN's [existing Windows build script](../../extras/package/win32/build.sh) and its [CI configuration](../../extras/ci/gitlab-ci.yml). The build validation workflow uses the exact dated image and dependency revision selected by the 3.0.24 source tree:

- Image: `registry.videolan.org/vlc-debian-win64-3.0:20260820114402`.
- Prebuilt dependency revision: `910cc9cada6080dd777fbafb2ebdd0c24f702c30`.

The workflow compiles, packages, gathers dependency source, and runs Windows integration checks. These are development checks, not a substitute for long-term playback testing. Recompute and verify dependency selection if dependencies or their build configuration change.

## Installer work

`extras/package/redesign/installer.nsi` uses NSIS Modern UI 2, as does VLC's installer. It provides welcome, GPL license, components, destination, installation, finish, and uninstall pages. Reinstallation overwrites packaged files in the fork's own directory.

Distribution choices implemented in this fork:

- Reframe name, original icon, unofficial publisher description, independent installation directory and uninstall identity, and optional Open With ProgIDs.
- Separate core/Qt preferences, cache, crash directory, IPC names, and Windows audio-session GUID.
- Disabled upstream installer updater.
- No ActiveX/NPAPI registration or legacy upstream-browser cleanup.
- Optional Open With entries which preserve default owners.
- Explicit file install/delete manifests, with no recursive uninstall deletion.
- License texts, design credit, Inter font license, exact fork source, and corresponding dependency source.

Validate a fresh install, upgrade of the fork, uninstall, and coexistence with stock VLC in an isolated Windows environment. Confirm the official installation, preferences, and associations survive those operations. Smoke-test playback, seeking, pause/resume, volume, fullscreen, subtitles, audio tracks, playlists, and opening paths with spaces/non-ASCII characters. An unsigned installer is possible; signing requires a separate signing identity and does not follow from VideoLAN's signature.
