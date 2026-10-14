#!/usr/bin/env python3
"""Build a QtCore harness from the actual subtitle request implementation.

The fixture child is this harness itself. It emits only dummy protocol responses;
no network access, downloaded code or account secrets are used.
"""
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[4]
source = (root / 'modules/gui/qt/redesign_subtitles.cpp').read_text(encoding='utf-8')
start = source.index('namespace\n{')
end = source.index('} // namespace', start) + len('} // namespace')
implementation = source[start:end]
headers = '''#include <QCoreApplication>
#include <QProcess>
#include <QTimer>
#include <QUrl>
#include <QStringList>
#include <QByteArray>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QVariant>
#include <QEventLoop>
#include <QElapsedTimer>
#include <functional>
#include <memory>
#include <cstdio>
template<typename... Args> struct QOverload {
    template<typename R, typename T> static constexpr auto of(R (T::*ptr)(Args...)) -> decltype(ptr) { return ptr; }
};
namespace Reframe { QString systemCurl() { return QCoreApplication::applicationFilePath(); } }
'''
harness = r'''
static int checks = 0;
static void require(bool result, const char *name)
{
    ++checks;
    if (!result) { fprintf(stderr, "FAIL: %s\n", name); exit(1); }
}
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (app.arguments().contains("-q"))
    {
        const QByteArray kind = qgetenv("REFRAME_SECURITY_FIXTURE");
        if (kind == "stall") { QTimer::singleShot(60000, &app, &QCoreApplication::quit); return app.exec(); }
        if (kind == "oversized")
        {
            const QByteArray bytes(16384, 'x');
            for (int i = 0; i < 800; ++i) fwrite(bytes.constData(), 1, bytes.size(), stdout);
        }
        else if (kind == "stderr")
        {
            const QByteArray bytes(16384, 'x');
            for (int i = 0; i < 8; ++i) fwrite(bytes.constData(), 1, bytes.size(), stderr);
        }
        else
        {
            QByteArray config;
            char buffer[4096];
            size_t count;
            while ((count = fread(buffer, 1, sizeof(buffer), stdin))) config.append(buffer, int(count));
            QJsonObject response;
            if (kind == "error") response["message"] = QString(8192, 'x');
            response["config"] = QString::fromUtf8(config);
            response["arguments"] = QJsonArray::fromStringList(app.arguments().mid(1));
            const QByteArray bytes = QJsonDocument(response).toJson(QJsonDocument::Compact);
            fwrite(bytes.constData(), 1, bytes.size(), stdout);
        }
        fputs(kind == "error" ? "\n400" : "\n200", stdout); fflush(stdout); fflush(stderr);
        return 0;
    }
    const QStringList rejected = {
        "http://api.opensubtitles.com/api/v1/subtitles",
        "https://api.opensubtitles.com.evil.invalid/api/v1/subtitles",
        "https://evil.invalid/api/v1/subtitles",
        "https://user:password@api.opensubtitles.com/api/v1/subtitles",
        "https://@api.opensubtitles.com/api/v1/subtitles",
        "https://api.opensubtitles.com:8443/api/v1/subtitles",
        "https://api.opensubtitles.com/api/v1/subtitles#fragment",
        "https://api.opensubtitles.com/not-the-api",
        "file:///C:/dummy.txt"
    };
    for (const auto url : rejected)
    {
        bool called = false;
        request(&app, QUrl(url), "DUMMY_KEY", "DUMMY_TOKEN", QJsonObject(),
                [&](QByteArray bytes, QString error) { called = bytes.isEmpty() && !error.isEmpty(); });
        require(called && app.findChildren<QProcess *>().isEmpty(), "credential destination rejected before process launch");
    }
    for (const auto field : QStringList({"DUMMY\r\nHeader: bad", QString(8193, 'x'), QString("x") + QChar(0)}))
    {
        bool called = false;
        request(&app, QUrl("https://api.opensubtitles.com/api/v1/subtitles"), field, QString(), QJsonObject(),
                [&](QByteArray bytes, QString error) { called = bytes.isEmpty() && !error.isEmpty(); });
        require(called && app.findChildren<QProcess *>().isEmpty(), "unsafe headers rejected before process launch");
    }
    require(safeDestination(QUrl("https://vip-api.opensubtitles.com/api/v1/subtitles"), true), "VIP API accepted");
    require(!safeDestination(QUrl("https://user:pass@cdn.invalid/file"), false), "download URL cannot carry credentials");
    auto run = [&](const char *kind, bool credentials) -> QPair<QByteArray, QString>
    {
        fprintf(stderr, "Fixture: %s\n", kind); fflush(stderr);
        qputenv("REFRAME_SECURITY_FIXTURE", kind);
        QObject owner;
        QEventLoop loop;
        QPair<QByteArray, QString> result;
        request(&owner, QUrl(credentials ? "https://api.opensubtitles.com/api/v1/subtitles" : "https://cdn.invalid/dummy.srt"),
                credentials ? "DUMMY_KEY" : "", "", QJsonObject(),
                [&](QByteArray bytes, QString error) { result = qMakePair(bytes, error); loop.quit(); });
        loop.exec();
        return result;
    };
    auto normal = run("normal", true);
    const auto arguments = QJsonDocument::fromJson(normal.first).object()["arguments"].toArray().toVariantList();
    require(normal.second.isEmpty(), "normal response accepted");
    require(arguments.first().toString() == "-q", "ambient curl config disabled first");
    require(!arguments.contains(QVariant("DUMMY_KEY")) && QJsonDocument::fromJson(normal.first).object()["config"].toString().contains("Api-Key: DUMMY_KEY"), "credentials travel through stdin");
    require(!arguments.contains(QVariant("--location")), "credentialed API never follows redirects");
    auto download = run("normal", false);
    const auto downloadArgs = QJsonDocument::fromJson(download.first).object()["arguments"].toArray().toVariantList();
    require(downloadArgs.contains(QVariant("--location")) && downloadArgs.contains(QVariant("--max-redirs")), "anonymous redirect count bounded");
    require(!QJsonDocument::fromJson(download.first).object()["config"].toString().contains("Api-Key") &&
            !QJsonDocument::fromJson(download.first).object()["config"].toString().contains("Authorization"), "anonymous download has no credential headers");
    require(downloadArgs.contains(QVariant("=https")), "download protocols restricted to HTTPS");
    require(run("error", true).second.size() == 4096, "service error display text bounded");
    require(run("oversized", true).second.contains("size limit"), "oversized stdout rejected");
    require(run("stderr", true).second.contains("size limit"), "oversized stderr rejected");
    require(run("stall", true).second.contains("timed out"), "independent watchdog kills stalled client");
    qputenv("REFRAME_SECURITY_FIXTURE", "stall");
    auto owner = new QObject;
    bool completionAfterDestruction = false;
    request(owner, QUrl("https://api.opensubtitles.com/api/v1/subtitles"), "DUMMY_KEY", "", QJsonObject(),
            [&](QByteArray, QString) { completionAfterDestruction = true; });
    QPointer<QProcess> child(owner->findChild<QProcess *>());
    QElapsedTimer teardown; teardown.start();
    delete owner;
    require(teardown.elapsed() < 2500, "starting child teardown is bounded");
    require(child.isNull() && !completionAfterDestruction, "owner destruction stops request without dangling callback");
    printf("PASS: %d subtitle request security checks\n", checks);
    return 0;
}
'''
output = Path(sys.argv[1])
output.write_text(headers + implementation + harness, encoding='utf-8', newline='\n')
