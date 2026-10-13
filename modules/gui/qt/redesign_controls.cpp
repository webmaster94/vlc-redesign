/* Copyright 2026 webmaster94. GPL-2.0-or-later.
 * Interface adapted from Juxtopposed, CC BY 4.0; see doc/redesign/licensing.md. */
#include "redesign_controls.hpp"
#include <QPainter>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QToolButton>
#include <QLabel>
#include <QButtonGroup>
#include <QTimer>
#include <cmath>

namespace Reframe {
// Figma INOUT_CUBIC is a Bezier, not Qt's polynomial InOutCubic.
QEasingCurve transitionCurve() {
    QEasingCurve curve(QEasingCurve::BezierSpline);
    curve.addCubicBezierSegment(QPointF(.42,0),QPointF(.58,1),QPointF(1,1));
    return curve;
}
static qreal spring(qreal t) {
    // Figma spring [mass=1, stiffness=2880, damping=120, velocity=0].
    if(t>=1)return 1;
    const qreal time=t*.208353817;
    const qreal a=-60+std::sqrt(720.0),b=-60-std::sqrt(720.0);
    return 1-(b*std::exp(a*time)-a*std::exp(b*time))/(b-a);
}
QEasingCurve springCurve() { QEasingCurve curve;curve.setCustomType(spring);return curve; }
void reveal(QWidget *widget) {
    widget->setWindowOpacity(0);widget->show();
    auto animation=new QPropertyAnimation(widget,"windowOpacity",widget);
    animation->setStartValue(0.0);animation->setEndValue(1.0);
    animation->setDuration(300);animation->setEasingCurve(transitionCurve());
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}
void animateHeight(QWidget *widget,bool open,int duration) {
    for(auto old:widget->findChildren<QPropertyAnimation *>("foldMotion")) { old->stop();delete old; }
    const int height=qMax(1,widget->layout()?widget->layout()->sizeHint().height():widget->sizeHint().height());
    const int start=widget->isVisible()?widget->height():0;
    widget->setMinimumHeight(0);widget->setMaximumHeight(start);widget->show();
    auto animation=new QPropertyAnimation(widget,"maximumHeight",widget);animation->setObjectName("foldMotion");
    animation->setStartValue(start);animation->setEndValue(open?height:0);animation->setDuration(duration);
    animation->setEasingCurve(duration==300?transitionCurve():springCurve());
    QObject::connect(animation,&QPropertyAnimation::finished,widget,[widget,open]{widget->setVisible(open);if(open)widget->setMaximumHeight(QWIDGETSIZE_MAX);});
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}
QWidget *fold(const QString &title,QWidget *content,QWidget *parent,bool expanded) {
    auto card=new QWidget(parent);card->setObjectName("settingsCard");
    auto layout=new QVBoxLayout(card);layout->setContentsMargins(16,6,16,16);layout->setSpacing(12);
    auto button=new QPushButton;button->setCheckable(true);button->setChecked(expanded);button->setObjectName("foldButton");
    button->setText((expanded?"Close ":"Open ")+title+QString::fromUtf8(expanded?"  \xE2\x8C\x84":"  \xE2\x8C\x83"));
    layout->addWidget(button);layout->addWidget(content);content->setVisible(expanded);
    QObject::connect(button,&QPushButton::toggled,card,[button,content,title](bool on){button->setText((on?"Close ":"Open ")+title+QString::fromUtf8(on?"  \xE2\x8C\x84":"  \xE2\x8C\x83"));animateHeight(content,on);});
    return card;
}
QWidget *segmented(const QStringList &labels,const QString &value,QWidget *parent,std::function<void(QString)> changed) {
    auto widget=new QWidget(parent);widget->setObjectName("segmented");auto row=new QHBoxLayout(widget);row->setContentsMargins(2,2,2,2);row->setSpacing(2);
    auto group=new QButtonGroup(widget);group->setExclusive(true);
    for(const auto &label:labels){auto b=new QPushButton(label);b->setCheckable(true);b->setChecked(label==value);group->addButton(b);row->addWidget(b);QObject::connect(b,&QPushButton::clicked,widget,[changed,label]{changed(label);});}
    return widget;
}
Toggle::Toggle(QWidget *parent):QAbstractButton(parent),motion(this),position(0) {
    setCheckable(true);setFixedSize(sizeHint());setCursor(Qt::PointingHandCursor);setFocusPolicy(Qt::StrongFocus);
    motion.setDuration(208);motion.setEasingCurve(springCurve());
    connect(&motion,&QVariantAnimation::valueChanged,this,[this](const QVariant &v){position=v.toReal();update();});
    connect(this,&QAbstractButton::toggled,this,[this](bool checked){motion.stop();motion.setStartValue(position);motion.setEndValue(checked?1.0:0.0);motion.start();});
}
void Toggle::paintEvent(QPaintEvent *) {
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);const auto accent=palette().highlight().color();
    p.setOpacity(isEnabled()?1:.4);p.setPen(Qt::NoPen);p.setBrush(palette().base());p.drawRoundedRect(rect().adjusted(0,0,-1,-1),5,5);
    const qreal amount=isVisible()?qBound(0.0,position,1.0):(isChecked()?1:0);
    p.setBrush(isChecked()?accent:QColor("#4b4b4b"));p.drawRoundedRect(QRectF(3+amount*29,3,27,26),3,3);
    p.setPen(isChecked()?accent:palette().text().color());p.setFont(QFont(font().family(),8));
    p.drawText(QRectF(isChecked()?3:33,0,28,32),Qt::AlignCenter,isChecked()?"ON":"OFF");
    if(hasFocus()){p.setPen(accent);p.setBrush(Qt::NoBrush);p.drawRoundedRect(rect().adjusted(1,1,-2,-2),5,5);}
}
Dialog::Dialog(QWidget *parent,const QString &title,QSize size):QDialog(parent) {
    setWindowFlags(Qt::Dialog|Qt::FramelessWindowHint);setAttribute(Qt::WA_DeleteOnClose);setWindowTitle(title);resize(size);setObjectName("designDialog");
    auto root=new QVBoxLayout(this);root->setContentsMargins(16,12,16,16);root->setSpacing(20);
    auto header=new QHBoxLayout;auto label=new QLabel(title);label->setObjectName("dialogHeading");header->addWidget(label,1);
    auto close=new QToolButton;close->setText(QString::fromUtf8("\xC3\x97"));close->setAccessibleName("Close");close->setFixedSize(32,32);header->addWidget(close);connect(close,&QToolButton::clicked,this,&QDialog::close);root->addLayout(header);
    body=new QVBoxLayout;body->setSpacing(18);root->addLayout(body,1);
}
void Dialog::mousePressEvent(QMouseEvent *e){if(e->button()==Qt::LeftButton&&e->pos().y()<54)dragOffset=e->globalPos()-frameGeometry().topLeft();else dragOffset=QPoint();QDialog::mousePressEvent(e);}
void Dialog::mouseMoveEvent(QMouseEvent *e){if((e->buttons()&Qt::LeftButton)&&!dragOffset.isNull())move(e->globalPos()-dragOffset);QDialog::mouseMoveEvent(e);}
void Dialog::closeEvent(QCloseEvent *e){if(beforeClose)beforeClose();QDialog::closeEvent(e);}
}
