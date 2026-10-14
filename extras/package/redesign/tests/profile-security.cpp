/* Copyright 2026 webmaster94. GPL-3.0-or-later. */
#include "redesign_profile.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>
#include <cstdio>
#include <limits>
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    int cases = 0;
    auto check = [&cases](bool valid) { ++cases; if (!valid) { fprintf(stderr, "FAIL case %d\n", cases); exit(1); } };
    for (const auto &key : QStringList({"extraintf", "intf", "lua-config", "lua-intf", "rc-host", "http-host",
         "http-password", "sout", "plugin-path", "logfile", "ui:extraArgs", "ui:webRemote", "ui:useSkin",
         "ui:systemProxy", "ui:VideosDirectory", "ui:MusicDirectory", "ordinal:unknown"}))
        check(!Reframe::profileValueSafe(key, QString("dummy")));
    check(Reframe::profileValueSafe("ui:theme", QString("Dark")));
    check(!Reframe::profileValueSafe("ui:theme", QString("unknown")));
    check(Reframe::profileValueSafe("ui:accent", QString("#008ba8")));
    check(!Reframe::profileValueSafe("ui:accent", QString("red; padding: 100px")));
    check(Reframe::profileValueSafe("equalizer-bands", QString("0 0 0 0 0 0 0 0 0 0")));
    check(!Reframe::profileValueSafe("equalizer-bands", QString("nan 0 0 0 0 0 0 0 0 0")));
    check(!Reframe::profileValueSafe("equalizer-preamp", std::numeric_limits<double>::infinity()));
    check(Reframe::profileValueSafe("audio-filter", QString("equalizer:compressor")));
    check(!Reframe::profileValueSafe("audio-filter", QString("unknown_module")));
    check(!Reframe::profileValueSafe("freetype-font", QString("//server/font")));
    check(Reframe::profileValueSafe("ui:toolbar", QStringList({"play", "seek", "fullscreen"})));
    check(!Reframe::profileValueSafe("ui:toolbar", QStringList({"arbitrary_action"})));
    QVariantMap parsed;
    check(Reframe::parseProfile("{\"ui:theme\":\"Dark\",\"ui:toolbar\":[\"play\",\"seek\"]}", &parsed));
    check(!Reframe::parseProfile("{\"extraintf\":\"dummy\"}", &parsed));
    check(!Reframe::parseProfile(QByteArray(256 * 1024 + 1, ' '), &parsed));
    check(!Reframe::parseProfile("[]", &parsed));
    check(!Reframe::parseProfile("{bad}", &parsed));
    check(!Reframe::parseProfile(QByteArray(10000, '[') + QByteArray(10000, ']'), &parsed));
    // Deterministic mutation corpus exercises JSON and the actual production validator.
    const QByteArray seed("{\"ui:theme\":\"Dark\",\"equalizer-preamp\":0,\"ui:toolbar\":[\"play\"]}");
    for (int i = 0; i < 20000; ++i)
    {
        QByteArray bytes = seed;
        bytes[i % bytes.size()] = char((i * 37) % 256);
        if (Reframe::parseProfile(bytes, &parsed))
            for (auto it = parsed.begin(); it != parsed.end(); ++it)
                check(Reframe::profileValueSafe(it.key(), it.value()));
    }
    printf("PASS: %d assertions and 20000 deterministic profile mutations\n", cases);
}
