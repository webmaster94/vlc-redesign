/* Copyright 2026 webmaster94. GPL-3.0-or-later. */
#ifndef VLC_REDESIGN_SEEK_HPP
#define VLC_REDESIGN_SEEK_HPP
#include "qt.hpp"
#include <QSlider>
#include <QTimer>
#include <QVector>
#include "redesign_seek_data.hpp"
class QProcess;
class QLabel;
class RedesignSeek : public QSlider
{
    intf_thread_t *p_intf;
    QTimer debounce;
    QWidget *preview;
    QLabel *image;
    QLabel *stamp;
    QProcess *process;
    QProcess *waveformProcess;
    QString waveformOutput;
    QString cache;
    QString media;
    QString subtitle;
    int hoverTime;
    double duration;
    bool draggingSubtitles;
    QPoint dragStart;
    qint64 originalDelay;
    QVector<ReframeSeekData::Cue> cues;
    QVector<float> waveform;
    void loadCues();
    void thumbnail();
    void decodeWaveform();
    void stopJobs();
    void trimCache();

  protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;

  public:
    RedesignSeek(intf_thread_t *, QWidget *);
    ~RedesignSeek();
    void refreshMedia();
    bool waveformEnabled() const;
    bool hasWaveform() const
    {
        return !waveform.isEmpty();
    }
};
#endif
