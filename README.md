# VLC redesign

An unofficial fork of [VideoLAN's VLC media player](https://github.com/videolan/vlc), starting from VLC 3.0.24.

The intended interface follows [VLC Redesign by Juxtopposed](https://www.figma.com/community/file/1691465999134370980/vlc-redesign). The design is published under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).

## Current status

The public fork, licensing review, stable source baseline, and Windows build validation workflow are set up. The redesigned interface and its installer are **not implemented yet**. The Figma Community description and license were accessible, but its design canvas did not render during the initial inspection. A view-only design file or exported frames are needed to implement and compare the actual screens.

There is no redesigned installer available for download. This fork is not an official VideoLAN release.

## Development

Development is on the `redesign` branch. The upstream 3.0.24 commit is `6de05adcbaf2e8b85fe86aad4169393098628119`.

The manual **Windows build validation** GitHub Actions workflow compiles the upstream player with VideoLAN's Windows x64 build image and the matching prebuilt dependencies. It validates the toolchain and produces logs. It does not publish an installer or a GitHub release.

See [the implementation notes](doc/redesign/implementation.md) and [the licensing findings](doc/redesign/licensing.md).

## Licensing and credits

VLC's existing source licenses and notices remain in force. See [COPYING](COPYING) and [COPYING.LIB](COPYING.LIB), and the notices in individual files. Distributing a modified executable requires access to the corresponding source, including modifications and build/installation scripts.

Design reference: **VLC Redesign**, by **Juxtopposed**, [original resource](https://www.figma.com/community/file/1691465999134370980/vlc-redesign), [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). The planned application is a software adaptation. No design artwork or fonts have been incorporated yet.

VideoLAN and VLC are upstream project names. This fork has no VideoLAN or Juxtopposed endorsement. A distinct product identity will be used for the distributed application.
