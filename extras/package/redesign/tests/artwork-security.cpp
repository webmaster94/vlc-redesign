/* Copyright 2026 webmaster94. GPL-3.0-or-later. */
// Only synthetic local fixtures are used. No remote address is requested.
#include "redesign_image.hpp"
#include <QGuiApplication>
#include <QImage>
#include <QFile>
#include <QTemporaryDir>
#include <QDir>
#include <QtPlugin>
#include <cstdio>
#include <windows.h>
#include <winioctl.h>
#include <cstring>

// The local MinGW 10 import library omits this CRT import used by the pinned
// MinGW 6 Qt archive. Resolve the real Windows CRT argc data for this probe only.
#if __GNUC__ >= 10
static int *crtArgc()
{
    return reinterpret_cast<int *>(GetProcAddress(GetModuleHandleW(L"msvcrt.dll"), "__argc"));
}
extern "C"
{
int *(__cdecl *__imp___p___argc)(void) = crtArgc;
}
#endif

Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)

static int failures = 0;
static void check(bool condition, const char *message)
{
    if (!condition)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}
static void writeFile(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    check(file.open(QIODevice::WriteOnly), "fixture open");
    check(file.write(bytes) == bytes.size(), "fixture write");
}
int main(int argc, char **argv)
{
#if __GNUC__ >= 10
    if (!crtArgc())
        return 2;
#endif
    QGuiApplication app(argc, argv);
    QTemporaryDir directory;
    check(directory.isValid(), "isolated temporary directory");
    const QString png = directory.path() + "/valid.png";
    const QString jpeg = directory.path() + "/valid.jpg";
    QImage small(32, 24, QImage::Format_RGB32);
    small.fill(Qt::blue);
    check(small.save(png, "PNG"), "PNG fixture encoder");
    check(small.save(jpeg, "JPEG"), "JPEG fixture encoder");
    check(!Reframe::safeArtwork(png).isNull(), "valid PNG accepted");
    check(!Reframe::safeArtwork(jpeg).isNull(), "valid JPEG accepted");
    const QString scaling = directory.path() + "/large-but-bounded.png";
    QImage large(2048, 1500, QImage::Format_RGB32);
    large.fill(Qt::green);
    check(large.save(scaling, "PNG"), "large fixture encoder");
    const QIcon scaled = Reframe::safeArtwork(scaling);
    check(!scaled.isNull(), "bounded image accepted");
    const QPixmap output = scaled.pixmap(4096, 4096);
    check(output.width() <= 1024 && output.height() <= 1024, "decoded output scaled to 1024");
    const QString wide = directory.path() + "/over-dimension.png";
    QImage wideImage(4097, 1, QImage::Format_RGB32);
    wideImage.fill(Qt::red);
    check(wideImage.save(wide, "PNG"), "over-dimension fixture encoder");
    check(Reframe::safeArtwork(wide).isNull(), "dimension ceiling rejects header before decode");
    const QString svg = directory.path() + "/disguised.png";
    writeFile(svg, "<svg xmlns='http://www.w3.org/2000/svg'><image href='https://invalid.example/x'/></svg>");
    check(Reframe::safeArtwork(svg).isNull(), "SVG disguised as PNG rejected");
    const QString badPng = directory.path() + "/bad.png";
    writeFile(badPng, QByteArray::fromHex("89504e470d0a1a0a") + "not a PNG");
    check(Reframe::safeArtwork(badPng).isNull(), "corrupt PNG rejected");
    const QString badJpeg = directory.path() + "/bad.jpg";
    writeFile(badJpeg, QByteArray::fromHex("ffd8ff") + "not a JPEG");
    check(Reframe::safeArtwork(badJpeg).isNull(), "corrupt JPEG rejected");
    const QString oversize = directory.path() + "/oversized.png";
    QFile huge(oversize);
    check(huge.open(QIODevice::WriteOnly), "oversize fixture open");
    check(huge.resize(16 * 1024 * 1024 + 1), "oversize fixture resize");
    huge.close();
    check(Reframe::safeArtwork(oversize).isNull(), "byte ceiling rejected");
    const QString empty = directory.path() + "/empty.png";
    writeFile(empty, QByteArray());
    check(Reframe::safeArtwork(empty).isNull(), "empty file rejected");
    check(Reframe::safeArtwork(directory.path() + "/missing.png").isNull(), "missing file rejected");
    check(Reframe::safeArtwork(directory.path()).isNull(), "directory rejected");
    check(Reframe::safeArtwork("relative.png").isNull(), "relative path rejected");
    check(Reframe::safeArtwork(":/icons/cone.svg").isNull(), "Qt resource rejected");
    check(Reframe::safeArtwork("https://invalid.example/cover.png").isNull(), "HTTPS URL rejected");
    check(Reframe::safeArtwork("file:///C:/missing.png").isNull(), "file URL rejected");
    check(Reframe::safeArtwork("\\\\invalid.example\\art\\cover.png").isNull(), "UNC path rejected");
    check(Reframe::safeArtwork("//invalid.example/art/cover.png").isNull(), "slash UNC rejected");
    check(Reframe::safeArtwork(png + ":stream").isNull(), "NTFS alternate stream rejected");
    check(Reframe::safeArtwork(png + QChar(0) + "hidden").isNull(), "embedded NUL rejected");
    // A junction is created only inside our temporary directory, targeting that
    // same directory. RemoveDirectoryW unlinks it before QTemporaryDir cleanup.
    const QString junction = directory.path() + "/junction";
    check(QDir().mkdir(junction), "junction fixture directory");
    HANDLE handle = CreateFileW(reinterpret_cast<const wchar_t *>(junction.utf16()), GENERIC_WRITE, 0,
                                nullptr, OPEN_EXISTING,
                                FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    check(handle != INVALID_HANDLE_VALUE, "junction fixture handle");
    if (handle != INVALID_HANDLE_VALUE)
    {
        struct MountPoint
        {
            DWORD tag;
            USHORT length, reserved, substituteOffset, substituteLength, printOffset, printLength;
            wchar_t text[1024];
        } data = {};
        const QString target = "\\??\\" + QDir::toNativeSeparators(directory.path());
        data.tag = IO_REPARSE_TAG_MOUNT_POINT;
        data.substituteLength = USHORT(target.size() * sizeof(wchar_t));
        data.printOffset = data.substituteLength + sizeof(wchar_t);
        data.length = 8 + data.printOffset + sizeof(wchar_t);
        std::memcpy(data.text, target.utf16(), data.substituteLength);
        DWORD returned = 0;
        const BOOL created = DeviceIoControl(handle, FSCTL_SET_REPARSE_POINT, &data, data.length + 8,
                                            nullptr, 0, &returned, nullptr);
        CloseHandle(handle);
        check(created, "local junction fixture created");
        if (created)
            check(Reframe::safeArtwork(junction + "/valid.png").isNull(), "junction rejected before stat");
    }
    check(RemoveDirectoryW(reinterpret_cast<const wchar_t *>(junction.utf16())), "unlink test junction");
    if (!failures)
        std::puts("PASS: 21 synthetic artwork cases, forced PNG/JPEG, path/reparse/byte/dimension limits and 1024px output");
    return failures ? 1 : 0;
}
