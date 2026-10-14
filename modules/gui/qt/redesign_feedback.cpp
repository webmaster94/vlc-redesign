/* Copyright 2026 webmaster94. GPL-3.0-or-later.
 * Opt-in regression checks, run only by the existing isolated smoke CLI. */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "redesign.hpp"
#include "redesign_seek.hpp"
#include "redesign_settings.hpp"
#include "redesign_controls.hpp"
#include "main_interface.hpp"
#include "input_manager.hpp"
#include "components/interface_widgets.hpp"
#include <vlc_configuration.h>
#include <vlc_aout.h>
#include <vlc_vout.h>
#include <QApplication>
#include <QSettings>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QListWidget>
#include <QToolButton>
#include <QPushButton>
#include <QComboBox>
#include <QDialog>
#include <QTabWidget>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QCursor>
#include <QKeyEvent>
#include <QTimer>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QPointer>
#include <QLabel>
#include <memory>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace
{
bool screenshotKey(QWidget *window)
{
    if (qgetenv("REFRAME_PRINT_PROBE") != "1")
        return true;
#ifdef Q_OS_WIN
    window->activateWindow();
    window->raise();
    const HWND handle = reinterpret_cast<HWND>(window->winId());
    SetForegroundWindow(handle);
    if (GetForegroundWindow() != handle)
        return false;
    INPUT input[2] = {};
    input[0].type = input[1].type = INPUT_KEYBOARD;
    input[0].ki.wVk = input[1].ki.wVk = VK_SNAPSHOT;
    input[1].ki.dwFlags = KEYEVENTF_KEYUP;
    if (SendInput(2, input, sizeof(INPUT)) != 2)
        return false;
    QEventLoop loop;
    QTimer::singleShot(150, &loop, &QEventLoop::quit);
    loop.exec();
    return true;
#else
    Q_UNUSED(window);
    return false;
#endif
}
bool dropFile(QListWidget *list, const QString &path)
{
    if (!list)
        return false;
    QMimeData mime;
    mime.setUrls(QList<QUrl>() << QUrl::fromLocalFile(path));
    QDragEnterEvent enter(QPoint(10, 10), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(list->viewport(), &enter);
    QDropEvent drop(QPointF(10, 10), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(list->viewport(), &drop);
    return drop.isAccepted() && list->count() == 1;
}
} // namespace
void RedesignShell::runFeedbackSmoke(const QString &path, std::function<void(const QString &)> report)
{
    char *raw = var_InheritString(p_intf, "qt-redesign-smoke-library");
    const QDir directory(qfu(raw ? raw : ""));
    free(raw);
    const auto songs = directory.entryList(QStringList() << "*.ogg", QDir::Files);
    if (songs.isEmpty())
    {
        report("FAIL: feedback checks require the generated music fixtures");
        return;
    }
    const QString song = directory.filePath(songs.first());
    auto phase = std::make_shared<int>(0);
    auto waited = std::make_shared<int>(0);
    auto next = std::make_shared<std::function<void()>>();
    auto dialog = std::make_shared<QPointer<QDialog>>();
    auto fail = [next, report](const QString &reason)
    {
        *next = nullptr;
        report("FAIL: " + reason);
    };
    *next = [this, path, directory, song, report, phase, waited, next, dialog, fail]
    {
        int delay = 450;
        if (*phase == 0)
        {
            if (settingsDialog)
                settingsDialog->hide();
            window->showNormal();
            showChrome();
            auto add = sidebar->findChild<QToolButton *>("pinItemButton");
            if (!add)
            {
                fail("sidebar plus button missing");
                return;
            }
            add->click();
            *dialog = window->findChild<QDialog *>("pinDialog");
        }
        else if (*phase == 1)
        {
            if (!*dialog || !dropFile((*dialog)->findChild<QListWidget *>("pinFiles"), song))
            {
                fail("pin dialog did not accept a local file drop");
                return;
            }
            (*dialog)->findChild<QPushButton *>("pinSave")->click();
            if (!getSettings()->value("Redesign/favorites").toStringList().contains(song))
            {
                fail("plus did not pin the selected file");
                return;
            }
            showLibrary("Home");
            applyLibraryView(false);
        }
        else if (*phase == 2)
        {
            auto mode = findChild<QToolButton *>("libraryViewButton");
            if (!mode || !homePanel || homePanel->findChildren<QListWidget *>("homeMedia").isEmpty())
            {
                fail("Home has no library rows to test");
                return;
            }
            mode->click();
            for (auto list : homePanel->findChildren<QListWidget *>("homeMedia"))
                if (list->viewMode() != QListView::ListMode)
                {
                    fail("library toggle left a Home row in tile mode");
                    return;
                }
            window->grab().save(path + ".feedback-home-list.png");
            mode->click();
            for (auto list : homePanel->findChildren<QListWidget *>("homeMedia"))
                if (list->viewMode() != QListView::IconMode)
                {
                    fail("Home rows did not return to tiles");
                    return;
                }
            openMedia(song);
            delay = 1000;
        }
        else if (*phase == 3)
        {
            auto timeline = dynamic_cast<RedesignSeek *>(seek);
            if (!timeline || !timeline->waveformEnabled() || !timeline->hasWaveform())
            {
                if (++*waited < 20)
                {
                    QTimer::singleShot(500, this, *next);
                    return;
                }
                fail("audio waveform was not generated automatically");
                return;
            }
            auto background = player->findChild<BackgroundWidget *>();
            if (!background || !background->isVisible() ||
                !background->accessibleDescription().contains("no album artwork"))
            {
                fail("audio without artwork did not show its music placeholder");
                return;
            }
            window->grab().save(path + ".feedback-audio.png");
            toggleMiniPlayer();
        }
        else if (*phase == 4)
        {
            auto control = miniPlayer ? miniPlayer->findChild<QToolButton *>("miniPlayPause") : nullptr;
            if (!control || control->property("glyph") != "pause")
            {
                fail("playing mini player did not show Pause");
                return;
            }
            control->click();
        }
        else if (*phase == 5)
        {
            auto control = miniPlayer->findChild<QToolButton *>("miniPlayPause");
            if (control->property("glyph") != "play" || THEMIM->getIM()->playingStatus() != PAUSE_S)
            {
                fail("mini player did not show Play after pausing");
                return;
            }
            control->click();
        }
        else if (*phase == 6)
        {
            if (miniPlayer->findChild<QToolButton *>("miniPlayPause")->property("glyph") != "pause")
            {
                fail("mini player did not show Pause after resuming");
                return;
            }
            miniPlayer->grab().save(path + ".feedback-mini.png");
            toggleMiniPlayer();
            QMetaObject::invokeMethod(window, "showResumePanel", Q_ARG(int64_t, int64_t(2000000)));
        }
        else if (*phase == 7)
        {
            auto prompt = window->resumePrompt();
            if (!prompt || !prompt->isVisible() || prompt->parentWidget() != player ||
                prompt->geometry().bottom() < player->height() - 90 ||
                qAbs(prompt->geometry().center().x() - player->rect().center().x()) > 2)
            {
                fail("Continue Watching was not centered above the playback controls");
                return;
            }
            window->grab().save(path + ".feedback-resume.png");
            QMetaObject::invokeMethod(window, "hideResumePanel");
            openMediaSettings(4);
            toggleFullscreen();
            window->activateWindow();
            window->raise();
            // Fullscreen changes native window geometry asynchronously on Windows.
            // Move into the panel after that change, then allow its hide animations to finish.
            QTimer::singleShot(200, this, [this]
            {
                QCursor::setPos(sidePanel->mapToGlobal(sidePanel->rect().center()));
                updateChromePointer();
            });
            delay = 800;
        }
        else if (*phase == 8)
        {
            if (!sidePanel->isVisible() || transport->isVisible())
            {
                window->grab().save(path + ".feedback-sidebar-failure.png");
                fail(QString("hovered sidebar did not take priority over playback controls (region %1, active %2, fullscreen %3, panel %4, transport %5)")
                     .arg(property("chromeRegion").toInt()).arg(window->isActiveWindow())
                     .arg(window->isFullScreen()).arg(sidePanel->isVisible()).arg(transport->isVisible()));
                return;
            }
            auto pin = sidePanel->findChild<QToolButton *>("pinPanelPin");
            if (!pin)
            {
                fail("sidebar pin missing");
                return;
            }
            pin->setChecked(true);
            QCursor::setPos(player->mapToGlobal(player->rect().center()));
            updateChromePointer();
            delay = 1400;
        }
        else if (*phase == 9)
        {
            if (!sidePanel->isVisible())
            {
                fail("pinned sidebar hid on pointer leave");
                return;
            }
            sidePanel->findChild<QToolButton *>("pinPanelPin")->setChecked(false);
            hideChrome();
            delay = 400;
        }
        else if (*phase == 10)
        {
            if (sidePanel->isVisible())
            {
                fail("unpinned sidebar did not hide after pointer leave");
                return;
            }
            QCursor::setPos(window->mapToGlobal(QPoint(window->width() / 2, window->height() - 8)));
            updateChromePointer();
            delay = 400;
        }
        else if (*phase == 11)
        {
            if (!transport->isVisible() || sidePanel->isVisible())
            {
                fail("playback controls did not take priority at their edge");
                return;
            }
            auto pin = transport->findChild<QToolButton *>("pinControlsPin");
            if (!pin)
            {
                fail("playback controls pin missing");
                return;
            }
            pin->setChecked(true);
            QCursor::setPos(player->mapToGlobal(player->rect().center()));
            hideChrome();
            delay = 400;
        }
        else if (*phase == 12)
        {
            if (!transport->isVisible())
            {
                fail("pinned playback controls hid");
                return;
            }
            transport->findChild<QToolButton *>("pinControlsPin")->setChecked(false);
            leaveFullscreen();
            delay = 500;
        }
        else if (*phase == 13)
        {
            if (window->isFullScreen() || !header->isVisible() || !transport->isVisible())
            {
                fail("leaving fullscreen did not restore controls");
                return;
            }
            if (!header->findChild<QToolButton *>("pinHeaderPin") ||
                !sidebar->findChild<QToolButton *>("pinSidebarPin"))
            {
                fail("title/navigation pins missing");
                return;
            }
            if (sidePanel)
                sidePanel->hide();
            showLibrary("Home");
            const qint64 previous = var_GetInteger(p_intf->obj.libvlc, "key-pressed");
            QKeyEvent print(QEvent::KeyPress, Qt::Key_Print, Qt::NoModifier);
            QApplication::sendEvent(files, &print);
            if (var_GetInteger(p_intf->obj.libvlc, "key-pressed") != previous)
            {
                fail("PrintScreen was routed into VLC hotkeys from the library");
                return;
            }
            window->toggleInterfaceFullScreen();
            if (!screenshotKey(window))
            {
                fail("could not send PrintScreen from the fullscreen library");
                return;
            }
            openSource(0);
            for (auto candidate : window->findChildren<QDialog *>("sourceDialog"))
                if (candidate->isVisible())
                    *dialog = candidate;
        }
        else if (*phase >= 14 && *phase <= 17)
        {
            const int kind = *phase - 14;
            if (!*dialog || !(*dialog)->isVisible() || (*dialog)->property("sourceKind") != kind ||
                !(*dialog)->findChildren<QTabWidget *>().isEmpty())
            {
                fail(QString("source %1 did not have its own tab-free dialog").arg(kind));
                return;
            }
            (*dialog)->grab().save(path + QString(".feedback-source%1.png").arg(kind));
            if (kind == 0)
            {
                if (!dropFile((*dialog)->findChild<QListWidget *>("sourceFiles"), song))
                {
                    fail("Open Files did not accept a local file drop");
                    return;
                }
                (*dialog)->findChild<QPushButton *>("sourceEnqueue")->click();
            }
            else
                (*dialog)->close();
            if (kind < 3)
            {
                openSource(kind + 1);
                for (auto candidate : window->findChildren<QDialog *>("sourceDialog"))
                    if (candidate->isVisible())
                        *dialog = candidate;
            }
            else
                openSettings(0);
        }
        else if (*phase == 18)
        {
            auto infos = settingsDialog->findChildren<QToolButton *>("settingInfo");
            bool nativeHelp = false;
            for (auto info : infos)
            {
                const QString key = info->property("settingKey").toString();
                const auto cfg = config_FindConfig(qtu(key));
                if (cfg && cfg->psz_longtext && *cfg->psz_longtext)
                    nativeHelp |= info->property("informationText").toString() == qtr(cfg->psz_longtext);
            }
            if (!nativeHelp)
            {
                fail("settings info icons did not retain complete native VLC explanations");
                return;
            }
            settingsDialog->grab().save(path + ".feedback-settings.png");
            settingsDialog->hide();
            showPlayer();
            openMediaSettings(4);
            auto combo = sidePanel->findChild<QComboBox *>("ui:visualizer");
            const int index = combo ? combo->findData("waveform") : -1;
            if (index < 0)
            {
                fail("waveform visualizer option missing");
                return;
            }
            combo->setCurrentIndex(index);
            QMetaObject::invokeMethod(combo, "activated", Q_ARG(int, index));
            delay = 1500;
        }
        else if (*phase == 19)
        {
            auto output = THEMIM->getVout();
            if (!output)
            {
                fail("selected waveform visualizer did not create an output");
                return;
            }
            const bool visual =
                var_Type(output, "reframe-visualization") && var_GetBool(output, "reframe-visualization");
            bool embedded = false;
            for (auto widget : player->findChildren<VideoWidget *>())
                embedded |= widget->isVisible();
            window->grab().save(path + ".feedback-visualizer.png");
            toggleFullscreen();
            if (!screenshotKey(window))
            {
                vlc_object_release(output);
                fail("could not send PrintScreen from the fullscreen player");
                return;
            }
            var_SetBool(output, "reframe-close-request", true);
            vlc_object_release(output);
            if (!embedded)
            {
                fail("audio visualization was not embedded in Reframe");
                return;
            }
            if (!visual)
            {
                fail("visualizer output was not identified for its close action");
                return;
            }
        }
        else if (*phase == 20)
        {
            auto output = THEMIM->getAout();
            char *value = output ? var_GetString(output, "visual") : nullptr;
            const QString visual = qfu(value ? value : "");
            free(value);
            if (output)
                vlc_object_release(output);
            if (!visual.isEmpty() || THEMIM->getIM()->playingStatus() != PLAYING_S)
            {
                fail("closing visualization did not disable it while audio kept playing");
                return;
            }
            if (!window->isFullScreen())
                toggleFullscreen();
            showChrome();
            if (sidePanel)
            {
                sidePanel->setProperty("chromeWasOpen", false);
                sidePanel->hide();
            }
            QCursor::setPos(transport->mapToGlobal(transport->rect().center()));
            controlButtons.value("settings")->click();
            delay = 350;
        }
        else
        {
            if (!sidePanel || !sidePanel->isVisible())
            {
                fail("panel opened from the controls hid before the pointer could reach it");
                return;
            }
            leaveFullscreen();
            *next = nullptr;
            report("PASS: existing playback/library/installer checks; music fallback and automatic audio "
                   "waveform; Home list/tiles; mini play/pause synchronization; bottom-centered resume "
                   "prompt; fullscreen hover priority, pins and exit restoration; PrintScreen not sent to "
                   "VLC; file/folder/disc/capture dialogs; plus-to-pin; full settings help; visualization "
                   "closes without stopping audio");
            return;
        }
        ++*phase;
        if (*next)
            QTimer::singleShot(delay, this, *next);
    };
    QTimer::singleShot(0, this, *next);
}
