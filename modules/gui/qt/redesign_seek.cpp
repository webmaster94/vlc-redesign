/* Copyright 2026 webmaster94. GPL-3.0-or-later.
 * Seek thumbnail, waveform, and draggable subtitle timeline from
 * Juxtopposed's VLC Redesign. CC BY 4.0; see doc/redesign/licensing.md. */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "redesign_seek.hpp"
#include "input_manager.hpp"
#include <vlc_configuration.h>
#include <vlc_input.h>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QLabel>
#include <QBoxLayout>
#include <QProcess>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QApplication>
#include <QSettings>
#include <QRegularExpression>
#include <QUrl>
#include <QtEndian>
#include <cmath>

namespace
{
QString clock(qint64 value)
{
    return QString("%1:%2:%3")
        .arg(value / 3600, 2, 10, QChar('0'))
        .arg(value / 60 % 60, 2, 10, QChar('0'))
        .arg(value % 60, 2, 10, QChar('0'));
}
double cueTime(const QString &s)
{
    auto parts = s.trimmed().replace(',', '.').split(':');
    if (parts.size() == 3)
        return parts[0].toDouble() * 3600 + parts[1].toDouble() * 60 + parts[2].toDouble();
    if (parts.size() == 2)
        return parts[0].toDouble() * 60 + parts[1].toDouble();
    return 0;
}
} // namespace
RedesignSeek::RedesignSeek(intf_thread_t *intf, QWidget *parent)
    : QSlider(Qt::Horizontal, parent), p_intf(intf), debounce(this), preview(nullptr), process(nullptr),
      hoverTime(0), duration(0), draggingSubtitles(false), originalDelay(0)
{
    setMouseTracking(true);
    setMinimumHeight(30);
    char *dir = config_GetUserDir(VLC_CACHE_DIR);
    cache = qfu(dir) + "/seek";
    free(dir);
    QDir().mkpath(cache);
    debounce.setSingleShot(true);
    debounce.setInterval(140);
    connect(&debounce, &QTimer::timeout, this, [this] { thumbnail(); });
    preview = new QWidget(nullptr, Qt::ToolTip | Qt::FramelessWindowHint);
    preview->setObjectName("seekPreview");
    auto layout = new QVBoxLayout(preview);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(8);
    image = new QLabel;
    image->setFixedSize(256, 144);
    image->setAlignment(Qt::AlignCenter);
    layout->addWidget(image);
    stamp = new QLabel;
    stamp->setAlignment(Qt::AlignCenter);
    layout->addWidget(stamp);
    connect(THEMIM, &MainInputManager::inputChanged, this, [this](bool) { refreshMedia(); });
    connect(THEMIM->getIM(), &InputManager::positionUpdated, this,
            [this](float, int64_t, int length)
            {
                duration = length;
                update();
            });
    connect(THEMIM->getIM(), &InputManager::synchroChanged, this, [this] { update(); });
}
RedesignSeek::~RedesignSeek()
{
    if (process)
    {
        process->kill();
        process->waitForFinished(1000);
    }
    delete preview;
}
void RedesignSeek::refreshMedia()
{
    cues.clear();
    waveform.clear();
    auto item = THEMIM->currentInputItem();
    char *uri = item ? input_item_GetURI(item) : nullptr;
    media = qfu(uri ? uri : "");
    free(uri);
    duration = item ? qMax<qint64>(0, input_item_GetDuration(item) / CLOCK_FREQ) : 0;
    const QString path = QUrl(media).isLocalFile() ? QUrl(media).toLocalFile() : media;
    auto bindings = getSettings()->value("Redesign/bindings/" + path).toStringList();
    if (bindings.isEmpty())
        bindings = getSettings()->value("Redesign/bindings/" + media).toStringList();
    subtitle = bindings.isEmpty() ? QString() : bindings.first();
    if (subtitle.isEmpty())
    {
        const QFileInfo f(path);
        const QString candidate = f.absolutePath() + "/" + f.completeBaseName() + ".srt";
        if (QFileInfo::exists(candidate))
            subtitle = candidate;
    }
    loadCues();
    if (getSettings()->value("Redesign/waveform", false).toBool())
        decodeWaveform();
    update();
}
void RedesignSeek::loadCues()
{
    QFile file(subtitle);
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QString text = QString::fromUtf8(file.readAll()).replace("\r", "");
    const auto blocks = text.split(QRegularExpression("\n\\s*\n"), QString::SkipEmptyParts);
    for (const auto &block : blocks)
    {
        const auto lines = block.split('\n');
        for (int i = 0; i < lines.size(); ++i)
        {
            if (!lines[i].contains("-->"))
                continue;
            const auto times = lines[i].split("-->");
            if (times.size() != 2)
                continue;
            Cue cue;
            cue.from = cueTime(times[0]);
            const auto end = times[1].trimmed().split(' ', QString::SkipEmptyParts);
            if (end.isEmpty())
                continue;
            cue.to = cueTime(end.first());
            cue.text = QStringList(lines.mid(i + 1)).join(' ');
            cue.text.remove(QRegularExpression("<[^>]+>"));
            cues << cue;
            break;
        }
    }
}
void RedesignSeek::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const bool timeline = getSettings()->value("Redesign/subtitleTimeline", false).toBool();
    const int h = timeline ? height() - 12 : height() - 8;
    const QRectF groove(3, 4, width() - 6, h);
    p.setPen(Qt::NoPen);
    p.setBrush(palette().base());
    p.drawRoundedRect(groove, 5, 5);
    const double position = (value() - minimum()) / double(qMax(1, maximum() - minimum()));
    QColor fill = palette().highlight().color();
    fill.setAlpha(65);
    p.setBrush(fill);
    p.drawRoundedRect(QRectF(3, 4, (width() - 6) * position, h), 5, 5);
    if (getSettings()->value("Redesign/waveform", false).toBool() && !waveform.isEmpty())
    {
        p.setPen(QPen(palette().highlight().color(), 1));
        for (int x = 4; x < width() - 4; x += 2)
        {
            const int i = qBound(0, x * waveform.size() / qMax(1, width()), waveform.size() - 1);
            const double amp = waveform[i] * h * .4;
            p.drawLine(QPointF(x, 4 + h / 2 - amp), QPointF(x, 4 + h / 2 + amp));
        }
    }
    if (timeline && duration > 0)
    {
        auto input = THEMIM->getInput();
        const double delay = input ? var_GetInteger(input, "spu-delay") / double(CLOCK_FREQ) : 0;
        p.setBrush(palette().highlight());
        for (const auto &cue : cues)
            p.drawRoundedRect(QRectF((cue.from + delay) / duration * width(), height() - 7,
                                     qMax(2.0, (cue.to - cue.from) / duration * width()), 5),
                              2, 2);
    }
    p.setBrush(palette().highlight());
    p.drawRoundedRect(QRectF(3 + (width() - 6) * position - 2, 2, 5, h + 4), 2, 2);
    if (!isEnabled())
    {
        p.setBrush(QColor(0, 0, 0, 100));
        p.drawRoundedRect(groove, 5, 5);
    }
}
void RedesignSeek::mousePressEvent(QMouseEvent *e)
{
    if (!isEnabled())
        return;
    if (e->button() == Qt::LeftButton && getSettings()->value("Redesign/subtitleTimeline", false).toBool() &&
        e->pos().y() >= height() - 10)
    {
        draggingSubtitles = true;
        dragStart = e->pos();
        auto input = THEMIM->getInput();
        originalDelay = input ? var_GetInteger(input, "spu-delay") : 0;
        return;
    }
    if (e->button() == Qt::LeftButton)
        setValue(minimum() + qBound(0, e->pos().x(), width()) * (maximum() - minimum()) / qMax(1, width()));
    QSlider::mousePressEvent(e);
}
void RedesignSeek::mouseReleaseEvent(QMouseEvent *e)
{
    if (draggingSubtitles)
    {
        draggingSubtitles = false;
        return;
    }
    QSlider::mouseReleaseEvent(e);
}
void RedesignSeek::mouseMoveEvent(QMouseEvent *e)
{
    if (draggingSubtitles)
    {
        auto input = THEMIM->getInput();
        if (input)
            var_SetInteger(input, "spu-delay",
                           originalDelay + qlonglong((e->pos().x() - dragStart.x()) /
                                                     double(qMax(1, width())) * duration * CLOCK_FREQ));
        update();
        return;
    }
    QSlider::mouseMoveEvent(e);
    if (!isEnabled() || duration <= 0)
        return;
    hoverTime =
        qBound(0, int(e->pos().x() / double(qMax(1, width())) * duration), qMax(0, int(duration) - 1));
    stamp->setText(clock(hoverTime));
    preview->adjustSize();
    preview->move(mapToGlobal(
        QPoint(qBound(0, e->pos().x() - preview->width() / 2, qMax(0, width() - preview->width())),
               -preview->height() - 12)));
    preview->show();
    if (getSettings()->value("Redesign/seekThumbnails", true).toBool())
        debounce.start();
}
void RedesignSeek::leaveEvent(QEvent *e)
{
    preview->hide();
    debounce.stop();
    QSlider::leaveEvent(e);
}
void RedesignSeek::thumbnail()
{
    if (media.isEmpty() || !preview->isVisible())
        return;
    const QString token = QString::fromLatin1(
        QCryptographicHash::hash((media + QString::number(hoverTime)).toUtf8(), QCryptographicHash::Sha256)
            .toHex());
    const QString output = cache + "/" + token + ".png";
    if (QFileInfo(output).size() > 0)
    {
        image->setPixmap(
            QPixmap(output).scaled(image->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        return;
    }
    if (process)
        return;
    process = new QProcess(this);
    auto job = process;
    const QString requested = media;
    const int time = hoverTime;
    connect(job, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, job, output, requested, time](int, QProcess::ExitStatus)
            {
                process = nullptr;
                job->deleteLater();
                if (requested == media && time == hoverTime && QFileInfo(output).size() > 0)
                    image->setPixmap(
                        QPixmap(output).scaled(image->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
                else if (preview->isVisible())
                    debounce.start();
            });
    connect(job, &QProcess::errorOccurred, this,
            [this, job](QProcess::ProcessError)
            {
                if (process == job)
                {
                    process = nullptr;
                    job->deleteLater();
                }
            });
    QTimer::singleShot(10000, job,
                       [job]
                       {
                           if (job->state() != QProcess::NotRunning)
                               job->kill();
                       });
    job->start(QCoreApplication::applicationFilePath(),
               QStringList() << "--intf=dummy"
                             << "--ignore-config"
                             << "--no-one-instance"
                             << "--no-audio"
                             << "--no-spu"
                             << "--no-osd"
                             << "--no-video-title-show"
                             << "--no-metadata-network-access"
                             << "--vout=dummy"
                             << "--video-filter=scene"
                             << "--scene-format=png"
                             << "--scene-width=256"
                             << "--scene-ratio=1"
                             << "--scene-replace" << QString("--scene-path=" + cache)
                             << QString("--scene-prefix=" + token)
                             << QString("--start-time=" + QString::number(time))
                             << QString("--stop-time=" + QString::number(time + 1.2)) << "--play-and-exit"
                             << media);
}
void RedesignSeek::decodeWaveform()
{
    if (media.isEmpty())
        return;
    const QString output =
        cache + "/" +
        QString::fromLatin1(QCryptographicHash::hash(media.toUtf8(), QCryptographicHash::Sha256).toHex()) +
        ".raw";
    auto load = [this, output]
    {
        QFile file(output);
        if (!file.open(QIODevice::ReadOnly))
            return;
        const QByteArray data = file.readAll();
        const int samples = data.size() / 2;
        if (!samples)
            return;
        waveform.clear();
        for (int bin = 0; bin < 1024; ++bin)
        {
            float peak = 0;
            for (int i = int(qint64(bin) * samples / 1024); i < int(qint64(bin + 1) * samples / 1024); ++i)
                peak = qMax(peak, qAbs(int(qFromLittleEndian<qint16>(
                                      reinterpret_cast<const uchar *>(data.constData()) + i * 2))) /
                                      32768.f);
            waveform << peak;
        }
        update();
    };
    if (QFileInfo(output).size() > 0)
    {
        load();
        return;
    }
    auto job = new QProcess(this);
    const QString requested = media;
    connect(job, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, job, load, requested](int, QProcess::ExitStatus)
            {
                if (media == requested)
                    load();
                job->deleteLater();
            });
    const QString path = QString(output).replace("'", "\\'");
    job->start(
        QCoreApplication::applicationFilePath(),
        QStringList()
            << "--intf=dummy"
            << "--ignore-config"
            << "--no-one-instance"
            << "--no-video"
            << "--no-metadata-network-access"
            << QString(
                   "--sout=#transcode{acodec=s16l,channels=1,samplerate=8000}:std{access=file,mux=raw,dst='" +
                   path + "'}")
            << "--play-and-exit" << media);
    QTimer::singleShot(120000, job,
                       [job]
                       {
                           if (job->state() != QProcess::NotRunning)
                               job->kill();
                       });
}
