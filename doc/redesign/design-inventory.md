# VLC redesign behavior inventory

Audited 12 October 2026. This is a behavior inventory, not a declaration that the implementation is complete.

## Evidence and method

The primary design is [VLC Redesign by Juxtopposed](https://www.figma.com/community/file/1691465999134370980/vlc-redesign), licensed CC BY 4.0. The local decoded `.fig` document is `vlc-redesign-research/decoded-design.json`, 12,563 nodes. This inventory traverses parent GUIDs and includes instance `symbolData.symbolOverrides.textData.characters`, because a text-only traversal misses menu labels and reused controls. GUIDs below identify the exact source nodes. Variant duplicates are grouped; sample show titles and album art are content examples, not required bundled media.

The exported `frames/Subtitles.png`, `frames/Settings - audio.png`, and `frames/Player-4.png` were also visually inspected. They confirm the compact live-panel styling, wide global EQ editor, App/Media/System settings grouping, and thin player chrome.

The creator's [21-minute walkthrough](https://www.youtube.com/watch?v=ERFeOgqk324) was inspected through YouTube's transcript UI. The direct caption endpoint returned an empty response. The transcript is auto-generated; timings below are approximate and its spelling is not authoritative. No complete transcript is copied here.

## Walkthrough checkpoints

- 2:26-2:48 distinguishes the current playback queue from saved playlists.
- 3:20-4:00 specifies seek previews and fullscreen chrome revealed by proximity hover.
- 4:24-4:52 specifies direct subtitle/audio tracks and audio-output selection.
- 6:02-6:16 specifies persistent subtitle matching across a file, folder, or playlist.
- 6:49-8:33 explains a draggable subtitle timeline, live side panels, exact size values, and saved show presets.
- 10:19-11:50 explains sorting, folder navigation, compact library beside playing video, and movable miniplayer/queue.
- 12:04-13:46 covers music metadata categories, named network playlists, pinned destinations, and Home sections.
- 15:08-15:55 covers accessible bookmarks, chapters, and a separate hotkey editor.
- 16:25-18:43 puts advanced controls within their settings category and groups Performance, Recording, and Extensions separately.
- 19:12-19:52 specifies drag/drop customization, profiles holding layouts and settings, and onboarding.

## Distinct user-facing screens and behavior

| Area | Intended behavior | Initial native implementation gap |
| --- | --- | --- |
| Video player | Thin bottom transport, prominent play control at left, title/menu at top, direct track/audio/queue actions; normal/fullscreen/UI-hidden states | Basic native video container and transport exist; the source design's queue and live right-side tabs need dedicated implementations |
| Fullscreen chrome | Top and bottom UI appear when pointer approaches their region, then disappear when it moves away | Hiding all shell chrome through minimal mode is not the same behavior |
| Seekbar | Thick seek area with hover thumbnail and time; duration can toggle to remaining time; optional waveform/subtitle cues | A standard slider and two time labels cover seeking only |
| Queue | Add/drag items, reorder, remove, play, display durations and total; adjacent Chapters and Bookmarks tabs | Stock playlist view retains functionality but is not the designed queue surface |
| Library | Folder hierarchy, clickable breadcrumb segments, back/forward, grid/list, sort menu, search, per-item context menus; compact beside video | Single-directory scan and replacement of video by full library are partial |
| Home | Continue Watching, Listen Again, frequently used folders/playlists/artists, recent streams | Recents reused as Home does not cover these separate sections |
| Music | Songs/Albums/Artists/Genres, metadata grouping, large cover art, optional waveform | A file listing in the Music folder is partial |
| Network | Saved sources/categories plus multiline network URLs and optional saved playlist name | Stock Open Network dialog is not the designed named collection flow |
| Saved playlists | Named and thumbnailed collections distinct from queue; create/edit/export/add items | Displaying the stock current playlist is not a saved-playlist library |
| Pinning | Favorite destinations/files/collections accessible in sidebar | Not present in initial shell |
| Resume playback | Continue Watching prompt near playback controls and remembered history | Existing resume panel functionality should be relocated/styled, not lost |
| Live settings | Separate Playback/Video/Audio/Subtitles tabs beside the video, each linking its global defaults and advanced controls | Initial effects/subtitle icons open global dialogs instead |
| Global settings | Thirteen functional categories, normal controls plus expandable category-local advanced controls; search and explanations | Initial twelve tabs contain sparse controls and buttons to old preferences |
| Onboarding | Choose media folder; select light/dark/system and accent; introduce basic hotkeys; Back/Next/Finish | Not supplied by a privacy-only first-run dialog |
| Customization | Drag buttons between available inventory and toolbar; reorder controls and metadata; cancel/save; alternate layouts | Native VLC toolbar editor alone does not customize the new shell |
| Profiles/presets | Media settings presets and app profiles; profile changes restore matching layout/settings | Initial shell has no profile store or selector |
| Keyboard shortcuts | Search/filter; Action, Hotkey, Global Hotkey columns; edit/reset/save/cancel | Static shortcut text is insufficient |
| Batch subtitle/audio tracks | File/folder/playlist target; computer/OpenSubtitles/link origin; automatic matching plus manual per-item assignment; persist bindings | Single currently playing subtitle attachment is insufficient |

The initial implementation assessment refers to `modules/gui/qt/redesign.cpp` before this audit's implementation pass. Re-check each gap against the latest code and runtime; new work can supersede it.

## Local playback-side panels

These are control-bar component variants, not global defaults. Embedded track choices must represent the actual current media. The playback panel contains speed, reverse, visualizer, sleep timer, statistics, and subtitle-timeline toggles. The audio/subtitle/video track pickers are separate compact states from their expanded effects panels.

### queue

Source node `491:1781`. Controls and supplied labels:

Queue; Chapters; Bookmarks; 4 items - 2h 3m; The Wand_S01E11.mp4; 23:06; The Wand_S01E12.mp4; 24:18; The Wand_S01E13.mp4; 25:00; The Wand_S01E14.mp4; 22:35.

### subtitles

Source node `491:2216`. Controls and supplied labels:

Subtitles; Audio; Video; Embedded Tracks; None; English; Japanese; Show Preferences; External Tracks; The_Wand_S01_EN.srt.

### audio

Source node `496:2495`. Controls and supplied labels:

Video; Audio; Subtitles; Embedded Tracks; None; Japanese; English; Output Device; Speakers; Headphones; Show Preferences.

### video

Source node `498:2674`. Controls and supplied labels:

Video; Audio; Subtitles; Embedded Tracks; Track 1; None; Show Preferences.

### video settings

Source node `498:2937`. Controls and supplied labels:

Video; Audio; Subtitles; Playback; Filters; Essential; Crop; Colors; Geometry; Expand Settings; Overlay; Advanced; Defaults; Rotate; 0 deg; Horizontal; None; Vertical; Flip; Default; Zoom; 1:1 Original; Aspect Ratio; HDR; Auto; Resolution; Best Available; Render; Local; Open Advanced Settings.

### audio settings

Source node `536:1059`. Controls and supplied labels:

Video; Audio; Subtitles; Playback; 2 Pass; Equalizer; Adjust Pitch; 4.0 semitones; Spatializer; Stereo Widener; Audio Track Delay; 0.000 s; -10 s; 0 s; Audio Track; 1; Audio Language; English; Synchronization; Filters; Defaults; Stereo Mode; Test; Mono; Compressor; Open Advanced Settings.

### subtitle settings

Source node `763:1958`. Controls and supplied labels:

Playback; Audio; Subtitles; Video; Subtitle Track Delay; -4.000 s; Subtitle Speed; 1.000 fps; Subtitle Duration Factor; 0.000; -10 s; 0 s; +10 s; Subtitle Font; Inter; Size Values; Text Color; #B8B8B8; Outline Color; #0C0C0C; Font Size; Auto; Outline Thickness; Normal; Background Opacity; 0 %; Subtitle Offset; 0px; Embedded Sub Format; Subtitle Track; 1; Subtitle Language; English; Synchronization; Appearance; Defaults; Open Advanced Settings.

### playback

Source node `2238:9432`. Controls and supplied labels:

Playback; Video; Audio; Subtitles; Show Subtitle Timeline on Seekbar; Show Statistics; Reverse; Audio Visualizer; Sleep Timer; Off; Speed; 1x; 0.25; 0.5; 0.75; 1; 1.25; 1.5; 1.75; 2.

### sub timeline

Source node `3744:12361`. Controls and supplied labels:

00:01:50*; 00:02:40*.

### bookmarks

Source node `6320:13763`. Controls and supplied labels:

Queue; Bookmarks; Chapters; 2 Bookmarks; DON’T BLINK 1; 00:00:42:023; DON’T BLINK 2; 00:00:44:125.

### chapters

Source node `6321:14024`. Controls and supplied labels:

Queue; Bookmarks; Chapters; 1; 00:02:15; Winter; Spring; 00:29:45; 2; 3; Summer; 00:51:30; 4; Fall; 01:13:50; 5; Winter (5 Years Later); 01:44:10.

## Expandable effects and selectors

EQ defaults are 10 VLC-frequency bands, 60 Hz through 16 kHz, plus preamp. The design contains both expanded vertical effect panels and a separate wide EQ editor; treat them as two presentations of the same effect state. Presets need preview icons.

### Audio - Equalizer

Source nodes `6114:13610`.

Expand Settings; Collapse Settings; Preamp; 12.0 dB; 60 Hz; 0.0 dB; 170 Hz; 310 Hz; 600 Hz; 1 KHz; 3 KHz; 6 KHz; 12 KHz; 14 KHz; 16 KHz.

### Audio - Compressor Settings

Source nodes `536:1513`.

Expand Settings; Collapse Settings; FM5 / Peak; 0.2; Attack; 25.0 ms; Release; 100.0 ms; Threshold; -11.0 dB; Ratio; 4.0 : 1; Knee Radius; 5.0 dB; Makeup Gain; 7.0 dB.

### Audio - Spatializer Settings

Source nodes `536:1703`.

Expand Settings; Collapse Settings; Size; 8.0; Width; 10.0; Wet; 4.0; Dry; 5.0; Damp.

### Audio - Stereo Widener

Source nodes `612:1223`.

Expand Settings; Collapse Settings; Delay Time; 20.0 ms; Feedback Gain; 0.2 %; Crossfeed; 0.3 %; Dry Mix; 0.8 %.

### Video - Essential

Source nodes `536:1855`.

Expand Settings; Collapse Settings; Hue; Contrast; Saturation; Gamma; Sharpen Sigma; Banding Removal Radius; Film Grain Variance; Brightness.

### Video - Crop

Source nodes `543:990`.

Expand Settings; Collapse Settings; Top; 0 px; Bottom; Left; Right.

### Video - Colors

Source nodes `614:1430`.

Expand Settings; Collapse Settings; Color Extraction; #FF0000; Negate Colors; Posterize; Saturation; Color Threshold; Similarity; Gradient; Color; Cartoon; Sepia Intensity.

### EQ

Source nodes `3178:16769`.

Open Equalizer; Close Equalizer; 16 KHz; 0.0 dB; 14 KHz; 12 KHz; 6 KHz; 3 KHz; 1 KHz; 600 Hz; 310 Hz; 170 Hz; 60 Hz; 12.0 dB; Preamp.

### EQ presets

Source nodes `2372:8487`.

None; Flat; Classical; Club; Dance; Full base; Full base & treble; Full treble; Headphones; Large hall; Live; Party; Pop; Reggae; Rock; Ska; Soft; Soft rock; Techno.

### Audio Visualizer

Source nodes `2372:8639`.

Disabled; Spectometer; Scope; Spectrum; Waveform.

### HDR Options

Source nodes `2372:8733`.

Auto; Off; HDR10; HDR10+; Dolby Vision; HLG.

### Setting Preset

Source nodes `2372:8834`.

Default Preset; New Preset; Custom Preset 01; Custom Preset 02; Anime; Loud and then Quiet Movies.

### sub filename

Source nodes `6456:13892`.

Title with extra characters; Off; Any title; Contains title; Exact title match.

### Values

Source nodes `804:1208`, `3176:16556`.

Relative; Pixels; 1; 3; 2; Remember last volume; Always starts at:; 200%.

### Sorting Options

Source nodes `856:2215`.

Unwatched First; Watched First; Newest First; Oldest First; Title A-Z; Title Z-A; Artist A-Z; Artist Z-A; Album Z-A; Album A-Z; Track Number Descending; Track Number Ascending; URI Descending; Disc Number Ascending; URI Ascending; Disc Number Descending.

### View Options

Source nodes `858:3290`.

Grid View; List View.

## Global settings categories

The source groups these beneath App, Media, and System. The thirteen categories are General, Interface, Privacy, Controls, Playback, Video, Audio, Subtitles, Library, Network, Performance, Recording, and Extensions. About is a menu destination, not a substitute for an omitted category. Labels below union the normal and expanded-advanced frame variants. Some source text contains typographical errors or copied labels; map behavior to the real VLC option before exposing it.

### global

Source nodes `999:5047`, `3046:9282`.

Global Settings; General; Privacy; Playback; Video; Audio; Subtitles; Controls; Library; Network; Performance; Recording; Extensions; Appearance; Interface language; English; Theme; Light; Dark; Accent color; Startup; Show confirmation and error dialogs; Check for updates automatically; Advanced Options; Diagnostics; Developer mode; Save a log; Log file; Browse; Log details; Errors only; Show minor warnings; Additional command line options; Reset all settings; Reset; Open configurations folder; Open; Log format; Plain text; Collect playback statistics; Developer Options; Write to the system log; Include debug messages in the system log; System log identity; System log facility; User.

### interface

Source nodes `3046:10057`, `3062:11379`.

Interface Settings; General; Privacy; Playback; Video; Audio; Subtitles; Controls; Library; Network; Performance; Recording; Extensions; Window; Play video inside main window; Resize window to fit video; Never; Keep window above other windows; Show window decorations; Show the file name in the title bar; Open fullscreen on; Current screen; Remember window size and position; Advanced Options; Playback Controls; Start in compact mode; Auto-hide the controls; In fullscreen only; Hide after; 0 s; 10s; 5 s; Show controls in fullscreen; Notifications and System Tray; Show a system tray icon; Show a popup when the track changew; When minimized; Popup duration; Start minimized to the tray; Playlist; Show the playlist as a tree; Skin; Use a custom skin; Skin file; Browse; Window opacity; 10%; 100%; Pause video when window is minimized; Bring VLC to the front when playback starts; Show cover art in the window when playing audio; Show the file browser inside the open dialog; Allow the app icon to change; Fullscreen controls opacity; Fullscreen controls sensitivity; Low; Medium; High.

### privacy

Source nodes `3062:11911`.

Privacy Settings; General; Privacy; Playback; Video; Audio; Subtitles; Controls; Library; Network; Performance; Recording; Extensions; History; Save recently played items; Remember the playback position; Never remember items matching; Clear history now; Clear; Fetch titles and cover art from the internet; Submit played tracks to last.fm; Last.fm username; Last.fm submission URL.

### playback

Source nodes `3062:12552`, `3072:13408`.

Playback Preferences; General; Privacy; Playback; Video; Audio; Subtitles; Controls; Library; Network; Performance; Recording; Extensions; Startup; Start playing as soon as a file opens; Continue where you left off; Always; Default playback speed; 0.25; 0.5; 0.75; 1; 1.25; 1.5; 1.75; 2; Pause when another app uses audio; Tracks; Preferred video quality; Best available; Preferred menu language; English; Default audio track; Default subtitle track; Seeking; Seek precisely; Show thumbnail while scrubbing; Files & Discs; Default optical drive; -; Repair damaged AVI files; Ask; Load matching .mkv files from the same folder; Advanced Options; Order; Repeat; Off; Shuffle; Stop after each item; When playback finishes; Stop; Time-shift buffer folder; Browse; Time-shift buffer size; Medium.

### video

Source nodes `3073:14187`, `3073:15375`.

Video Defaults; General; Privacy; Playback; Video; Audio; Subtitles; Controls; Library; Network; Performance; Recording; Extensions; Output; Enable video; Start videos in fullscreen; Stop the screensaver during playback; Play video as desktop wallpaper; Picture; Hue; Reset picture; Reset; Brightness; Contrast; Saturation; Gamma; Advanced Options; Deinterlacing; Automatic; Method; Framing; Aspect ratio; Crop; None; Scale video to the window; Zoom; Fit; Position in the window; Center; Snapshots; Number files sequentially; Format; PNG; Save to; Browse; File name prefix; 1; Show a preview after capture; Size; Same as video; Custom aspect ratios; Custom crop ratios; Monitor pixel aspect ratio; 1:1; Time-shift buffer size; Medium.

### audio

Source nodes `3073:15879`, `3180:17398`.

Audio Defaults; General; Privacy; Playback; Video; Audio; Subtitles; Controls; Library; Network; Performance; Recording; Extensions; Output; Enable audio; Output device; System default; Speaker setup; Automatic; Volume; Maximum volume; 100%; 200%; 300%; Startup volume; Volume step; 1; Loudness; Normalize loudness; Target level; Night mode; Replay gain; Off; Replay gain preamp; 0.0; Advanced Options; Effects; Equalizer; Equalizer preset; Two-pass; Spatial audio; Spatial audio mode; Headphone virtualization; Stereo widening; Compressor; Pitch shift; 0.00; Chorus/flanger; Karaoke; Tracks; Preferred audio language; System language; Audio delay; Visualization; Visualizer; Pass compressed audio to the receiver; Detect Dolby Surround; Resampling quality; Balanced; Media role; Visualization size; Match window.

### subtitles

Source nodes `3180:17989`, `6536:13908`.

Subtitle Defaults; General; Privacy; Playback; Video; Audio; Subtitles; Controls; Library; Network; Performance; Recording; Extensions; Tracks & Languages; Enable subtitles; Preferred language; System language; Text encoding; Automatic; Loading Subtitles; Load subtitle files automatically; File name matching; Title with extra characters; Appearance; Text color; #FFFFFF; Font; System, sans-serif; Background; None; Size values; Font size; Bold; Text opacity; 0%; 50%; 100%; Outline thickness; Normal; Outline color; #000000; Shadow; Shadow color; Shadow distance; Small; Background color; Background opacity; Vertical position; Bottom; Advanced Options; Timing; Subtitle delay; -5 s; 0 s; +5 s; On-Screen Messages; Show feedback for shortcuts; Show filename when a video starts; Show filename for; 1 s; 10 s; Show feedback for; 0.5 s; 5 s; Load subtitles from these folders; Ignore styling in ASS and SSA subtitles; Shadow angle; 0; Text direction; Monospace font; System monospace; Visualization size; Match window.

### controls

Source nodes `6536:14482`.

Mouse, Keyboard & Remote Controls; General; Privacy; Controls; Playback; Video; Audio; Subtitles; Library; Network; Performance; Recording; Extensions; Keyboard Shortcuts; Reset all shortcuts; Reset; Defaults; Edit Keyboard Shortcuts; Skip Amounts; Very short skip; 3 s; Short skip; 10 s; Medium skip; 60 s; Long skip; 300 s; Mouse; Reverse scroll direction; Single click on the video; Play / Pause; Double click on the video; Toggle fullscreen; Scroll wheel; Volume; Horizontal scroll; Seek; Advanced Options; Remotes; Web remote control; Port.

### library

Source nodes `6542:15251`.

Playlists, Folders & Files; General; Privacy; Controls; Playback; Video; Audio; Subtitles; Library; Network; Performance; Recording; Extensions; Opening Files; Only allow one VLC window; Apply to files opened from the file manager; Add to the playlist instead of replacing it; When opening a folder; Include subfolders; Show hidden files; Metadata & Artwork; Fetch titles and artwork; From the file only; Use the media library; Advanced Options.

### network

Source nodes `6543:15649`.

Network Connections & Security; General; Privacy; Controls; Playback; Video; Audio; Subtitles; Library; Network; Performance; Recording; Extensions; Buffering; Normal; Local files; Discs; Network streams; Live capture; Proxy; HTTP proxy; System proxy; HTTP proxy password; Advanced Options; Security; Verify certificates against the system trust store; Cipher preference; Balanced.

### performance

Source nodes `6571:15958`.

Performance Settings; General; Privacy; Controls; Playback; Video; Audio; Subtitles; Library; Network; Performance; Recording; Extensions; Decoding; Hardware-accelerated decoding; Automatic; Video post-processing; Off; Skip the H.264 loop filter; Never; Decoding threads; 0; Smooth Playback; Drop frames when the system can't keep up; Give VLC real-time priority; Process priority; Rescan plugins; Rescan; Advanced Options; Developer; Encoder; Decoder; Clock synchronisation; Default; Clock jitter; 5000 ms; Clock reference averaging; 40.

### recording

Source nodes `6571:16429`.

Recording, Casting & Streaming Out; General; Privacy; Controls; Playback; Video; Audio; Subtitles; Library; Network; Performance; Recording; Extensions; Save recordings to; ~/Videos; Browse; Record without re-encoding when possible; Casting; Discover cast devices; Cast quality; Automatic; Advanced Options; Streaming; Play locally while streaming; Encoding preset; Medium; Include audio; Include video; include subtitles; Tuning; None.

### extensions

Source nodes `6575:17137`.

Add-ons & Scripts; General; Privacy; Controls; Playback; Video; Audio; Subtitles; Library; Network; Performance; Recording; Extensions; Add-ons; Find more extensions; Add; Remove; Load add-ons on startup; Advanced Options.

## Creation dialogs, menus, and customization

### Onboarding

Source nodes `1899:3055`, `1902:3670`, `1902:4020`, `1902:4325`, `1902:4634`, `1902:4940`, `1902:5245`, `1902:5550`, `1902:6160`, `1902:6465`, `1902:6770`.

Welcome to VLC!; Next; Choose a Media Folder to get started with; ~/Videos; Browse; Select your preferred theme and color.; Light; Dark; System; Back; Learn the basic keyboard shortcuts.; Finish; Toggle Fullscreen; Increase / Decrease Volume; Show / Hide File Browser; Take a Snapshot; Show / Hide Queue; Record; F; L; ↑; /; ↓ ; Q; SHIFT; R; S; Play / Pause; Cycle Subtitle Track; Previous / Next Item; Previous / Next Frame; Cycle Audio Track; Seek Backward / Forward; Space; V; B; ←; →; ,; .; <; >; Find the full list in the menu..

### Add Subtitles to Playlist/Folder

Source nodes `1911:7545`, `1919:8707`, `1919:9278`, `3336:11929`.

Videos / Shows / The Wand / S01; From Computer; Add Subtitles; The_Wand_S01E02.mp4; The_Wand_S01E03.mp4; The_Wand_S01E04.mp4; The_Wand_S01E05.mp4; The_Wand_S01E06.mp4; The_Wand_S01E07.mp4; The_Wand_S01E08.mp4; The_Wand_S01E09.mp4; The_Wand_S01E10.mp4; The_Wand_S01E11.mp4; The_Wand_S01E12.mp4; The_Wand_S01E13.mp4; The_Wand_S01E14.mp4; The_Wand_S01E15.mp4; The_Wand_S01E01.mp4; The_Wand_S01E01.srt; The_Wand_S01E02.srt; The_Wand_S01E03.srt; The_Wand_S01E04.srt; The_Wand_S01E05.srt; The_Wand_S01E06.srt; The_Wand_S01E07.srt; The_Wand_S01E08.srt; The_Wand_S01E09.srt; The_Wand_S01E10.srt; The_Wand_S01E11.srt; The_Wand_S01E12.srt; The_Wand_S01E13.srt; The_Wand_S01E14.srt; The_Wand_S01E15.srt; Cancel; Save; The_Wand_S01E01_English.srt; The_Wand_S01E01_Spanish.srt; The_Wand_S01E01_Japanese.srt; The_Wand_S01E01_German.srt; The_Wand_S01E01_Dutch.srt; The_Wand_S01E01_French.srt; The Wand S01E01; English; The_Wand_S01E01_EN.srt.

### subtitle multiple

Source nodes `1915:8397`.

The_Wand_S01E01.srt.

### subtitle one

Source nodes `1919:9121`.

The_Wand_S01E01.srt.

### target

Source nodes `1959:5325`.

To This Folder; To This File; To This Playlist.

### origin

Source nodes `1963:5465`.

From Computer; From OpenSubtitles; From Link ....

### Open network stream

Source nodes `6216:13614`.

Open Network Stream; e.g., https://www.example.com/stream.avi; Network URL ; You can enter multiple links to load.; Playlist Name (optional); The playlist will be saved and displayed with this title.; e.g., Favorite TV Channels; Cancel; Save.

### new playlist

Source nodes `6259:14954`.

New Playlist; Playlist Details; The playlist will be saved and displayed with this title and thumbnail.; e.g., Favorite Videos; Select Files; These files will be added to the playlist.; Drag files into the box or click here to select files.; Cancel; Save.

### Keyboard Shortcuts

Source nodes `2437:14832`.

Keyboard Shortcuts; Search Keyboard Shortcuts...; All Fields; Show interface controls in fullscreen; I; Show playback position and time; T; Exit fullscreen / close dialog; esc; Toggle fullscreen; F; Basic; Action; Hotkey; Global Hotkey; Playback; Play / pause; Space; Stop playback; S; Next track; N; Previous track; P; Next program / Service ID; X; Cancel; Save.

### Profiles

Source nodes `6648:20666`.

Movies; Shows; Music; Work; New; Open File(s); Ctrl + O; Open Folder; Ctrl + F; Open Network Stream; Ctrl + N; Open Disc; Ctrl + D; Open Capture Device; Ctrl + C; Convert; Ctrl + R; Open Recent; Profile (Movies); Keyboard Shortcuts; Customize Player; Settings; Ctrl + P; About VLC; Shift + F1.

### menu

Source nodes `6062:13354`.

Open Capture Device; Ctrl + C; Open Network Stream; Ctrl + N; Settings; Ctrl + P; Convert; Ctrl + R; Open File(s); Ctrl + O; Profile (Movies); About VLC; Shift + F1; Customize Player; Open Recent; Open Folder; Ctrl + F; Keyboard Shortcuts; Open Disc; Ctrl + D.

### right-click menu

Source nodes `6065:13616`.

Quit VLC; Ctrl + Q; Playlist; Subtitle; Hide UI; Ctrl + \; Next Item; N; Playback; Stop; S; Previous Item; P; Video; Play; Space; Record; R; Audio; More Settings; Open Media.

### file context menu

Source nodes `2404:13340`.

Play; Stream; Convert; Add to Queue; Add to Playlist; Properties & Metadata; Add Subtitle Tracks; Add Audio Tracks; Show in File Explorer; Mark as Watched; Favorite; Go to Source.

### folder context menu

Source nodes `2139:8935`.

Add File; Add Folder; Resync; Info; Edit Subtitle Tracks; Edit Audio Tracks.

### playlist context menu

Source nodes `2459:9389`.

Play; Favorite; Export Playlist; Add to Queue; Add Subtitle Tracks; Add Audio Tracks; Properties & Metadata; Shuffle.

### context menu

Source nodes `2107:5903`.

.

### Player - customize

Source nodes `2107:5604`, `2160:6137`, `2160:6564`, `2160:7005`, `2160:7437`, `2160:7869`, `2160:8301`, `2160:8732`, `6587:19923`.

S01; The Wand S01E11; You’ve watched till 00:45:08 already.; Continue Watching; Customize Player; Drag items around and out of the box below to re-order them. Drag items into the box to remove them from the player.; Spacer; Divider; Playback Speed; Forward / 10s; Backward / 10s; Forward  / 15s; Backward15s; Stop; Repeat; Shuffle; Record; Snapshot; Picture in Picture; HDR; Zoom; Right Sidebar; Quit VLC; VLC Settings; Go Home; Favorite; Thumbnail; File Name; Artist Name; Folder Name; Playlist Name; Properties; Replay; Chapters; Bookmark; Bookmarks; Playlists; Cancel; Save.

The Profiles component has the same main-menu entries as the normal menu plus profile selection. The separate Setting Preset selector contains Default/New/Custom presets and media-oriented named presets. The source walkthrough specifies profiles restore layouts and all settings. These are distinct from EQ presets.

## Motion specifications from prototype actions

Only nondeleted `prototypeInteractions` were considered. A duration attached to `INSTANT_TRANSITION` is stored metadata, not evidence of a visible timed animation. Do not turn such switches into 1.25-second UI stalls.

| Motion | Source evidence | Intended native treatment |
| --- | --- | --- |
| Player/library/titlebar layout transitions | `187:338` to `247:445`, `247:456` to `187:328`; SMART_ANIMATE, 208.354 ms, SPRING_PRESET_THREE, easing `[1,2880,120,0]` | Match coordinated geometry changes with the spring response, not merely a fixed 300 ms sidebar shrink |
| Hide/show UI | Player `247:445`, F trigger to `3328:11554`; SMART_ANIMATE, 300 ms INOUT_CUBIC | Animate chrome departure/reveal without hiding playback itself |
| Volume drag state | `244:183` / `244:210`; SMART_ANIMATE, 300 ms INOUT_CUBIC | Native volume remains immediately responsive; animate only the visual indication |
| Customization rearrangement | `6587:19727` to `6587:19923`, DRAG; SMART_ANIMATE, 600 ms INOUT_CUBIC | Animate reordered/drop targets while retaining real drag/drop state |
| Hover audio value | `491:1732` to `2245:9805`, ON_HOVER, 50 ms INOUT_CUBIC | Fast, anchored value tooltip |
| Seek thumbnail | `2151:9673` is the thumbnail overlay frame; time readout included | Anchor preview to pointer/seek position and clamp within window |
| Control-side state switches | `491:1730` to subtitles state stores 104.177 ms with spring `[1,11520,240,0]`; many other entries use INSTANT_TRANSITION | Show the side panel promptly; avoid treating inherited stale transition durations as a blanket animation spec |
| Ordinary ease-in/out | INOUT_CUBIC easing `[0.42,0,0.58,1]` | Use the stored cubic where explicitly animated |
| Ease-out | OUT_CUBIC easing `[0,0,0.58,1]`, usually paired with INSTANT_TRANSITION | Preserve instant semantics unless visual walkthrough proves an animated change |

Prototype hover/click/drag variants also cover icon active/hover states, expanded effects, selected tracks, queue/chapter/bookmark states, sorting/view overlays, and menus. Dynamic child content should share the same state store rather than reusing screenshots as UI.

## VLC 3.0.24 capability map

These are confirmed local source options. A registered configuration option does not automatically provide live runtime mutation: apply to the current object's registered variable/callback where present, otherwise rebuild the affected filter or mark the change as applying to the next output. Preserve the other entries in audio/video filter chains. Persistent values require `config_Put*` of the registered type followed by `config_SaveConfigFile`.

| Design control | Real VLC 3 option/API | Primary local source |
| --- | --- | --- |
| Windows HDR mode | `d3d11-hdr-mode`: `auto`, `never`, `always`, `generate` | `modules/video_output/win32/direct3d11.c:85` |
| HDR10+/Dolby Vision/HLG choices | No one-to-one mode selector verified; those labels must not claim forced formats through the four-option D3D11 control | Same HDR declaration; individual metadata handling is not a user mode selector |
| Tone mapping | `tone-mapping`, `tone-mapping-param`, `tone-mapping-desat`, `tone-mapping-warn`, dependent on built OpenGL/libplacebo support | `modules/video_output/opengl/vout_helper.h:204` |
| Loudness normalize | `normvol` filter, `norm-max-level`, `norm-buff-size` | `modules/audio_filter/normvol.c:83` |
| Night mode | Can be an explicit compressor preset, not a verified independent `night-mode` core option | `modules/audio_filter/compressor.c:183` |
| Compressor | `compressor-rms-peak`, `compressor-attack`, `compressor-release`, `compressor-threshold`, `compressor-ratio`, `compressor-knee`, `compressor-makeup-gain` | `modules/audio_filter/compressor.c:183` |
| Spatializer | `spatializer` filter; `spatializer-roomsize`, `spatializer-width`, `spatializer-wet`, `spatializer-dry`, `spatializer-damp` | `modules/audio_filter/spatializer/spatializer.cpp:79` |
| Stereo widen | `stereo_widen` module; `stereowiden-delay`, `stereowiden-feedback`, `stereowiden-crossfeed`, `stereowiden-dry-mix` | `modules/gui/qt/components/extended_panels.cpp:1358`; `modules/audio_filter/stereo_widen.c:71` |
| Pitch shift | `scaletempo_pitch` module; `pitch-shift` in semitones, -12 to 12 | `modules/audio_filter/scaletempo.c:73`; `modules/audio_filter/Makefile.am:20` |
| Chorus/flanger | `chorus_flanger` module, shortcut `delay`; `delay-time`, `sweep-depth`, `sweep-rate`, `feedback-gain`, `wet-mix`, `dry-mix` | `modules/audio_filter/chorus_flanger.c:75` |
| Karaoke | `karaoke` filter | `modules/audio_filter/karaoke.c:33` |
| Headphone virtualization | `headphone` mixer with `headphone-dim`, `headphone-compensate`, `headphone-dolby`; ambisonics/binaural paths include `spatialaudio-headphones` | `modules/audio_filter/channel_mixer/headphone.c:77`; `spatialaudio.cpp:63` |
| Replay gain | `audio-replay-gain-mode`, `audio-replay-gain-preamp`, `audio-replay-gain-default`, `audio-replay-gain-peak-protection` | `src/libvlc-module.c:1516` |
| Audio output device | Held aout `aout_DeviceSet`; get real device choices rather than hardcoded Speakers/Headphones | `modules/gui/qt/input_manager.cpp:1244` |
| Volume | `playlist_VolumeGet/Set`, `playlist_MuteGet/Set`; 1.0 = 100%; `volume-save` and `volume-step` | `src/playlist/aout.c:46`; `src/libvlc-module.c:1498` |
| Audio delay | Current input `audio-delay` in CLOCK_FREQ units; persistent `audio-desync` in milliseconds | `modules/gui/qt/components/extended_panels.cpp:1545`; `src/libvlc-module.c:1511` |
| Subtitle delay/speed/duration | Current input `spu-delay`, `sub-fps`; subtitle-duration filter `subsdelay-factor`; persistent `sub-delay` uses 1/10-second units | `modules/gui/qt/components/extended_panels.cpp:1554`; `modules/demux/subtitle.c:80` |
| Subtitle matching | `sub-autodetect-file`, `sub-autodetect-fuzzy`, `sub-autodetect-path` | `src/libvlc-module.c:1687` |
| Subtitle font, exact/relative size | `freetype-font`, `freetype-fontsize`, `freetype-rel-fontsize`; `sub-text-scale` is a separate percentage multiplier, 10-500 | `modules/text_renderer/freetype/freetype.c:162` |
| Subtitle styling | `freetype-bold`, `freetype-opacity`, `freetype-color`, background/outline/shadow `freetype-*`; opacity 0-255, not percentages | `modules/text_renderer/freetype/freetype.c:180` |
| Subtitle shadow geometry | `freetype-shadow-angle`, `freetype-shadow-distance`, `freetype-text-direction`; `sub-margin` offset | `modules/text_renderer/freetype/freetype.c:224`; `src/libvlc-module.c:1698` |
| Subtitle language/track defaults | `sub-language`, `sub-track`, `sub-track-id`; live `spu-es` IDs are per media | `src/libvlc-module.c:1723` |
| Audio language/track defaults | `audio-language`, `audio-track`; live `audio-es` | `src/libvlc-module.c:1720` |
| Subtitle embedded formatting | `subsdec-formatted` is registered and consumed by the USF decoder only. It does not supply a general ignore-format switch for SRT or ASS/SSA. No exposed libass ignore-style option was verified; that requires decoder policy work | `modules/codec/subsusf.c:55`, `142`; `modules/codec/libass.c:213` |
| Subtitle encoding | `subsdec-encoding`, `subsdec-autodetect-utf8` | `modules/codec/subsdec.c:192` |
| Persistent per-file attached tracks | `input_item_slave_New` / `input_item_AddSlave`; store item-URI to slave-URI mappings for reloading; live `input_AddSlave` attaches current media | `include/vlc_input_item.h:256`; `include/vlc_input.h:579` |
| Bookmarks | `INPUT_GET_BOOKMARKS`, `INPUT_ADD_BOOKMARK`, `INPUT_DEL_BOOKMARK`; retain timestamps/names across reopening if design requires it | `include/vlc_input.h:442`; `modules/gui/qt/dialogs/bookmarks.cpp` |
| Sleep timer | No dedicated core setting verified; UI QTimer can trigger chosen stop/quit behavior | Existing `playlist_Stop` / exit behavior must be explicit |
| Reverse | Current input negative `rate`; capability may reject reverse for a particular media | `modules/gui/qt/input_manager.cpp:890` |
| Hardware decoding/threads | `avcodec-hw`, `avcodec-threads`, `avcodec-skiploopfilter` | `modules/codec/avcodec/avcodec.c:129` |
| Smooth playback | `drop-late-frames`, `skip-frames`, Windows `high-priority` | `src/libvlc-module.c:1564`, `2057` |
| Clock settings | `clock-synchro`, `clock-jitter`, `cr-average` | `src/libvlc-module.c:1911` |
| Plugin rescan | `reset-plugins-cache` is a startup action, not an instant settings bool; `plugins-cache`, `plugins-scan` are registered | `src/libvlc-module.c:2033`, `2807` |
| Buffering | `file-caching`, `live-caching`, `disc-caching`, `network-caching`, milliseconds | `src/libvlc-module.c:1864` |
| Proxy/security | `http-proxy`; `gnutls-system-trust`, `gnutls-dir-trust`, `gnutls-priorities`; don't invent a balanced cipher enum that the backend does not expose | `src/libvlc-module.c:1816`; `modules/misc/gnutls.c:787` |
| Native recording | `input-record-path`, `input-record-native`; runtime input `record` | `src/libvlc-module.c:1921` |
| Casting | Chromecast `sout-chromecast-*`, including conversion-quality and audio-passthrough | `modules/stream_out/chromecast/cast.cpp:247` |
| Snapshots | `snapshot-path`, `snapshot-prefix`, `snapshot-format`, `snapshot-preview`, `snapshot-sequential`, `snapshot-width`, `snapshot-height` | `src/libvlc-module.c:1599` |
| Folder recursion | `recursive` choice plus shell-specific hidden-file filtering | `src/libvlc-module.c:2119` |
| Video picture | `adjust` filter; float `hue`, `contrast`, `brightness`, `saturation`, `gamma` | `modules/video_filter/adjust.c:83` |
| Video essential filters | `sharpen` / `sharpen-sigma`; `gradfun` / `gradfun-radius`, `gradfun-strength`; `grain` / `grain-variance` | `modules/video_filter/sharpen.c:74`, `gradfun.c:65`, `grain.c:70` |
| Rotate/flip | `transform` filter / `transform-type` for discrete transforms; `rotate` filter / `rotate-angle` for arbitrary angles | `modules/video_filter/transform.c:65`; `rotate.c:77` |
| Crop and framing | Current held vout `crop`, `crop-top`, `crop-bottom`, `crop-left`, `crop-right`, `aspect-ratio`, `zoom`; use real variable choices | `src/video_output/vout_intf.c:173` |
| Color filters | `extract-component`, `posterize-level`, `colorthres-color`, `colorthres-saturationthres`, `colorthres-similaritythres`, `gradient-mode`, `gradient-type`, `gradient-cartoon`, `sepia-intensity`; `invert` filter for negation | Their registrations in `modules/video_filter/` |
| Visualizer | `audio-visual` module selection; `effect-list` chooses visual plugin effect; display only built effects | `src/libvlc-module.c:1540`; `modules/visualization/visual/visual.c` |
| Streaming included tracks | `sout-audio`, `sout-video`, `sout-spu`; encoder presets are module-specific rather than a universal Medium enum | `src/libvlc-module.c:1971` |
| Interface options | `qt-max-volume`, `qt-fs-controller`, `qt-fs-opacity`, `qt-fs-sensitivity`, `qt-system-tray`, `qt-start-minimized`, `qt-pause-minimized`, `qt-name-in-title`, `qt-video-autoresize`, `qt-fullscreen-screennumber`, `qt-auto-raise`, `qt-icon-change` | `modules/gui/qt/qt.cpp:230` |
| History/resume | `qt-recentplay`, `qt-recentplay-filter`, `qt-continue`; actual recent positions use `RecentsMRL` | `modules/gui/qt/qt.cpp:257`; `modules/gui/qt/recents.cpp` |
| Extensions startup | `qt-autoload-extensions`; add-on operations use existing add-on manager, not manual plugin DLL removal | `modules/gui/qt/qt.cpp:308`; native plugins remain engine components |
| OSD | `osd`, `video-title-show`, `video-title-timeout`, `video-title-position`; shortcut name in feedback is a new UI requirement rather than an existing checkbox | `src/libvlc-module.c:1585`, `1679` |
| Persistent profile/layout state | Shell QSettings store; restoring values needs to apply registered runtime settings as well as storage | `modules/gui/qt/qt.cpp` and redesign shell |

The `headphone` module and `binauralizer` submodule register as audio filters, so they can be selected through the audio-filter chain when the required module is built. The `ambisonics` module registers as an audio renderer, with `spatialaudio-headphones` and `hrtf-file` options. Keep those two capabilities distinct and verify channel-layout requirements against actual media.

## Verification requirements

A category title or an Advanced Options button does not satisfy the contained controls. A button that only saves a value without changing playback does not satisfy a live control. A profile name without loading its settings/layout does not satisfy profiles. A screenshot of the supplied panel does not satisfy interaction.

Verify each completed flow using actual local media and an idle launch, then while playing, after switching tracks, and after restart. Include persistence for saved playlists, mapped subtitle/audio tracks, pins, bookmarks, profiles, and layout. Animation checks should cover compact/full-library changes, side effects panes, hide/reveal, and drag/drop geometry, while playback remains active. Capability-limited formats must show accurate available choices rather than claiming an unsupported engine operation.

## Complete top-level frame register

This register includes component collections as well as screens, so it is an audit trail rather than a demand for one window per row.

| Node | Name | Size |
| --- | --- | --- |
| `42:356` | Icon | 548 x 812 |
| `177:98` | Icon Wrapper | 72 x 122 |
| `187:328` | Player | 1920 x 1080 |
| `244:184` | volume | 130 x 646 |
| `247:445` | Player | 1920 x 1080 |
| `491:1780` | Controls | 1960 x 11139 |
| `500:3063` | Button | 400 x 140 |
| `536:1014` | Toggle | 120 x 194 |
| `536:1513` | Audio - Compressor Settings | 400 x 821 |
| `536:1703` | Audio - Spatializer Settings | 400 x 821 |
| `536:1855` | Video - Essential | 400 x 821 |
| `543:990` | Video - Crop | 400 x 821 |
| `612:1223` | Audio - Stereo Widener | 400 x 821 |
| `614:1430` | Video - Colors | 400 x 888 |
| `804:1208` | Values | 253 x 404 |
| `848:1712` | play | 94 x 116 |
| `848:1996` | sidebar | 4492 x 1028 |
| `856:2215` | Sorting Options | 265 x 776 |
| `858:3290` | View Options | 265 x 104 |
| `970:2703` | Player | 1920 x 1080 |
| `970:3091` | Miniplayer | 431 x 2816.375 |
| `970:3308` | Player | 1920 x 1080 |
| `970:3792` | Player | 1920 x 1080 |
| `970:4028` | Player | 1920 x 1080 |
| `999:5047` | Settings - global | 1048 x 865 |
| `1899:3055` | Onboarding | 1090 x 759 |
| `1902:3670` | Onboarding | 1090 x 759 |
| `1902:4020` | Onboarding | 1090 x 759 |
| `1902:4325` | Onboarding | 1090 x 759 |
| `1902:4634` | Onboarding | 1090 x 759 |
| `1902:4940` | Onboarding | 1090 x 759 |
| `1902:5245` | Onboarding | 1090 x 759 |
| `1902:5550` | Onboarding | 1090 x 759 |
| `1902:6160` | Onboarding | 1090 x 759 |
| `1902:6465` | Onboarding | 1090 x 759 |
| `1902:6770` | Onboarding | 1090 x 759 |
| `1904:7238` | Current Time | 114 x 164 |
| `1905:7312` | Duration | 114 x 164 |
| `1911:7545` | Add Subtitles to Playlist/Folder | 1048 x 800 |
| `1915:8397` | subtitle multiple | 519 x 239 |
| `1919:8707` | Add Subtitles to Playlist/Folder | 1048 x 800 |
| `1919:9121` | subtitle one | 519 x 239 |
| `1919:9278` | Add Subtitles to Playlist/Folder | 1048 x 800 |
| `1959:5325` | target | 240 x 520 |
| `1963:5465` | origin | 240 x 525 |
| `2107:5604` | Player - customize | 1920 x 1080 |
| `2107:5903` | context menu | 265 x 296 |
| `2107:5935` | option | 301 x 292 |
| `2139:8935` | folder context menu | 265 x 296 |
| `2151:9673` | thumbnail | 272 x 189 |
| `2160:6137` | Player - customize | 1920 x 1080 |
| `2160:6564` | Player - customize | 1920 x 1080 |
| `2160:7005` | Player - customize | 1920 x 1080 |
| `2160:7437` | Player - customize | 1920 x 1080 |
| `2160:7869` | Player - customize | 1920 x 1080 |
| `2160:8301` | Player - customize | 1920 x 1080 |
| `2160:8732` | Player - customize | 1920 x 1080 |
| `2245:9805` | audio value | 121 x 37 |
| `2372:8300` | Dropdown | 406 x 369 |
| `2372:8487` | EQ presets | 406 x 460 |
| `2372:8639` | Audio Visualizer | 406 x 369 |
| `2372:8733` | HDR Options | 406 x 422 |
| `2372:8834` | Setting Preset | 406 x 412 |
| `2404:9167` | media | 377 x 228 |
| `2404:13340` | file context menu | 265 x 584 |
| `2425:13624` | Player - Light | 1920 x 1080 |
| `2437:14832` | Keyboard Shortcuts | 1048 x 800 |
| `2459:9352` | playlist | 377 x 228 |
| `2459:9389` | playlist context menu | 265 x 392 |
| `3021:9835` | setting category | 228 x 140 |
| `3046:9282` | Settings - global | 1048 x 865 |
| `3046:10057` | Settings - interface | 1048 x 865 |
| `3062:11379` | Settings - interface | 1048 x 865 |
| `3062:11911` | Settings - privacy | 1048 x 865 |
| `3062:12552` | Settings - playback | 1048 x 865 |
| `3072:13408` | Settings - playback | 1048 x 865 |
| `3073:14187` | Settings - video | 1048 x 865 |
| `3073:15375` | Settings - video | 1048 x 865 |
| `3073:15879` | Settings - audio | 1048 x 865 |
| `3176:16556` | Values | 360 x 216 |
| `3178:16769` | EQ | 824 x 628 |
| `3180:17398` | Settings - audio | 1048 x 865 |
| `3180:17989` | Settings - subtitles | 1048 x 865 |
| `3328:11554` | Player - hide UI | 1920 x 1080 |
| `3336:11929` | Add Subtitles to Playlist/Folder | 1048 x 800 |
| `4767:23726` | Player - Music | 1920 x 1080 |
| `4778:25123` | Player - Music | 1920 x 1080 |
| `6062:13354` | menu | 375 x 585 |
| `6065:13616` | right-click menu | 292 x 683 |
| `6114:13610` | Audio - Equalizer | 400 x 1623 |
| `6216:13614` | Open network stream | 1048 x 631 |
| `6259:14954` | new playlist | 713 x 589 |
| `6261:15078` | Player | 1920 x 1080 |
| `6456:13892` | sub filename | 406 x 422 |
| `6536:13908` | Settings - subtitles | 1048 x 865 |
| `6536:14482` | Settings - controls | 1048 x 865 |
| `6542:15251` | Settings - library | 1048 x 865 |
| `6543:15649` | Settings - network | 1048 x 865 |
| `6571:15958` | Settings - performance | 1048 x 865 |
| `6571:16429` | Settings - recording | 1048 x 865 |
| `6575:17137` | Settings - extensions | 1048 x 865 |
| `6587:19923` | Player - customize | 1920 x 1080 |
| `6648:20666` | Profiles | 651 x 633 |
