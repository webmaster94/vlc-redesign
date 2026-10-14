/* Copyright 2026 webmaster94. GPL-3.0-or-later.
 * Local adversarial regression tests for the production timeline/PCM parser.
 * Build with Qt5Core and optionally -fsanitize=address,undefined. */
#include "../../../../modules/gui/qt/redesign_seek_data.hpp"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QElapsedTimer>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <random>

using namespace ReframeSeekData;
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    assert(!workerMediaAllowed(QString()));
    assert(!workerMediaAllowed("--reset-config"));
    assert(!workerMediaAllowed(":sout=invalid"));
    assert(!workerMediaAllowed(QString("file:///media") + QChar(0) + "suffix"));
    assert(workerMediaAllowed("file:///C:/media/--title.ogg"));
    assert(workerMediaAllowed("https://example.invalid/video.mp4"));
    QTemporaryDir directory;
    assert(directory.isValid());
    const QString path = directory.path() + "/hostile.srt";
    auto parse = [&](const QByteArray &bytes)
    {
        QFile file(path);
        assert(file.open(QIODevice::WriteOnly));
        assert(file.write(bytes) == bytes.size());
        file.close();
        return loadCues(path);
    };
    auto valid = parse("1\r\n00:00:01,000 --> 00:00:02,500\r\n<b>Hello</b> world\r\n\r\n"
                       "2\r\n00:03.000 --> 00:04.000 align:start\r\nsecond\r\n");
    assert(valid.size() == 2 && valid[0].from == 1 && valid[0].to == 2.5);
    assert(valid[0].text == "Hello world" && valid[1].text == "second");
    for (const QByteArray &stamp : {QByteArray("nan:00:00"), QByteArray("1e300:00:00"),
                                  QByteArray("-1:00:00"), QByteArray("00:61:00"),
                                  QByteArray("00:00:Inf"), QByteArray("00:00:invalid")})
        assert(parse(stamp + " --> 00:01:00\ntext\n\n").isEmpty());
    assert(parse("00:00:02 --> 00:00:01\nreverse\n").isEmpty());
    assert(parse("00:00:02 --> 00:00:02\nzero\n").isEmpty());
    assert(parse("00:00:01 --> 00:00:02\n" + QByteArray(MaxLineBytes + 1, 'x')).isEmpty());
    auto longText = parse("00:00:01 --> 00:00:02\n" + QByteArray(4096, 'x'));
    assert(longText.size() == 1 && longText[0].text.size() == MaxCueText);
    QByteArray many;
    for (int i = 0; i < MaxCues + 100; ++i)
        many += "00:00:01 --> 00:00:02\ntext\n\n";
    assert(parse(many).size() == MaxCues);
    assert(plainText(QString(8192, '<')).size() == MaxCueText);
    assert(plainText("<b>A</b><i>B</i>") == "AB");
    {
        QFile file(path);
        assert(file.open(QIODevice::WriteOnly));
        assert(file.resize(MaxSubtitleBytes + 1));
    }
    QElapsedTimer timer;
    timer.start();
    assert(loadCues(path).isEmpty());
    assert(timer.elapsed() < 1000);
    const QString raw = directory.path() + "/hostile.raw";
    for (int count : {0, 1, 31, 1023, 1024, 1025, 8192})
    {
        QFile file(raw);
        assert(file.open(QIODevice::WriteOnly));
        QByteArray samples(count * 2, 0);
        for (int i = 0; i < count; ++i)
            qToLittleEndian<qint16>(i % 2 ? -32768 : 32767,
                reinterpret_cast<uchar *>(samples.data()) + i * 2);
        assert(file.write(samples) == samples.size());
        file.close();
        const auto wave = loadWaveform(raw);
        assert(wave.size() == (count ? 1024 : 0));
        for (float peak : wave)
            assert(std::isfinite(peak) && peak >= 0 && peak <= 1);
    }
    {
        QFile file(raw);
        assert(file.open(QIODevice::WriteOnly));
        assert(file.write("odd", 3) == 3);
    }
    assert(loadWaveform(raw).isEmpty());
    {
        QFile file(raw);
        assert(file.open(QIODevice::WriteOnly));
        assert(file.resize(MaxWaveformBytes + 2));
    }
    assert(loadWaveform(raw).isEmpty());
    // Deterministic malformed byte corpus exercises the actual implementation,
    // including UTF-8 decoding, timestamps, delimiters and tag handling.
    std::mt19937 random(0x53454355);
    for (int attempt = 0; attempt < 5000; ++attempt)
    {
        QByteArray bytes(random() % 2048, '\0');
        for (int i = 0; i < bytes.size(); ++i)
            bytes[i] = char(random() & 255);
        if (attempt % 2)
            bytes.prepend("00:00:01 --> 00:00:02\n");
        const auto cues = parse(bytes);
        assert(cues.size() <= MaxCues);
        for (const auto &cue : cues)
            assert(std::isfinite(cue.from) && std::isfinite(cue.to) &&
                   cue.to > cue.from && cue.text.size() <= MaxCueText);
    }
    std::printf("PASS: valid SRT/VTT; malformed timestamps/UTF-8/tags; oversized files/lines; "
                "cue/text caps; PCM bounds; 5000 randomized cases (Qt %s).\n", qVersion());
}
