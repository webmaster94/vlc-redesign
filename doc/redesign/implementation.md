# Implementation notes

Checked 14 October 2026.

## Baseline

- Upstream source: VideoLAN VLC, tag `3.0.24`, commit `6de05adcbaf2e8b85fe86aad4169393098628119`.
- Existing installed player: Windows VLC 3.0.20, confirmed from `vlc.exe` file version and the uninstall record.
- Latest stable Windows x64 release: [3.0.24 in VideoLAN's download directory](https://download.videolan.org/pub/videolan/vlc/last/win64/).
- Public fork: [webmaster94/vlc-redesign](https://github.com/webmaster94/vlc-redesign).
- Development branch: `redesign`.

## Design access

The [Figma Community page](https://www.figma.com/community/file/1691465999134370980/vlc-redesign) identifies Juxtopposed as the creator and displays CC BY 4.0. The initial canvas did not load. The user then supplied 25 exported PNGs and a view-only copy link. Its prototype was readable, and local raw `.fig` data was decoded for measurements and transition settings. That resolved the access issue.

The creator's [walkthrough](https://www.youtube.com/watch?v=ERFeOgqk324) and the decoded prototype were used to inventory panels, interactions and transitions. The former preference, effects, subtitle and toolbar dialogs are replaced by the redesign's own native panels and editors. Advanced module options are built into their settings category from VLC's configuration schema. Files, folders, discs, capture devices and network streams have separate opening windows. Disc and capture backends retain their native controls inside these windows; transcoding retains its existing backend.

## Implemented coverage

| Design area | Implementation |
| --- | --- |
| Player and chrome | Thin transport, remaining time, proximity reveal, full/UI-hidden states, configurable mouse actions and auto-hide |
| Seek interactions | Hover thumbnails, draggable SRT/VTT timeline, cached audio waveform |
| Live media panels | Playback, Video, Audio, Subtitles; filter controls, presets, live synchronization and font appearance |
| Track pickers | Current embedded video/audio/subtitle choices, external tracks and actual audio-output devices |
| Queue | Actual playlist items, drag ordering, durations and total, Chapters and Bookmarks |
| Library | Full and compact views, folder breadcrumbs, search, sorting, grid/list, media properties and context actions |
| Music and Home | Local metadata, Songs/Albums/Artists/Genres, cover art, continue watching, listening history, frequent destinations and pins |
| Collections | Named, editable and exportable saved playlists; named network collections |
| Batch tracks | Filename/episode matching, manual assignments, links, persistent audio/subtitle bindings and OpenSubtitles search/download |
| Global preferences | Thirteen categories, search, category-local advanced controls, recording, performance and extensions |
| Customization | Toolbar, second row, title bar and sidebar drop zones; real control and metadata items; Save, Cancel and Reset |
| Profiles | Native settings and layout snapshots, apply, rename, delete, JSON import/export; password fields excluded |
| First run and shortcuts | Three onboarding pages; editable normal/global hotkey table with search, Reset, Save and Cancel |

Transitions use the prototype's actual easing: normalized damped springs for 208 ms expansion and 104 ms local panels, 300 ms cubic Bezier chrome movement, 600 ms customization movement and 50 ms icon hover. Entries marked instant in Figma remain instant.

Windows-specific behavior includes remembered window geometry, reversed wheel directions, notification duration, native renderer discovery/selection, startup options parsed by the Windows launcher, and optional pausing when another process produces audio. Scope and Waveform are separate visualizer modes.

OpenSubtitles is implemented against the official current REST contract. Publisher registration and a valid application key are external prerequisites for live service operation. The key can be supplied by the packager; user account credentials are optional for anonymous service allowances and never embedded in source. Returned file links are fetched over HTTPS without forwarding the API key or token.

## Interface approach

`modules/gui/qt/redesign.cpp` adds a native shell around VLC's video widget and playlist. Transport controls connect to `InputManager`, `MainInputManager`, and `ActionsManager`; opening media uses the existing `Open` helper. The library scans selected folders and recent media, and generates thumbnails through an isolated dummy-interface VLC subprocess. Native gain and preset controls use VLC's equalizer. Small changes to the FreeType and libass modules make the live subtitle controls effective during playback.

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
