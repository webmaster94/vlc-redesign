# Reframe licensing and public distribution

Reviewed 12-13 October 2026 against primary sources and the pinned Windows dependency build. Public hosting of this fork is permitted. The combined Reframe release uses GPL-3.0-or-later, with retained notices for its separately licensed dependencies and Juxtopposed's CC BY 4.0 design.

## Public hosting is allowed by VLC's license

VLC is open source. Its upstream README specifies GPLv2-or-later for the player and notes that dependencies can make some platform builds effectively GPLv3. The repository contains both GPL and LGPL license texts. A public GitHub fork is allowed; personal use is not the only permitted use. [Upstream README](https://github.com/videolan/vlc#license).

VideoLAN expressly allows distributing original or modified versions when the license terms are followed and says no separate permission from its team is necessary. [VideoLAN legal FAQ](https://docs.videolan.me/vlc-user/en/support/faq/legalconcerns.html).

Private modification and internal use do not force public publication. Binary distribution changes that: recipients must receive the GPL rights and access to the corresponding modified source. A private repository does not avoid those obligations if its executable is shared outside the private use context. [FSF GPL FAQ](https://www.gnu.org/licenses/gpl-faq.en.html#GPLRequireSourcePostedPublic).

GitHub's standard forks of public repositories are public, and their visibility cannot be changed independently. Private work would use a separate private repository or local clone rather than a normal private GitHub fork of `videolan/vlc`. [GitHub fork visibility](https://docs.github.com/en/pull-requests/reference/forks#visibility-of-forks).

## Combined application license

Reframe's release policy is GPL-3.0-or-later for the combined application and the new native redesign implementation. Original VLC headers, copyright notices, and dependency licenses remain intact. The later-version grant in VLC's GPL-2.0-or-later source permits using GPLv3 for the combined program; this is a permitted license-version choice, not a claim to own or rewrite upstream copyright. [FSF GPL compatibility FAQ](https://www.gnu.org/licenses/gpl-faq.en.html#v2v3Compatibility), [upstream GPL text, section 9](https://raw.githubusercontent.com/videolan/vlc/master/COPYING).

The FSF expressly states that a CC BY 4.0 work can be combined with GPLv3 material and the resulting larger work distributed under GPLv3. This gives the design adaptation a direct, documented compatibility basis. The FSF's current license list also describes CC BY 4.0 as compatible with all GPL versions. Selecting GPLv3 here is a clear project policy, not an assertion that every GPLv2 use of CC BY 4.0 is prohibited. CC BY 4.0 should remain the stated license of the original design. [FSF licensing announcement](https://www.fsf.org/blogs/licensing/cc-by-4-0-and-cc-by-sa-4-0-added-to-our-list-of-free-licenses), [FSF license list](https://www.gnu.org/licenses/license-list.html.html#ccby).

This is CC BY, not CC BY-SA. BY-SA has different compatibility rules; its special one-way GPLv3 compatibility is not the license of this resource. [Figma resource](https://www.figma.com/community/file/1691465999134370980/vlc-redesign), [CC BY 4.0 legal code](https://creativecommons.org/licenses/by/4.0/legalcode.en).

## What to ship with the GPLv3 installer

The installer should display GPLv3 as the combined application's license. Preserve the original GPLv2 and LGPL texts as component notices. The packaging layout should include:

- `COPYING.txt`: complete GNU GPLv3 text, used by the installer license page.
- `COPYING.GPL2.txt`: original upstream GPLv2 text, retained for the original source notices.
- `COPYING.LIB.txt`: original LGPLv2.1 text; include LGPLv3 where a bundled library uses it.
- `CREDITS.txt`: VideoLAN/upstream credit, Juxtopposed/design attribution and adaptation notice, source/release links, and font attribution.
- `licenses/`: the matching bundled libraries' license, copyright, and other required notices.
- `fonts/LICENSE.txt`: the SIL Open Font License for the unmodified Inter fonts.

Under GPLv3, preserve notices, identify modifications, supply the license, and provide Corresponding Source for the distributed build. Publish equivalent, free source-download access alongside the binary under section 6(d). Include the fork's source, matching dependency source, applied patches, and the build/installation scripts. Provide the preferred form for editing adapted design assets where those are part of Corresponding Source. An upstream-only link cannot substitute for the fork's modifications. [GNU GPLv3 text, sections 1, 5 and 6](https://opensource.org/license/gpl-3.0).

The combined release and new redesign source should say `GPL-3.0-or-later`; unchanged upstream files keep their existing GPL-2.0-or-later, LGPL, or other headers. Include the GPLv3 text in the repository without deleting the original `COPYING` and `COPYING.LIB`. README, About, installed credits, and release notes should distinguish the combined-release license from those retained component licenses.

This paragraph defines the packaging requirements for the new policy. It does not retroactively claim older installer artifacts already displayed GPLv3; rebuild the installer after updating its staged license text and notices.

The existing Windows installer uses NSIS and Modern UI 2. Its script defines welcome, reinstall/update, GPL license, component-selection, destination, install, and finish pages. Reusing this packaging is a practical way to match the current installer. [Upstream NSIS installer](https://raw.githubusercontent.com/videolan/vlc/master/extras/package/win32/NSIS/vlc.win32.nsi.in).

For a distinct fork, I recommend its own application name, installation folder, uninstall key, file-association identifiers, and updater target. That prevents the fork's installer from taking over an existing stock VLC installation. This is an implementation recommendation, not a condition stated by the GPL.

## Branding is a separate question

VideoLAN lists VLC media player, VideoLAN, and the cone among its registered trademarks. Its documentation permits use of its logo to refer to the project or a VideoLAN product, without implying endorsement. This does not establish unrestricted permission to brand an independently modified product as an official VideoLAN release. [VideoLAN legal page](https://www.videolan.org/legal.html), [logo guidance](https://docs.videolan.me/vlc-user/en/support/faq/legalconcerns.html#may-i-use-the-videolan-logo).

Use a distinct distributed product name and icon, describe it accurately as an unofficial VLC fork, and credit VideoLAN in the README and About dialog. That avoids depending on a broad trademark permission which these sources do not provide. References to VLC as the upstream project should remain clear and factual.

The VideoLAN legal page was available in the search index, but direct retrieval returned HTTP 418. The legal FAQ was directly readable. No design-specific trademark permission was found.

## Figma design is licensed under CC BY 4.0

The main agent inspected the specific [VLC redesign resource](https://www.figma.com/community/file/1691465999134370980/vlc-redesign) through the collaborative browser. Its live page identified the creator as Juxtopposed and expressly displayed "Licensed under CC BY 4.0". The page also displayed "Open in Figma", "For Figma", and "Last updated 13 hours ago". This verifies the resource license and creator, although the separate rights to every embedded asset have not been checked. The web-fetch tool alone could not retrieve this page.

Figma's official policy says free Community design files are published under CC BY 4.0. That permits copying and adapting for commercial or noncommercial use with creator credit. Paid files instead use the Community Paid Resource License, permit qualifying outputs, and restrict redistribution of the underlying file. Free previews of paid files are CC BY 4.0. The Community Free Resource License applies by default to free plugins/widgets, not to free design files. [Figma copyright and licensing](https://help.figma.com/hc/en-us/articles/360042296374-Figma-Community-copyright-and-licensing).

For this CC BY 4.0 design, credit Juxtopposed, retain supplied notices, link the original resource and CC BY 4.0, and state that the design was adapted into the application. Include that attribution in the repository and installed credits. CC BY 4.0 does not grant trademark or patent rights and covers only rights the licensor can grant; stock photos, album/movie artwork, fonts, and third-party icon sets need their own rights checked. Replace illustrative cover art with the user's media thumbnails or separately licensed placeholders. [CC BY 4.0 legal code, sections 2-3](https://creativecommons.org/licenses/by/4.0/legalcode.en).

The native redesign code uses GPL-3.0-or-later under the combined-release policy above. Keep the original design credit and CC BY 4.0 link. Copyright in the original design remains with its creator; implementing or adapting it does not erase the original license and attribution.

Making the repository private does not by itself grant rights to bundled assets. The verified CC BY 4.0 license supports public adaptation with attribution. Review the design file or an authorized export for third-party asset credits before importing its artwork.

## Pinned Windows dependency compatibility check

The workflow pins VideoLAN's Windows contrib artifact `vlc-contrib-x86_64-w64-mingw32-910cc9cada6080dd777fbafb2ebdd0c24f702c30.tar.zst`. On 13 October, the audit read the expanded prefix at `/root/vlc-redesign-build/prebuilt/x86_64-w64-mingw32`, its `.pc` versions and installed headers, and selected source notices from the matching dependency-source archive in `vlc-redesign-releases/v0.1.0`. The default contrib bootstrap enables both `GPL` and `GNUV3` and describes that combination as GPL version 3. [Pinned workflow](../../.github/workflows/redesign-build.yml), [contrib bootstrap](../../contrib/bootstrap), [prebuilt artifact](https://artifacts.videolan.org/vlc-3.0/win64/vlc-contrib-x86_64-w64-mingw32-910cc9cada6080dd777fbafb2ebdd0c24f702c30.tar.zst).

No GPL-2.0-only compatibility blocker was found among the reviewed libraries in this build. The main GPL dependencies contain later-version grants, not merely a bare copy of the GPLv2 license document:

| Reviewed dependency | Concrete primary evidence | GPLv3 conclusion |
| --- | --- | --- |
| x264, pinned `e067ab0b530395f90b578f6d05ab0a225e2efdf9` | Prebuilt `include/x264.h` and matching source headers specify GPLv2 or later | Later option permits GPLv3 |
| x265 4.1 | Prebuilt `include/x265.h`, `x265_config.h`, and source headers specify GPLv2 or later | Later option permits GPLv3 |
| FAAD2 2.11.3 | Prebuilt `include/faad.h` and source README/header notices specify GPLv2 or later | Later option permits GPLv3 |
| libmad 0.16.4, libmpeg2 0.5.1 | Prebuilt `mad.h`, `mpeg2dec/mpeg2.h` and matching source notices specify GPLv2 or later | Later option permits GPLv3 |
| a52dec 0.7.4, libdca 0.0.7 | Source `include/a52.h`, `include/dca.h` and implementation notices specify GPLv2 or later | Later option permits GPLv3 |
| libdvdnav 6.1.1, libdvdread 6.1.3, libdvdcss 1.4.3 | Implementation/header notices in the source archives specify GPLv2 or later | Later option permits GPLv3 |
| sidplay-libs 2.1.1, aribb24 1.0.3 | Library headers/implementation notices contain GNU later-version grants | No GPLv2-only restriction found |
| FFmpeg libraries | Compiled `libavcodec.a`, `libavformat.a`, `libavutil.a` contain LGPL 2.1-or-later license identifiers; compiled `libpostproc.a` reports GPLv2-or-later | Compatible GNU license versions; preserve FFmpeg notices/source |
| libsamplerate 0.1.9 | Archive `COPYING` contains a two-clause BSD grant | Compatible permissive license |
| projectM 2.0.1 source | `COPYING` and `projectM.hpp` specify LGPLv2.1 or later | Compatible LGPL grant |
| Qt 5.6.3 | Installed Qt headers provide LGPLv2.1/LGPLv3 alternatives | GPLv3 distribution supported under the appropriate LGPL terms |

These records are evidence from the actual pinned dependencies, not a claim that every optional library which VLC could ever load has been cleared. Build tools that are separate executables need their own notices if redistributed, but their licenses do not automatically become the player's combined license. FFmpeg's official licensing page confirms its LGPLv2.1-or-later base and GPLv2-or-later optional portions. [FFmpeg licensing](https://ffmpeg.org/legal.html), [FSF compatibility matrix](https://www.gnu.org/licenses/gpl-faq.en.html#AllCompatibility).

A source download or prebuilt prefix may contain libraries that the final player does not link. Keep matching versions, patches, build configuration, and notices for the libraries actually included in each release. Refresh this check when the pinned contrib changes. The release's dependency-source archive and collected notices provide the concrete audit material; this targeted compatibility check is not a byte-for-byte reproducibility certification.

## Release conclusion

A public GitHub fork and public installer distribution are permitted under the verified copyright licenses. The repository does not need to be private. Release Reframe under the GPLv3 policy above, retain the original component licenses, attribute the CC BY 4.0 design, and publish matching Corresponding Source beside the installer. VideoLAN explicitly permits modified redistribution without asking it for separate permission. [VideoLAN redistribution guidance](https://docs.videolan.me/vlc-user/en/support/faq/legalconcerns.html#may-i-redistribute-a-piece-of-videolan-software).

Use Reframe's own branding and keep the unofficial-fork statement. The copyright grants do not grant unrestricted VideoLAN trademarks or rights to third-party illustrative media. Codec patent and libdvdcss jurisdiction questions are separate from whether GPL source can be hosted publicly; VideoLAN notes the libdvdcss distribution issue in its legal FAQ. [VideoLAN legal FAQ](https://docs.videolan.me/vlc-user/en/support/faq/legalconcerns.html#is-libdvdcss-legal).
