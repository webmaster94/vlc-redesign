/* Copyright 2026 webmaster94. GPL-2.0-or-later.
 * Interface adapted from Juxtopposed, CC BY 4.0; see doc/redesign/licensing.md. */
#ifndef VLC_REDESIGN_CONTROLS_HPP
#define VLC_REDESIGN_CONTROLS_HPP
#include <QAbstractButton>
#include <QDialog>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include <functional>
#include <QIcon>

namespace Reframe {
QIcon icon(const QString &,const QColor & = QColor("#8f8f8f"));
QEasingCurve transitionCurve();
QEasingCurve springCurve();
void reveal(QWidget *);
void animateHeight(QWidget *, bool, int duration = 300);
QWidget *fold(const QString &, QWidget *, QWidget *, bool expanded = false);
QWidget *segmented(const QStringList &, const QString &, QWidget *, std::function<void(QString)>);

class Toggle : public QAbstractButton {
    QVariantAnimation motion;
    qreal position;
protected:
    void paintEvent(QPaintEvent *) override;
public:
    explicit Toggle(QWidget *parent = nullptr);
    QSize sizeHint() const override { return QSize(68,32); }
};

class Dialog : public QDialog {
    QPoint dragOffset;
protected:
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void closeEvent(QCloseEvent *) override;
public:
    explicit Dialog(QWidget *parent, const QString &title, QSize size);
    QVBoxLayout *body;
    std::function<void()> beforeClose;
};
}
#endif
