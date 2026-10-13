/* Copyright 2026 webmaster94. GPL-2.0-or-later. */
#ifndef VLC_REDESIGN_SEEK_HPP
#define VLC_REDESIGN_SEEK_HPP
#include "qt.hpp"
#include <QSlider>
#include <QTimer>
#include <QVector>
class QProcess;
class QLabel;
class RedesignSeek : public QSlider {
    intf_thread_t *p_intf;
    QTimer debounce;
    QWidget *preview;
    QLabel *image;
    QLabel *stamp;
    QProcess *process;
    QString cache;
    QString media;
    QString subtitle;
    int hoverTime;
    double duration;
    bool draggingSubtitles;
    QPoint dragStart;
    qint64 originalDelay;
    struct Cue {double from,to;QString text;};
    QVector<Cue> cues;
    QVector<float> waveform;
    void loadCues();
    void thumbnail();
    void decodeWaveform();
protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;
public:
    RedesignSeek(intf_thread_t *,QWidget *);
    ~RedesignSeek();
    void refreshMedia();
};
#endif
