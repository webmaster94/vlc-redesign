# Reframe security review

Reviewed 13 October 2026 for the v0.4.0 Windows release, based on VLC 3.0.24.

The 14 October v0.5.0 follow-up reviewed the new source dialogs, pin callbacks, fullscreen chrome and visualization ownership. An independent source review found an uninitialized sidebar comparison during saved header-pin setup and unsafe window-provider lifetime checks. The sidebar is now initialized before pin setup. Window creation checks the active interface and its publication under the same lock used to unpublish it during teardown. Window callbacks also require the generation of the interface that created them, so opening another interface cannot revive callbacks belonging to a destroyed one. These findings were established from source; no exploit was attempted or demonstrated.

Three independent adversarial reviews covered the new native parsers, network integration, Windows launcher and installer. The review concentrated on changes introduced by Reframe: imported profiles, subtitle sidecars, generated waveforms and thumbnails, media artwork, service responses and subprocesses. Tests used synthetic local fixtures and dummy credentials. They did not attack third-party services or run a malicious script.

## Findings and fixes

| Finding | Trigger and impact | Change |
| --- | --- | --- |
| Imported profiles could restore arbitrary VLC options | A shared profile could enable scripting interfaces, remote-control services or output destinations. Source review established the configuration-to-interface path; arbitrary code execution was not dynamically attempted. | Profiles now accept only a bounded list of appearance and media settings. Import, export, save and restore use the same validator. Unknown or unsafe imported keys reject the entire file. Existing stored profiles skip unsafe keys. |
| Credentialed requests followed redirects | An unexpected service redirect could forward API headers or login data. | Credentialed requests require the exact HTTPS API host and normal port, reject user information and fragments, and never follow redirects. Anonymous subtitle downloads send no API key or bearer token. |
| Subprocesses depended on ambient configuration and unlimited output | PATH lookup or curl configuration could change request behavior. Large responses or stalled children could consume resources. | Requests use Windows System32 curl, disable its configuration with the first `-q` argument, send credentials over stdin and enforce output caps and independent timers. Dialog destruction cancels its requests. Update links are built from a fixed repository and a validated version tag. |
| Malformed subtitle text caused excessive CPU use | The old tag-removal expression was quadratic on the shipped Qt 5.6 build. A 64 KiB synthetic input took about 1.5 seconds; a 1 MiB probe was terminated after more than 46 CPU seconds. | Linear tag removal, 8 MiB input, 8 KiB line, 2,048-character text and 20,000-cue limits. Timeline parsing only runs when enabled. |
| Waveform generation and artwork lacked resource limits | Long or malformed media could create large temporary PCM files; artwork could dispatch to extra image decoders or remote paths. | One bounded waveform job, 30-second watchdog, 32 MiB PCM cap, 64 MiB seek-cache eviction and no waveform for media longer than 30 minutes. Artwork accepts local PNG/JPEG only, at most 16 MiB and 4,096 pixels per axis, with output at most 1,024 pixels. Windows remote drives, reparse points and alternate streams are rejected. |
| Media metadata could become Qt rich text | Titles containing HTML-like text could invoke rich-text behavior. | Dynamic title and filename labels explicitly use plain text. |
| Installer could launch playback elevated | The default finish action inherited the administrator token from installation. | The finish page tells users to launch from the shortcut; it no longer runs the player from the elevated installer. |
| Additional-options input was unbounded | Large files or excessive arguments could consume resources at startup. | A 16 KiB file cap, embedded-NUL rejection and a 512-argument cap. Arguments use Windows argument parsing, with no shell interpretation. |
| Raw playlist strings could become worker options | Some playlist parsers preserve raw media strings. Preview workers appended them without terminating option parsing, allowing option-like strings to alter the child player's behavior. | Preview workers terminate option parsing with `--`; seek workers also reject option-like values and embedded NUL. Regression cases cover those values and ordinary media URIs. |

The build image is pinned by registry digest. Dependency inputs and published artifacts have SHA-256 verification; GitHub Actions dependencies are pinned by commit.

## Regression evidence

The repository's `extras/package/redesign/tests` contains probes that exercise production helpers. The Windows build workflow compiles them with the same dependency archives as the player and runs them before release:

- Profile validation: 323 assertions and 20,000 deterministic JSON mutations.
- Subtitle timeline/PCM parsing: valid fixtures, size limits, malformed timestamps and 5,000 malformed cases.
- Network helper: 27 checks covering destinations, header injection, credential transport, redirects, oversized output, timeout and destruction while a child starts.
- Artwork: 21 checks including valid raster images, disguised SVG, oversized dimensions, invalid paths and an actual temporary Windows junction.

Parser probes also passed a local Linux build with AddressSanitizer and UndefinedBehaviorSanitizer. Those sanitizer runs used host Qt 5.15; separate Windows runs used the shipped Qt 5.6 archives. Modified native sources passed syntax checks against the pinned Windows headers. Windows launcher probes covered UTF-8, literal shell punctuation, limits and `--ignore-config`. An isolated NSIS compile checked the installer script. Release smoke checks cover real playback and installation behavior.

These are targeted adversarial regression tests, not exhaustive fuzzing of VLC or every dependency.

## Remaining limits

No review can establish that this player has zero vulnerabilities. Reframe still processes untrusted media through VLC and its codec libraries; this work does not prove those libraries safe. The inherited Qt 5.6 dependency is old and outside normal support. Some VideoLAN patches are present, but a complete dependency vulnerability inventory and migration to supported Qt remain future work. Version age alone does not establish that a particular CVE applies.

The reviewed player and DLLs have DEP, ASLR flags, high-entropy VA and relocation tables. The inherited compiler build has no Control Flow Guard. The installer and cache generator lack relocation tables, so their ASLR flags do not establish effective address randomization. Installer signing remains unavailable. Checksums establish correspondence with release assets, not publisher identity.

Artwork limits can replace unsupported formats, cloud-backed images or large covers with placeholders. Filesystem checks are defensive validation, not a sandbox against another process concurrently changing the user's files. Media playback itself still permits user-requested network locations. Additional startup arguments and advanced local settings remain deliberate expert features; they are excluded from shared profiles.

OpenSubtitles live login/search/download verification remains pending the registered app key. The request helper was tested with local protocol fixtures; no claim is made that an authenticated provider round trip passed.

Sources: [VideoLAN security notices](https://images.videolan.org/security/), [VLC 3.0.24 release](https://images.videolan.org/vlc/releases/3.0.24.html), [Qt archive support warning](https://download.qt.io/archive/qt/5.6/), [OpenSubtitles integration contract](opensubtitles-api.md).
