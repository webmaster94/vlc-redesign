/* Reframe interface, adapted from Juxtopposed's VLC Redesign, CC BY 4.0.
 * Implementation copyright 2026 webmaster94. GPL-3.0-or-later.
 * Changes: native player/library/settings implementation, 2026-10-12.
 * Design: https://www.figma.com/community/file/1691465999134370980/vlc-redesign */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "redesign.hpp"
#include "main_interface.hpp"
#include "input_manager.hpp"
#include "actions_manager.hpp"
#include "dialogs_provider.hpp"
#include "recents.hpp"
#include "redesign_settings.hpp"
#include "redesign_controls.hpp"
#include "redesign_seek.hpp"
#include <vlc_configuration.h>
#include <vlc_aout.h>
#include <QApplication>
#include <QBoxLayout>
#include <QFormLayout>
#include <QToolButton>
#include <QPushButton>
#include <QButtonGroup>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QStackedWidget>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QScrollArea>
#include <QMenuBar>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QStyledItemDelegate>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QProcess>
#include <QTimer>
#include <QDateTime>
#include <QSettings>
#include <QUrl>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QDesktopServices>
#include <QPropertyAnimation>
#include <QFontDatabase>
#include <QSizeGrip>
#include <QFile>
#include <QShortcut>
#include <cmath>
#include <algorithm>
#include <memory>
#include <QScrollBar>

namespace
{
int fullscreenChanged(vlc_object_t *, const char *, vlc_value_t, vlc_value_t value, void *opaque)
{
    QApplication::postEvent(static_cast<RedesignShell *>(opaque), new RedesignFullscreenEvent(value.b_bool));
    return VLC_SUCCESS;
}
QIcon lineIcon(const QString &name, const QColor &color = QColor("#b8b8b8"))
{
    QPixmap pm(48, 48);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(2, 2);
    p.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    auto line = [&p](int x, int y, int a, int b) { p.drawLine(x, y, a, b); };
    if (name == "home")
    {
        QPolygonF roof;
        roof << QPointF(3, 10) << QPointF(12, 3) << QPointF(21, 10);
        p.drawPolyline(roof);
        p.drawRoundedRect(QRectF(5, 9, 14, 12), 2, 2);
        p.drawRect(10, 14, 4, 7);
    }
    else if (name == "videos" || name == "folder")
    {
        QPainterPath path;
        path.moveTo(3, 7);
        path.lineTo(10, 7);
        path.lineTo(12, 9);
        path.lineTo(21, 9);
        path.lineTo(21, 20);
        path.lineTo(3, 20);
        path.closeSubpath();
        p.drawPath(path);
        if (name == "videos")
        {
            p.setBrush(color);
            QPolygonF tri;
            tri << QPointF(10, 11) << QPointF(15, 14) << QPointF(10, 17);
            p.drawPolygon(tri);
        }
    }
    else if (name == "play" || name == "brand")
    {
        if (name == "brand")
        {
            p.setPen(Qt::NoPen);
            p.setBrush(color);
            p.drawRoundedRect(QRectF(2, 2, 20, 20), 5, 5);
            p.setBrush(QColor("#111111"));
        }
        else
            p.setBrush(color);
        QPolygonF tri;
        tri << QPointF(9, 6) << QPointF(18, 12) << QPointF(9, 18);
        p.drawPolygon(tri);
    }
    else if (name == "pause")
    {
        p.setBrush(color);
        p.drawRoundedRect(QRectF(7, 6, 3, 12), 1, 1);
        p.drawRoundedRect(QRectF(14, 6, 3, 12), 1, 1);
    }
    else if (name == "playpause")
    {
        p.setBrush(color);
        p.drawPolygon(QPolygonF() << QPointF(3, 6) << QPointF(11, 12) << QPointF(3, 18));
        p.drawRoundedRect(QRectF(14, 6, 2, 12), 1, 1);
        p.drawRoundedRect(QRectF(19, 6, 2, 12), 1, 1);
    }
    else if (name == "seek")
    {
        p.drawPolyline(QPolygonF() << QPointF(10, 6) << QPointF(3, 12) << QPointF(10, 18));
        p.drawPolyline(QPolygonF() << QPointF(14, 6) << QPointF(21, 12) << QPointF(14, 18));
    }
    else if (name == "chevrondown" || name == "chevronup")
    {
        const bool down = name == "chevrondown";
        p.drawPolyline(QPolygonF() << QPointF(6, down ? 9 : 15) << QPointF(12, down ? 15 : 9)
                                   << QPointF(18, down ? 9 : 15));
    }
    else if (name == "check")
    {
        p.drawPolyline(QPolygonF() << QPointF(4, 12) << QPointF(10, 18) << QPointF(21, 5));
    }
    else if (name == "frame")
    {
        p.drawRoundedRect(QRectF(2, 4, 20, 16), 2, 2);
        p.setBrush(color);
        p.drawPolygon(QPolygonF() << QPointF(7, 8) << QPointF(13, 12) << QPointF(7, 16));
        line(16, 8, 16, 16);
    }
    else if (name == "music")
    {
        line(10, 5, 10, 17);
        line(10, 5, 19, 3);
        line(19, 3, 19, 15);
        p.drawEllipse(5, 15, 5, 5);
        p.drawEllipse(14, 13, 5, 5);
    }
    else if (name == "network")
    {
        p.drawEllipse(10, 10, 4, 4);
        p.drawArc(5, 5, 14, 14, -55 * 16, 110 * 16);
        p.drawArc(5, 5, 14, 14, 125 * 16, 110 * 16);
        p.drawArc(1, 1, 22, 22, -50 * 16, 100 * 16);
        p.drawArc(1, 1, 22, 22, 130 * 16, 100 * 16);
    }
    else if (name == "playlist")
    {
        line(3, 6, 20, 6);
        line(3, 11, 13, 11);
        line(3, 16, 11, 16);
        QPolygonF tri;
        tri << QPointF(16, 11) << QPointF(21, 15) << QPointF(16, 19);
        p.drawPolygon(tri);
    }
    else if (name == "recent")
    {
        p.drawArc(4, 4, 16, 16, -80 * 16, 290 * 16);
        line(12, 7, 12, 12);
        line(12, 12, 16, 14);
        line(3, 4, 3, 9);
        line(3, 9, 8, 9);
    }
    else if (name == "add")
    {
        line(12, 4, 12, 20);
        line(4, 12, 20, 12);
    }
    else if (name == "back" || name == "forward")
    {
        int d = name == "back" ? 1 : -1;
        line(12 - 6 * d, 12, 12 + 6 * d, 12);
        line(12 - 6 * d, 12, 12, 6);
        line(12 - 6 * d, 12, 12, 18);
    }
    else if (name == "previous" || name == "next")
    {
        bool prev = name == "previous";
        int x = prev ? 16 : 8;
        QPolygonF tri;
        tri << QPointF(x, 6) << QPointF(prev ? 7 : 17, 12) << QPointF(x, 18);
        p.setBrush(color);
        p.drawPolygon(tri);
        line(prev ? 5 : 19, 6, prev ? 5 : 19, 18);
    }
    else if (name == "stop")
    {
        p.drawRoundedRect(QRectF(6, 6, 12, 12), 1, 1);
    }
    else if (name == "audio")
    {
        for (int i = 0; i < 5; i++)
        {
            int h = i == 2 ? 18 : (i % 2 ? 12 : 6);
            line(4 + i * 4, 12 - h / 2, 4 + i * 4, 12 + h / 2);
        }
    }
    else if (name == "sliders")
    {
        for (int x = 5; x <= 19; x += 7)
        {
            line(x, 3, x, 21);
            p.drawRoundedRect(QRectF(x - 3, x == 12 ? 7 : 13, 6, 3), 1, 1);
        }
    }
    else if (name == "pointer")
    {
        QPolygonF shape;
        shape << QPointF(5, 3) << QPointF(19, 15) << QPointF(13, 16) << QPointF(10, 21) << QPointF(5, 3);
        p.drawPolygon(shape);
    }
    else if (name == "shield")
    {
        QPainterPath path;
        path.moveTo(12, 3);
        path.cubicTo(7, 4, 4, 6, 4, 8);
        path.cubicTo(4, 16, 8, 20, 12, 22);
        path.cubicTo(16, 20, 20, 16, 20, 8);
        path.cubicTo(20, 6, 17, 4, 12, 3);
        p.drawPath(path);
    }
    else if (name == "keyboard")
    {
        p.drawRoundedRect(QRectF(2, 5, 20, 14), 3, 3);
        for (int x = 6; x < 20; x += 4)
            for (int y = 9; y < 14; y += 4)
                p.drawPoint(x, y);
        line(6, 16, 18, 16);
    }
    else if (name == "record")
    {
        p.drawEllipse(4, 4, 16, 16);
        p.setBrush(color);
        p.drawEllipse(9, 9, 6, 6);
    }
    else if (name == "puzzle")
    {
        QPainterPath path;
        path.moveTo(4, 4);
        path.lineTo(10, 4);
        path.cubicTo(8, -1, 16, -1, 14, 4);
        path.lineTo(20, 4);
        path.lineTo(20, 10);
        path.cubicTo(25, 8, 25, 16, 20, 14);
        path.lineTo(20, 20);
        path.lineTo(14, 20);
        path.cubicTo(16, 15, 8, 15, 10, 20);
        path.lineTo(4, 20);
        path.lineTo(4, 4);
        p.drawPath(path);
    }
    else if (name == "favorite")
    {
        p.drawPolyline(QPolygonF() << QPointF(12, 2) << QPointF(15, 9) << QPointF(22, 10) << QPointF(17, 15)
                                   << QPointF(18, 22) << QPointF(12, 18) << QPointF(6, 22) << QPointF(7, 15)
                                   << QPointF(2, 10) << QPointF(9, 9) << QPointF(12, 2));
    }
    else if (name == "bookmark" || name == "bookmarks")
    {
        QPolygonF shape;
        shape << QPointF(6, 3) << QPointF(18, 3) << QPointF(18, 21) << QPointF(12, 17) << QPointF(6, 21)
              << QPointF(6, 3);
        p.drawPolygon(shape);
    }
    else if (name == "snapshot")
    {
        p.drawRoundedRect(QRectF(3, 7, 18, 13), 3, 3);
        p.drawRect(7, 4, 10, 3);
        p.drawEllipse(8, 10, 8, 8);
    }
    else if (name == "mini")
    {
        p.drawRoundedRect(QRectF(2, 4, 20, 16), 2, 2);
        p.drawRect(11, 12, 8, 5);
    }
    else if (name == "repeat" || name == "replay")
    {
        p.drawArc(4, 4, 16, 16, 20 * 16, 300 * 16);
        line(20, 4, 20, 10);
        line(20, 10, 14, 10);
    }
    else if (name == "shuffle")
    {
        line(3, 5, 9, 5);
        line(9, 5, 15, 19);
        line(15, 19, 21, 19);
        line(3, 19, 9, 19);
        line(9, 19, 15, 5);
        line(15, 5, 21, 5);
        line(18, 2, 21, 5);
        line(21, 5, 18, 8);
    }
    else if (name.startsWith("back") || name.startsWith("forward"))
    {
        p.drawArc(4, 4, 16, 16, 30 * 16, 300 * 16);
        p.setFont(QFont("Inter", 6));
        p.drawText(QRect(4, 4, 16, 16), Qt::AlignCenter, name.endsWith("15") ? "15" : "10");
    }
    else if (name == "speed" || name == "hdr" || name == "zoom")
    {
        p.setFont(QFont("Inter", name == "hdr" ? 7 : 9));
        p.drawText(QRect(0, 2, 24, 20), Qt::AlignCenter,
                   name == "speed" ? "1x"
                   : name == "hdr" ? "HDR"
                                   : "1:1");
    }
    else if (name == "volume" || name == "mute")
    {
        QPolygonF sp;
        sp << QPointF(3, 9) << QPointF(7, 9) << QPointF(12, 5) << QPointF(12, 19) << QPointF(7, 15)
           << QPointF(3, 15);
        p.drawPolygon(sp);
        if (name == "volume")
        {
            p.drawArc(10, 5, 10, 14, -60 * 16, 120 * 16);
        }
        else
        {
            line(17, 8, 22, 16);
            line(22, 8, 17, 16);
        }
    }
    else if (name == "settings")
    {
        p.drawEllipse(8, 8, 8, 8);
        for (int i = 0; i < 8; i++)
        {
            p.save();
            p.translate(12, 12);
            p.rotate(i * 45);
            p.drawRoundedRect(QRectF(-2, -10, 4, 4), 1, 1);
            p.restore();
        }
    }
    else if (name == "subtitles")
    {
        p.drawRoundedRect(QRectF(2, 5, 20, 14), 3, 3);
        line(5, 11, 10, 11);
        line(13, 11, 19, 11);
        line(5, 15, 13, 15);
        line(16, 15, 19, 15);
    }
    else if (name == "search")
    {
        p.drawEllipse(4, 3, 12, 12);
        line(14, 14, 21, 21);
    }
    else if (name == "grid")
    {
        for (int x = 4; x < 20; x += 10)
            for (int y = 4; y < 20; y += 10)
                p.drawRoundedRect(QRectF(x, y, 6, 6), 1, 1);
    }
    else if (name == "list")
    {
        for (int y = 6; y < 21; y += 6)
        {
            line(3, y, 5, y);
            line(9, y, 21, y);
        }
    }
    else if (name == "close")
    {
        line(6, 6, 18, 18);
        line(6, 18, 18, 6);
    }
    else if (name == "minimize")
    {
        line(5, 12, 19, 12);
    }
    else if (name == "maximize")
    {
        p.drawRect(5, 5, 14, 14);
    }
    else if (name == "fullscreen")
    {
        line(4, 9, 4, 4);
        line(4, 4, 9, 4);
        line(15, 4, 20, 4);
        line(20, 4, 20, 9);
        line(4, 15, 4, 20);
        line(4, 20, 9, 20);
        line(15, 20, 20, 20);
        line(20, 20, 20, 15);
    }
    else if (name == "more")
    {
        p.setBrush(color);
        for (int x = 5; x < 22; x += 7)
            p.drawEllipse(QPointF(x, 12), 1, 1);
    }
    return QIcon(pm);
}

class TitleBar : public QWidget
{
    QPoint offset;

  public:
    explicit TitleBar(QWidget *p) : QWidget(p) {}
    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton)
            offset = e->globalPos() - window()->frameGeometry().topLeft();
    }
    void mouseMoveEvent(QMouseEvent *e) override
    {
        if ((e->buttons() & Qt::LeftButton) && !window()->isMaximized() && !window()->isFullScreen())
            window()->move(e->globalPos() - offset);
    }
    void mouseDoubleClickEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton)
        {
            if (window()->isMaximized())
                window()->showNormal();
            else
                window()->showMaximized();
        }
    }
};

class ClickSlider : public QSlider
{
  public:
    explicit ClickSlider(QWidget *p) : QSlider(Qt::Horizontal, p) {}
    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton && isEnabled())
        {
            setValue(minimum() +
                     qBound(0, e->pos().x(), width()) * (maximum() - minimum()) / qMax(1, width()));
        }
        QSlider::mousePressEvent(e);
    }
};

class VolumeSlider : public ClickSlider
{
  public:
    explicit VolumeSlider(QWidget *parent) : ClickSlider(parent)
    {
        setFixedSize(112, 30);
        setCursor(Qt::PointingHandCursor);
    }
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF area = QRectF(rect()).adjusted(4, 6, -4, -6);
        QPainterPath wedge;
        wedge.moveTo(area.left() + 2, area.center().y() - 3);
        wedge.lineTo(area.right() - 3, area.top());
        wedge.quadTo(area.topRight(), QPointF(area.right(), area.top() + 3));
        wedge.lineTo(area.right(), area.bottom() - 3);
        wedge.quadTo(area.bottomRight(), QPointF(area.right() - 3, area.bottom()));
        wedge.lineTo(area.left() + 2, area.center().y() + 3);
        wedge.quadTo(QPointF(area.left(), area.center().y() + 3),
                     QPointF(area.left(), area.center().y() + 1));
        wedge.lineTo(area.left(), area.center().y() - 1);
        wedge.quadTo(QPointF(area.left(), area.center().y() - 3),
                     QPointF(area.left() + 2, area.center().y() - 3));
        wedge.closeSubpath();
        painter.fillPath(wedge, palette().button());
        const qreal ratio = (value() - minimum()) / double(qMax(1, maximum() - minimum()));
        painter.setClipRect(QRectF(area.left(), area.top(), area.width() * ratio, area.height()));
        QLinearGradient gradient(area.topLeft(), area.topRight());
        gradient.setColorAt(0, QColor("#6eaa50"));
        gradient.setColorAt(.5, QColor("#c4a345"));
        gradient.setColorAt(1, QColor("#bf5443"));
        painter.fillPath(wedge, gradient);
        painter.setClipping(false);
        painter.setPen(QPen(palette().text().color(), 1));
        painter.drawPath(wedge);
        if (underMouse() || hasFocus())
        {
            const qreal x = area.left() + area.width() * ratio;
            const qreal height = 3 + 6 * ratio;
            painter.setPen(QPen(palette().windowText().color().lighter(160), 2));
            painter.drawLine(QPointF(x, area.center().y() - height), QPointF(x, area.center().y() + height));
        }
    }
    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (event->buttons() & Qt::LeftButton)
            setValue(minimum() + qRound(qBound(0.0, (event->pos().x() - 4) / double(width() - 8), 1.0) *
                                        (maximum() - minimum())));
        else
            ClickSlider::mouseMoveEvent(event);
    }
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton)
        {
            setFocus();
            setValue(minimum() + qRound(qBound(0.0, (event->pos().x() - 4) / double(width() - 8), 1.0) *
                                        (maximum() - minimum())));
            event->accept();
        }
        else
            ClickSlider::mousePressEvent(event);
    }
};

class MediaDelegate : public QStyledItemDelegate
{
  public:
    explicit MediaDelegate(QObject *p) : QStyledItemDelegate(p) {}
    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        const auto view = qobject_cast<QListWidget *>(parent());
        return QSize(320, view && view->viewMode() == QListView::ListMode ? 56 : 232);
    }
    void paint(QPainter *p, const QStyleOptionViewItem &o, const QModelIndex &i) const override
    {
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        if (o.rect.height() < 100)
        {
            if (o.state & QStyle::State_Selected)
            {
                QColor tint = o.palette.highlight().color();
                tint.setAlpha(40);
                p->fillRect(o.rect, tint);
            }
            QIcon icon = qvariant_cast<QIcon>(i.data(Qt::DecorationRole));
            icon.paint(p, QRect(o.rect.left() + 8, o.rect.top() + 4, 70, 40));
            p->setPen(o.palette.text().color());
            p->drawText(o.rect.adjusted(92, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, i.data().toString());
            p->restore();
            return;
        }
        QRect r = o.rect.adjusted(8, 6, -8, -6), image(r.left(), r.top(), r.width(), r.width() * 9 / 16);
        p->setPen(Qt::NoPen);
        p->setBrush(o.palette.alternateBase());
        p->drawRoundedRect(image, 8, 8);
        QIcon icon = qvariant_cast<QIcon>(i.data(Qt::DecorationRole));
        if (i.data(Qt::UserRole + 1).toBool())
        {
            QPixmap pm = icon.pixmap(320, 180);
            QPainterPath clip;
            clip.addRoundedRect(image, 8, 8);
            p->setClipPath(clip);
            p->drawPixmap(image, pm);
            p->setClipping(false);
        }
        else
        {
            icon.paint(p, image.adjusted(65, 30, -65, -30));
        }
        if (o.state & QStyle::State_Selected)
        {
            QColor tint = o.palette.highlight().color();
            tint.setAlpha(45);
            p->setBrush(tint);
            tint.setAlpha(150);
            p->setPen(QPen(tint, 1));
            p->drawRoundedRect(image, 8, 8);
            p->setPen(Qt::NoPen);
        }
        const qreal progress = i.data(Qt::UserRole + 2).toReal();
        if (progress > 0)
        {
            p->setBrush(o.palette.highlight());
            p->drawRect(QRect(image.left(), image.bottom() - 3,
                              qRound(image.width() * qBound<qreal>(0, progress, 1)), 4));
        }
        const qint64 seconds = i.data(Qt::UserRole + 4).toLongLong();
        if (seconds > 0)
        {
            const QString duration = QString("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QChar('0'));
            const int width = o.fontMetrics.width(duration) + 12;
            const QRect badge(image.right() - width - 6, image.bottom() - 26, width, 20);
            p->setBrush(QColor(0, 0, 0, 190));
            p->drawRoundedRect(badge, 4, 4);
            p->setPen(Qt::white);
            p->drawText(badge, Qt::AlignCenter, duration);
        }
        p->setPen(o.palette.text().color());
        p->setFont(o.font);
        p->drawText(QRect(r.left(), image.bottom() + 10, r.width(), 24), Qt::AlignTop | Qt::AlignLeft,
                    o.fontMetrics.elidedText(i.data().toString(), Qt::ElideMiddle, r.width()));
        const QString artist = i.data(Qt::UserRole + 3).toString();
        if (!artist.isEmpty())
        {
            p->setPen(o.palette.text().color().darker(130));
            p->drawText(QRect(r.left(), image.bottom() + 35, r.width(), 20), Qt::AlignTop | Qt::AlignLeft,
                        o.fontMetrics.elidedText(artist, Qt::ElideRight, r.width()));
        }
        p->restore();
    }
};

QString clockText(qint64 seconds)
{
    seconds = qMax<qint64>(0, seconds);
    return QString("%1:%2:%3")
        .arg(seconds / 3600, 2, 10, QChar('0'))
        .arg(seconds / 60 % 60, 2, 10, QChar('0'))
        .arg(seconds % 60, 2, 10, QChar('0'));
}
} // namespace
QAbstractItemDelegate *Reframe::mediaDelegate(QObject *parent)
{
    return new MediaDelegate(parent);
}
QIcon Reframe::icon(const QString &name, const QColor &color)
{
    return lineIcon(name, color);
}

QToolButton *RedesignShell::button(const QString &name, const QString &text)
{
    QToolButton *b = new Reframe::ToolButton;
    b->setProperty("glyph", name);
    b->setIcon(lineIcon(name));
    b->setIconSize(QSize(22, 22));
    b->setText(text);
    b->setToolTip(text.isEmpty() ? name : text);
    b->setAccessibleName(b->toolTip());
    b->setCursor(Qt::PointingHandCursor);
    b->setAutoRaise(true);
    b->setMinimumSize(34, 34);
    if (!text.isEmpty())
        b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    return b;
}

RedesignShell::RedesignShell(intf_thread_t *intf, MainInterface *w, QWidget *panel)
    : QWidget(w), p_intf(intf), window(w), player(panel), settingsDialog(nullptr), historyPosition(-1),
      systemPalette(QApplication::palette()), thumbnailProcess(nullptr), compact(false), minimal(false)
{
    navigationAnimation = nullptr;
    sidePanel = nullptr;
    libraryDock = nullptr;
    miniPlayer = nullptr;
    remainingTime = false;
    customizing = false;
    sortMode = 0;
    homePanel = nullptr;
    customLeft = nullptr;
    customTop = nullptr;
    extraToolbar = nullptr;
    breadcrumbs = nullptr;
    musicMode = "Songs";
    lastResumeSave = -1;
    const QDir fonts(QCoreApplication::applicationDirPath() + "/fonts");
    for (const QString &f : fonts.entryList(QStringList() << "*.ttf", QDir::Files))
        QFontDatabase::addApplicationFont(fonts.filePath(f));
    setObjectName("redesignShell");
    char *cache = config_GetUserDir(VLC_CACHE_DIR);
    cacheDirectory = QString::fromUtf8(cache ? cache : "");
    free(cache);
    cacheDirectory += "/thumbnails";
    QDir().mkpath(cacheDirectory);
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(1, 1, 1, 1);
    root->setSpacing(0);
    header = new TitleBar(this);
    header->setObjectName("titleBar");
    header->setFixedHeight(40);
    header->setProperty("chromeHeight", 40);
    QHBoxLayout *title = new QHBoxLayout(header);
    title->setContentsMargins(8, 0, 6, 0);
    title->setSpacing(4);
    auto logo = button("brand");
    logo->setToolTip("Reframe, an unofficial VLC fork");
    title->addWidget(logo);
    connect(logo, &QToolButton::clicked, this, [this] { showLibrary("Home"); });
    auto menuButton = button("more");
    menuButton->setToolTip("Main menu");
    title->addWidget(menuButton);
    connect(menuButton, &QToolButton::clicked, this,
            [this, menuButton] { mainMenu(menuButton->mapToGlobal(QPoint(0, menuButton->height()))); });
    auto navToggle = button("list");
    navToggle->setToolTip("Toggle navigation sidebar");
    title->addWidget(navToggle);
    connect(navToggle, &QToolButton::clicked, this,
            [this]
            {
                leaveFullscreen();
                sidebar->show();
                setNavigationWidth(!compact);
            });
    title->addStretch();
    auto titleLabel = new QLabel("Reframe");
    titleLabel->setObjectName("windowTitle");
    title->addWidget(titleLabel);
    title->addStretch();
    // operator<< returns a reference. Own the list before range iteration.
    const QStringList windowActions = QStringList() << "minimize"
                                                    << "maximize"
                                                    << "close";
    for (const QString &name : windowActions)
    {
        auto b = button(name);
        title->addWidget(b);
        connect(b, &QToolButton::clicked, this,
                [this, name]
                {
                    if (name == "minimize")
                        window->showMinimized();
                    else if (name == "maximize")
                    {
                        if (window->isMaximized())
                            window->showNormal();
                        else
                            window->showMaximized();
                    }
                    else if (name == "fullscreen")
                        window->toggleInterfaceFullScreen();
                    else
                        window->close();
                });
    }
    root->addWidget(header);
    auto body = new QWidget(this);
    bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(8, 8, 8, 8);
    bodyLayout->setSpacing(16);
    sidebar = new QWidget(body);
    sidebar->setObjectName("navigation");
    sidebar->setFixedWidth(196);
    auto navigation = new QVBoxLayout(sidebar);
    navigation->setContentsMargins(8, 10, 8, 10);
    navigation->setSpacing(8);
    QButtonGroup *navGroup = new QButtonGroup(this);
    navGroup->setExclusive(true);
    const QStringList names = QStringList() << "Home"
                                            << "Videos"
                                            << "Music"
                                            << "Network"
                                            << "Playlists"
                                            << "Recent";
    const QStringList icons = QStringList() << "home"
                                            << "videos"
                                            << "music"
                                            << "network"
                                            << "playlist"
                                            << "recent";
    for (int i = 0; i < names.size(); ++i)
    {
        auto b = button(icons[i], names[i]);
        b->setProperty("section", names[i]);
        b->setCheckable(true);
        b->setFixedHeight(40);
        navGroup->addButton(b);
        navigation->addWidget(b);
        connect(b, &QToolButton::clicked, this, [this, names, i] { showLibrary(names[i]); });
    }
    auto add = button("add", "Add item");
    navigation->addWidget(add);
    connect(add, &QToolButton::clicked, this,
            [this, add]
            {
                QMenu m;
                m.addAction("Open File(s)", THEDP, &DialogsProvider::openFileDialog);
                m.addAction("Open Folder", this,
                            [this]
                            {
                                auto path = QFileDialog::getExistingDirectory(this, "Open Folder");
                                if (!path.isEmpty())
                                    browse(path);
                            });
                m.addAction("Open Network Stream", this, [this] { openNetwork(); });
                m.addAction("New Playlist", this, [this] { newPlaylist(); });
                m.exec(add->mapToGlobal(QPoint(0, add->height())));
            });
    navigation->addStretch();
    auto prefs = button("settings", "Settings");
    navigation->addWidget(prefs);
    connect(prefs, &QToolButton::clicked, this, [this] { openSettings(); });
    bodyLayout->addWidget(sidebar);
    pages = new QStackedWidget(body);
    pages->setObjectName("content");
    browser = new QWidget(pages);
    browser->setObjectName("browser");
    auto library = new QVBoxLayout(browser);
    library->setContentsMargins(8, 0, 8, 0);
    library->setSpacing(12);
    auto tools = new QHBoxLayout;
    tools->setContentsMargins(0, 0, 0, 8);
    tools->setSpacing(12);
    auto back = button("back"), forward = button("forward");
    tools->addWidget(back);
    tools->addWidget(forward);
    connect(back, &QToolButton::clicked, this,
            [this]
            {
                if (historyPosition > 0)
                    browse(history[--historyPosition], false);
            });
    connect(forward, &QToolButton::clicked, this,
            [this]
            {
                if (historyPosition + 1 < history.size())
                    browse(history[++historyPosition], false);
            });
    location = new QLabel;
    location->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    location->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
    connect(location, &QLabel::linkActivated, this,
            [this](const QString &url) { browse(QUrl(url).toLocalFile()); });
    tools->addWidget(location, 1);
    count = new QLabel;
    tools->addWidget(count);
    auto folder = button("folder");
    folder->setToolTip("Browse a media folder");
    tools->addWidget(folder);
    connect(folder, &QToolButton::clicked, this,
            [this]
            {
                QString d = QFileDialog::getExistingDirectory(this, "Open media folder", currentDirectory);
                if (!d.isEmpty())
                {
                    getSettings()->setValue("Redesign/" + section + "Directory", d);
                    browse(d);
                }
            });
    auto grid = button("grid");
    grid->setToolTip("Switch grid or list view");
    tools->addWidget(grid);
    auto sorting = button("sliders");
    sorting->setToolTip("Sort media");
    tools->insertWidget(tools->indexOf(grid), sorting);
    connect(sorting, &QToolButton::clicked, this,
            [this, sorting]
            {
                QMenu menu;
                const QStringList labels = QStringList() << "Name"
                                                         << "Date modified"
                                                         << "File size"
                                                         << "Duration"
                                                         << "Artist"
                                                         << "Album"
                                                         << "Genre"
                                                         << "Track number";
                for (int i = 0; i < labels.size(); ++i)
                {
                    auto action = menu.addAction(labels[i]);
                    action->setCheckable(true);
                    action->setChecked(i == sortMode);
                    connect(action, &QAction::triggered, this,
                            [this, i]
                            {
                                sortMode = i;
                                refresh();
                            });
                }
                menu.addSeparator();
                auto descending = menu.addAction("Descending");
                descending->setCheckable(true);
                descending->setChecked(getSettings()->value("Redesign/sortDescending", false).toBool());
                connect(descending, &QAction::toggled, this,
                        [this](bool on)
                        {
                            getSettings()->setValue("Redesign/sortDescending", on);
                            refresh();
                        });
                menu.exec(sorting->mapToGlobal(QPoint(0, sorting->height())));
            });
    connect(grid, &QToolButton::clicked, this,
            [this, grid]
            {
                bool isGrid = files->viewMode() == QListView::IconMode;
                files->setViewMode(isGrid ? QListView::ListMode : QListView::IconMode);
                files->setGridSize(isGrid ? QSize(320, 56) : QSize(320, 232));
                grid->setIcon(lineIcon(isGrid ? "list" : "grid"));
            });
    search = new QLineEdit;
    search->setPlaceholderText("Search");
    search->setAccessibleName("Search media");
    search->setMinimumWidth(220);
    search->setMaximumWidth(300);
    search->setClearButtonEnabled(true);
    search->addAction(lineIcon("search"), QLineEdit::LeadingPosition);
    tools->addWidget(search);
    connect(search, &QLineEdit::textChanged, this,
            [this](const QString &term)
            {
                const auto lists = homePanel ? homePanel->findChildren<QListWidget *>("homeMedia")
                                             : QList<QListWidget *>({files});
                for (auto list : lists)
                    for (int i = 0; i < list->count(); ++i)
                        list->item(i)->setHidden(!list->item(i)->text().contains(term, Qt::CaseInsensitive));
            });
    library->addLayout(tools);
    auto musicCategories = Reframe::segmented(QStringList() << "Songs"
                                                            << "Albums"
                                                            << "Artists"
                                                            << "Genres",
                                              "Songs", browser,
                                              [this](const QString &category)
                                              {
                                                  musicMode = category;
                                                  musicGroup.clear();
                                                  refresh();
                                              });
    musicCategories->setObjectName("musicCategories");
    musicCategories->hide();
    library->addWidget(musicCategories);
    files = new QListWidget(browser);
    files->setObjectName("mediaGrid");
    files->setViewMode(QListView::IconMode);
    files->setResizeMode(QListView::Adjust);
    files->setMovement(QListView::Static);
    files->setWrapping(true);
    files->setGridSize(QSize(320, 232));
    files->setIconSize(QSize(304, 171));
    files->setSelectionMode(QAbstractItemView::ExtendedSelection);
    files->setItemDelegate(new MediaDelegate(files));
    files->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(files, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem *item)
            {
                QString path = item->data(Qt::UserRole).toString();
                if (QFileInfo(path).isDir())
                    browse(path);
                else
                    openMedia(path);
            });
    connect(files, &QListWidget::customContextMenuRequested, this, &RedesignShell::contextMenu);
    library->addWidget(files, 1);
    empty = new QLabel;
    empty->setAlignment(Qt::AlignCenter);
    empty->setWordWrap(true);
    empty->setObjectName("emptyState");
    library->addWidget(empty, 1);
    pages->addWidget(browser);
    videoPage = new QWidget(pages);
    videoLayout = new QVBoxLayout(videoPage);
    videoLayout->setContentsMargins(0, 0, 0, 0);
    videoLayout->addWidget(player);
    pages->addWidget(videoPage);
    bodyLayout->addWidget(pages, 1);
    root->addWidget(body, 1);

    transport = new QWidget(this);
    transport->setObjectName("transport");
    transport->setFixedHeight(54);
    transport->setProperty("chromeHeight", 54);
    auto bar = new QHBoxLayout(transport);
    transportLayout = bar;
    bar->setContentsMargins(12, 4, 12, 4);
    bar->setSpacing(8);
    auto previous = button("previous");
    bar->addWidget(previous);
    connect(previous, &QToolButton::clicked, THEMIM, &MainInputManager::prev);
    playButton = button("play");
    playButton->setObjectName("playButton");
    playButton->setToolTip("Play / Pause");
    playButton->setFixedSize(54, 30);
    bar->addWidget(playButton);
    connect(playButton, &QToolButton::clicked, THEAM, &ActionsManager::play);
    auto next = button("next");
    bar->addWidget(next);
    connect(next, &QToolButton::clicked, THEMIM, &MainInputManager::next);
    auto stop = button("stop");
    bar->addWidget(stop);
    connect(stop, &QToolButton::clicked, THEMIM, &MainInputManager::stop);
    elapsed = new QLabel("00:00:00");
    elapsed->setObjectName("elapsedTime");
    elapsed->setMinimumWidth(62);
    bar->addWidget(elapsed);
    seek = new RedesignSeek(p_intf, transport);
    seek->setObjectName("seek");
    seek->setRange(0, 100000);
    seek->setEnabled(false);
    seek->setAccessibleName("Playback position");
    bar->addWidget(seek, 1);
    connect(seek, &QSlider::valueChanged, this,
            [this](int v) { THEMIM->getIM()->sliderUpdate(v / 100000.f); });
    duration = new QLabel("00:00:00");
    duration->setMinimumWidth(62);
    duration->installEventFilter(this);
    bar->addWidget(duration);
    connect(THEMIM->getIM(), &InputManager::positionUpdated, this,
            [this](float pos, int64_t time, int length)
            {
                elapsed->setText(clockText(time / CLOCK_FREQ));
                duration->setText(
                    (remainingTime ? "-" : "") +
                    clockText(remainingTime ? qMax<qint64>(0, length - time / CLOCK_FREQ) : length));
                if (!seek->isSliderDown())
                {
                    QSignalBlocker blocked(seek);
                    seek->setValue(qMax(0, qRound(pos * 100000)));
                }
            });
    connect(THEMIM->getIM(), &InputManager::positionUpdated, this,
            [this](float position, int64_t time, int length)
            {
                const qint64 second = time / CLOCK_FREQ;
                if (!config_GetInt(p_intf, "qt-recentplay") || !config_GetInt(p_intf, "qt-continue") ||
                    second % 5 || second == lastResumeSave || !THEMIM->currentInputItem())
                    return;
                lastResumeSave = second;
                char *raw = input_item_GetURI(THEMIM->currentInputItem());
                const QString uri = qfu(raw ? raw : "");
                free(raw);
                QVariantMap resume;
                resume["position"] = position;
                resume["time"] = second;
                resume["duration"] = length;
                getSettings()->setValue("Redesign/resume/" + uri, resume);
            });
    connect(THEMIM->getIM(), &InputManager::inputCanSeek, seek, &QSlider::setEnabled);
    connect(THEMIM->getIM(), &InputManager::playingStatusChanged, this,
            [this](int state)
            {
                playButton->setProperty("glyph", state == PLAYING_S ? "pause" : "play");
                playButton->setIcon(lineIcon(state == PLAYING_S ? "pause" : "play", QColor("#111111")));
                if (state == END_S)
                    seek->setEnabled(false);
            });
    auto mute = button("volume");
    mute->setToolTip("Mute / Unmute");
    bar->addWidget(mute);
    connect(mute, &QToolButton::clicked, THEAM, &ActionsManager::toggleMuteAudio);
    connect(THEMIM, &MainInputManager::soundMuteChanged, this,
            [mute](bool m) { mute->setIcon(lineIcon(m ? "mute" : "volume")); });
    volume = new VolumeSlider(transport);
    volume->setRange(0, qMax(1, int(config_GetInt(p_intf, "qt-max-volume"))));
    volume->setAccessibleName("Volume");
    volume->setObjectName("volumeSlider");
    volume->setValue(qMax(0, qRound(playlist_VolumeGet(THEPL) * 100)));
    bar->addWidget(volume);
    connect(volume, &QSlider::valueChanged, this,
            [this](int v)
            {
                playlist_VolumeSet(THEPL, v / 100.f);
                volume->setToolTip(QString::number(v) + "%");
            });
    connect(THEMIM, &MainInputManager::volumeChanged, this,
            [this](float v)
            {
                QSignalBlocker blocked(volume);
                volume->setValue(qRound(v * 100));
            });
    auto eq = button("audio"), sub = button("subtitles"), settings = button("settings"),
         playlist = button("playlist");
    bar->addWidget(eq);
    bar->addWidget(sub);
    bar->addWidget(settings);
    bar->addWidget(playlist);
    auto fullscreen = button("fullscreen");
    fullscreen->setToolTip("Toggle fullscreen");
    fullscreen->setObjectName("fullscreenButton");
    bar->addWidget(fullscreen);
    connect(fullscreen, &QToolButton::clicked, this, [this] { toggleFullscreen(); });
    eq->setToolTip("Audio effects");
    sub->setToolTip("Subtitles and synchronization");
    settings->setToolTip("Settings");
    playlist->setToolTip("Playlist");
    connect(eq, &QToolButton::clicked, this, [this] { openTracks(6); });
    connect(sub, &QToolButton::clicked, this, [this] { openTracks(7); });
    connect(settings, &QToolButton::clicked, this, [this] { openMediaSettings(4); });
    connect(playlist, &QToolButton::clicked, this, [this] { showQueue(); });
    auto grip = new QSizeGrip(transport);
    grip->setObjectName("resizeGrip");
    bar->addWidget(grip);
    root->addWidget(transport);
    connect(THEMIM, &MainInputManager::inputChanged, this,
            [this](bool has)
            {
                lastResumeSave = -1;
                setProperty("audioFocusPaused", false);
                if (has)
                {
                    if (!miniPlayer)
                        showPlayer();
                    QTimer::singleShot(250, this, [this] { applyMediaBindings(); });
                }
                else
                {
                    bindingInput.clear();
                    seek->setEnabled(false);
                    QSignalBlocker blocked(seek);
                    seek->setValue(0);
                }
            });
    connect(THEMIM->getIM(), &InputManager::nameChanged, this,
            [titleLabel](const QString &name) { titleLabel->setText(name.isEmpty() ? "Reframe" : name); });
    for (auto b : transport->findChildren<QToolButton *>())
        controlButtons[b->property("glyph").toString()] = b;
    chromeTimer = new QTimer(this);
    chromeTimer->setSingleShot(true);
    connect(chromeTimer, &QTimer::timeout, this,
            [this]
            {
                if (chromeCanHide())
                {
                    Reframe::animateHeight(header, false, 300);
                    Reframe::animateHeight(transport, false, 300);
                }
            });
    videoClickTimer = new QTimer(this);
    videoClickTimer->setSingleShot(true);
    videoClickTimer->setInterval(QApplication::doubleClickInterval());
    connect(videoClickTimer, &QTimer::timeout, this,
            [this]
            {
                if (!THEMIM->getIM()->hasInput())
                    return;
                const QString action =
                    getSettings()->value("Redesign/singleClick", "Play / Pause").toString();
                if (action == "Play / Pause")
                    THEMIM->togglePlayPause();
                else if (action == "Toggle fullscreen")
                    THEAM->doAction(FULLSCREEN_ACTION);
            });
    qApp->installEventFilter(this);
    player->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(player, &QWidget::customContextMenuRequested, this,
            [this](const QPoint &p) { playerMenu(player->mapToGlobal(p)); });
    auto libraryShortcut = new QShortcut(QKeySequence(Qt::Key_L), window);
    connect(libraryShortcut, &QShortcut::activated, this, [this] { toggleLibraryDock(); });
    auto queueShortcut = new QShortcut(QKeySequence(Qt::Key_Q), window);
    connect(queueShortcut, &QShortcut::activated, this, [this] { toggleQueue(); });
    auto metadataTimer = new QTimer(this);
    metadataTimer->setInterval(350);
    connect(metadataTimer, &QTimer::timeout, this, [this] { pollMetadata(); });
    metadataTimer->start();
    startAudioFocus();
    auto edgeTimer = new QTimer(this);
    edgeTimer->setInterval(80);
    connect(edgeTimer, &QTimer::timeout, this,
            [this]
            {
                if (!window->isActiveWindow())
                    return;
                const auto point = window->mapFromGlobal(QCursor::pos());
                if (!chromeCanHide())
                    return;
                if (!minimal && !window->isFullScreen())
                {
                    if (getSettings()->value("Redesign/autoHideControls").toString() == "Always" &&
                        isPlayerVisible() && THEMIM->getIM()->hasInput())
                    {
                        const QPoint previous = property("lastControlPointer").toPoint();
                        setProperty("lastControlPointer", point);
                        if (point != previous && rect().contains(point))
                            showChrome();
                        else if (!chromeTimer->isActive() && header->isVisible())
                            chromeTimer->start(qMax(500, int(config_GetInt(p_intf, "mouse-hide-timeout"))));
                    }
                    return;
                }
                if (window->isFullScreen() && !config_GetInt(p_intf, "qt-fs-controller"))
                    return;
                if (point.x() < 0 || point.x() > window->width())
                    return;
                if (point.y() >= 0 && point.y() < 54)
                {
                    if (!header->isVisible())
                        Reframe::animateHeight(header, true, 300);
                    chromeTimer->start(qMax(500, int(config_GetInt(p_intf, "mouse-hide-timeout"))));
                }
                else if (point.y() < window->height() && point.y() > window->height() - 80)
                {
                    if (!transport->isVisible())
                        Reframe::animateHeight(transport, true, 300);
                    chromeTimer->start(qMax(500, int(config_GetInt(p_intf, "mouse-hide-timeout"))));
                }
            });
    edgeTimer->start();
    layoutControls();
    applyTheme();
    showLibrary("Home");
    setProperty("windowReady", true);
    var_AddCallback(THEPL, "fullscreen", fullscreenChanged, this);
    if (THEMIM->getIM()->hasInput())
        showPlayer();
}

RedesignShell::~RedesignShell()
{
    var_DelCallback(THEPL, "fullscreen", fullscreenChanged, this);
    libvlc_MetadataCancel(p_intf->obj.libvlc, this);
    for (auto item : metadataRequests)
        input_item_Release(item);
    if (thumbnailProcess)
    {
        thumbnailProcess->kill();
        thumbnailProcess->waitForFinished(1000);
    }
}

void RedesignShell::applyTheme()
{
    if (!appliedToolbar.isEmpty() && getSettings()->contains("Redesign/toolbar") &&
        getSettings()->value("Redesign/toolbar").toStringList() != appliedToolbar)
        layoutControls();
    accent = getSettings()->value("Redesign/accent", "#d37918").toString();
    QString theme = getSettings()->value("Redesign/theme", "Dark").toString();
    bool light =
        theme == "Light" || (theme == "System" && systemPalette.color(QPalette::Window).lightness() > 128);
    const QString bg = light ? "#f5f5f5" : "#0e0e0e", panel = light ? "#e9e9e9" : "#191919",
                  text = light ? "#303030" : "#8f8f8f", line = light ? "#d0d0d0" : "#292929";
    QApplication::setStyle("Fusion");
    QPalette p = systemPalette;
    p.setColor(QPalette::Window, QColor(bg));
    p.setColor(QPalette::Base, QColor(bg));
    p.setColor(QPalette::AlternateBase, QColor(panel));
    p.setColor(QPalette::Button, QColor(panel));
    p.setColor(QPalette::Mid, QColor(line));
    p.setColor(QPalette::Text, QColor(text));
    p.setColor(QPalette::WindowText, QColor(text));
    p.setColor(QPalette::ButtonText, QColor(text));
    p.setColor(QPalette::Highlight, QColor(accent));
    p.setColor(QPalette::HighlightedText, QColor("#101010"));
    p.setColor(QPalette::Disabled, QPalette::Text, QColor("#727272"));
    QApplication::setPalette(p);
    QFont font(QFontDatabase().families().contains("Inter") ? "Inter" : "Segoe UI");
    font.setPixelSize(14);
    QApplication::setFont(font);
    QString css =
        QString(
            "QWidget { color:%1; } QWidget#redesignShell, QWidget#browser, QDialog { background:%2; }"
            "QWidget#redesignShell {border:1px solid %4;} "
            "QWidget#titleBar, QWidget#navigation, QWidget#transport { background:%3; } QWidget#navigation { "
            "border-radius:7px; }"
            "QToolButton { background:transparent; border:0; border-radius:5px; padding:3px; }"
            "QPushButton:hover { background:%4; } QToolButton:checked { background:%4; "
            "color:%5; }"
            "QWidget#navigation QToolButton { text-align:left; padding:7px; }"
            "QToolButton#playButton { background:%5; color:#111; } QToolButton#playButton:hover { "
            "background:%5; }"
            "QCheckBox::indicator { width:16px; height:16px; background:%2; border:1px solid %1; "
            "border-radius:3px; } QCheckBox::indicator:checked { background:%5; border-color:%5; }"
            "QPushButton { background:%3; border:1px solid %4; border-radius:5px; padding:9px 16px; }"
            "QLineEdit,QComboBox,QSpinBox,QDoubleSpinBox { background:%3; border:1px solid %4; "
            "border-radius:5px; padding:7px; min-height:20px; }"
            "QLineEdit:focus,QComboBox:focus,QDoubleSpinBox:focus { border-color:%5; }"
            "QListWidget#mediaGrid { background:%2; border:0; outline:0; } QListWidget#settingsNav { "
            "background:%3; border:0; padding:8px; border-radius:7px; }"
            "QListWidget#settingsNav::item { padding:12px; border-radius:4px; } "
            "QListWidget#settingsNav::item:selected { background:%4; color:%5; }"
            "QLabel#emptyState { color:%1; font-size:16px; } QLabel#sectionHeading { font-size:16px; "
            "font-weight:500; padding:14px 0 4px; } QLabel#dialogHeading {font-size:16px;}"
            "QWidget#settingsCard,QWidget#segmented,QWidget#queuePanel,QWidget#libraryDock "
            "{background:%3;border-radius:6px;} QWidget#settingsContent,QWidget#mediaSettings "
            "{background:%2;}"
            "QWidget#segmented QPushButton {border:0;padding:7px 10px;} QWidget#segmented "
            "QPushButton:checked {background:%4;color:%5;} QPushButton#foldButton {border:0;} "
            "QPushButton#mediaTab {border:0;font-size:12px;padding:10px 5px;} QPushButton#mediaTab:checked "
            "{border-bottom:2px solid %5;} QWidget#seekPreview {background:%3;border-radius:6px;}"
            "QSlider::groove:horizontal { height:10px; background:%2; border-radius:4px; } "
            "QSlider::sub-page:horizontal { background:%5; border-radius:4px; }"
            "QSlider::handle:horizontal { background:%5; width:7px; margin:-3px 0; border-radius:3px; }"
            "QSlider::groove:vertical { width:10px; background:%2; border-radius:4px; } "
            "QSlider::add-page:vertical { background:%5; border-radius:4px; }"
            "QSlider::handle:vertical { background:%5; height:7px; margin:0 -4px; border-radius:3px; }"
            "QMenu { background:%3; border:1px solid %4; padding:7px; } QMenu::item { padding:9px 26px; } "
            "QMenu::item:selected { background:%4; } QMenu::separator { height:1px; background:%4; "
            "margin:5px; }"
            "QScrollArea { border:0; background:%2; } QScrollBar:vertical { background:%2; width:10px; } "
            "QScrollBar::handle:vertical { background:%4; min-height:20px; border-radius:4px; }"
            "QTreeView,QTableView,QTabWidget::pane { background:%2; border:1px solid %4; } "
            "QHeaderView::section { background:%3; padding:7px; border:0; }"
            "QTabBar::tab { background:%3; padding:9px 14px; } QTabBar::tab:selected { border-bottom:2px "
            "solid %5; }")
            .arg(text, bg, panel, line, accent);
    css += "QComboBox::drop-down {border:0;width:28px;background:transparent;} "
           "QComboBox::down-arrow {image:url(:/reframe/chevron.svg);width:12px;height:12px;} "
           "QLabel#sectionHeading,QLabel#dialogHeading {color:" +
           (light ? QString("#303030") : QString("#c6c6c6")) + ";}";
    css += QString("QWidget#trackPicker {background:%1;border-radius:10px;} "
                   "QListWidget#trackChoices,QListWidget#outputDeviceChoices {background:%2;border:0;} "
                   "QPushButton#trackPreferencesButton {background:%2;border:0;} "
                   "QListWidget {background:%2;border:1px solid %3;outline:0;} "
                   "QListWidget::item {padding:8px;} ")
               .arg(panel, bg, line);
    const QColor selected(accent);
    css += QString("QSlider::sub-page:horizontal,QSlider::add-page:vertical {background:rgba(%1,%2,%3,65);} "
                   "QAbstractScrollArea::corner {background:%4;border:0;} "
                   "QPushButton#mediaTab {background:transparent;border-radius:0;font-size:14px;} ")
               .arg(selected.red())
               .arg(selected.green())
               .arg(selected.blue())
               .arg(bg);
    css += QString("QDialog#onboardingDialog QLabel#onboardingHeading {font-size:24px;color:%1;} "
                   "QWidget#onboardingThemes {background:%2;border-radius:6px;} "
                   "QWidget#onboardingThemes QPushButton {border:0;padding:10px 8px;} "
                   "QWidget#onboardingThemes QPushButton:checked {background:%3;color:%4;} "
                   "QLabel#shortcutKeycap {background:%2;border:1px solid %3;border-radius:6px;"
                   "padding:0 8px;font-size:14px;color:%1;} "
                   "QLabel#shortcutAction {font-size:16px;} ")
               .arg(light ? QString("#303030") : QString("#c6c6c6"), panel, line, accent);
    qApp->setStyleSheet(css);
    for (QToolButton *b : findChildren<QToolButton *>())
    {
        QString glyph = b->property("glyph").toString();
        if (!glyph.isEmpty())
            b->setIcon(lineIcon(glyph, QColor(b == playButton                      ? "#111111"
                                              : glyph == "brand" || b->isChecked() ? accent
                                                                                   : text)));
    }
    window->setWindowIcon(lineIcon("brand", QColor(accent)));
    qApp->setWindowIcon(window->windowIcon());
    if (auto timeline = dynamic_cast<RedesignSeek *>(seek))
    {
        if (timeline->property("waveformState").toBool() !=
            getSettings()->value("Redesign/waveform", false).toBool())
        {
            timeline->setProperty("waveformState", getSettings()->value("Redesign/waveform", false));
            timeline->refreshMedia();
        }
        timeline->update();
    }
}

void RedesignShell::setNavigationWidth(bool collapse)
{
    if (compact == collapse && sidebar->width() == (collapse ? 48 : 196))
        return;
    compact = collapse;
    if (navigationAnimation)
    {
        navigationAnimation->stop();
        navigationAnimation->deleteLater();
    }
    sidebar->setMinimumWidth(48);
    navigationAnimation = new QPropertyAnimation(sidebar, "maximumWidth", this);
    navigationAnimation->setStartValue(sidebar->width());
    navigationAnimation->setEndValue(collapse ? 48 : 196);
    navigationAnimation->setDuration(208);
    navigationAnimation->setEasingCurve(Reframe::springCurve());
    navigationAnimation->start();
    for (QToolButton *b : sidebar->findChildren<QToolButton *>())
        b->setToolButtonStyle(collapse ? Qt::ToolButtonIconOnly : Qt::ToolButtonTextBesideIcon);
}
void RedesignShell::showPlayer()
{
    if (miniPlayer)
    {
        miniPlayer->layout()->removeWidget(player);
        videoLayout->addWidget(player);
        miniPlayer->close();
        miniPlayer = nullptr;
    }
    pages->setCurrentWidget(videoPage);
    bodyLayout->setContentsMargins(0, 0, 8, 0);
    bodyLayout->setSpacing(0);
    setNavigationWidth(getSettings()->value("Redesign/compactOnPlayback", true).toBool());
}
bool RedesignShell::isPlayerVisible() const
{
    return pages->currentWidget() == videoPage;
}
void RedesignShell::setMinimal(bool m)
{
    if (minimal == m)
        return;
    minimal = m;
    Reframe::animateHeight(header, !m, 300);
    sidebar->setVisible(!m);
    Reframe::animateHeight(transport, !m, 300);
    if (sidePanel)
        sidePanel->hide();
    if (libraryDock)
        libraryDock->hide();
    if (extraToolbar)
        extraToolbar->setVisible(!m);
    if (m)
        showPlayer();
}

void RedesignShell::showLibrary(const QString &name)
{
    leaveFullscreen();
    const bool alongside = THEMIM->getIM()->hasInput() && isPlayerVisible() && name != "Home";
    section = name;
    musicGroup.clear();
    search->clear();
    setNavigationWidth(!alongside ? false : true);
    for (auto button : sidebar->findChildren<QToolButton *>())
        if (button->property("section").isValid())
            button->setChecked(button->property("section").toString() == name);
    pages->setCurrentWidget(browser);
    if (name == "Home" || name == "Recent" || name == "Playlists" || name == "Network")
    {
        currentDirectory.clear();
        refresh();
    }
    else
        browse(getSettings()
                   ->value("Redesign/" + name + "Directory",
                           QStandardPaths::writableLocation(name == "Music" ? QStandardPaths::MusicLocation
                                                                            : QStandardPaths::MoviesLocation))
                   .toString());
    if (alongside)
    {
        showPlayer();
        if (libraryDock)
        {
            bodyLayout->removeWidget(libraryDock);
            delete libraryDock;
            libraryDock = nullptr;
        }
        toggleLibraryDock();
    }
    else
    {
        bodyLayout->setContentsMargins(8, 8, 8, 8);
        bodyLayout->setSpacing(16);
        if (libraryDock)
            libraryDock->hide();
        if (sidePanel)
            sidePanel->hide();
        if (THEMIM->getIM()->hasInput() && !miniPlayer)
            toggleMiniPlayer();
    }
}

void RedesignShell::browse(const QString &directory, bool remember)
{
    currentDirectory = QDir(directory).absolutePath();
    if (remember)
    {
        while (history.size() > historyPosition + 1)
            history.removeLast();
        history << currentDirectory;
        historyPosition = history.size() - 1;
    }
    pages->setCurrentWidget(browser);
    refresh();
}

void RedesignShell::refresh()
{
    if (homePanel)
        homePanel->hide();
    if (auto music = browser->findChild<QWidget *>("musicCategories"))
        music->setVisible(section == "Music");
    files->clear();
    thumbnailQueue.clear();
    if (currentDirectory.isEmpty())
        location->setText(section);
    else
    {
        QString html;
        const QString root =
            getSettings()->value("Redesign/" + section + "Directory", currentDirectory).toString();
        QString cursor = QDir(root).absolutePath();
        html = "<a style='color:#8f8f8f;text-decoration:none' href='" +
               QUrl::fromLocalFile(cursor).toString().toHtmlEscaped() + "'>" + section.toHtmlEscaped() +
               "</a>";
        const auto relative = QDir(cursor).relativeFilePath(currentDirectory);
        if (relative != ".")
            for (const auto &part : relative.split('/', QString::SkipEmptyParts))
            {
                cursor = QDir(cursor).filePath(part);
                html += " &gt; <a style='color:#8f8f8f;text-decoration:none' href='" +
                        QUrl::fromLocalFile(cursor).toString().toHtmlEscaped() + "'>" + part.toHtmlEscaped() +
                        "</a>";
            }
        location->setText(html);
    }
    QStringList paths;
    if (currentDirectory.isEmpty())
    {
        if (section == "Playlists" || section == "Network")
        {
            const auto ids = getSettings()->value("Redesign/collections").toStringList();
            for (const auto &id : ids)
            {
                const auto data = getSettings()->value("Redesign/collection/" + id).toMap();
                if (data.value("network").toBool() == (section == "Network"))
                    paths << ("collection:" + id);
            }
        }
        else
            paths = RecentsMRL::getInstance(p_intf)->recentList();
    }
    else
    {
        const QStringList filters =
            QString::fromUtf8(section == "Music" ? EXTENSIONS_AUDIO : EXTENSIONS_MEDIA)
                .split(';', QString::SkipEmptyParts);
        QDir d(currentDirectory);
        d.setNameFilters(filters);
        QDir::Filters visibility = QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Readable;
        if (config_GetInt(p_intf, "show-hiddenfiles"))
            visibility |= QDir::Hidden;
        d.setFilter(visibility);
        d.setSorting(QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
        for (const QFileInfo &f : d.entryInfoList())
            paths << f.absoluteFilePath();
        paths += getSettings()->value("Redesign/libraryReferences/" + currentDirectory).toStringList();
    }
    for (const auto &path : paths)
        if (QFileInfo(path).isFile())
            requestMetadata(path);
    std::sort(paths.begin(), paths.end(),
              [this](const QString &a, const QString &b)
              {
                  const auto x = metadata.value(a), y = metadata.value(b);
                  const QFileInfo fa(a), fb(b);
                  if (fa.isDir() != fb.isDir())
                      return fa.isDir();
                  int comparison = 0;
                  switch (sortMode)
                  {
                  case 1:
                      comparison = fa.lastModified() < fb.lastModified()   ? -1
                                   : fa.lastModified() > fb.lastModified() ? 1
                                                                           : 0;
                      break;
                  case 2:
                      comparison = fa.size() < fb.size() ? -1 : fa.size() > fb.size() ? 1 : 0;
                      break;
                  case 3:
                      comparison = x.value("duration").toLongLong() < y.value("duration").toLongLong()   ? -1
                                   : x.value("duration").toLongLong() > y.value("duration").toLongLong() ? 1
                                                                                                         : 0;
                      break;
                  case 4:
                      comparison = x.value("artist").toString().compare(y.value("artist").toString(),
                                                                        Qt::CaseInsensitive);
                      break;
                  case 5:
                      comparison = x.value("album").toString().compare(y.value("album").toString(),
                                                                       Qt::CaseInsensitive);
                      break;
                  case 6:
                      comparison = x.value("genre").toString().compare(y.value("genre").toString(),
                                                                       Qt::CaseInsensitive);
                      break;
                  case 7:
                      comparison = x.value("track").toInt() - y.value("track").toInt();
                      break;
                  default:
                      break;
                  }
                  if (!comparison)
                      comparison = fa.fileName().compare(fb.fileName(), Qt::CaseInsensitive);
                  return getSettings()->value("Redesign/sortDescending", false).toBool() ? comparison > 0
                                                                                         : comparison < 0;
              });
    if (section == "Music" && musicMode != "Songs")
    {
        const QString key = musicMode == "Albums" ? "album" : musicMode == "Artists" ? "artist" : "genre";
        if (musicGroup.isEmpty())
        {
            QSet<QString> groups;
            for (const auto &path : paths)
            {
                QString name = metadata.value(path).value(key).toString();
                if (name.isEmpty())
                    name = "Unknown " + key;
                if (groups.contains(name))
                    continue;
                groups.insert(name);
                auto item = new QListWidgetItem(name, files);
                item->setIcon(Reframe::icon("music", QColor(accent)));
                const QUrl art(metadata.value(path).value("art").toString());
                if (art.isLocalFile())
                {
                    item->setIcon(QIcon(art.toLocalFile()));
                    item->setData(Qt::UserRole + 1, true);
                }
                item->setData(Qt::UserRole, QString("group:" + name));
            }
            count->setText(QString::number(groups.size()) + " " + musicMode.toLower());
            files->show();
            empty->hide();
            return;
        }
        QStringList members;
        for (const auto &path : paths)
        {
            QString name = metadata.value(path).value(key).toString();
            if (name.isEmpty())
                name = "Unknown " + key;
            if (name == musicGroup)
                members << path;
        }
        paths = members;
    }
    for (const QString &path : paths)
    {
        if (path.startsWith("collection:"))
        {
            const auto id = path.mid(11);
            const auto data = getSettings()->value("Redesign/collection/" + id).toMap();
            auto item = new QListWidgetItem(data.value("name").toString(), files);
            item->setData(Qt::UserRole, path);
            item->setIcon(data.value("thumbnail").toString().isEmpty()
                              ? lineIcon("playlist", QColor(accent))
                              : QIcon(data.value("thumbnail").toString()));
            continue;
        }
        QUrl url(path);
        QString local = url.isLocalFile() ? url.toLocalFile() : path;
        QFileInfo f(local);
        bool folder = f.isDir();
        if (!folder && !f.exists() && url.scheme().isEmpty())
            continue;
        auto item = new QListWidgetItem(f.fileName().isEmpty() ? path : f.fileName(), files);
        item->setToolTip(local);
        item->setData(Qt::UserRole, local);
        const auto info = metadata.value(local);
        if (section == "Music" && !info.value("title").toString().isEmpty())
            item->setText(info.value("title").toString());
        item->setData(Qt::UserRole + 2,
                      getSettings()
                          ->value("Redesign/resume/" + QUrl::fromLocalFile(local).toString())
                          .toMap()
                          .value("position"));
        item->setData(Qt::UserRole + 3, info.value("artist"));
        item->setData(Qt::UserRole + 4, info.value("duration"));
        item->setIcon(lineIcon(folder ? "folder" : section == "Music" ? "music" : "videos", QColor(accent)));
        const QUrl art(info.value("art").toString());
        if (art.isLocalFile())
        {
            item->setIcon(QIcon(art.toLocalFile()));
            item->setData(Qt::UserRole + 1, true);
        }
        if (!folder && f.exists())
        {
            QByteArray fingerprint = (f.absoluteFilePath() + QString::number(f.size()) +
                                      QString::number(f.lastModified().toMSecsSinceEpoch()))
                                         .toUtf8();
            QString thumb = cacheDirectory + "/" +
                            QString::fromLatin1(
                                QCryptographicHash::hash(fingerprint, QCryptographicHash::Sha256).toHex()) +
                            ".png";
            thumbnails[local] = thumb;
            if (QFileInfo(thumb).size() > 0)
            {
                item->setIcon(QIcon(thumb));
                item->setData(Qt::UserRole + 1, true);
            }
            if (!QFileInfo::exists(thumb) &&
                QStringList({"mp4", "mkv", "avi", "mov", "webm", "m4v", "wmv", "ts", "mpeg", "mpg", "flv"})
                    .contains(f.suffix().toLower()))
                thumbnailQueue.enqueue(local);
        }
    }
    count->setText(QString::number(files->count()) + " items");
    bool none = files->count() == 0;
    files->setVisible(!none);
    empty->setVisible(none);
    empty->setText(
        currentDirectory.isEmpty()
            ? "Your recent media will appear here.\nOpen a file or browse a media folder to get started."
            : "No media in this folder.\nUse the folder button to choose your videos or music.");
    nextThumbnail();
    refreshPins();
    if (section == "Home")
        renderHome();
}

void RedesignShell::openMedia(const QString &path)
{
    if (path.startsWith("artist:"))
    {
        section = "Music";
        musicMode = "Artists";
        musicGroup = path.mid(7);
        browse(getSettings()
                   ->value("Redesign/MusicDirectory",
                           QStandardPaths::writableLocation(QStandardPaths::MusicLocation))
                   .toString());
        return;
    }
    if (!path.startsWith("group:") && config_GetInt(p_intf, "qt-recentplay"))
    {
        auto usage = getSettings()->value("Redesign/usage").toMap();
        const QUrl url = QUrl::fromUserInput(path, QString(), QUrl::AssumeLocalFile);
        const QString destination = path.startsWith("collection:") ? path
                                    : url.isLocalFile() ? QFileInfo(url.toLocalFile()).absolutePath()
                                                        : QString();
        if (!destination.isEmpty())
            usage[destination] = usage.value(destination).toInt() + 1;
        getSettings()->setValue("Redesign/usage", usage);
    }
    if (path.startsWith("group:"))
    {
        musicGroup = path.mid(6);
        refresh();
        return;
    }
    if (path.startsWith("collection:"))
    {
        currentCollection = path.mid(11);
        const auto data = getSettings()->value("Redesign/collection/" + currentCollection).toMap();
        const auto paths = data.value("paths").toStringList();
        bool first = true;
        for (const auto &item : paths)
        {
            Open::openMRL(p_intf, QUrl::fromUserInput(item, QString(), QUrl::AssumeLocalFile).toString(),
                          first);
            first = false;
        }
        showPlayer();
        showQueue();
        return;
    }
    QUrl url = QUrl::fromUserInput(path, QString(), QUrl::AssumeLocalFile);
    showPlayer();
    if (window->isPlaylistVisible())
        window->togglePlaylist();
    Open::openMRL(p_intf, url.toString());
}

void RedesignShell::nextThumbnail()
{
    if (thumbnailProcess || thumbnailQueue.isEmpty())
        return;
    thumbnailPath = thumbnailQueue.dequeue();
    QString output = thumbnails.value(thumbnailPath);
    if (output.isEmpty())
        return;
    thumbnailProcess = new QProcess(this);
    QProcess *process = thumbnailProcess;
    QString path = thumbnailPath;
    QStringList args;
    args << "--intf=dummy"
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
         << "--scene-width=320"
         << "--scene-ratio=1"
         << "--scene-replace" << ("--scene-path=" + cacheDirectory)
         << ("--scene-prefix=" + QFileInfo(output).completeBaseName()) << "--start-time=1"
         << "--stop-time=2"
         << "--play-and-exit" << QUrl::fromLocalFile(path).toString();
    auto done = [this, process, path, output]
    {
        if (thumbnailProcess != process)
            return;
        thumbnailProcess = nullptr;
        process->deleteLater();
        if (QFileInfo(output).size() > 0)
            for (int i = 0; i < files->count(); ++i)
            {
                auto item = files->item(i);
                if (item->data(Qt::UserRole).toString() == path)
                {
                    item->setIcon(QIcon(output));
                    item->setData(Qt::UserRole + 1, true);
                }
            }
        QTimer::singleShot(0, this, [this] { nextThumbnail(); });
    };
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [done](int, QProcess::ExitStatus) { done(); });
    connect(process, &QProcess::errorOccurred, this, [done](QProcess::ProcessError) { done(); });
    QTimer::singleShot(10000, process,
                       [process]
                       {
                           if (process->state() != QProcess::NotRunning)
                               process->kill();
                       });
    process->start(QCoreApplication::applicationFilePath(), args);
}

void RedesignShell::openSettings(int selectedPage)
{
    leaveFullscreen();
    if (settingsDialog)
    {
        static_cast<RedesignPreferences *>(settingsDialog)->selectPage(selectedPage);
        settingsDialog->show();
        settingsDialog->raise();
        settingsDialog->activateWindow();
        return;
    }
    settingsDialog = new RedesignPreferences(p_intf, window, selectedPage, [this] { applyTheme(); });
    connect(settingsDialog, &QObject::destroyed, this, [this] { settingsDialog = nullptr; });
    Reframe::reveal(settingsDialog);
}

/* An opt-in integration check drives the actual widgets and checks VLC state.
 * It runs only when the private CLI smoke option is supplied. */
void RedesignShell::runSmoke(const QString &reportPath)
{
    auto report = [this, reportPath](const QString &result)
    {
        QFile file(reportPath);
        if (file.open(QIODevice::WriteOnly))
            file.write(result.toUtf8());
        window->grab().save(reportPath + ".player.png");
        if (settingsDialog)
            settingsDialog->grab().save(reportPath + ".settings.png");
        THEDP->quit();
    };
    if (!THEMIM->getInput())
    {
        report("FAIL: input did not start");
        return;
    }
    const QStringList parts = elapsed->text().split(':');
    if (parts.size() != 3 || parts[0].toInt() != 0 || parts[1].toInt() != 0 || parts[2].toInt() > 30)
    {
        report("FAIL: elapsed-time units");
        return;
    }
    playButton->click();
    QTimer::singleShot(
        250, this,
        [this, report, reportPath]
        {
            auto input = THEMIM->getInput();
            if (!input || var_GetInteger(input, "state") != PAUSE_S)
            {
                report("FAIL: pause control");
                return;
            }
            playButton->click();
            seek->setValue(30000);
            QTimer::singleShot(
                250, this,
                [this, report, reportPath]
                {
                    auto input = THEMIM->getInput();
                    if (!input || var_GetInteger(input, "state") != PLAYING_S)
                    {
                        report("FAIL: resume control");
                        return;
                    }
                    if (std::abs(var_GetFloat(input, "position") - .3f) > .1f)
                    {
                        report("FAIL: seek control");
                        return;
                    }
                    showQueue();
                    auto queue = sidePanel ? sidePanel->findChild<QListWidget *>("queueItems") : nullptr;
                    if (!queue)
                    {
                        report("FAIL: redesigned queue control");
                        return;
                    }
                    sidePanel->show();
                    updateQueue();
                    if (queue->count() == 0)
                    {
                        report("FAIL: queue backend contents");
                        return;
                    }
                    sidePanel->hide();
                    openSettings(0);
                    openSettings(6);
                    auto nav = settingsDialog->findChild<QListWidget *>("settingsNav");
                    if (!nav || nav->currentRow() != 6)
                    {
                        report("FAIL: settings navigation");
                        return;
                    }
                    auto preamp = settingsDialog->findChild<QSlider *>("preampSlider");
                    if (!preamp)
                    {
                        report("FAIL: equalizer widget");
                        return;
                    }
                    preamp->setValue(73);
                    if (std::abs(config_GetFloat(p_intf, "equalizer-preamp") - 7.3f) > .05f)
                    {
                        report("FAIL: equalizer config type");
                        return;
                    }
                    auto eq = settingsDialog->findChild<QAbstractButton *>("filter:equalizer");
                    if (!eq)
                    {
                        report("FAIL: audio filter control");
                        return;
                    }
                    eq->setChecked(true);
                    char *chain = config_GetPsz(p_intf, "audio-filter");
                    const bool saved = QString::fromUtf8(chain ? chain : "").split(':').contains("equalizer");
                    free(chain);
                    if (!saved)
                    {
                        report("FAIL: audio filter persistence");
                        return;
                    }
                    openMediaSettings(7);
                    auto delay = sidePanel->findChild<QDoubleSpinBox *>("subtitleDelay");
                    if (!delay || !delay->isEnabled())
                    {
                        report("FAIL: subtitle control state");
                        return;
                    }
                    delay->setValue(.25);
                    if (var_GetInteger(THEMIM->getInput(), "spu-delay") != CLOCK_FREQ / 4)
                    {
                        report("FAIL: subtitle delay units");
                        return;
                    }
                    sidePanel->hide();
                    auto index = std::make_shared<int>(0);
                    auto capture = std::make_shared<std::function<void()>>();
                    *capture = [this, report, index, capture, reportPath]
                    {
                        if (*index < 13)
                        {
                            openSettings(*index);
                            settingsDialog->layout()->activate();
                            auto scroll = settingsDialog->findChildren<QScrollArea *>();
                            for (auto area : scroll)
                                if (area->isVisible() && area->horizontalScrollBar()->maximum() > 0)
                                {
                                    *capture = nullptr;
                                    QString detail;
                                    for (auto child : area->widget()->findChildren<QWidget *>())
                                        if (child->minimumSizeHint().width() > area->viewport()->width())
                                            detail += QString("\n%1 %2 minimum %3")
                                                          .arg(child->metaObject()->className(),
                                                               child->objectName())
                                                          .arg(child->minimumSizeHint().width());
                                    report(QString("FAIL: horizontal overflow on settings page %1, viewport "
                                                   "%2, content %3%4")
                                               .arg(*index)
                                               .arg(area->viewport()->width())
                                               .arg(area->widget()->width())
                                               .arg(detail));
                                    return;
                                }
                            settingsDialog->grab().save(reportPath + QString(".page%1.png").arg(*index));
                            ++*index;
                            QTimer::singleShot(100, this, *capture);
                            return;
                        }
                        openSettings(6);
                        auto preamp = settingsDialog->findChild<QSlider *>("preampSlider");
                        if (preamp)
                        {
                            auto parent = preamp->parentWidget();
                            while (parent && !qobject_cast<QScrollArea *>(parent))
                                parent = parent->parentWidget();
                            if (parent)
                                static_cast<QScrollArea *>(parent)->ensureWidgetVisible(
                                    settingsDialog->findChild<QWidget *>("equalizerPanel"), 20, 20);
                        }
                        *capture = nullptr;
                        runFlowSmoke(reportPath, report);
                    };
                    QTimer::singleShot(100, this, *capture);
                });
        });
}
