# VLC redesign licensing findings

Checked 12 October 2026 against primary sources. This note covers source and installer redistribution, branding, and the linked Figma design.

## Public hosting is allowed by VLC's license

VLC is open source. Its upstream README specifies GPLv2-or-later for the player and notes that dependencies can make some platform builds effectively GPLv3. The repository contains both GPL and LGPL license texts. A public GitHub fork is allowed; personal use is not the only permitted use. [Upstream README](https://github.com/videolan/vlc#license).

VideoLAN expressly allows distributing original or modified versions when the license terms are followed and says no separate permission from its team is necessary. [VideoLAN legal FAQ](https://docs.videolan.me/vlc-user/en/support/faq/legalconcerns.html).

Private modification and internal use do not force public publication. Binary distribution changes that: recipients must receive the GPL rights and access to the corresponding modified source. A private repository does not avoid those obligations if its executable is shared outside the private use context. [FSF GPL FAQ](https://www.gnu.org/licenses/gpl-faq.en.html#GPLRequireSourcePostedPublic).

GitHub's standard forks of public repositories are public, and their visibility cannot be changed independently. Private work would use a separate private repository or local clone rather than a normal private GitHub fork of `videolan/vlc`. [GitHub fork visibility](https://docs.github.com/en/pull-requests/reference/forks#visibility-of-forks).

## What to ship with an installer

Under GPLv2 sections 1-3 and 6:

- Keep copyright, license, and warranty notices, and provide the GPL text.
- Mark modified files with notices of the changes and their dates.
- License the distributed derivative player under the GPL without extra restrictions on recipients.
- Provide complete corresponding source for the exact distributed build. This includes the source of its modules, interface definitions, and compilation and installation scripts, with the license's system-component exception.
- Equivalent source-download access alongside the binary download is an allowed approach. A three-year written offer is another option, but placing a source archive beside each installer is simpler.

An upstream-only source link does not cover the fork's modifications. A GitHub release should carry the installer, the matching source snapshot, build and packaging scripts, and the necessary dependency source and notices. Verify the selected dependencies before settling the release's GPL version. [VLC's GPLv2 text](https://raw.githubusercontent.com/videolan/vlc/master/COPYING).

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

The FSF describes CC BY 4.0 as compatible with all GPL versions, while recommending against using Creative Commons licenses for software code. Use GPL notices on original implementation code and retain the design/asset attribution. [FSF license list, CC BY](https://www.gnu.org/licenses/license-list.html.html#ccby).

Making the repository private does not by itself grant rights to bundled assets. The verified CC BY 4.0 license supports public adaptation with attribution. Review the design file or an authorized export for third-party asset credits before importing its artwork.

## Remaining release work

The VLC copyright terms and the verified Figma CC BY 4.0 license support a public fork and public installer release with the notices and source described above. The main agent created the public fork [webmaster94/vlc-redesign](https://github.com/webmaster94/vlc-redesign). Third-party assets in the design still need review. The main agent also verified the latest stable Windows x64 installer as VLC 3.0.24 in the [official download directory](https://download.videolan.org/pub/videolan/vlc/last/win64/). Before publishing binaries, also inventory the dependencies actually bundled. VideoLAN identifies libdvdcss distribution as legally controversial in some countries, including the United States, so GPL compliance alone is not a complete jurisdiction-specific clearance for every optional module. This note does not assert that hosting any possible VLC build is free of all patent, trademark, or other legal issues. [VideoLAN legal FAQ](https://docs.videolan.me/vlc-user/en/support/faq/legalconcerns.html#is-libdvdcss-legal).

