/* Copyright 2026 webmaster94. GPL-3.0-or-later. */
#ifndef VLC_REDESIGN_SEEK_DATA_HPP
#define VLC_REDESIGN_SEEK_DATA_HPP
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QVector>
#include <QtEndian>
#include <cmath>

namespace ReframeSeekData
{
static const qint64 MaxSubtitleBytes = 8 * 1024 * 1024;
static const int MaxLineBytes = 8192;
static const int MaxCueText = 2048;
static const int MaxCues = 20000;
static const qint64 MaxWaveformBytes = 32 * 1024 * 1024;
struct Cue
{
    double from, to;
    QString text;
};

inline bool cueTime(QString value, double &seconds)
{
    const auto parts = value.trimmed().replace(',', '.').split(':');
    if (parts.size() < 2 || parts.size() > 3)
        return false;
    seconds = 0;
    for (int i = 0; i < parts.size(); ++i)
    {
        bool valid = false;
        const double component = parts[i].toDouble(&valid);
        if (!valid || !std::isfinite(component) || component < 0 || (i > 0 && component >= 60))
            return false;
        seconds = seconds * 60 + component;
    }
    return std::isfinite(seconds) && seconds <= 7 * 24 * 3600;
}

// Linear tag removal: an unterminated tag remains plain text.
inline QString plainText(const QString &text)
{
    QString result;
    result.reserve(qMin(text.size(), MaxCueText));
    int tag = -1;
    for (int i = 0; i < text.size() && result.size() < MaxCueText; ++i)
    {
        if (text[i] == '<' && tag < 0)
            tag = i;
        else if (text[i] == '>' && tag >= 0)
            tag = -1;
        else if (tag < 0)
            result += text[i];
    }
    if (tag >= 0 && result.size() < MaxCueText)
        result += text.mid(tag, MaxCueText - result.size());
    return result;
}

inline QVector<Cue> loadCues(const QString &path)
{
    QVector<Cue> cues;
    QFile file(path);
    // Avoid devices, FIFOs and indefinite reads. The byte budget is also enforced
    // after opening, so growth or replacement cannot bypass the initial stat.
    if (!QFileInfo(path).isFile() || !file.open(QIODevice::ReadOnly) || file.isSequential() ||
        file.size() > MaxSubtitleBytes)
        return cues;
    qint64 remaining = MaxSubtitleBytes;
    bool inCue = false, invalidBlock = false;
    Cue cue = {0, 0, QString()};
    while (!file.atEnd() && remaining > 0 && cues.size() < MaxCues)
    {
        // QFile::readLine reserves one byte for its internal NUL terminator.
        const QByteArray bytes = file.readLine(qMin<qint64>(remaining + 2, MaxLineBytes + 2));
        remaining -= bytes.size();
        if (remaining < 0 || bytes.size() > MaxLineBytes)
        {
            // Reject rather than accumulating the continuation of a giant line.
            return QVector<Cue>();
        }
        const QString line = QString::fromUtf8(bytes).trimmed();
        if (line.isEmpty())
        {
            if (inCue && !invalidBlock)
                cues << cue;
            inCue = invalidBlock = false;
            cue.text.clear();
            continue;
        }
        const int arrow = line.indexOf("-->");
        if (arrow >= 0)
        {
            const QString end = line.mid(arrow + 3).trimmed().section(' ', 0, 0);
            inCue = cueTime(line.left(arrow), cue.from) && cueTime(end, cue.to) && cue.to > cue.from;
            invalidBlock = !inCue;
            cue.text.clear();
        }
        else if (inCue && cue.text.size() < MaxCueText)
        {
            if (!cue.text.isEmpty())
                cue.text += ' ';
            cue.text += plainText(line).left(MaxCueText - cue.text.size());
        }
    }
    if (remaining == 0 && !file.atEnd())
        return QVector<Cue>();
    if (inCue && !invalidBlock && cues.size() < MaxCues)
        cues << cue;
    return cues;
}

inline QVector<float> loadWaveform(const QString &path)
{
    QVector<float> waveform;
    QFile file(path);
    if (!QFileInfo(path).isFile() || !file.open(QIODevice::ReadOnly) || file.isSequential() ||
        file.size() > MaxWaveformBytes)
        return waveform;
    const QByteArray data = file.read(MaxWaveformBytes + 1);
    if (data.size() > MaxWaveformBytes || data.size() % 2)
        return waveform;
    const int samples = data.size() / 2;
    if (!samples)
        return waveform;
    for (int bin = 0; bin < 1024; ++bin)
    {
        float peak = 0;
        for (int i = int(qint64(bin) * samples / 1024); i < int(qint64(bin + 1) * samples / 1024); ++i)
            peak = qMax(peak, qAbs(int(qFromLittleEndian<qint16>(
                                  reinterpret_cast<const uchar *>(data.constData()) + i * 2))) /
                                  32768.f);
        waveform << peak;
    }
    return waveform;
}
} // namespace ReframeSeekData
#endif
