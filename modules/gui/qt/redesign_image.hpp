/* Copyright 2026 webmaster94. GPL-3.0-or-later. */
#ifndef VLC_REDESIGN_IMAGE_HPP
#define VLC_REDESIGN_IMAGE_HPP
#include <QIcon>
#include <QString>

namespace Reframe
{
// Artwork is untrusted. Fail closed to an empty icon, with no external requests.
QIcon safeArtwork(const QString &path);
} // namespace Reframe
#endif
