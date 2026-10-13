# Implementation notes

Checked 12 October 2026.

## Baseline

- Upstream source: VideoLAN VLC, tag `3.0.24`, commit `6de05adcbaf2e8b85fe86aad4169393098628119`.
- Existing installed player: Windows VLC 3.0.20, confirmed from `vlc.exe` file version and the uninstall record.
- Latest stable Windows x64 release: [3.0.24 in VideoLAN's download directory](https://download.videolan.org/pub/videolan/vlc/last/win64/).
- Public fork: [webmaster94/vlc-redesign](https://github.com/webmaster94/vlc-redesign).
- Development branch: `redesign`.

## Design access

The [Figma Community page](https://www.figma.com/community/file/1691465999134370980/vlc-redesign) identifies Juxtopposed as the creator and displays CC BY 4.0. Its cover is a promotional image, not a usable interface specification. The embedded design canvas remained blank, and the available browser could not capture its screenshot. Opening the Community canvas did not provide inspectable frames. Implementation is waiting for an accessible design file or exported screens; no layout has been invented from the cover image.

Before implementation, inventory the player, library, playlists, settings, menus, compact view, fullscreen view, and their empty/loading/error states actually present in the design. Record dimensions, spacing, colors, type sizes, assets, and interactions. Review any font/icon/artwork licenses separately. Use actual user media metadata and thumbnails in place of illustrative commercial cover art.

## Interface approach

VLC 3.0.24 has a native Qt Widgets interface in `modules/gui/qt`. Its `MainInterface`, input manager, controls, and playlist components already connect to VLC's playback engine. Prefer adapting those components so opening files, playback, subtitles, audio devices, streaming, hotkeys, and the advanced dialogs keep their existing behavior. Choose the final component structure after inspecting the design.

The `master` branch is a development version and has a different interface architecture. Starting from a stable tag avoids making the installable fork depend on unreleased VLC changes. This choice can be revisited if the design requires library capabilities absent from 3.0.

## Windows build

Use VideoLAN's [existing Windows build script](../../extras/package/win32/build.sh) and its [CI configuration](../../extras/ci/gitlab-ci.yml). The build validation workflow uses the exact dated image and dependency revision selected by the 3.0.24 source tree:

- Image: `registry.videolan.org/vlc-debian-win64-3.0:20260820114402`.
- Prebuilt dependency revision: `910cc9cada6080dd777fbafb2ebdd0c24f702c30`.

The workflow is compilation validation only. It does not establish playback correctness, installer correctness, or redistribution compliance. Recompute and verify the dependency selection if upstream dependencies or their build configuration change.

## Installer work

Adapt `extras/package/win32/NSIS/vlc.win32.nsi.in` and its helpers. Preserve VLC's welcome, license, components, destination, installation, finish, maintenance, and uninstall flows.

Before producing a distributable installer:

- Set a distinct product name, icon, publisher description, installation folder, uninstall identity, application registration, ProgIDs, context-menu entries, and autoplay handlers.
- Isolate the core configuration, Qt settings, cache, crash paths, and single-instance behavior from stock VLC.
- Prevent the stock VLC updater from replacing the fork with an upstream installer. Either disable it or implement a fork-specific update mechanism.
- Avoid registering stock VLC ActiveX/NPAPI components and remove installer cleanup which targets old upstream browser plugins.
- Make file associations an explicit component choice and preserve existing association owners.
- Keep install/uninstall operations scoped to the fork's own files and registry entries.
- Include license notices, design credit, and corresponding source for the exact binary and bundled dependencies.

Validate a fresh install, upgrade of the fork, uninstall, and coexistence with stock VLC in an isolated Windows environment. Confirm the official installation, preferences, and associations survive those operations. Smoke-test playback, seeking, pause/resume, volume, fullscreen, subtitles, audio tracks, playlists, and opening paths with spaces/non-ASCII characters. An unsigned installer is possible; signing requires a separate signing identity and does not follow from VideoLAN's signature.
