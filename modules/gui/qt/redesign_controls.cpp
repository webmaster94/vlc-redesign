/* Copyright 2026 webmaster94. GPL-3.0-or-later.
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
#include <QApplication>
#include <QDesktopWidget>
#include <QSizeGrip>
#include <QAbstractItemView>
#include <QResizeEvent>
#include <QShowEvent>
#include <QStyledItemDelegate>
#include <cmath>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace Reframe
{
QString systemCurl()
{
#ifdef Q_OS_WIN
    wchar_t directory[MAX_PATH];
    const UINT length = GetSystemDirectoryW(directory, MAX_PATH);
    if (!length || length >= MAX_PATH)
        return QString();
    return QString::fromWCharArray(directory, int(length)) + "/curl.exe";
#else
    return QString();
#endif
}
class ChoiceDelegate : public QStyledItemDelegate
{
  public:
    explicit ChoiceDelegate(QObject *parent) : QStyledItemDelegate(parent) {}
    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        return QSize(240, 44);
    }
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        painter->save();
        if (option.state & QStyle::State_Selected)
            icon("check", option.palette.highlight().color())
                .paint(painter, QRect(option.rect.left() + 8, option.rect.top() + 10, 24, 24));
        painter->setPen(option.palette.text().color());
        painter->setFont(option.font);
        const QRect text = option.rect.adjusted(44, 0, -8, 0);
        painter->drawText(
            text, Qt::AlignVCenter | Qt::AlignLeft,
            option.fontMetrics.elidedText(index.data().toString(), Qt::ElideRight, text.width()));
        painter->restore();
    }
};
QAbstractItemDelegate *choiceDelegate(QObject *parent)
{
    return new ChoiceDelegate(parent);
}
QStringList defaultToolbar()
{
    return QStringList({"previous", "play", "next", "stop", "elapsed", "seek", "duration", "volume",
                        "volumeSlider", "audio", "subtitles", "settings", "playlist"});
}
// Figma INOUT_CUBIC is a Bezier, not Qt's polynomial InOutCubic.
QEasingCurve transitionCurve()
{
    QEasingCurve curve(QEasingCurve::BezierSpline);
    curve.addCubicBezierSegment(QPointF(.42, 0), QPointF(.58, 1), QPointF(1, 1));
    return curve;
}
static qreal spring(qreal t)
{
    // Figma spring [mass=1, stiffness=2880, damping=120, velocity=0].
    if (t >= 1)
        return 1;
    const qreal time = t * .208353817;
    const qreal a = -60 + std::sqrt(720.0), b = -60 - std::sqrt(720.0);
    return 1 - (b * std::exp(a * time) - a * std::exp(b * time)) / (b - a);
}
QEasingCurve springCurve()
{
    QEasingCurve curve;
    curve.setCustomType(spring);
    return curve;
}
void reveal(QWidget *widget)
{
    widget->show();
}
void centerOnParent(QDialog *dialog)
{
    QWidget *parent = dialog->parentWidget();
    const QRect available = QApplication::desktop()->availableGeometry(parent);
    dialog->resize(dialog->size().boundedTo(available.size() - QSize(32, 32)));
    const QPoint center = parent ? parent->frameGeometry().center() : available.center();
    const QPoint topLeft = center - QPoint(dialog->width() / 2, dialog->height() / 2);
    dialog->move(qBound(available.left(), topLeft.x(), available.right() - dialog->width() + 1),
                 qBound(available.top(), topLeft.y(), available.bottom() - dialog->height() + 1));
}
void animateHeight(QWidget *widget, bool open, int duration)
{
    for (auto old : widget->findChildren<QPropertyAnimation *>("foldMotion"))
    {
        old->stop();
        delete old;
    }
    const int height =
        widget->property("chromeHeight").toInt() > 0
            ? widget->property("chromeHeight").toInt()
            : qMax(1, widget->layout() ? widget->layout()->sizeHint().height() : widget->sizeHint().height());
    const int start = widget->isVisible() ? widget->height() : 0;
    widget->setMinimumHeight(0);
    widget->setMaximumHeight(start);
    widget->show();
    auto animation = new QPropertyAnimation(widget, "maximumHeight", widget);
    animation->setObjectName("foldMotion");
    animation->setStartValue(start);
    animation->setEndValue(open ? height : 0);
    animation->setDuration(duration);
    animation->setEasingCurve(duration == 300 ? transitionCurve() : springCurve());
    QObject::connect(animation, &QPropertyAnimation::finished, widget,
                     [widget, open]
                     {
                         widget->setVisible(open);
                         if (open)
                             widget->setMaximumHeight(QWIDGETSIZE_MAX);
                     });
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}
void animateWidth(QWidget *widget, int width, int duration)
{
    widget->setMinimumWidth(0);
    widget->setMaximumWidth(0);
    auto animation = new QPropertyAnimation(widget, "maximumWidth", widget);
    animation->setDuration(duration);
    animation->setStartValue(0);
    animation->setEndValue(width);
    animation->setEasingCurve(springCurve());
    QObject::connect(animation, &QPropertyAnimation::finished, widget,
                     [widget, width] { widget->setFixedWidth(width); });
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}
QWidget *fold(const QString &title, QWidget *content, QWidget *parent, bool expanded)
{
    auto card = new QWidget(parent);
    card->setObjectName("settingsCard");
    card->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    content->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    content->setObjectName("foldContent");
    auto layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 6, 16, 16);
    layout->setSpacing(12);
    auto button = new QPushButton;
    button->setCheckable(true);
    button->setChecked(expanded);
    button->setObjectName("foldButton");
    button->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    button->setText((expanded ? "Hide " : "Show ") + title);
    button->setIcon(icon(expanded ? "chevronup" : "chevrondown"));
    layout->addWidget(button);
    layout->addWidget(content);
    content->setVisible(expanded);
    QObject::connect(button, &QPushButton::toggled, card,
                     [button, content, title](bool on)
                     {
                         button->setText((on ? "Hide " : "Show ") + title);
                         button->setIcon(icon(on ? "chevronup" : "chevrondown"));
                         animateHeight(content, on, 300);
                     });
    return card;
}
QWidget *segmented(const QStringList &labels, const QString &value, QWidget *parent,
                   std::function<void(QString)> changed)
{
    auto widget = new QWidget(parent);
    widget->setObjectName("segmented");
    auto row = new QHBoxLayout(widget);
    row->setContentsMargins(2, 2, 2, 2);
    row->setSpacing(2);
    auto group = new QButtonGroup(widget);
    group->setExclusive(true);
    for (const auto &label : labels)
    {
        auto b = new QPushButton(label);
        b->setCheckable(true);
        b->setChecked(label == value);
        group->addButton(b);
        row->addWidget(b);
        QObject::connect(b, &QPushButton::clicked, widget, [changed, label] { changed(label); });
    }
    return widget;
}
Toggle::Toggle(QWidget *parent) : QAbstractButton(parent), motion(this), position(0)
{
    setCheckable(true);
    setFixedSize(sizeHint());
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    motion.setDuration(208);
    motion.setEasingCurve(springCurve());
    connect(&motion, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &v)
            {
                position = v.toReal();
                update();
            });
    connect(this, &QAbstractButton::toggled, this,
            [this](bool checked)
            {
                motion.stop();
                if (!isVisible())
                {
                    position = checked ? 1.0 : 0.0;
                    update();
                    return;
                }
                motion.setStartValue(position);
                motion.setEndValue(checked ? 1.0 : 0.0);
                motion.start();
            });
}
void Toggle::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const auto accent = palette().highlight().color();
    p.setOpacity(isEnabled() ? 1 : .4);
    p.setPen(Qt::NoPen);
    const bool dark = palette().window().color().lightness() < 128;
    p.setBrush(dark ? QColor("#080808") : QColor("#dddddd"));
    p.setPen(QPen(dark ? QColor("#303030") : QColor("#b8b8b8"), 1));
    p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 4, 4);
    const qreal amount = isVisible() ? qBound(0.0, position, 1.0) : (isChecked() ? 1 : 0);
    p.setPen(Qt::NoPen);
    QColor labelFill = isChecked() ? accent : palette().button().color();
    labelFill.setAlpha(isChecked() ? 30 : 120);
    p.setBrush(labelFill);
    p.drawRoundedRect(QRectF(isChecked() ? 2 : 33, 2, 29, 24), 2, 2);
    p.setBrush(isChecked() ? accent : dark ? QColor("#454545") : QColor("#aaaaaa"));
    p.drawRoundedRect(QRectF(2 + amount * 31, 2, 29, 24), 2, 2);
    p.setPen(isChecked() ? accent : QColor("#929292"));
    QFont labelFont = font();
    labelFont.setPixelSize(11);
    p.setFont(labelFont);
    p.drawText(QRectF(isChecked() ? 2 : 33, 0, 29, 28), Qt::AlignCenter, isChecked() ? "ON" : "OFF");
    if (hasFocus())
    {
        p.setPen(accent);
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(rect().adjusted(1, 1, -2, -2), 5, 5);
    }
}
Dialog::Dialog(QWidget *parent, const QString &title, QSize size) : QDialog(parent), grip(nullptr)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowTitle(title);
    resize(size);
    setObjectName("designDialog");
    setMinimumSize(qMin(size.width(), 360), qMin(size.height(), 240));
    grip = new QSizeGrip(this);
    grip->setObjectName("dialogResizeGrip");
    grip->setFixedSize(16, 16);
    auto root = new QVBoxLayout(this);
    root->setContentsMargins(16, 12, 16, 16);
    root->setSpacing(20);
    auto header = new QHBoxLayout;
    auto label = new QLabel(title);
    label->setObjectName("dialogHeading");
    header->addWidget(label, 1);
    auto close = new QToolButton;
    close->setText(QString::fromUtf8("\xC3\x97"));
    close->setAccessibleName("Close");
    close->setFixedSize(32, 32);
    header->addWidget(close);
    connect(close, &QToolButton::clicked, this, &QDialog::close);
    root->addLayout(header);
    body = new QVBoxLayout;
    body->setSpacing(18);
    root->addLayout(body, 1);
}
void Dialog::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && e->pos().y() < 54)
        dragOffset = e->globalPos() - frameGeometry().topLeft();
    else
        dragOffset = QPoint();
    QDialog::mousePressEvent(e);
}
void Dialog::mouseMoveEvent(QMouseEvent *e)
{
    if ((e->buttons() & Qt::LeftButton) && !dragOffset.isNull())
        move(e->globalPos() - dragOffset);
    QDialog::mouseMoveEvent(e);
}
void Dialog::closeEvent(QCloseEvent *e)
{
    if (beforeClose)
        beforeClose();
    QDialog::closeEvent(e);
}
void Dialog::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    if (isWindow())
    {
        for (int inset = 0; inset < 6; ++inset)
        {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 0, 0, 8 + inset * 5));
            p.drawRoundedRect(rect().adjusted(inset, inset + 2, -inset - 1, -inset - 1), 14, 14);
        }
    }
    const QRect frame = isWindow() ? rect().adjusted(6, 6, -7, -7) : rect().adjusted(0, 0, -1, -1);
    p.setPen(QPen(palette().window().color().lightness() < 128 ? QColor("#454545") : QColor("#b8b8b8"), 1));
    p.setBrush(palette().window());
    p.drawRoundedRect(frame, isWindow() ? 12 : 6, isWindow() ? 12 : 6);
}
void Dialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    if (grip)
    {
        grip->move(width() - 26, height() - 26);
        grip->raise();
    }
}
void Dialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    grip->setVisible(isWindow());
    grip->raise();
}
static void anchorDropdown(QComboBox *combo)
{
    QWidget *popup = combo->view()->window();
    if (!popup->isVisible() || popup == combo->window())
        return;
    popup->setMinimumWidth(combo->width());
    const QRect available = QApplication::desktop()->availableGeometry(combo);
    const QPoint below = combo->mapToGlobal(QPoint(0, combo->height() + 2));
    QPoint position = below;
    if (position.y() + popup->height() > available.bottom() + 1)
        position.setY(combo->mapToGlobal(QPoint()).y() - popup->height() - 2);
    position.setX(qBound(available.left(), position.x(), available.right() - popup->width() + 1));
    position.setY(qMax(available.top(), position.y()));
    popup->move(position);
}
void Dropdown::showPopup()
{
    QComboBox::showPopup();
    anchorDropdown(this);
}
void FontDropdown::showPopup()
{
    QFontComboBox::showPopup();
    anchorDropdown(this);
}
ToolButton::ToolButton(QWidget *parent) : QToolButton(parent), hover(this), amount(0)
{
    hover.setDuration(50);
    hover.setEasingCurve(transitionCurve());
    connect(&hover, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value)
            {
                amount = value.toReal();
                update();
            });
}
void ToolButton::enterEvent(QEvent *event)
{
    hover.stop();
    hover.setStartValue(amount);
    hover.setEndValue(1.0);
    hover.start();
    QToolButton::enterEvent(event);
}
void ToolButton::leaveEvent(QEvent *event)
{
    hover.stop();
    hover.setStartValue(amount);
    hover.setEndValue(0.0);
    hover.start();
    QToolButton::leaveEvent(event);
}
void ToolButton::paintEvent(QPaintEvent *event)
{
    if (amount > 0)
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        QColor color = palette().button().color().lighter(145);
        color.setAlpha(qRound(amount * 255));
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawRoundedRect(rect(), 5, 5);
    }
    QToolButton::paintEvent(event);
}
} // namespace Reframe
