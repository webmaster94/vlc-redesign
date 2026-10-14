/* Copyright 2026 webmaster94. GPL-3.0-or-later. */
#include "redesign_image.hpp"
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QPixmap>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace
{
const qint64 byteLimit = 16 * 1024 * 1024;
const int dimensionLimit = 4096;
const qint64 pixelLimit = 16 * 1024 * 1024;

bool localPath(const QString &path)
{
    const QString normal = QDir::fromNativeSeparators(path);
    if (normal.isEmpty() || normal.contains(QChar(0)) || !QDir::isAbsolutePath(normal) ||
        normal.startsWith("//") || normal.startsWith(":/"))
        return false;
#ifdef Q_OS_WIN
    // Reject URL/resource syntax, device paths and NTFS alternate data streams.
    if (normal.size() < 3 || !normal[0].isLetter() || normal[1] != ':' || normal[2] != '/' ||
        normal.indexOf(':', 2) >= 0)
        return false;
    const wchar_t root[] = {normal[0].unicode(), L':', L'\\', 0};
    const UINT drive = GetDriveTypeW(root);
    return drive == DRIVE_FIXED || drive == DRIVE_REMOVABLE || drive == DRIVE_CDROM || drive == DRIVE_RAMDISK;
#else
    return normal.startsWith('/') && !normal.contains(':');
#endif
}

bool boundedSize(const QSize &size)
{
    return size.width() > 0 && size.height() > 0 && size.width() <= dimensionLimit &&
           size.height() <= dimensionLimit && qint64(size.width()) * size.height() <= pixelLimit;
}

bool plainLocalFilePath(const QString &path)
{
#ifdef Q_OS_WIN
    // Inspect each component before following it. A local-looking junction can
    // otherwise cause QFileInfo/canonicalFilePath to contact an SMB share.
    // Rejecting reparse points also excludes some cloud-backed artwork paths.
    const QString normal = QDir::fromNativeSeparators(path);
    QString cursor = normal.left(3);
    const auto parts = normal.mid(3).split('/', QString::SkipEmptyParts);
    auto ordinary = [](const QString &component)
    {
        const DWORD attributes = GetFileAttributesW(reinterpret_cast<const wchar_t *>(component.utf16()));
        return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_REPARSE_POINT);
    };
    if (!ordinary(cursor))
        return false;
    for (const auto &part : parts)
    {
        if (part == "." || part == "..")
            return false;
        if (!cursor.endsWith('/'))
            cursor += '/';
        cursor += part;
        if (!ordinary(cursor))
            return false;
    }
#else
    Q_UNUSED(path);
#endif
    return true;
}
} // namespace

QIcon Reframe::safeArtwork(const QString &path)
{
    if (!localPath(path) || !plainLocalFilePath(path))
        return QIcon();
    const QFileInfo info(path);
    if (!info.isFile())
        return QIcon();
    const QString canonical = info.canonicalFilePath();
    if (!localPath(canonical) || !plainLocalFilePath(canonical))
        return QIcon();
    QFile file(canonical);
    if (!file.open(QIODevice::ReadOnly) || file.size() <= 0 || file.size() > byteLimit)
        return QIcon();
    // Bound the read as well as the metadata check, including concurrent growth.
    QByteArray bytes = file.read(byteLimit + 1);
    if (file.error() != QFile::NoError || bytes.isEmpty() || bytes.size() > byteLimit)
        return QIcon();
    file.close();
    QByteArray format;
    if (bytes.startsWith(QByteArray::fromHex("89504e470d0a1a0a")))
        format = "png";
    else if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xff &&
             static_cast<unsigned char>(bytes[1]) == 0xd8 && static_cast<unsigned char>(bytes[2]) == 0xff)
        format = "jpeg";
    else
        return QIcon();
    QBuffer buffer(&bytes);
    if (!buffer.open(QIODevice::ReadOnly))
        return QIcon();
    QImageReader reader(&buffer, format);
    // Never dispatch by filename or allow another plugin to sniff the content.
    reader.setAutoDetectImageFormat(false);
    reader.setDecideFormatFromContent(false);
    const QSize declared = reader.size();
    if (!boundedSize(declared))
        return QIcon();
    if (declared.width() > 1024 || declared.height() > 1024)
        reader.setScaledSize(declared.scaled(QSize(1024, 1024), Qt::KeepAspectRatio));
    QImage image = reader.read();
    if (image.isNull() || !boundedSize(image.size()))
        return QIcon();
    if (image.width() > 1024 || image.height() > 1024)
        image = image.scaled(1024, 1024, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return QIcon(QPixmap::fromImage(image));
}
