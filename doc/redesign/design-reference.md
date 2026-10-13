# Design measurements and interaction reference

Source: [VLC Redesign by Juxtopposed](https://www.figma.com/community/file/1691465999134370980/vlc-redesign), CC BY 4.0. The user supplied exported frames and a view-only copy for inspection on 12 October 2026.

Main player frames are 1920 by 1080 logical pixels, with 1056 by 594 variants. The global settings frame is 1048 by 865. Typography is Inter. Common dark surfaces include `#0e0e0e`, `#191919`, and `#1b1b1b`; common text/icon grays include `#8f8f8f`, `#969696`, and `#c6c6c6`. The primary orange in the decoded paints is `#d37918`.

The prototype contains 300 ms Smart Animate transitions with `INOUT_CUBIC`, and approximately 208 ms spring transitions. It also uses instant transitions for menus and state changes. The native sidebar uses a 300 ms cubic animation. Fullscreen remains driven by VLC and Windows.

The player has a compact navigation rail, a video area, and one transport row. The library uses an expanded sidebar, navigation/search tools, and media cards. Settings use category navigation and appearance choices. Example video art and collections are references only; the application displays actual user files and thumbnails.

The unmodified Inter 3.19 Windows-hinted Regular and Medium fonts come from the [Inter project's release](https://github.com/rsms/inter/releases/tag/v3.19), with its SIL OFL 1.1 notice.
