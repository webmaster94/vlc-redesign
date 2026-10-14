/* Copyright 2026 webmaster94. GPL-3.0-or-later. */
#ifndef VLC_REDESIGN_PROFILE_HPP
#define VLC_REDESIGN_PROFILE_HPP
#include <QVariant>
namespace Reframe
{
bool profileValueSafe(const QString &, const QVariant &);
QVariantMap portableProfile(const QVariantMap &);
bool parseProfile(const QByteArray &, QVariantMap *);
} // namespace Reframe
#endif
