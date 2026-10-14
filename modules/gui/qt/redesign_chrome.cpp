/* Copyright 2026 webmaster94. GPL-3.0-or-later. */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "redesign.hpp"
#include "redesign_controls.hpp"
#include "main_interface.hpp"
#include <QApplication>
#include <QCursor>
#include <QToolButton>
#include <QBoxLayout>
#include <QPropertyAnimation>
#include <QSettings>
#include <QTimer>
#include <QDateTime>
#include <vlc_configuration.h>

namespace
{
bool containsPointer(QWidget *widget, const QPoint &point)
{
    return widget && widget->isVisible() && widget->rect().contains(widget->mapFromGlobal(point));
}
void visibleChrome(QWidget *widget, bool show, bool horizontal = false)
{
    if (!widget)
        return;
    if (widget->property("chromeTarget").isValid() && widget->property("chromeTarget").toBool() == show)
        if (widget->isVisible() == show || !widget->findChildren<QPropertyAnimation *>().isEmpty())
            return;
    widget->setProperty("chromeTarget", show);
    if (!horizontal)
    {
        Reframe::animateHeight(widget, show, 300);
        return;
    }
    if (!widget->property("chromeWidth").isValid() && widget->isVisible() && widget->width() > 30)
        widget->setProperty("chromeWidth", widget->width());
    const int full =
        widget->property("chromeWidth").toInt() > 0 ? widget->property("chromeWidth").toInt() : 196;
    for (auto animation : widget->findChildren<QPropertyAnimation *>(QString(), Qt::FindDirectChildrenOnly))
        if (animation->propertyName() == "maximumWidth")
        {
            animation->stop();
            delete animation;
        }
    const int start = widget->isVisible() ? widget->width() : 0;
    widget->setMinimumWidth(0);
    widget->setMaximumWidth(start);
    widget->show();
    auto animation = new QPropertyAnimation(widget, "maximumWidth", widget);
    animation->setObjectName("chromeWidthMotion");
    animation->setDuration(208);
    animation->setStartValue(start);
    animation->setEndValue(show ? full : 0);
    animation->setEasingCurve(Reframe::transitionCurve());
    QObject::connect(animation, &QPropertyAnimation::finished, widget,
                     [widget, show, full]
                     {
                         widget->setVisible(show);
                         if (show)
                             widget->setFixedWidth(full);
                     });
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}
} // namespace
QToolButton *RedesignShell::chromePin(QWidget *widget, const QString &key)
{
    auto pin = button("pin");
    pin->setCheckable(true);
    pin->setProperty("chromePin", true);
    pin->setObjectName(key + "Pin");
    const QString setting = "Redesign/" + key;
    pin->setProperty("pinSetting", setting);
    pin->setChecked(getSettings()->value(setting, false).toBool());
    widget->setProperty("chromePinned", pin->isChecked());
    auto changed = [this, widget, pin, setting](bool checked)
    {
        widget->setProperty("chromePinned", checked);
        getSettings()->setValue(setting, checked);
        pin->setToolTip(checked ? "Unpin. Hide when the pointer leaves." : "Pin. Keep this panel visible.");
        pin->setAccessibleName(pin->toolTip());
        pin->setIcon(
            Reframe::icon("pin", checked ? palette().highlight().color() : palette().text().color()));
        if (checked && (widget != sidePanel || widget->property("chromeWasOpen").toBool()))
            visibleChrome(widget, true, widget == sidePanel || widget == sidebar);
    };
    changed(pin->isChecked());
    connect(pin, &QToolButton::toggled, widget, changed);
    return pin;
}
void RedesignShell::showChrome()
{
    visibleChrome(header, true);
    visibleChrome(transport, true);
    if (!window->isFullScreen() && !minimal)
    {
        visibleChrome(sidebar, true, true);
        if (sidePanel && sidePanel->property("chromeWasOpen").toBool())
            visibleChrome(sidePanel, true, true);
    }
    if (chromeCanHide())
        chromeTimer->start(qMax(1000, int(config_GetInt(p_intf, "mouse-hide-timeout"))));
    else
        chromeTimer->stop();
}
void RedesignShell::hideChrome()
{
    if (!chromeCanHide())
        return;
    const QPoint point = QCursor::pos();
    if (navigationAnimation && navigationAnimation->state() == QAbstractAnimation::Running)
        navigationAnimation->stop();
    if (!header->property("chromePinned").toBool() && !containsPointer(header, point))
        visibleChrome(header, false);
    if (!transport->property("chromePinned").toBool() && !containsPointer(transport, point))
        visibleChrome(transport, false);
    if (sidePanel && sidePanel->property("chromeWasOpen").toBool() &&
        !sidePanel->property("chromePinned").toBool() && !containsPointer(sidePanel, point))
        visibleChrome(sidePanel, false, true);
    if ((window->isFullScreen() || minimal) && !sidebar->property("chromePinned").toBool() &&
        !containsPointer(sidebar, point))
        visibleChrome(sidebar, false, true);
}
void RedesignShell::updateChromePointer()
{
    if (!window->isActiveWindow() || !chromeCanHide())
        return;
    const QPoint global = QCursor::pos(), point = window->mapFromGlobal(global);
    if (!window->rect().contains(point))
        return;
    const bool full = window->isFullScreen() || minimal;
    const bool overTransport = containsPointer(transport, global);
    const bool overHeader = containsPointer(header, global);
    if (sidePanel && sidePanel->property("chromeWasOpen").toBool() &&
        property("panelGraceUntil").toLongLong() > QDateTime::currentMSecsSinceEpoch() &&
        !containsPointer(sidePanel, global))
    {
        visibleChrome(sidePanel, true, true);
        if (overTransport)
            visibleChrome(transport, true);
        return;
    }
    if (containsPointer(sidePanel, global))
        setProperty("panelGraceUntil", 0);
    const bool right = !overTransport && !overHeader && sidePanel &&
                       sidePanel->property("chromeWasOpen").toBool() &&
                       (containsPointer(sidePanel, global) ||
                        (point.x() >= window->width() - 24 && point.y() < window->height() - 80));
    const bool left =
        !overTransport && !overHeader && full &&
        (containsPointer(sidebar, global) || (point.x() < 24 && point.y() < window->height() - 80));
    const bool bottom =
        !right && !left && (containsPointer(transport, global) || point.y() > window->height() - 80);
    const bool top = !right && !left && !bottom && (containsPointer(header, global) || point.y() < 54);
    const int region = right ? 4 : left ? 3 : bottom ? 2 : top ? 1 : 0;
    if (region)
    {
        chromeTimer->stop();
        visibleChrome(header, top || header->property("chromePinned").toBool());
        visibleChrome(transport, bottom || transport->property("chromePinned").toBool());
        if (sidePanel && sidePanel->property("chromeWasOpen").toBool())
            visibleChrome(sidePanel, right || sidePanel->property("chromePinned").toBool(), true);
        if (full)
            visibleChrome(sidebar, left || sidebar->property("chromePinned").toBool(), true);
    }
    else if (!chromeTimer->isActive())
        chromeTimer->start(qMax(500, int(config_GetInt(p_intf, "mouse-hide-timeout"))));
    setProperty("chromeRegion", region);
}
void RedesignShell::positionResumePrompt()
{
    auto prompt = window->resumePrompt();
    if (!prompt || !player->isVisible())
        return;
    const int width = qMin(560, qMax(1, player->width() - 32));
    prompt->resize(width, 52);
    prompt->move((player->width() - width) / 2, qMax(0, player->height() - 76));
    prompt->raise();
}
void RedesignShell::showSidePanel()
{
    if (!sidePanel)
        return;
    sidePanel->setProperty("chromeWasOpen", true);
    setProperty("panelGraceUntil", QDateTime::currentMSecsSinceEpoch() + 1200);
    visibleChrome(sidePanel, true, true);
    if (chromeCanHide())
        chromeTimer->start(qMax(1200, int(config_GetInt(p_intf, "mouse-hide-timeout"))));
}
