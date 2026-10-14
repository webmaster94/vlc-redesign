/* Copyright 2026 webmaster94. GPL-3.0-or-later.
 * Native Juxtopposed VLC Redesign interactions. CC BY 4.0 design attribution
 * and modified-source licensing are recorded in doc/redesign/licensing.md. */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "redesign.hpp"
#include "redesign_controls.hpp"
#include "redesign_settings.hpp"
#include "main_interface.hpp"
#include "input_manager.hpp"
#include "components/interface_widgets.hpp"
#include <QMouseEvent>
#include "actions_manager.hpp"
#include "dialogs_provider.hpp"
#include "dialogs/firstrun.hpp"
#include "recents.hpp"
#include <vlc_configuration.h>
#include <vlc_plugin.h>
#include <vlc_input.h>
#include <vlc_input_item.h>
#include <vlc_aout.h>
#include <vlc_actions.h>
#include "redesign_seek.hpp"
#include <QApplication>
#include <QBoxLayout>
#include <QListWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QStackedWidget>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QSettings>
#include <QSignalBlocker>
#include <QMenu>
#include <QUrl>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QUuid>
#include <QDesktopServices>
#include <QMessageBox>
#include <QInputDialog>
#include <QMimeData>
#include <QDropEvent>
#include <QKeyEvent>
#include <QTimer>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVersionNumber>
#include <QVariantAnimation>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QSlider>
#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QCheckBox>
#include <QDesktopWidget>
#include <QSizeGrip>
#include <QCursor>
#include <memory>
#include <functional>
#include <algorithm>

namespace
{
class OnboardingPreview : public QWidget
{
    QPixmap image;

  public:
    explicit OnboardingPreview(QWidget *parent = nullptr) : QWidget(parent)
    {
        setObjectName("onboardingPreview");
        setMinimumSize(240, 180);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }
    void setImage(const QPixmap &pixmap)
    {
        image = pixmap;
        update();
    }

  protected:
    void paintEvent(QPaintEvent *) override
    {
        if (image.isNull())
            return;
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        QSize size = image.size();
        size.scale(contentsRect().size() - QSize(8, 8), Qt::KeepAspectRatio);
        QRect frame(QPoint(), size);
        frame.moveCenter(contentsRect().center());
        painter.drawPixmap(frame, image);
        painter.setPen(QPen(palette().mid().color(), 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(frame.adjusted(-1, -1, 1, 1), 7, 7);
    }
};
class AccentSwatch : public QAbstractButton
{
    QColor color;

  public:
    AccentSwatch(const QString &value, const QString &name, QWidget *parent)
        : QAbstractButton(parent), color(value)
    {
        setObjectName("accentSwatch");
        setProperty("accentColor", value);
        setAccessibleName(name);
        setToolTip(name);
        setCheckable(true);
        setCursor(Qt::PointingHandCursor);
        setFixedSize(52, 52);
    }

  protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawEllipse(QRectF(8, 8, 36, 36));
        if (isChecked() || hasFocus())
        {
            painter.setPen(QPen(palette().windowText().color(), isChecked() ? 2 : 1));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(QRectF(3, 3, 46, 46));
        }
        if (isChecked())
        {
            painter.setPen(QPen(color.lightness() > 160 ? QColor("#111111") : QColor("#ffffff"), 2.5,
                                Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPolyline(QPolygonF() << QPointF(18, 26) << QPointF(24, 32) << QPointF(34, 21));
        }
    }
};
QString timeText(qint64 seconds)
{
    return QString("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QChar('0'));
}
QStringList selectedPaths(QListWidget *list)
{
    QStringList result;
    for (auto item : list->selectedItems())
        result << item->data(Qt::UserRole).toString();
    return result;
}
QStringList queuePaths(playlist_t *pl)
{
    QStringList paths;
    vlc_playlist_locker lock(pl);
    std::function<void(playlist_item_t *)> visit = [&](playlist_item_t *node)
    {
        if (node->i_children < 0)
        {
            char *uri = input_item_GetURI(node->p_input);
            paths << qfu(uri);
            free(uri);
        }
        else
            for (int i = 0; i < node->i_children; ++i)
                visit(node->pp_children[i]);
    };
    visit(pl->p_playing);
    return paths;
}
// Reordering uses the 600 ms INOUT_CUBIC movement in the design.
class ControlMotionDelegate : public QStyledItemDelegate
{
  public:
    QHash<QString, QPoint> previous;
    qreal progress = 1;
    explicit ControlMotionDelegate(QObject *parent) : QStyledItemDelegate(parent) {}
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        auto view = static_cast<QListWidget *>(parent());
        auto copy = option;
        const QString key = index.data().toString();
        if (progress < 1 && previous.contains(key))
        {
            const QPoint old = view->viewport()->mapFromGlobal(previous.value(key));
            copy.rect.moveTopLeft(old + (option.rect.topLeft() - old) * progress);
        }
        QStyledItemDelegate::paint(painter, copy, index);
    }
};
class ControlList : public QListWidget
{
  public:
    explicit ControlList(QWidget *parent) : QListWidget(parent)
    {
        setProperty("controlDropZone", true);
        setItemDelegate(new ControlMotionDelegate(this));
    }
    void dropEvent(QDropEvent *event) override
    {
        const auto lists = window()->findChildren<QListWidget *>();
        QHash<QString, QPoint> positions;
        for (auto list : lists)
            if (list->property("controlDropZone").toBool())
                for (int i = 0; i < list->count(); ++i)
                    positions[list->item(i)->text()] =
                        list->viewport()->mapToGlobal(list->visualItemRect(list->item(i)).topLeft());
        QListWidget::dropEvent(event);
        // The originating view removes moved rows after dropEvent returns.
        QTimer::singleShot(
            0, this,
            [lists, positions]
            {
                for (auto list : lists)
                    if (list->property("controlDropZone").toBool())
                    {
                        for (auto old : list->findChildren<QVariantAnimation *>("controlMotion"))
                        {
                            old->stop();
                            delete old;
                        }
                        auto delegate = static_cast<ControlMotionDelegate *>(list->itemDelegate());
                        delegate->previous = positions;
                        auto animation = new QVariantAnimation(list);
                        animation->setObjectName("controlMotion");
                        animation->setStartValue(0.);
                        animation->setEndValue(1.);
                        animation->setDuration(600);
                        animation->setEasingCurve(Reframe::transitionCurve());
                        QObject::connect(animation, &QVariantAnimation::valueChanged, list,
                                         [list, delegate](const QVariant &value)
                                         {
                                             delegate->progress = value.toReal();
                                             list->viewport()->update();
                                         });
                        animation->start(QAbstractAnimation::DeleteWhenStopped);
                    }
            });
    }
};
void controlItems(QListWidget *list, const QStringList &keys)
{
    const QHash<QString, QString> names = {{"elapsed", "Elapsed time"},
                                           {"duration", "Duration"},
                                           {"seek", "Seekbar"},
                                           {"volumeSlider", "Volume slider"},
                                           {"fileName", "File name"},
                                           {"artistName", "Artist name"},
                                           {"folderName", "Folder name"},
                                           {"playlistName", "Playlist name"},
                                           {"back10", "Back 10 seconds"},
                                           {"back15", "Back 15 seconds"},
                                           {"forward10", "Forward 10 seconds"},
                                           {"forward15", "Forward 15 seconds"},
                                           {"mini", "Mini player"},
                                           {"frame", "Next frame"},
                                           {"hdr", "HDR"}};
    for (const auto &key : keys)
    {
        QString label = names.value(key, key);
        if (!label.isEmpty())
            label[0] = label[0].toUpper();
        auto item = new QListWidgetItem(Reframe::icon(key), label, list);
        item->setData(Qt::UserRole, key);
        item->setToolTip(label);
    }
}
class FileList : public QListWidget
{
  public:
    explicit FileList(QWidget *p = nullptr) : QListWidget(p)
    {
        setAcceptDrops(true);
        setDragDropMode(QAbstractItemView::InternalMove);
        setDefaultDropAction(Qt::MoveAction);
        setSelectionMode(QAbstractItemView::ExtendedSelection);
    }
    void dropEvent(QDropEvent *e) override
    {
        if (e->mimeData()->hasUrls())
        {
            for (const auto &url : e->mimeData()->urls())
            {
                if (!url.isLocalFile())
                    continue;
                auto path = url.toLocalFile();
                auto item = new QListWidgetItem(QFileInfo(path).fileName(), this);
                item->setData(Qt::UserRole, path);
            }
            e->acceptProposedAction();
        }
        else
            QListWidget::dropEvent(e);
    }
};
void fileItems(QListWidget *list, const QStringList &paths)
{
    for (const auto &path : paths)
    {
        const QUrl url(path);
        const QString local = url.isLocalFile() ? url.toLocalFile() : path;
        auto item = new QListWidgetItem(
            QFileInfo(local).fileName().isEmpty() ? path : QFileInfo(local).fileName(), list);
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);
    }
}
QString saveCollection(QSettings *settings, QVariantMap data, const QString &previous = QString())
{
    QString id = previous.isEmpty() ? QUuid::createUuid().toString() : previous;
    auto ids = settings->value("Redesign/collections").toStringList();
    if (!ids.contains(id))
    {
        ids << id;
        settings->setValue("Redesign/collections", ids);
    }
    settings->setValue("Redesign/collection/" + id, data);
    return id;
}
} // namespace

void RedesignShell::openMediaSettings(int page)
{
    showPlayer();
    if (sidePanel)
    {
        auto preferences = dynamic_cast<RedesignPreferences *>(sidePanel);
        if (preferences)
        {
            if (preferences->currentPage() == page && sidePanel->isVisible())
            {
                sidePanel->hide();
                return;
            }
            preferences->selectPage(page);
            sidePanel->show();
            return;
        }
        bodyLayout->removeWidget(sidePanel);
        delete sidePanel;
        sidePanel = nullptr;
    }
    auto prefs = new RedesignPreferences(
        p_intf, window, page, [this] { applyTheme(); }, true);
    prefs->setWindowFlags(Qt::Widget);
    prefs->setParent(this);
    prefs->setFixedWidth(400);
    sidePanel = prefs;
    bodyLayout->addWidget(sidePanel);
    connect(prefs, &QObject::destroyed, this,
            [this, prefs]
            {
                if (sidePanel == prefs)
                    sidePanel = nullptr;
            });
    prefs->show();
    Reframe::animateWidth(prefs, 400, 104);
}
void RedesignShell::openTracks(int selected)
{
    showPlayer();
    if (sidePanel)
    {
        bodyLayout->removeWidget(sidePanel);
        delete sidePanel;
        sidePanel = nullptr;
    }
    auto panel = new QWidget(this);
    panel->setObjectName("trackPicker");
    panel->setFixedWidth(360);
    sidePanel = panel;
    auto layout = new QVBoxLayout(panel);
    layout->setContentsMargins(16, 8, 16, 20);
    layout->setSpacing(20);
    auto stack = new QStackedWidget;
    auto tabs = new QWidget(panel);
    auto tabLayout = new QHBoxLayout(tabs);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->setSpacing(0);
    auto tabGroup = new QButtonGroup(tabs);
    for (int page = 5; page <= 7; ++page)
    {
        const QString label = QStringList({"Video", "Audio", "Subtitles"})[page - 5];
        auto tab = new QPushButton(label);
        tab->setObjectName("mediaTab");
        tab->setIcon(Reframe::icon(page == 5 ? "videos" : page == 6 ? "audio" : "subtitles"));
        tab->setIconSize(QSize(20, 20));
        tab->setCheckable(true);
        tab->setChecked(selected == page);
        tabGroup->addButton(tab);
        tabLayout->addWidget(tab, 1);
        connect(tab, &QPushButton::clicked, stack, [stack, page] { stack->setCurrentIndex(page - 5); });
    }
    auto top = new QHBoxLayout;
    top->setSpacing(4);
    top->addWidget(tabs, 1);
    auto close = button("close");
    close->setToolTip("Close track picker");
    top->addWidget(close);
    connect(close, &QToolButton::clicked, panel, &QWidget::hide);
    layout->addLayout(top);
    layout->addWidget(stack, 1);
    for (int page = 5; page <= 7; ++page)
    {
        auto widget = new QWidget;
        auto controls = new QVBoxLayout(widget);
        controls->setContentsMargins(0, 0, 0, 0);
        controls->setSpacing(16);
        controls->addWidget(new QLabel("Embedded Tracks"));
        auto list = new QListWidget;
        list->setObjectName("trackChoices");
        list->setItemDelegate(Reframe::choiceDelegate(list));
        list->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        list->setFixedHeight(44);
        controls->addWidget(list);
        auto update = [this, list, page]
        {
            auto input = THEMIM->getInput();
            if (!input)
            {
                list->clear();
                list->setFixedHeight(44);
                new QListWidgetItem("No media playing", list);
                return;
            }
            const char *variable = page == 5 ? "video-es" : page == 6 ? "audio-es" : "spu-es";
            vlc_value_t values, texts;
            if (var_Change(input, variable, VLC_VAR_GETCHOICES, &values, &texts))
                return;
            const int active = var_GetInteger(input, variable);
            QString signature = QString::number(quintptr(input)) + QString::number(active);
            for (int i = 0; i < values.p_list->i_count; ++i)
                signature += QString::number(values.p_list->p_values[i].i_int) +
                             qfu(texts.p_list->p_values[i].psz_string);
            if (list->property("signature").toString() == signature)
            {
                var_FreeList(&values, &texts);
                return;
            }
            list->setProperty("signature", signature);
            list->clear();
            for (int i = 0; i < values.p_list->i_count; ++i)
            {
                auto item = new QListWidgetItem(qfu(texts.p_list->p_values[i].psz_string), list);
                item->setData(Qt::UserRole, int(values.p_list->p_values[i].i_int));
                if (item->data(Qt::UserRole).toInt() == active)
                    list->setCurrentItem(item);
            }
            var_FreeList(&values, &texts);
            list->setFixedHeight(qBound(44, list->count() * 44, 220));
        };
        update();
        auto timer = new QTimer(list);
        timer->setInterval(400);
        connect(timer, &QTimer::timeout, list, update);
        timer->start();
        connect(THEMIM, &MainInputManager::inputChanged, list, [update](bool) { update(); });
        connect(list, &QListWidget::itemClicked, this,
                [this, page](QListWidgetItem *item)
                {
                    auto input = THEMIM->getInput();
                    if (input)
                        var_SetInteger(input,
                                       page == 5   ? "video-es"
                                       : page == 6 ? "audio-es"
                                                   : "spu-es",
                                       item->data(Qt::UserRole).toInt());
                });
        if (page == 6)
        {
            auto outputHeader = new QHBoxLayout;
            outputHeader->addWidget(new QLabel("Output Device"), 1);
            auto reload = button("replay");
            reload->setToolTip("Refresh output devices");
            outputHeader->addWidget(reload);
            controls->addLayout(outputHeader);
            auto devices = new QListWidget;
            devices->setObjectName("outputDeviceChoices");
            devices->setItemDelegate(Reframe::choiceDelegate(devices));
            devices->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            controls->addWidget(devices);
            auto refreshDevices = [this, devices]
            {
                devices->clear();
                auto output = THEMIM->getAout();
                if (!output)
                {
                    new QListWidgetItem("Start playback to select a device", devices);
                    devices->setFixedHeight(44);
                    return;
                }
                char *active = aout_DeviceGet(output);
                char **ids, **names;
                int count = aout_DevicesList(output, &ids, &names);
                for (int i = 0; i < count; ++i)
                {
                    auto item = new QListWidgetItem(qfu(names[i]), devices);
                    item->setData(Qt::UserRole, qfu(ids[i]));
                    if (qfu(active ? active : "") == qfu(ids[i]))
                        devices->setCurrentItem(item);
                    free(ids[i]);
                    free(names[i]);
                }
                if (count > 0)
                {
                    free(ids);
                    free(names);
                }
                free(active);
                if (count <= 0)
                {
                    auto item = new QListWidgetItem("Default output device", devices);
                    item->setData(Qt::UserRole, QString());
                    devices->setCurrentItem(item);
                }
                devices->setFixedHeight(qBound(44, devices->count() * 44, 220));
                vlc_object_release(output);
            };
            refreshDevices();
            connect(reload, &QToolButton::clicked, devices, refreshDevices);
            connect(THEMIM, &MainInputManager::inputChanged, devices,
                    [refreshDevices](bool) { refreshDevices(); });
            connect(devices, &QListWidget::itemClicked, this,
                    [this](QListWidgetItem *item)
                    {
                        auto output = THEMIM->getAout();
                        if (output && item->data(Qt::UserRole).isValid())
                        {
                            aout_DeviceSet(output, qtu(item->data(Qt::UserRole).toString()));
                        }
                        if (output)
                            vlc_object_release(output);
                    });
        }
        else if (page == 7)
        {
            controls->addWidget(new QLabel("External Tracks"));
            auto add = new QPushButton("Add Subtitle Tracks");
            controls->addWidget(add);
            connect(add, &QPushButton::clicked, this,
                    [this]
                    {
                        auto paths =
                            QFileDialog::getOpenFileNames(this, "Add Subtitle Tracks", QString(),
                                                          "Subtitles (*.srt *.ass *.ssa *.vtt *.sub *.idx)");
                        auto input = THEMIM->getInput();
                        auto item = THEMIM->currentInputItem();
                        if (!input || !item)
                            return;
                        char *raw = input_item_GetURI(item);
                        const QString uri = qfu(raw);
                        free(raw);
                        const QString path = QUrl(uri).isLocalFile() ? QUrl(uri).toLocalFile() : uri;
                        auto bindings = getSettings()->value("Redesign/bindings/" + path).toStringList();
                        for (const auto &file : paths)
                        {
                            input_AddSlave(input, SLAVE_TYPE_SPU, qtu(QUrl::fromLocalFile(file).toString()),
                                           true, true, false);
                            if (!bindings.contains(file))
                                bindings << file;
                        }
                        getSettings()->setValue("Redesign/bindings/" + path, bindings);
                        if (auto slider = dynamic_cast<RedesignSeek *>(seek))
                            slider->refreshMedia();
                    });
        }
        auto preferences = new QPushButton("Show Preferences");
        preferences->setObjectName("trackPreferencesButton");
        controls->addStretch();
        controls->addWidget(preferences);
        connect(preferences, &QPushButton::clicked, this, [this, page] { openMediaSettings(page); });
        stack->addWidget(widget);
    }
    stack->setCurrentIndex(selected - 5);
    bodyLayout->addWidget(panel);
    panel->show();
    Reframe::animateWidth(panel, 360, 104);
}
void RedesignShell::applyMediaBindings()
{
    auto input = THEMIM->getInput();
    auto item = THEMIM->currentInputItem();
    if (!input || !item)
    {
        bindingInput.clear();
        return;
    }
    char *raw = input_item_GetURI(item);
    const QString uri = qfu(raw ? raw : "");
    free(raw);
    const QString id = uri + QString::number(quintptr(input));
    if (bindingInput == id)
        return;
    bindingInput = id;
    const QString path = QUrl(uri).isLocalFile() ? QUrl(uri).toLocalFile() : uri;
    QStringList attached = window->property("activeSlaveInput").toString() == id
                               ? window->property("activeSlaveUris").toStringList()
                               : QStringList();
    window->setProperty("activeSlaveInput", id);
    QVariantMap defaultsMap;
    const auto precise = getSettings()->value("Redesign/preciseSubtitleDelay");
    if (precise.isValid() && qint64(precise.toDouble()) == config_GetInt(p_intf, "sub-delay"))
        defaultsMap["sub-delay"] = precise;
    const auto localKeys = window->property("reframeLocalKeys").toStringList();
    for (const auto &key : localKeys)
    {
        if (key == "sub-delay" && defaultsMap.contains(key))
            continue;
        auto cfg = config_FindConfig(qtu(key));
        if (!cfg)
            continue;
        const int type = cfg->i_type & 0xF0;
        if (type == CONFIG_ITEM_STRING)
        {
            char *value = config_GetPsz(p_intf, qtu(key));
            defaultsMap[key] = qfu(value ? value : "");
            free(value);
        }
        else if (type == CONFIG_ITEM_FLOAT)
            defaultsMap[key] = config_GetFloat(p_intf, qtu(key));
        else
            defaultsMap[key] = QVariant::fromValue<qlonglong>(config_GetInt(p_intf, qtu(key)));
    }
    if (!defaultsMap.isEmpty())
    {
        auto preferences = new RedesignPreferences(
            p_intf, window, 4, [this] { applyTheme(); }, true);
        preferences->restore(defaultsMap);
        preferences->deleteLater();
        window->setProperty("reframeLocalKeys", QStringList());
    }
    for (int kind = 0; kind < 2; ++kind)
    {
        const QString prefix = kind ? "Redesign/audioBindings/" : "Redesign/bindings/";
        auto tracks = getSettings()->value(prefix + path).toStringList();
        if (tracks.isEmpty())
            tracks = getSettings()->value(prefix + uri).toStringList();
        for (const auto &track : tracks)
        {
            const QString source = QUrl::fromUserInput(track, QString(), QUrl::AssumeLocalFile).toString();
            if (!attached.contains(source) && input_AddSlave(input, kind ? SLAVE_TYPE_AUDIO : SLAVE_TYPE_SPU,
                                                             qtu(source), false, true, false) == VLC_SUCCESS)
                attached << source;
        }
    }
    window->setProperty("activeSlaveUris", attached);
    const QString folder = QFileInfo(path).absolutePath();
    const auto defaults = getSettings()->value("Redesign/mediaDefaults/" + folder).toMap();
    if (!defaults.isEmpty())
    {
        auto preferences = new RedesignPreferences(
            p_intf, window, 4, [this] { applyTheme(); }, true);
        preferences->restore(defaults);
        preferences->deleteLater();
    }
}
void RedesignShell::toggleLibraryDock()
{
    if (libraryDock)
    {
        bodyLayout->removeWidget(libraryDock);
        delete libraryDock;
        libraryDock = nullptr;
        return;
    }
    showPlayer();
    libraryDock = new QWidget(this);
    libraryDock->setObjectName("libraryDock");
    libraryDock->setFixedWidth(376);
    auto layout = new QVBoxLayout(libraryDock);
    layout->setContentsMargins(8, 8, 8, 8);
    auto header = new QHBoxLayout;
    auto close = button("close");
    auto name = new QLabel("Library");
    header->addWidget(name, 1);
    header->addWidget(close);
    layout->addLayout(header);
    auto search = new QLineEdit;
    search->setPlaceholderText("Search Library");
    layout->addWidget(search);
    auto list = new QListWidget;
    list->setObjectName("compactLibrary");
    layout->addWidget(list, 1);
    for (int i = 0; i < files->count(); ++i)
        list->addItem(files->item(i)->clone());
    connect(list, &QListWidget::itemDoubleClicked, this,
            [this, list, name](QListWidgetItem *item)
            {
                auto path = item->data(Qt::UserRole).toString();
                if (QFileInfo(path).isDir())
                {
                    browse(path);
                    showPlayer();
                    list->clear();
                    for (int i = 0; i < files->count(); ++i)
                        list->addItem(files->item(i)->clone());
                    name->setText(QFileInfo(path).fileName());
                }
                else
                    openMedia(path);
            });
    connect(search, &QLineEdit::textChanged, list,
            [list](const QString &value)
            {
                for (int i = 0; i < list->count(); ++i)
                    list->item(i)->setHidden(!list->item(i)->text().contains(value, Qt::CaseInsensitive));
            });
    connect(close, &QToolButton::clicked, this, [this] { toggleLibraryDock(); });
    bodyLayout->insertWidget(1, libraryDock);
    Reframe::animateWidth(libraryDock, 376, 208);
}
void RedesignShell::showQueue(int selected)
{
    showPlayer();
    if (sidePanel && sidePanel->objectName() == "queuePanel")
    {
        auto tabs = sidePanel->findChild<QStackedWidget *>("queueTabs");
        tabs->setCurrentIndex(selected);
        for (auto button : sidePanel->findChild<QWidget *>("queueSelectors")->findChildren<QPushButton *>())
            button->setChecked(button->text() ==
                               QStringList({"Queue", "Chapters", "Bookmarks"}).value(selected));
        sidePanel->show();
        return;
    }
    if (sidePanel)
    {
        bodyLayout->removeWidget(sidePanel);
        delete sidePanel;
        sidePanel = nullptr;
    }
    sidePanel = new QWidget(this);
    sidePanel->setObjectName("queuePanel");
    sidePanel->setFixedWidth(360);
    auto layout = new QVBoxLayout(sidePanel);
    layout->setContentsMargins(12, 8, 12, 12);
    layout->setSpacing(16);
    auto close = button("close");
    auto tabs = Reframe::segmented(
        QStringList() << "Queue"
                      << "Chapters"
                      << "Bookmarks",
        QStringList({"Queue", "Chapters", "Bookmarks"})[selected], sidePanel,
        [this](const QString &value)
        {
            auto stack = sidePanel->findChild<QStackedWidget *>("queueTabs");
            stack->setCurrentIndex(QStringList({"Queue", "Chapters", "Bookmarks"}).indexOf(value));
            updateQueue();
        });
    tabs->setObjectName("queueSelectors");
    auto top = new QHBoxLayout;
    top->addWidget(tabs, 1);
    top->addWidget(close);
    layout->addLayout(top);
    connect(close, &QToolButton::clicked, this, [this] { sidePanel->hide(); });
    auto stack = new QStackedWidget;
    stack->setObjectName("queueTabs");
    layout->addWidget(stack, 1);
    for (int i = 0; i < 3; ++i)
    {
        auto page = new QWidget;
        auto body = new QVBoxLayout(page);
        body->setContentsMargins(0, 0, 0, 0);
        auto list = new QListWidget;
        list->setObjectName(i == 0 ? "queueItems" : i == 1 ? "chapterItems" : "bookmarkItems");
        list->setSpacing(6);
        list->setSelectionMode(QAbstractItemView::ExtendedSelection);
        body->addWidget(list, 1);
        if (i == 0)
        {
            auto summary = new QLabel;
            summary->setObjectName("queueSummary");
            body->insertWidget(0, summary);
            list->setDragDropMode(QAbstractItemView::InternalMove);
            list->setDefaultDropAction(Qt::MoveAction);
            connect(list->model(), &QAbstractItemModel::rowsMoved, list,
                    [this, list](const QModelIndex &, int, int, const QModelIndex &, int)
                    {
                        vlc_playlist_locker lock(THEPL);
                        for (int row = 0; row < list->count(); ++row)
                        {
                            auto item =
                                playlist_ItemGetById(THEPL, list->item(row)->data(Qt::UserRole).toInt());
                            if (item)
                                playlist_TreeMove(THEPL, item, THEPL->p_playing, row);
                        }
                    });
            connect(list, &QListWidget::itemDoubleClicked, this,
                    [this](QListWidgetItem *row)
                    {
                        vlc_playlist_locker lock(THEPL);
                        auto item = playlist_ItemGetById(THEPL, row->data(Qt::UserRole).toInt());
                        if (item)
                            playlist_Control(THEPL, PLAYLIST_VIEWPLAY, pl_Locked, THEPL->p_playing, item);
                    });
            auto buttons = new QHBoxLayout;
            auto add = new QPushButton("Add Files");
            auto remove = new QPushButton("Remove");
            auto save = new QPushButton("Save Playlist");
            buttons->addWidget(add);
            buttons->addWidget(remove);
            buttons->addWidget(save);
            body->addLayout(buttons);
            connect(add, &QPushButton::clicked, this,
                    [this]
                    {
                        auto paths = QFileDialog::getOpenFileNames(this, "Add files to Queue");
                        for (const auto &path : paths)
                            Open::openMRL(p_intf, QUrl::fromLocalFile(path).toString(), false);
                        updateQueue();
                    });
            connect(remove, &QPushButton::clicked, this,
                    [this, list]
                    {
                        vlc_playlist_locker lock(THEPL);
                        for (auto row : list->selectedItems())
                        {
                            auto item = playlist_ItemGetById(THEPL, row->data(Qt::UserRole).toInt());
                            if (item)
                                playlist_NodeDelete(THEPL, item);
                        }
                    });
            connect(save, &QPushButton::clicked, this, [this] { newPlaylist(); });
        }
        else if (i == 1)
        {
            connect(list, &QListWidget::itemDoubleClicked, this,
                    [this](QListWidgetItem *row)
                    {
                        auto input = THEMIM->getInput();
                        if (input)
                            var_SetInteger(input, "chapter", row->data(Qt::UserRole).toInt());
                    });
        }
        else
        {
            auto add = new QPushButton("Add Bookmark");
            body->addWidget(add);
            connect(add, &QPushButton::clicked, this,
                    [this]
                    {
                        auto input = THEMIM->getInput();
                        if (!input)
                            return;
                        seekpoint_t bookmark;
                        if (!input_Control(input, INPUT_GET_BOOKMARK, &bookmark))
                        {
                            QByteArray name = QString("Bookmark %1")
                                                  .arg(timeText(bookmark.i_time_offset / CLOCK_FREQ))
                                                  .toUtf8();
                            bookmark.psz_name = name.data();
                            input_Control(input, INPUT_ADD_BOOKMARK, &bookmark);
                        }
                        updateQueue();
                    });
            connect(list, &QListWidget::itemDoubleClicked, this,
                    [this](QListWidgetItem *row)
                    {
                        auto input = THEMIM->getInput();
                        if (input)
                            var_SetInteger(input, "time", row->data(Qt::UserRole).toLongLong());
                    });
            auto remove = new QPushButton("Remove Bookmark");
            body->addWidget(remove);
            connect(remove, &QPushButton::clicked, this,
                    [this, list]
                    {
                        auto input = THEMIM->getInput();
                        if (input && list->currentRow() >= 0)
                        {
                            input_Control(input, INPUT_DEL_BOOKMARK, list->currentRow());
                            updateQueue();
                        }
                    });
        }
        stack->addWidget(page);
    }
    auto timer = new QTimer(sidePanel);
    timer->setInterval(750);
    connect(timer, &QTimer::timeout, this, [this] { updateQueue(); });
    timer->start();
    stack->setCurrentIndex(selected);
    bodyLayout->addWidget(sidePanel);
    sidePanel->show();
    updateQueue();
    Reframe::animateWidth(sidePanel, 360, 208);
}
bool RedesignShell::isQueueVisible() const
{
    return sidePanel && sidePanel->objectName() == "queuePanel" && sidePanel->isVisible();
}
void RedesignShell::toggleQueue()
{
    if (isQueueVisible())
        sidePanel->hide();
    else
        showQueue();
}
void RedesignShell::updateQueue()
{
    if (!sidePanel || sidePanel->objectName() != "queuePanel" || !sidePanel->isVisible())
        return;
    auto list = sidePanel->findChild<QListWidget *>("queueItems");
    QStringList names;
    QList<int> ids;
    QList<qint64> durations;
    {
        vlc_playlist_locker lock(THEPL);
        std::function<void(playlist_item_t *)> visit = [&](playlist_item_t *node)
        {
            if (node->i_children < 0)
            {
                char *name = input_item_GetName(node->p_input);
                names << qfu(name);
                free(name);
                ids << node->i_id;
                durations << qMax<qint64>(0, input_item_GetDuration(node->p_input) / CLOCK_FREQ);
            }
            else
                for (int i = 0; i < node->i_children; ++i)
                    visit(node->pp_children[i]);
        };
        visit(THEPL->p_playing);
    }
    qint64 total = 0;
    for (const auto duration : durations)
        total += duration;
    if (auto summary = sidePanel->findChild<QLabel *>("queueSummary"))
        summary->setText(QString("%1 items - %2").arg(ids.size()).arg(timeText(total)));
    QString signature;
    for (int i = 0; i < ids.size(); ++i)
        signature += QString::number(ids[i]) + names[i] + QString::number(durations[i]);
    if (list->property("signature").toString() != signature)
    {
        QSignalBlocker block(list);
        list->clear();
        for (int i = 0; i < names.size(); ++i)
        {
            auto row = new QListWidgetItem(names[i] + "   " + timeText(durations[i]), list);
            row->setData(Qt::UserRole, ids[i]);
            row->setSizeHint(QSize(300, 48));
        }
        list->setProperty("signature", signature);
    }
    auto replace = [this](QListWidget *list, const QList<QPair<QString, QVariant>> &rows)
    {
        QString signature = QString::number(quintptr(THEMIM->getInput()));
        for (const auto &row : rows)
            signature += QString::number(row.first.size()) + row.first + row.second.toString();
        if (list->property("signature").toString() == signature)
            return;
        const QVariant selected = list->currentItem() ? list->currentItem()->data(Qt::UserRole) : QVariant();
        list->clear();
        for (const auto &data : rows)
        {
            auto row = new QListWidgetItem(data.first, list);
            row->setData(Qt::UserRole, data.second);
            if (data.second == selected)
                list->setCurrentItem(row);
        }
        list->setProperty("signature", signature);
    };
    auto chapters = sidePanel->findChild<QListWidget *>("chapterItems"),
         bookmarks = sidePanel->findChild<QListWidget *>("bookmarkItems");
    QList<QPair<QString, QVariant>> chapterRows, bookmarkRows;
    auto input = THEMIM->getInput();
    if (input)
    {
        vlc_value_t values, texts;
        if (!var_Change(input, "chapter", VLC_VAR_GETCHOICES, &values, &texts))
        {
            for (int i = 0; i < values.p_list->i_count; ++i)
                chapterRows << qMakePair(qfu(texts.p_list->p_values[i].psz_string),
                                         QVariant(int(values.p_list->p_values[i].i_int)));
            var_FreeList(&values, &texts);
        }
        seekpoint_t **points;
        int count;
        if (!input_Control(input, INPUT_GET_BOOKMARKS, &points, &count))
        {
            for (int i = 0; i < count; ++i)
            {
                bookmarkRows << qMakePair(QString(qfu(points[i]->psz_name) + "   " +
                                                  timeText(points[i]->i_time_offset / CLOCK_FREQ)),
                                          QVariant(qlonglong(points[i]->i_time_offset)));
                vlc_seekpoint_Delete(points[i]);
            }
            free(points);
        }
    }
    replace(chapters, chapterRows);
    replace(bookmarks, bookmarkRows);
}

void RedesignShell::openNetwork()
{
    auto dialog = new Reframe::Dialog(window, "Open Network Stream", QSize(1048, 631));
    dialog->body->addWidget(new QLabel("Network URL"));
    auto urls = new QPlainTextEdit;
    urls->setPlaceholderText("e.g., https://www.example.com/stream.avi");
    dialog->body->addWidget(urls, 1);
    dialog->body->addWidget(new QLabel("You can enter multiple links to load."));
    auto name = new QLineEdit;
    name->setPlaceholderText("e.g., Favorite TV Channels");
    dialog->body->addWidget(new QLabel("Playlist Name (optional)"));
    dialog->body->addWidget(name);
    dialog->body->addWidget(new QLabel("The playlist will be saved and displayed with this title."));
    auto row = new QHBoxLayout;
    row->addStretch();
    auto cancel = new QPushButton("Cancel"), save = new QPushButton("Save");
    row->addWidget(cancel);
    row->addWidget(save);
    dialog->body->addLayout(row);
    connect(cancel, &QPushButton::clicked, dialog, &QDialog::close);
    connect(save, &QPushButton::clicked, this,
            [this, dialog, urls, name]
            {
                QStringList paths;
                for (const auto &line :
                     urls->toPlainText().split(QRegularExpression("[\\r\\n]+"), QString::SkipEmptyParts))
                {
                    QUrl url(line.trimmed());
                    if (!url.isValid() || url.scheme().isEmpty())
                    {
                        QMessageBox::warning(dialog, "Invalid network URL",
                                             "Enter a complete URL with a protocol.");
                        return;
                    }
                    paths << url.toString();
                }
                if (paths.isEmpty())
                    return;
                if (!name->text().trimmed().isEmpty())
                {
                    QVariantMap data;
                    data["name"] = name->text().trimmed();
                    data["paths"] = paths;
                    data["network"] = true;
                    saveCollection(getSettings(), data);
                }
                bool first = true;
                for (const auto &path : paths)
                {
                    Open::openMRL(p_intf, path, first);
                    first = false;
                }
                dialog->close();
                showPlayer();
                showQueue();
            });
    Reframe::reveal(dialog);
}
void RedesignShell::newPlaylist(const QString &id, const QStringList &initialPaths)
{
    auto dialog =
        new Reframe::Dialog(window, id.isEmpty() ? "New Playlist" : "Edit Playlist", QSize(713, 589));
    dialog->setObjectName("playlistEditor");
    auto data = getSettings()->value("Redesign/collection/" + id).toMap();
    auto name = new QLineEdit(data.value("name").toString());
    name->setObjectName("playlistName");
    name->setPlaceholderText("e.g., Favorite Videos");
    dialog->body->addWidget(new QLabel("Playlist Details"));
    dialog->body->addWidget(name);
    auto thumbnail = new QPushButton("Choose Thumbnail");
    thumbnail->setProperty("path", data.value("thumbnail"));
    dialog->body->addWidget(thumbnail);
    connect(thumbnail, &QPushButton::clicked, dialog,
            [dialog, thumbnail]
            {
                auto path = QFileDialog::getOpenFileName(dialog, "Playlist thumbnail", QString(),
                                                         "Images (*.png *.jpg *.jpeg *.webp)");
                if (!path.isEmpty())
                {
                    thumbnail->setProperty("path", path);
                    thumbnail->setText(QFileInfo(path).fileName());
                }
            });
    dialog->body->addWidget(new QLabel("Select Files. Drag files into the box, or choose files below."));
    auto list = new FileList;
    list->setObjectName("playlistFiles");
    fileItems(list, id.isEmpty() ? (initialPaths.isEmpty() ? queuePaths(THEPL) : initialPaths)
                                 : data.value("paths").toStringList());
    dialog->body->addWidget(list, 1);
    auto add = new QPushButton("Select Files");
    dialog->body->addWidget(add);
    connect(add, &QPushButton::clicked, dialog,
            [dialog, list]
            { fileItems(list, QFileDialog::getOpenFileNames(dialog, "Select playlist files")); });
    auto row = new QHBoxLayout;
    auto remove = new QPushButton("Remove Selected");
    row->addWidget(remove);
    connect(remove, &QPushButton::clicked, list, [list] { qDeleteAll(list->selectedItems()); });
    row->addStretch();
    auto cancel = new QPushButton("Cancel"), save = new QPushButton("Save");
    save->setObjectName("playlistSave");
    row->addWidget(cancel);
    row->addWidget(save);
    dialog->body->addLayout(row);
    connect(cancel, &QPushButton::clicked, dialog, &QDialog::close);
    connect(save, &QPushButton::clicked, this,
            [this, dialog, list, name, thumbnail, id]
            {
                if (name->text().trimmed().isEmpty())
                {
                    name->setFocus();
                    return;
                }
                QStringList paths;
                for (int i = 0; i < list->count(); ++i)
                    paths << list->item(i)->data(Qt::UserRole).toString();
                QVariantMap data;
                data["name"] = name->text().trimmed();
                data["paths"] = paths;
                data["thumbnail"] = thumbnail->property("path");
                data["network"] = false;
                saveCollection(getSettings(), data, id);
                dialog->close();
                showLibrary("Playlists");
            });
    Reframe::reveal(dialog);
}
void RedesignShell::batchTracks(const QStringList &supplied, int initialKind)
{
    QStringList paths = supplied.isEmpty() ? selectedPaths(files) : supplied;
    if (paths.isEmpty())
        paths = queuePaths(THEPL);
    QStringList expanded;
    for (const auto &path : paths)
    {
        if (path.startsWith("collection:"))
            expanded += getSettings()
                            ->value("Redesign/collection/" + path.mid(11))
                            .toMap()
                            .value("paths")
                            .toStringList();
        else if (QFileInfo(path).isDir())
        {
            QDir folder(path);
            for (const auto &file : folder.entryInfoList(
                     QString::fromUtf8(EXTENSIONS_MEDIA).split(';', QString::SkipEmptyParts), QDir::Files))
                expanded << file.absoluteFilePath();
        }
        else
            expanded << path;
    }
    for (auto &path : expanded)
        if (QUrl(path).isLocalFile())
            path = QUrl(path).toLocalFile();
    paths = expanded;
    auto dialog = new Reframe::Dialog(window, "Add Tracks to Playlist / Folder", QSize(1048, 800));
    auto top = new QHBoxLayout;
    auto kind = new Reframe::Dropdown;
    kind->addItems(QStringList() << "Add Subtitles"
                                 << "Add Audio Tracks");
    kind->setCurrentIndex(initialKind);
    auto origin = new Reframe::Dropdown;
    origin->addItems(QStringList() << "From Computer"
                                   << "From OpenSubtitles"
                                   << "From Link");
    top->addWidget(kind);
    top->addStretch();
    top->addWidget(origin);
    dialog->body->addLayout(top);
    auto table = new QTableWidget(paths.size(), 2);
    table->setHorizontalHeaderLabels(QStringList() << "Media"
                                                   << "Assigned tracks");
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    dialog->body->addWidget(table, 1);
    for (int i = 0; i < paths.size(); ++i)
    {
        table->setItem(i, 0,
                       new QTableWidgetItem(
                           QFileInfo(QUrl(paths[i]).isLocalFile() ? QUrl(paths[i]).toLocalFile() : paths[i])
                               .fileName()));
        auto choose = new QPushButton("Choose tracks");
        choose->setProperty("tracks", getSettings()->value("Redesign/bindings/" + paths[i]));
        table->setCellWidget(i, 1, choose);
        connect(choose, &QPushButton::clicked, dialog,
                [choose, dialog, kind]
                {
                    auto tracks = QFileDialog::getOpenFileNames(
                        dialog, kind->currentIndex() ? "Choose audio tracks" : "Choose subtitle tracks",
                        QString(),
                        kind->currentIndex() ? "Audio (*.aac *.ac3 *.flac *.m4a *.mp3 *.ogg *.wav)"
                                             : "Subtitles (*.srt *.ass *.ssa *.vtt *.sub *.idx)");
                    if (!tracks.isEmpty())
                    {
                        choose->setProperty("tracks", tracks);
                        choose->setText(QString::number(tracks.size()) + " track(s)");
                        choose->setToolTip(tracks.join('\n'));
                    }
                });
    }
    auto match = new QPushButton("Choose tracks and match filenames");
    dialog->body->addWidget(match);
    connect(match, &QPushButton::clicked, dialog,
            [dialog, paths, table, kind]
            {
                auto tracks = QFileDialog::getOpenFileNames(
                    dialog, "Choose tracks", QString(),
                    kind->currentIndex() ? "Audio (*.aac *.ac3 *.flac *.m4a *.mp3 *.ogg *.wav)"
                                         : "Subtitles (*.srt *.ass *.ssa *.vtt *.sub *.idx)");
                for (int row = 0; row < paths.size(); ++row)
                {
                    QStringList matched;
                    const auto base = QFileInfo(paths[row]).completeBaseName().toLower();
                    const auto episode =
                        QRegularExpression("s\\d+e\\d+", QRegularExpression::CaseInsensitiveOption)
                            .match(base)
                            .captured();
                    for (const auto &track : tracks)
                    {
                        const auto name = QFileInfo(track).completeBaseName().toLower();
                        if (name.startsWith(base) || (!episode.isEmpty() && name.contains(episode)))
                            matched << track;
                    }
                    if (!matched.isEmpty())
                    {
                        auto button = static_cast<QPushButton *>(table->cellWidget(row, 1));
                        button->setProperty("tracks", matched);
                        button->setText(QString::number(matched.size()) + " track(s)");
                        button->setToolTip(matched.join('\n'));
                    }
                }
            });
    connect(
        origin, QOverload<int>::of(&QComboBox::activated), dialog,
        [this, dialog, origin, table, kind](int selected)
        {
            if (selected == 1)
            {
                if (kind->currentIndex() == 0)
                    openSubtitles(dialog, table);
                else
                    QMessageBox::information(
                        dialog, "OpenSubtitles",
                        "OpenSubtitles supplies subtitle files. Choose Computer or Link for audio tracks.");
                origin->setCurrentIndex(0);
            }
            else if (selected == 2)
            {
                auto input = new Reframe::Dialog(dialog, "Track Links", QSize(519, 239));
                auto links = new QPlainTextEdit;
                links->setPlaceholderText("One track URL per line");
                input->body->addWidget(links);
                auto add = new QPushButton("Assign to selected media");
                input->body->addWidget(add);
                connect(add, &QPushButton::clicked, input,
                        [input, links, table]
                        {
                            int row = table->currentRow();
                            if (row < 0)
                                return;
                            QStringList tracks = links->toPlainText().split(QRegularExpression("[\\r\\n]+"),
                                                                            QString::SkipEmptyParts);
                            auto b = static_cast<QPushButton *>(table->cellWidget(row, 1));
                            b->setProperty("tracks", tracks);
                            b->setText(QString::number(tracks.size()) + " track(s)");
                            input->close();
                        });
                Reframe::reveal(input);
            }
        });
    auto row = new QHBoxLayout;
    row->addStretch();
    auto cancel = new QPushButton("Cancel"), save = new QPushButton("Save");
    row->addWidget(cancel);
    row->addWidget(save);
    dialog->body->addLayout(row);
    connect(cancel, &QPushButton::clicked, dialog, &QDialog::close);
    connect(save, &QPushButton::clicked, this,
            [this, dialog, paths, table, kind]
            {
                for (int i = 0; i < paths.size(); ++i)
                {
                    auto tracks = table->cellWidget(i, 1)->property("tracks").toStringList();
                    getSettings()->setValue(
                        "Redesign/" +
                            (kind->currentIndex() ? QString("audioBindings/") : QString("bindings/")) +
                            paths[i],
                        tracks);
                }
                bindingInput.clear();
                applyMediaBindings();
                dialog->close();
            });
    Reframe::reveal(dialog);
}
void RedesignShell::startFirstRun()
{
    setProperty("privacyExpected", var_InheritBool(p_intf, "qt-privacy-ask"));
    char *firstRunSmoke = var_InheritString(p_intf, "qt-redesign-first-run-smoke");
    if (firstRunSmoke && *firstRunSmoke)
        getSettings()->setValue("Redesign/onboarded", false);
    free(firstRunSmoke);
    char *preview = var_InheritString(p_intf, "qt-redesign-preview"),
         *smoke = var_InheritString(p_intf, "qt-redesign-smoke");
    const bool diagnostic = (preview && *preview) || (smoke && *smoke);
    free(preview);
    free(smoke);
    if (diagnostic)
        return;
    auto ready = [this]
    {
        if (!getSettings()->value("Redesign/onboarded", false).toBool())
            onboarding();
        if (getSettings()->value("Redesign/checkUpdates", false).toBool())
            QTimer::singleShot(5000, this, [this] { checkUpdates(); });
    };
    if (auto privacy = FirstRun::CheckAndRun(window, p_intf))
    {
        connect(privacy, &QDialog::finished, this,
                [this, ready](int result)
                {
                    if (result == QDialog::Accepted)
                        QTimer::singleShot(0, this, ready);
                });
        Reframe::reveal(privacy);
        privacy->raise();
        privacy->activateWindow();
    }
    else
        ready();
}
void RedesignShell::runFirstRunSmoke(const QString &reportPath)
{
    auto report = [this, reportPath](const QString &result)
    {
        QFile file(reportPath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text))
            file.write(result.toUtf8());
        THEDP->quit();
    };
    auto privacy = window->findChild<QDialog *>("privacyDialog");
    auto welcome = window->findChild<QDialog *>("onboardingDialog");
    if (!property("privacyExpected").toBool())
    {
        if (privacy || !welcome || !welcome->isVisible())
            report("FAIL: saved privacy choice did not skip straight to onboarding on relaunch");
        else
            report("PASS: privacy choice persisted and relaunch opened onboarding without another prompt");
        return;
    }
    if (!privacy || !privacy->isVisible() || welcome || QApplication::activeModalWidget() != privacy)
    {
        report("FAIL: privacy was not the only visible first-run dialog before onboarding");
        return;
    }
    const QRect available = QApplication::desktop()->availableGeometry(window);
    QPoint expected = window->frameGeometry().center() - QPoint(privacy->width() / 2, privacy->height() / 2);
    expected.setX(qBound(available.left(), expected.x(), available.right() - privacy->width() + 1));
    expected.setY(qBound(available.top(), expected.y(), available.bottom() - privacy->height() + 1));
    if ((privacy->pos() - expected).manhattanLength() > 3)
    {
        report("FAIL: privacy prompt was not centered over the final main window placement");
        return;
    }
    privacy->grab().save(reportPath + ".privacy.png");
    auto access = privacy->findChild<QCheckBox *>("metadataNetworkAccess");
    auto buttons = privacy->findChild<QDialogButtonBox *>();
    if (!access || !buttons || buttons->buttons().isEmpty())
    {
        report("FAIL: privacy choice controls missing");
        return;
    }
    access->setChecked(false);
    buttons->buttons().first()->click();
    QTimer::singleShot(
        100, this,
        [this, report, reportPath]
        {
            auto dialog = window->findChild<QDialog *>("onboardingDialog");
            auto privacy = window->findChild<QDialog *>("privacyDialog");
            if (!dialog || !dialog->isVisible() || (privacy && privacy->isVisible()) ||
                QApplication::activeModalWidget() != dialog ||
                config_GetInt(p_intf, "metadata-network-access") || config_GetInt(p_intf, "qt-privacy-ask"))
            {
                report("FAIL: Continue did not save the privacy choice and open onboarding alone");
                return;
            }
            dialog->grab().save(reportPath + ".onboarding.png");
            report("PASS: centered privacy dialog appeared alone; Continue saved opt-out before onboarding");
        });
}
void RedesignShell::onboarding()
{
    if (auto existing = window->findChild<QDialog *>("onboardingDialog"))
    {
        existing->raise();
        return;
    }
    auto dialog = new Reframe::Dialog(window, "Welcome to Reframe!", QSize(1090, 759));
    dialog->setObjectName("onboardingDialog");
    dialog->setWindowModality(Qt::WindowModal);
    auto stack = new QStackedWidget;
    stack->setObjectName("onboardingPages");
    dialog->body->addWidget(stack, 1);
    auto folderPage = new QWidget;
    auto folderLayout = new QVBoxLayout(folderPage);
    folderLayout->addStretch();
    auto folderHeading = new QLabel("Choose a Media Folder to get started with");
    folderHeading->setObjectName("sectionHeading");
    folderLayout->addWidget(folderHeading);
    auto path = new QLineEdit(
        getSettings()->value("Redesign/VideosDirectory", QString(QDir::homePath() + "/Videos")).toString());
    folderLayout->addWidget(path);
    auto browseButton = new QPushButton("Browse");
    folderLayout->addWidget(browseButton);
    folderLayout->addStretch();
    connect(browseButton, &QPushButton::clicked, dialog,
            [dialog, path]
            {
                auto folder = QFileDialog::getExistingDirectory(dialog, "Choose media folder", path->text());
                if (!folder.isEmpty())
                    path->setText(folder);
            });
    stack->addWidget(folderPage);
    auto appearance = new QWidget;
    auto themeLayout = new QHBoxLayout(appearance);
    themeLayout->setContentsMargins(24, 24, 24, 24);
    themeLayout->setSpacing(32);
    auto choices = new QWidget(appearance);
    choices->setObjectName("onboardingAppearanceControls");
    choices->setFixedWidth(260);
    auto choicesLayout = new QVBoxLayout(choices);
    choicesLayout->setContentsMargins(0, 0, 0, 0);
    choicesLayout->setSpacing(20);
    choicesLayout->addStretch();
    auto heading = new QLabel("Make it yours");
    heading->setObjectName("onboardingHeading");
    choicesLayout->addWidget(heading);
    auto description = new QLabel("Choose your preferred theme and accent color.");
    description->setWordWrap(true);
    choicesLayout->addWidget(description);
    auto themeLabel = new QLabel("Theme");
    choicesLayout->addWidget(themeLabel);
    auto preview = new OnboardingPreview(appearance);
    auto updatePreview = [this, preview] { preview->setImage(window->grab()); };
    auto themes = Reframe::segmented(QStringList() << "Light"
                                                   << "Dark"
                                                   << "System",
                                     getSettings()->value("Redesign/theme", "Dark").toString(), appearance,
                                     [this, preview, updatePreview](QString theme)
                                     {
                                         getSettings()->setValue("Redesign/theme", theme);
                                         applyTheme();
                                         QTimer::singleShot(0, preview, updatePreview);
                                     });
    themes->setObjectName("onboardingThemes");
    choicesLayout->addWidget(themes);
    choicesLayout->addSpacing(8);
    choicesLayout->addWidget(new QLabel("Accent color"));
    auto colors = new QGridLayout;
    colors->setSpacing(12);
    auto group = new QButtonGroup(choices);
    group->setExclusive(true);
    const QStringList accents(
        {"#d37918", "#ad4129", "#93751b", "#1a882b", "#008ba8", "#1a56b3", "#8035c8", "#ba3a88", "#8f8f8f"});
    const QStringList names({"Orange", "Red", "Yellow", "Green", "Cyan", "Blue", "Purple", "Pink", "Gray"});
    for (int i = 0; i < accents.size(); ++i)
    {
        const QString color = accents[i];
        auto b = new AccentSwatch(color, names[i], choices);
        group->addButton(b);
        b->setChecked(QColor(accent) == QColor(color));
        colors->addWidget(b, i / 3, i % 3, Qt::AlignLeft);
        connect(b, &QAbstractButton::clicked, dialog,
                [this, color, preview, updatePreview]
                {
                    getSettings()->setValue("Redesign/accent", color);
                    applyTheme();
                    QTimer::singleShot(0, preview, updatePreview);
                });
    }
    choicesLayout->addLayout(colors);
    choicesLayout->addStretch();
    themeLayout->addWidget(choices);
    themeLayout->addWidget(preview, 1);
    connect(stack, &QStackedWidget::currentChanged, preview,
            [preview, updatePreview](int index)
            {
                if (index == 1)
                    QTimer::singleShot(0, preview, updatePreview);
            });
    stack->addWidget(appearance);
    auto keys = new QWidget;
    keys->setObjectName("onboardingKeys");
    auto keysLayout = new QVBoxLayout(keys);
    keysLayout->setContentsMargins(32, 8, 32, 8);
    keysLayout->setSpacing(24);
    auto keysHeading = new QLabel("Learn the basic keyboard shortcuts");
    keysHeading->setObjectName("onboardingHeading");
    keysHeading->setAlignment(Qt::AlignCenter);
    keysLayout->addWidget(keysHeading);
    keysLayout->addStretch();
    auto columns = new QHBoxLayout;
    columns->setSpacing(40);
    auto left = new QVBoxLayout, right = new QVBoxLayout;
    left->setSpacing(14);
    right->setSpacing(14);
    auto hotkey = [this](const char *name)
    {
        char *value = config_GetPsz(p_intf, name);
        QString text = value ? QString::fromUtf8(value).section('\t', 0, 0) : QString();
        free(value);
        QStringList parts = text.split('+');
        for (auto &part : parts)
            if (part.size() == 1)
                part = part.toUpper();
        text = parts.join('+');
        text.replace("Left", QString::fromUtf8("\xE2\x86\x90"));
        text.replace("Right", QString::fromUtf8("\xE2\x86\x92"));
        text.replace("Up", QString::fromUtf8("\xE2\x86\x91"));
        text.replace("Down", QString::fromUtf8("\xE2\x86\x93"));
        return text.isEmpty() ? QString("Unassigned") : text;
    };
    auto shortcut =
        [keys](QVBoxLayout *column, const QString &glyph, const QString &label, const QStringList &bindings)
    {
        auto row = new QWidget(keys);
        row->setObjectName("onboardingShortcutRow");
        row->setMinimumHeight(58);
        auto layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(12);
        auto icon = new QLabel;
        icon->setObjectName("shortcutIcon");
        icon->setProperty("glyph", glyph);
        icon->setPixmap(Reframe::icon(glyph, keys->palette().text().color()).pixmap(28, 28));
        icon->setFixedSize(32, 32);
        layout->addWidget(icon);
        auto title = new QLabel(label);
        title->setWordWrap(true);
        title->setObjectName("shortcutAction");
        layout->addWidget(title, 1);
        for (int i = 0; i < bindings.size(); ++i)
        {
            if (i)
                layout->addWidget(new QLabel("/"));
            auto cap = new QLabel(bindings[i]);
            cap->setObjectName("shortcutKeycap");
            cap->setAlignment(Qt::AlignCenter);
            cap->setMinimumWidth(38);
            cap->setFixedHeight(42);
            layout->addWidget(cap);
        }
        column->addWidget(row);
    };
    shortcut(left, "playpause", "Play / Pause", {hotkey("key-play-pause")});
    shortcut(left, "seek", "Seek backward / forward",
             {hotkey("key-jump-extrashort"), hotkey("key-jump+extrashort")});
    shortcut(left, "next", "Previous / next item", {hotkey("key-prev"), hotkey("key-next")});
    // VLC's engine supports forward frame stepping only.
    shortcut(left, "frame", "Next video frame", {hotkey("key-frame-next")});
    shortcut(left, "subtitles", "Cycle subtitle track", {hotkey("key-subtitle-track")});
    shortcut(left, "audio", "Cycle audio track", {hotkey("key-audio-track")});
    shortcut(right, "fullscreen", "Toggle fullscreen", {hotkey("key-toggle-fullscreen")});
    shortcut(right, "volume", "Increase / decrease volume", {hotkey("key-vol-up"), hotkey("key-vol-down")});
    shortcut(right, "playlist", "Show / hide queue", {"Q"});
    shortcut(right, "folder", "Show / hide file browser", {"L"});
    shortcut(right, "snapshot", "Take a snapshot", {hotkey("key-snapshot")});
    shortcut(right, "record", "Record", {hotkey("key-record")});
    columns->addLayout(left, 1);
    columns->addLayout(right, 1);
    keysLayout->addLayout(columns);
    keysLayout->addStretch();
    keysLayout->addWidget(new QLabel("Find the full list in Settings > Controls > Keyboard Shortcuts."));
    stack->addWidget(keys);
    connect(stack, &QStackedWidget::currentChanged, keys,
            [keys](int index)
            {
                if (index == 2)
                    for (auto icon : keys->findChildren<QLabel *>("shortcutIcon"))
                        icon->setPixmap(Reframe::icon(icon->property("glyph").toString(),
                                                      QApplication::palette().text().color())
                                            .pixmap(28, 28));
            });
    auto footer = new QHBoxLayout;
    auto back = new QPushButton("Back"), next = new QPushButton("Next");
    next->setObjectName("onboardingNext");
    footer->addWidget(back);
    footer->addStretch();
    footer->addWidget(next);
    dialog->body->addLayout(footer);
    back->setEnabled(false);
    connect(back, &QPushButton::clicked, dialog,
            [stack, back, next]
            {
                stack->setCurrentIndex(stack->currentIndex() - 1);
                back->setEnabled(stack->currentIndex() > 0);
                next->setText("Next");
            });
    connect(next, &QPushButton::clicked, this,
            [this, dialog, stack, path, back, next]
            {
                if (stack->currentIndex() == 2)
                {
                    getSettings()->setValue("Redesign/onboarded", true);
                    getSettings()->setValue("Redesign/VideosDirectory", path->text());
                    dialog->close();
                    showLibrary("Videos");
                }
                else
                {
                    stack->setCurrentIndex(stack->currentIndex() + 1);
                    back->setEnabled(true);
                    next->setText(stack->currentIndex() == 2 ? "Finish" : "Next");
                }
            });
    Reframe::reveal(dialog);
    Reframe::centerOnParent(dialog);
}
void RedesignShell::toggleMiniPlayer()
{
    if (miniPlayer)
    {
        showPlayer();
        window->showNormal();
        window->resize(previousSize);
        return;
    }
    previousSize = window->size();
    if (sidePanel)
        sidePanel->hide();
    pages->setCurrentWidget(browser);
    miniPlayer = new Reframe::Dialog(window, "Now Playing", QSize(431, 300));
    miniPlayer->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    static_cast<Reframe::Dialog *>(miniPlayer)->beforeClose = [this]
    {
        videoLayout->addWidget(player);
        player->show();
    };
    auto layout = static_cast<Reframe::Dialog *>(miniPlayer)->body;
    videoLayout->removeWidget(player);
    layout->addWidget(player, 1);
    auto actions = new QHBoxLayout;
    auto pause = button("play"), restore = button("maximize"), queue = button("playlist");
    actions->addWidget(pause);
    actions->addStretch();
    actions->addWidget(queue);
    actions->addWidget(restore);
    layout->addLayout(actions);
    connect(pause, &QToolButton::clicked, THEAM, &ActionsManager::play);
    connect(restore, &QToolButton::clicked, this, [this] { toggleMiniPlayer(); });
    connect(queue, &QToolButton::clicked, this, [this] { showQueue(); });
    const auto instance = miniPlayer;
    connect(miniPlayer, &QObject::destroyed, this,
            [this, instance]
            {
                if (miniPlayer == instance)
                    miniPlayer = nullptr;
            });
    Reframe::reveal(miniPlayer);
    const QRect area = QApplication::desktop()->availableGeometry(window);
    const QRect parent = window->frameGeometry().intersected(area);
    const int right = parent.isEmpty() ? area.right() : parent.right();
    const int bottom = parent.isEmpty() ? area.bottom() : parent.bottom();
    miniPlayer->move(qMax(area.left(), right - miniPlayer->width() - 16),
                     qMax(area.top(), bottom - miniPlayer->height() - 16));
}
void RedesignShell::layoutControls()
{
    QStringList order = getSettings()->value("Redesign/toolbar", Reframe::defaultToolbar()).toStringList();
    order.removeAll("fullscreen");
    order << "fullscreen";
    appliedToolbar = getSettings()->value("Redesign/toolbar", Reframe::defaultToolbar()).toStringList();
    const auto auxiliary = getSettings()->value("Redesign/leftControls").toStringList() +
                           getSettings()->value("Redesign/topControls").toStringList() +
                           getSettings()->value("Redesign/toolbar2").toStringList();
    for (auto b : controlButtons)
        b->hide();
    const QHash<QString, QWidget *> widgets = {
        {"elapsed", elapsed}, {"seek", seek}, {"duration", duration}, {"volumeSlider", volume}};
    for (auto widget : widgets)
        widget->hide();
    while (auto item = transportLayout->takeAt(0))
    {
        if (item->widget() && item->widget()->property("customLabel").toBool())
            delete item->widget();
        delete item;
    }
    const QHash<QString, int> actions = {{"record", RECORD_ACTION}, {"snapshot", SNAPSHOT_ACTION},
                                         {"repeat", LOOP_ACTION},   {"shuffle", RANDOM_ACTION},
                                         {"frame", FRAME_ACTION},   {"reverse", REVERSE_ACTION}};
    for (const auto &name : order + auxiliary)
    {
        if (QStringList({"thumbnail", "fileName", "artistName", "folderName", "playlistName"}).contains(name))
        {
            if (order.contains(name))
            {
                auto label = new QLabel(transport);
                label->setObjectName("controlMetadata");
                label->setProperty("metadataField", name);
                label->setProperty("customLabel", true);
                label->setTextFormat(Qt::PlainText);
                label->setMaximumWidth(180);
                if (name == "thumbnail")
                    label->setFixedSize(40, 28);
                transportLayout->addWidget(label);
                label->show();
            }
            continue;
        }
        if (name == "spacer")
        {
            if (!order.contains(name))
                continue;
            transportLayout->addStretch();
            continue;
        }
        if (name == "divider")
        {
            if (!order.contains(name))
                continue;
            auto line = new QLabel("|");
            line->setProperty("customLabel", true);
            transportLayout->addWidget(line);
            continue;
        }
        QWidget *widget = widgets.value(name, nullptr);
        if (!widget)
        {
            auto b = controlButtons.value(name, nullptr);
            if (!b)
            {
                b = button(name);
                b->setToolTip(name);
                controlButtons[name] = b;
                if (actions.contains(name))
                    connect(b, &QToolButton::clicked, this,
                            [this, name, actions] { THEAM->doAction(actions.value(name)); });
                else
                    connect(b, &QToolButton::clicked, this,
                            [this, name]
                            {
                                if (name.startsWith("back") || name.startsWith("forward"))
                                {
                                    auto input = THEMIM->getInput();
                                    if (input)
                                    {
                                        const qint64 amount = name.endsWith("15") ? 15 : 10;
                                        var_SetInteger(
                                            input, "time",
                                            qMax<qint64>(0, var_GetInteger(input, "time") +
                                                                (name.startsWith("back") ? -amount : amount) *
                                                                    CLOCK_FREQ));
                                    }
                                }
                                else if (name == "replay")
                                {
                                    auto input = THEMIM->getInput();
                                    if (input)
                                    {
                                        var_SetInteger(input, "time", 0);
                                        var_SetInteger(input, "state", PLAYING_S);
                                    }
                                }
                                else if (name == "bookmark")
                                {
                                    auto input = THEMIM->getInput();
                                    if (input)
                                    {
                                        seekpoint_t point;
                                        if (!input_Control(input, INPUT_GET_BOOKMARK, &point))
                                        {
                                            QByteArray text =
                                                QString("Bookmark %1")
                                                    .arg(timeText(point.i_time_offset / CLOCK_FREQ))
                                                    .toUtf8();
                                            point.psz_name = text.data();
                                            input_Control(input, INPUT_ADD_BOOKMARK, &point);
                                        }
                                    }
                                }
                                else if (name == "mini")
                                    toggleMiniPlayer();
                                else if (name == "library")
                                    toggleLibraryDock();
                                else if (name == "home")
                                    showLibrary("Home");
                                else if (name == "playlists")
                                    showLibrary("Playlists");
                                else if (name == "quit")
                                    THEDP->quit();
                                else if (name == "chapters")
                                    showQueue(1);
                                else if (name == "bookmarks")
                                    showQueue(2);
                                else if (name == "preferences")
                                    openSettings();
                                else if (name == "properties")
                                    THEDP->mediaInfoDialog();
                                else if (name == "favorite")
                                {
                                    auto item = THEMIM->currentInputItem();
                                    if (item)
                                    {
                                        char *uri = input_item_GetURI(item);
                                        auto favorites =
                                            getSettings()->value("Redesign/favorites").toStringList();
                                        const QString path = qfu(uri);
                                        free(uri);
                                        if (favorites.contains(path))
                                            favorites.removeAll(path);
                                        else
                                            favorites << path;
                                        getSettings()->setValue("Redesign/favorites", favorites);
                                    }
                                }
                                else
                                    openMediaSettings(name == "hdr" || name == "zoom" ? 5 : 4);
                            });
            }
            widget = b;
        }
        if (order.contains(name))
        {
            widget->show();
            transportLayout->addWidget(widget, name == "seek" ? 1 : 0);
        }
        else
            widget->hide();
    }
    if (auto grip = transport->findChild<QWidget *>("resizeGrip"))
        transportLayout->addWidget(grip);
    transport->updateGeometry();
    populateAuxiliaryControls();
}
void RedesignShell::populateAuxiliaryControls()
{
    const QHash<QString, QWidget *> fields = {
        {"elapsed", elapsed}, {"duration", duration}, {"seek", seek}, {"volumeSlider", volume}};
    for (auto field : fields)
        if (field->parentWidget() != transport)
        {
            field->setParent(transport);
            field->hide();
        }
    for (QWidget **slot : {&customLeft, &customTop, &extraToolbar})
    {
        if (*slot)
        {
            delete *slot;
            *slot = nullptr;
        }
    }
    auto fill = [this, fields](QWidget *host, const QStringList &names, bool vertical)
    {
        auto layout = new QBoxLayout(vertical ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight, host);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setSpacing(8);
        for (const auto &name : names)
        {
            if (name == "spacer")
            {
                layout->addStretch();
                continue;
            }
            if (name == "divider")
            {
                layout->addWidget(new QLabel(vertical ? "_" : "|"));
                continue;
            }
            if (fields.contains(name) && !vertical)
            {
                auto field = fields.value(name);
                layout->addWidget(field, name == "seek" ? 1 : 0);
                field->show();
                continue;
            }
            if (QStringList({"thumbnail", "fileName", "artistName", "folderName", "playlistName"})
                    .contains(name))
            {
                auto label = new QLabel;
                label->setObjectName("controlMetadata");
                label->setProperty("metadataField", name);
                label->setTextFormat(Qt::PlainText);
                label->setMaximumWidth(220);
                label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
                if (name == "thumbnail")
                    label->setFixedSize(40, 28);
                else
                    label->setMinimumWidth(70);
                layout->addWidget(label);
                continue;
            }
            auto b = button(name);
            b->setToolTip(name);
            layout->addWidget(b);
            auto original = controlButtons.value(name, nullptr);
            if (original)
                connect(b, &QToolButton::clicked, original, &QToolButton::click);
        }
    };
    const auto left = getSettings()->value("Redesign/leftControls").toStringList();
    if (!left.isEmpty())
    {
        customLeft = new QWidget(sidebar);
        fill(customLeft, left, true);
        auto layout = static_cast<QVBoxLayout *>(sidebar->layout());
        layout->insertWidget(qMax(0, layout->count() - 2), customLeft);
    }
    const auto top = getSettings()->value("Redesign/topControls").toStringList();
    if (!top.isEmpty())
    {
        customTop = new QWidget(header);
        fill(customTop, top, false);
        static_cast<QHBoxLayout *>(header->layout())->insertWidget(4, customTop);
    }
    const auto second = getSettings()->value("Redesign/toolbar2").toStringList();
    if (!second.isEmpty())
    {
        extraToolbar = new QWidget(this);
        extraToolbar->setObjectName("transport");
        extraToolbar->setFixedHeight(54);
        fill(extraToolbar, second, false);
        static_cast<QVBoxLayout *>(layout())->addWidget(extraToolbar);
    }
    if (auto old = findChild<QTimer *>("metadataControlsTimer"))
    {
        old->stop();
        delete old;
    }
    auto timer = new QTimer(this);
    timer->setObjectName("metadataControlsTimer");
    timer->setInterval(350);
    connect(timer, &QTimer::timeout, this,
            [this]
            {
                auto item = THEMIM->currentInputItem();
                char *raw = item ? input_item_GetURI(item) : nullptr;
                const QUrl url(qfu(raw ? raw : ""));
                free(raw);
                const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
                for (auto label : findChildren<QLabel *>("controlMetadata"))
                {
                    const QString field = label->property("metadataField").toString();
                    QString value;
                    if (field == "thumbnail")
                    {
                        const QString image = thumbnails.value(path);
                        if (QFileInfo(image).size() > 0)
                            label->setPixmap(QPixmap(image).scaled(label->size(), Qt::KeepAspectRatio,
                                                                   Qt::SmoothTransformation));
                        continue;
                    }
                    if (field == "fileName")
                        value = QFileInfo(path).fileName();
                    else if (field == "folderName")
                        value = QFileInfo(path).dir().dirName();
                    else if (field == "playlistName")
                        value = getSettings()
                                    ->value("Redesign/collection/" + currentCollection)
                                    .toMap()
                                    .value("name")
                                    .toString();
                    else if (field == "artistName" && item)
                    {
                        char *artist = input_item_GetArtist(item);
                        value = qfu(artist ? artist : "");
                        free(artist);
                    }
                    label->setToolTip(value);
                    label->setText(label->fontMetrics().elidedText(value, Qt::ElideMiddle, 220));
                }
            });
    timer->start();
}
void RedesignShell::customize()
{
    auto dialog = new Reframe::Dialog(window, "Customize Player", QSize(1100, 650));
    dialog->body->addWidget(new QLabel("Drag items around and out of the boxes to reorder or remove them."));
    const QStringList available = QStringList() << "previous"
                                                << "play"
                                                << "next"
                                                << "stop"
                                                << "elapsed"
                                                << "seek"
                                                << "duration"
                                                << "volume"
                                                << "volumeSlider"
                                                << "audio"
                                                << "subtitles"
                                                << "settings"
                                                << "playlist"
                                                << "record"
                                                << "snapshot"
                                                << "repeat"
                                                << "shuffle"
                                                << "back10"
                                                << "forward10"
                                                << "back15"
                                                << "forward15"
                                                << "frame"
                                                << "reverse"
                                                << "replay"
                                                << "mini"
                                                << "library"
                                                << "hdr"
                                                << "zoom"
                                                << "home"
                                                << "preferences"
                                                << "quit"
                                                << "chapters"
                                                << "bookmark"
                                                << "bookmarks"
                                                << "playlists"
                                                << "favorite"
                                                << "properties"
                                                << "speed"
                                                << "spacer"
                                                << "divider";
    const QStringList metadataFields = QStringList() << "thumbnail"
                                                     << "fileName"
                                                     << "artistName"
                                                     << "folderName"
                                                     << "playlistName";
    auto selected = new ControlList(dialog);
    auto inventory = new ControlList(dialog);
    selected->setObjectName("toolbarItems");
    inventory->setObjectName("availableControls");
    for (auto list : {selected, inventory})
    {
        list->setViewMode(QListView::IconMode);
        list->setMovement(QListView::Snap);
        list->setDragDropMode(QAbstractItemView::DragDrop);
        list->setDefaultDropAction(Qt::MoveAction);
        list->setAcceptDrops(true);
        list->setDragEnabled(true);
        list->setGridSize(QSize(120, 72));
        list->setIconSize(QSize(24, 24));
    }
    const auto order =
        getSettings()->value("Redesign/toolbar", QStringList(available.mid(0, 13))).toStringList();
    controlItems(selected, order);
    for (const auto &name : available)
        if (!order.contains(name))
            controlItems(inventory, QStringList({name}));
    dialog->body->addWidget(new QLabel("Player toolbar"));
    dialog->body->addWidget(selected, 1);
    auto positions = new QHBoxLayout;
    auto left = new ControlList(dialog);
    auto top = new ControlList(dialog);
    auto second = new ControlList(dialog);
    const QList<QPair<QListWidget *, QString>> placements = {
        {left, "leftControls"}, {top, "topControls"}, {second, "toolbar2"}};
    for (const auto &slot : placements)
    {
        auto container = new QWidget;
        auto box = new QVBoxLayout(container);
        box->addWidget(new QLabel(slot.second == "leftControls"  ? "Left sidebar"
                                  : slot.second == "topControls" ? "Title bar"
                                                                 : "Second toolbar row"));
        slot.first->setViewMode(QListView::IconMode);
        slot.first->setDragDropMode(QAbstractItemView::DragDrop);
        slot.first->setDefaultDropAction(Qt::MoveAction);
        slot.first->setAcceptDrops(true);
        slot.first->setDragEnabled(true);
        controlItems(slot.first, getSettings()->value("Redesign/" + slot.second).toStringList());
        box->addWidget(slot.first);
        positions->addWidget(container);
    }
    dialog->body->addLayout(positions, 1);
    controlItems(inventory, metadataFields);
    dialog->body->addWidget(new QLabel("Available controls"));
    dialog->body->addWidget(inventory, 1);
    auto row = new QHBoxLayout;
    auto reset = new QPushButton("Reset"), cancel = new QPushButton("Cancel"), save = new QPushButton("Save");
    row->addWidget(reset);
    row->addStretch();
    row->addWidget(cancel);
    row->addWidget(save);
    dialog->body->addLayout(row);
    connect(reset, &QPushButton::clicked, dialog,
            [selected, inventory, available, metadataFields, placements]
            {
                for (const auto &slot : placements)
                    slot.first->clear();
                selected->clear();
                inventory->clear();
                controlItems(selected, available.mid(0, 13));
                controlItems(inventory, available.mid(13));
                controlItems(inventory, metadataFields);
            });
    connect(cancel, &QPushButton::clicked, dialog, &QDialog::close);
    connect(save, &QPushButton::clicked, this,
            [this, dialog, selected, placements]
            {
                QStringList names;
                for (int i = 0; i < selected->count(); ++i)
                    names << selected->item(i)->data(Qt::UserRole).toString();
                getSettings()->setValue("Redesign/toolbar", names);
                for (const auto &slot : placements)
                {
                    QStringList items;
                    for (int i = 0; i < slot.first->count(); ++i)
                        items << slot.first->item(i)->data(Qt::UserRole).toString();
                    getSettings()->setValue("Redesign/" + slot.second, items);
                }
                layoutControls();
                dialog->close();
            });
    Reframe::reveal(dialog);
}
void RedesignShell::mainMenu(const QPoint &position)
{
    QMenu menu;
    menu.addAction("Open File(s)\tCtrl+O", THEDP, &DialogsProvider::openFileDialog);
    menu.addAction("Open Folder\tCtrl+F", this,
                   [this]
                   {
                       auto path = QFileDialog::getExistingDirectory(this, "Open Folder");
                       if (!path.isEmpty())
                           browse(path);
                   });
    menu.addAction("Open Network Stream\tCtrl+N", this, [this] { openNetwork(); });
    menu.addAction("Open Disc\tCtrl+D", THEDP, &DialogsProvider::openDiscDialog);
    menu.addAction("Open Capture Device\tCtrl+C", THEDP, &DialogsProvider::openCaptureDialog);
    menu.addAction("Convert\tCtrl+R", THEDP, &DialogsProvider::openAndTranscodingDialogs);
    auto recent = menu.addMenu("Open Recent");
    for (const auto &path : RecentsMRL::getInstance(p_intf)->recentList())
        recent->addAction(QFileInfo(QUrl(path).toLocalFile()).fileName(), this,
                          [this, path] { openMedia(path); });
    menu.addSeparator();
    auto profiles = menu.addMenu("Profile");
    for (const auto &name : getSettings()->value("Redesign/profileNames").toStringList())
    {
        auto action = profiles->addAction(name);
        action->setCheckable(true);
        action->setChecked(getSettings()->value("Redesign/activeProfile").toString() == name);
        connect(action, &QAction::triggered, this,
                [this, name]
                {
                    auto preferences = new RedesignPreferences(p_intf, window, 0, [this] { applyTheme(); });
                    preferences->restore(getSettings()->value("Redesign/profiles/" + name).toMap());
                    preferences->deleteLater();
                    getSettings()->setValue("Redesign/activeProfile", name);
                    layoutControls();
                    applyTheme();
                });
    }
    profiles->addAction("New Profile", this,
                        [this]
                        {
                            const auto name =
                                QInputDialog::getText(window, "New Profile", "Profile name").trimmed();
                            if (name.isEmpty())
                                return;
                            auto preferences =
                                new RedesignPreferences(p_intf, window, 0, [this] { applyTheme(); });
                            preferences->saveAppProfile(name);
                            preferences->deleteLater();
                        });
    profiles->addAction("Manage Profiles", this,
                        [this]
                        {
                            openSettings();
                            static_cast<RedesignPreferences *>(settingsDialog)->manageProfiles();
                        });
    menu.addAction("Keyboard Shortcuts", this,
                   [this]
                   {
                       openSettings(3);
                       static_cast<RedesignPreferences *>(settingsDialog)->editShortcuts();
                   });
    menu.addAction("Customize Player", this, [this] { customize(); });
    menu.addAction("Settings\tCtrl+P", this, [this] { openSettings(); });
    menu.addAction(
        "About Reframe", this,
        [this]
        {
            auto dialog = new Reframe::Dialog(window, "About Reframe", QSize(650, 450));
            auto label = new QLabel(
                "Reframe, an unofficial VLC 3.0.24 fork.\n\nVideoLAN playback engine, GPL v2 or "
                "later.\nInterface adapted from VLC Redesign by Juxtopposed, CC BY 4.0.\nImplementation by "
                "webmaster94, 2026.\n\nNot endorsed by VideoLAN or Juxtopposed.");
            label->setWordWrap(true);
            dialog->body->addWidget(label);
            auto source = new QLabel(
                "<a href='https://github.com/webmaster94/vlc-redesign'>Source, licenses and releases</a>");
            source->setOpenExternalLinks(true);
            dialog->body->addWidget(source);
            Reframe::reveal(dialog);
        });
    menu.exec(position);
}
void RedesignShell::playerMenu(const QPoint &position)
{
    QMenu menu;
    menu.addAction("Play / Pause\tSpace", THEAM, &ActionsManager::play);
    menu.addAction("Stop\tS", THEMIM, &MainInputManager::stop);
    menu.addAction("Previous Item\tP", THEMIM, &MainInputManager::prev);
    menu.addAction("Next Item\tN", THEMIM, &MainInputManager::next);
    menu.addSeparator();
    for (const auto &pair :
         QList<QPair<QString, int>>({{"Playback", 4}, {"Video", 5}, {"Audio", 6}, {"Subtitle", 7}}))
        menu.addAction(pair.first, this, [this, pair] { openMediaSettings(pair.second); });
    menu.addAction("Queue", this, [this] { showQueue(); });
    menu.addAction("Record", THEAM, &ActionsManager::record);
    menu.addSeparator();
    menu.addAction("Hide UI", window, &MainInterface::toggleMinimalView);
    menu.addAction("More Settings", this, [this] { openSettings(); });
    menu.addAction("Open Media", THEDP, &DialogsProvider::openFileDialog);
    menu.addAction("Quit Reframe\tCtrl+Q", THEDP, &DialogsProvider::quit);
    menu.exec(position);
}
bool RedesignShell::eventFilter(QObject *target, QEvent *event)
{
    if (target == this && event->type() == RedesignFullscreenEvent::eventType())
    {
        if (static_cast<RedesignFullscreenEvent *>(event)->fullscreen != var_GetBool(THEPL, "fullscreen"))
            return true;
        if (static_cast<RedesignFullscreenEvent *>(event)->fullscreen)
        {
            showPlayer();
            if (!window->isFullScreen())
                window->toggleInterfaceFullScreen();
        }
        else
            leaveFullscreen();
        return true;
    }
    if (target == window && event->type() == QEvent::WindowStateChange)
    {
        sidebar->setVisible(!window->isFullScreen() && !minimal);
        showChrome();
        if (auto grip = transport->findChild<QWidget *>("resizeGrip"))
            grip->setVisible(!window->isFullScreen() && !window->isMaximized());
    }
    if ((event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride) &&
        window->isFullScreen())
    {
        auto widget = qobject_cast<QWidget *>(target);
        auto key = static_cast<QKeyEvent *>(event);
        if (widget && widget->window() == window && key->key() == Qt::Key_Escape)
        {
            if (event->type() == QEvent::KeyPress)
                leaveFullscreen();
            key->accept();
            return true;
        }
    }
    const bool videoSurface = qobject_cast<VideoWidget *>(target) || qobject_cast<BackgroundWidget *>(target);
    if (videoSurface &&
        (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease ||
         event->type() == QEvent::MouseButtonDblClick))
    {
        auto mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton)
        {
            if (event->type() == QEvent::MouseButtonPress)
                setProperty("videoPressPosition", mouse->globalPos());
            else if (event->type() == QEvent::MouseButtonDblClick)
            {
                videoClickTimer->stop();
                setProperty("ignoreVideoRelease", true);
                const QString action =
                    getSettings()->value("Redesign/doubleClick", "Toggle fullscreen").toString();
                if (action == "Play / Pause")
                    THEMIM->togglePlayPause();
                else if (action == "Toggle fullscreen")
                    THEAM->doAction(FULLSCREEN_ACTION);
                return true;
            }
            else if (property("ignoreVideoRelease").toBool())
                setProperty("ignoreVideoRelease", false);
            else if ((mouse->globalPos() - property("videoPressPosition").toPoint()).manhattanLength() <
                     QApplication::startDragDistance())
                videoClickTimer->start();
        }
    }
    if (target == duration && event->type() == QEvent::MouseButtonRelease)
    {
        remainingTime = !remainingTime;
        return true;
    }
    if (chromeCanHide() && event->type() == QEvent::MouseMove)
    {
        const auto point = window->mapFromGlobal(QCursor::pos());
        if (point.y() < 80 || point.y() > window->height() - 100)
        {
            if (point.y() < 80 && !header->isVisible())
                Reframe::animateHeight(header, true, 300);
            if (point.y() > window->height() - 100 && !transport->isVisible())
                Reframe::animateHeight(transport, true, 300);
            chromeTimer->start(qMax(1000, int(config_GetInt(p_intf, "mouse-hide-timeout"))));
        }
    }
    return QWidget::eventFilter(target, event);
}
void RedesignShell::showChrome()
{
    if (!header->isVisible())
        Reframe::animateHeight(header, true, 300);
    if (!transport->isVisible())
        Reframe::animateHeight(transport, true, 300);
    if (chromeCanHide())
        chromeTimer->start(qMax(1000, int(config_GetInt(p_intf, "mouse-hide-timeout"))));
}
bool RedesignShell::chromeCanHide() const
{
    const QString mode = getSettings()->value("Redesign/autoHideControls", "In fullscreen only").toString();
    return minimal ||
           (isPlayerVisible() && (mode == "Always" || (window->isFullScreen() && mode != "Never")));
}
void RedesignShell::leaveFullscreen()
{
    if (!property("windowReady").toBool())
        return;
    if (window->isFullScreen() || window->isInterfaceFullScreen() || var_GetBool(THEPL, "fullscreen"))
    {
        window->exitFullscreen();
        showChrome();
    }
}
void RedesignShell::toggleFullscreen()
{
    if (window->isFullScreen())
        leaveFullscreen();
    else
    {
        showPlayer();
        window->toggleInterfaceFullScreen();
    }
}
void RedesignShell::checkUpdates()
{
    auto process = new QProcess(this);
    connect(
        process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
        [this, process](int status, QProcess::ExitStatus)
        {
            if (status == 0)
            {
                auto data = QJsonDocument::fromJson(process->readAllStandardOutput()).object();
                const auto tag = data.value("tag_name").toString();
                if (QVersionNumber::compare(QVersionNumber::fromString(tag.mid(tag.startsWith('v') ? 1 : 0)),
                                            QVersionNumber(0, 3, 0)) > 0 &&
                    tag != getSettings()->value("Redesign/lastRelease").toString())
                {
                    getSettings()->setValue("Redesign/lastRelease", tag);
                    if (QMessageBox::question(window, "Reframe update",
                                              tag + " is available. Open the release page?") ==
                        QMessageBox::Yes)
                        QDesktopServices::openUrl(QUrl(data.value("html_url").toString()));
                }
            }
            process->deleteLater();
        });
    process->start("curl.exe",
                   QStringList() << "--silent"
                                 << "--fail"
                                 << "--max-time"
                                 << "15"
                                 << "--user-agent"
                                 << "Reframe"
                                 << "https://api.github.com/repos/webmaster94/vlc-redesign/releases/latest");
}

void RedesignShell::runFlowSmoke(const QString &reportPath, std::function<void(const QString &)> report)
{
    if (auto input = THEMIM->getInput())
        var_SetInteger(input, "state", PAUSE_S);
    if (settingsDialog)
        settingsDialog->hide();
    auto step = std::make_shared<int>(0);
    auto check = std::make_shared<std::function<void()>>();
    auto next = std::make_shared<std::function<void()>>();
    *next = [this, step, next, check, report, reportPath]
    {
        if (*step == 0)
            openMediaSettings(4);
        else if (*step < 4)
            openMediaSettings(*step + 4);
        else if (*step < 7)
            openTracks(*step - 4 + 5);
        else if (*step < 10)
            showQueue(*step - 7);
        else if (*step == 10)
            toggleMiniPlayer();
        else if (*step == 11)
        {
            showPlayer();
            customize();
        }
        else if (*step == 12)
            newPlaylist();
        else if (*step == 13)
            openNetwork();
        else if (*step == 14 || *step == 15)
            batchTracks(queuePaths(THEPL), *step - 14);
        else if (*step == 16)
            onboarding();
        else if (*step == 17 || *step == 18)
        {
            auto dialog = window->findChild<QDialog *>("onboardingDialog");
            if (dialog)
                dialog->findChild<QPushButton *>("onboardingNext")->click();
        }
        else if (*step == 19)
        {
            auto prefs = static_cast<RedesignPreferences *>(settingsDialog);
            prefs->editShortcuts();
        }
        else if (*step == 20)
            static_cast<RedesignPreferences *>(settingsDialog)->manageProfiles();
        else if (*step == 21)
        {
            auto bindings = new QTableWidget(1, 2, window);
            bindings->setItem(0, 0, new QTableWidgetItem("Integration sample"));
            bindings->setCellWidget(0, 1, new QPushButton("Choose tracks"));
            bindings->setCurrentCell(0, 0);
            openSubtitles(window, bindings);
        }
        else
        {
            if (sidePanel)
                sidePanel->hide();
            *next = nullptr;
            *check = nullptr;
            runInteractionSmoke(reportPath, report);
            return;
        }
        QTimer::singleShot(250, this, *check);
    };
    *check = [this, step, next, check, report, reportPath]
    {
        QWidget *surface = window;
        QDialog *dialog = nullptr;
        if (*step < 10)
            surface = sidePanel;
        else if (*step == 10)
            surface = miniPlayer;
        else
            for (auto candidate : window->findChildren<QDialog *>())
                if (candidate->isVisible() && candidate != settingsDialog)
                    dialog = candidate;
        if (dialog)
            surface = dialog;
        if (!surface || !surface->isVisible() || (*step > 10 && !dialog))
        {
            *next = nullptr;
            *check = nullptr;
            report(QString("FAIL: missing flow surface %1").arg(*step));
            return;
        }
        auto fail = [next, check, report](const QString &reason)
        {
            *next = nullptr;
            *check = nullptr;
            report("FAIL: " + reason);
        };
        if (*step == 17)
        {
            auto choices = surface->findChild<QWidget *>("onboardingAppearanceControls");
            auto preview = surface->findChild<QWidget *>("onboardingPreview");
            const auto colors = surface->findChildren<QAbstractButton *>("accentSwatch");
            QAbstractButton *original = nullptr;
            int selected = 0;
            for (auto color : colors)
                if (color->isChecked())
                {
                    original = color;
                    ++selected;
                }
            if (!choices || !preview || colors.size() != 9 || selected != 1 ||
                choices->mapTo(surface, QPoint(choices->width(), 0)).x() >=
                    preview->mapTo(surface, QPoint()).x())
            {
                fail("appearance choices must be left of the preview with one selected accent");
                return;
            }
            auto changed = colors.first() == original ? colors.last() : colors.first();
            changed->click();
            if (!changed->isChecked() || original->isChecked() ||
                QColor(getSettings()->value("Redesign/accent").toString()) !=
                    QColor(changed->property("accentColor").toString()) ||
                QApplication::palette().highlight().color() !=
                    QColor(changed->property("accentColor").toString()))
            {
                fail("accent selection did not update its check state, saved choice and live theme");
                return;
            }
            original->click();
            const QString savedTheme = getSettings()->value("Redesign/theme", "Dark").toString();
            auto themes = surface->findChild<QWidget *>("onboardingThemes");
            for (auto button : themes->findChildren<QPushButton *>())
                if (button->text() == "Light")
                    button->click();
            if (getSettings()->value("Redesign/theme").toString() != "Light" ||
                QApplication::palette().window().color().lightness() < 128)
            {
                fail("theme selection did not update the live appearance");
                return;
            }
            for (auto button : themes->findChildren<QPushButton *>())
                if (button->text() == savedTheme)
                    button->click();
        }
        if (*step == 18)
        {
            const auto rows = surface->findChildren<QWidget *>("onboardingShortcutRow");
            if (rows.size() != 12)
            {
                fail("shortcut guide must contain twelve individual icon and keycap rows");
                return;
            }
            for (auto row : rows)
            {
                auto icon = row->findChild<QLabel *>("shortcutIcon");
                const auto caps = row->findChildren<QLabel *>("shortcutKeycap");
                if (!icon || !icon->pixmap() || icon->pixmap()->isNull() || caps.isEmpty() ||
                    row->height() < 58)
                {
                    fail("shortcut row is missing its icon, keycaps or required spacing");
                    return;
                }
                for (auto other : rows)
                    if (other != row && row->geometry().intersects(other->geometry()))
                    {
                        fail("shortcut rows overlap");
                        return;
                    }
            }
        }
        surface->grab().save(reportPath + QString(".flow%1.png").arg(*step));
        for (auto scroll : surface->findChildren<QScrollArea *>())
            if (scroll->isVisible() && scroll->horizontalScrollBar()->maximum() > 0)
            {
                *next = nullptr;
                *check = nullptr;
                QString detail;
                for (auto child : scroll->widget()->findChildren<QWidget *>())
                    if (child->minimumSizeHint().width() > scroll->viewport()->width())
                        detail += QString("\n%1 %2 minimum %3")
                                      .arg(child->metaObject()->className(), child->objectName())
                                      .arg(child->minimumSizeHint().width());
                report(QString("FAIL: horizontal overflow on flow %1, viewport %2, content %3%4")
                           .arg(*step)
                           .arg(scroll->viewport()->width())
                           .arg(scroll->widget()->width())
                           .arg(detail));
                return;
            }
        surface->grab().save(reportPath + QString(".flow%1.png").arg(*step));
        if (*step == 10 && !miniPlayer->isAncestorOf(player))
        {
            *next = nullptr;
            *check = nullptr;
            report("FAIL: mini player did not contain the actual player");
            return;
        }
        if (dialog && (*step < 16 || *step > 17))
            dialog->close();
        ++*step;
        QTimer::singleShot(0, this, *next);
    };
    QTimer::singleShot(0, this, *next);
}
void RedesignShell::runInteractionSmoke(const QString &reportPath,
                                        std::function<void(const QString &)> report)
{
    char *raw = var_InheritString(p_intf, "qt-redesign-smoke-library");
    const QString directory = qfu(raw ? raw : "");
    free(raw);
    auto phase = std::make_shared<int>(0);
    auto next = std::make_shared<std::function<void()>>();
    auto expected = std::make_shared<QStringList>();
    auto collection = std::make_shared<QString>();
    auto initialQueue = std::make_shared<int>(queuePaths(THEPL).size());
    auto popup = std::make_shared<QComboBox *>(nullptr);
    auto fold = std::make_shared<QPushButton *>(nullptr);
    auto fail = [next, report](const QString &reason)
    {
        *next = nullptr;
        report("FAIL: " + reason);
    };
    *next = [this, reportPath, report, directory, phase, next, expected, collection, initialQueue, popup,
             fold, fail]
    {
        int delay = 350;
        if (*phase == 0)
        {
            if (QDir(directory).entryList(QStringList() << "*.ogg", QDir::Files).size() != 3)
            {
                fail("integration music directory must contain three generated Ogg tracks");
                return;
            }
            if (settingsDialog)
                settingsDialog->hide();
            pages->setCurrentWidget(browser);
            getSettings()->setValue("Redesign/MusicDirectory", directory);
            showLibrary("Music");
            delay = 1200;
        }
        else if (*phase == 1)
        {
            if (!files->isVisible() || files->count() != 3)
            {
                fail("generated Ogg tracks did not appear in the music library");
                return;
            }
            files->selectAll();
            for (auto item : files->selectedItems())
                *expected << QUrl::fromUserInput(item->data(Qt::UserRole).toString(), QString(),
                                                 QUrl::AssumeLocalFile)
                                 .toString();
            window->grab().save(reportPath + ".regression-selection.png");
            // Trigger the real library context-menu action so its captured selection is tested.
            QTimer::singleShot(50, this,
                               [fail]
                               {
                                   auto menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
                                   if (menu)
                                       for (auto action : menu->actions())
                                           if (action->menu() && action->text() == "Add to Playlist")
                                               for (auto child : action->menu()->actions())
                                                   if (child->text() == "New Playlist")
                                                   {
                                                       child->trigger();
                                                       menu->close();
                                                       return;
                                                   }
                                   if (menu)
                                       menu->close();
                                   fail("New Playlist context action was unavailable");
                               });
            contextMenu(files->visualItemRect(files->item(0)).center());
        }
        else if (*phase == 2)
        {
            auto dialog = window->findChild<QDialog *>("playlistEditor");
            auto list = dialog ? dialog->findChild<QListWidget *>("playlistFiles") : nullptr;
            if (!list || list->count() != expected->size() || list->count() != 3 ||
                !list->visualItemRect(list->item(0)).isValid())
            {
                fail("New Playlist did not render the selected library files");
                return;
            }
            for (int i = 0; i < list->count(); ++i)
                if (!expected->contains(list->item(i)->data(Qt::UserRole).toString()))
                {
                    fail("New Playlist used queue contents instead of the library selection");
                    return;
                }
            dialog->grab().save(reportPath + ".regression-selected-files.png");
            dialog->findChild<QLineEdit *>("playlistName")->setText("Generated music selection");
            dialog->findChild<QPushButton *>("playlistSave")->click();
        }
        else if (*phase == 3)
        {
            for (const auto &id : getSettings()->value("Redesign/collections").toStringList())
            {
                const auto data = getSettings()->value("Redesign/collection/" + id).toMap();
                if (data.value("name").toString() == "Generated music selection")
                {
                    *collection = id;
                    if (data.value("paths").toStringList() != *expected)
                    {
                        fail("saved playlist lost selected paths or added duplicates");
                        return;
                    }
                }
            }
            if (collection->isEmpty())
            {
                fail("selected-file playlist was not saved");
                return;
            }
            openMedia("collection:" + *collection);
            delay = 1200;
        }
        else if (*phase == 4)
        {
            auto input = THEMIM->getInput();
            if (!input || var_GetInteger(input, "state") != PLAYING_S ||
                queuePaths(THEPL).size() != *initialQueue + 3)
            {
                fail("saved Ogg playlist did not start or appended an unexpected number of tracks");
                return;
            }
            showQueue();
            auto queue = sidePanel->findChild<QListWidget *>("queueItems");
            queue->setProperty("smokeInsertedRows", 0);
            connect(queue->model(), &QAbstractItemModel::rowsInserted, queue,
                    [queue](const QModelIndex &, int first, int last)
                    {
                        queue->setProperty("smokeInsertedRows",
                                           queue->property("smokeInsertedRows").toInt() + last - first + 1);
                    });
            Open::openMRL(p_intf, expected->first(), false);
        }
        else if (*phase == 5)
        {
            updateQueue();
            auto list = sidePanel->findChild<QListWidget *>("queueItems");
            if (!list || list->count() != *initialQueue + 4 ||
                list->property("smokeInsertedRows").toInt() < 1 ||
                !list->visualItemRect(list->item(0)).isValid() ||
                list->visualItemRect(list->item(0)).height() < 40)
            {
                fail("queue backend had tracks but the view did not lay out visible rows");
                return;
            }
            sidePanel->grab().save(reportPath + ".regression-queue.png");
            // Remove the extra copy through the same queue button the user uses.
            list->setCurrentItem(list->item(list->count() - 1));
            for (auto button : sidePanel->findChildren<QPushButton *>())
                if (button->text() == "Remove")
                    button->click();
            sidePanel->hide();
            auto fullscreen = controlButtons.value("fullscreen");
            if (!fullscreen || !transport->isAncestorOf(fullscreen) ||
                fullscreen->mapTo(transport, QPoint()).x() < transport->width() / 2 || volume->height() < 28)
            {
                fail("fullscreen placement or volume wedge geometry");
                return;
            }
            getSettings()->setValue("Redesign/autoHideControls", "In fullscreen only");
            config_PutInt(p_intf, "mouse-hide-timeout", 500);
            fullscreen->click();
            QCursor::setPos(window->frameGeometry().center());
            delay = 1600;
        }
        else if (*phase == 6)
        {
            if (!window->isFullScreen() || header->isVisible() || transport->isVisible())
            {
                fail("title and transport did not hide in interface fullscreen");
                return;
            }
            QCursor::setPos(window->mapToGlobal(QPoint(window->width() / 2, 4)));
            window->activateWindow();
            delay = 500;
        }
        else if (*phase == 7)
        {
            if (!header->isVisible())
            {
                fail("fullscreen chrome did not reveal at the top edge");
                return;
            }
            QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
            QApplication::sendEvent(window, &escape);
        }
        else if (*phase == 8)
        {
            if (window->isFullScreen() || var_GetBool(THEPL, "fullscreen") || !transport->isVisible())
            {
                fail("Escape did not restore normal interface fullscreen state");
                return;
            }
            openMedia(QDir(directory).filePath("generated-video.y4m"));
            delay = 1200;
        }
        else if (*phase == 9)
        {
            auto output = THEMIM->getVout();
            if (!output)
            {
                fail("generated video did not create a real video output");
                return;
            }
            vlc_object_release(output);
            toggleFullscreen();
            QCursor::setPos(window->frameGeometry().center());
            delay = 1600;
        }
        else if (*phase == 10)
        {
            auto output = THEMIM->getVout();
            bool videoFullscreen = output && var_GetBool(output, "fullscreen");
            if (output)
                vlc_object_release(output);
            if (!window->isFullScreen() || !videoFullscreen || header->isVisible() || transport->isVisible())
            {
                fail("video fullscreen state and chrome hiding were inconsistent");
                return;
            }
            // Native video surfaces send Escape to VLC's action dispatcher.
            var_SetInteger(p_intf->obj.libvlc, "key-pressed", KEY_ESC);
        }
        else if (*phase == 11)
        {
            auto output = THEMIM->getVout();
            bool videoFullscreen = output && var_GetBool(output, "fullscreen");
            if (output)
                vlc_object_release(output);
            if (window->isFullScreen() || videoFullscreen || !transport->isVisible())
            {
                fail("Escape did not exit video fullscreen");
                return;
            }
            toggleFullscreen();
            showLibrary("Home");
        }
        else if (*phase == 12)
        {
            if (window->isFullScreen() || !miniPlayer || !miniPlayer->isVisible())
            {
                fail("library navigation did not leave fullscreen and open Now Playing");
                return;
            }
            const QRect parent =
                window->frameGeometry().intersected(QApplication::desktop()->availableGeometry(window));
            if (std::abs(miniPlayer->geometry().right() - (parent.right() - 16)) > 2 ||
                std::abs(miniPlayer->geometry().bottom() - (parent.bottom() - 16)) > 2 ||
                !miniPlayer->findChild<QSizeGrip *>("dialogResizeGrip"))
            {
                fail("Now Playing did not start at the bottom right with a resize handle");
                return;
            }
            miniPlayer->grab().save(reportPath + ".regression-mini-player.png");
            showPlayer();
            openSettings(0);
        }
        else if (*phase == 13)
        {
            for (auto combo : settingsDialog->findChildren<QComboBox *>())
                if (combo->isVisible() && combo->count() > 1)
                {
                    *popup = combo;
                    break;
                }
            if (!*popup || !settingsDialog->findChild<QSizeGrip *>("dialogResizeGrip"))
            {
                fail("settings dropdown or resize handle unavailable");
                return;
            }
            (*popup)->showPopup();
        }
        else if (*phase == 14)
        {
            auto container = (*popup)->view()->window();
            const QRect available = QApplication::desktop()->availableGeometry(*popup);
            const QPoint below = (*popup)->mapToGlobal(QPoint(0, (*popup)->height() + 2));
            if (!container->isVisible() || (below.y() + container->height() <= available.bottom() + 1 &&
                                            std::abs(container->y() - below.y()) > 2))
            {
                fail("dropdown did not open immediately below its field");
                return;
            }
            container->grab().save(reportPath + ".regression-dropdown.png");
            (*popup)->hidePopup();
            for (auto button : settingsDialog->findChildren<QPushButton *>("foldButton"))
                if (button->isVisible() && button->text().contains("Advanced Options"))
                {
                    *fold = button;
                    break;
                }
            if (!*fold)
            {
                fail("advanced options fold unavailable");
                return;
            }
            (*fold)->click();
            delay = 500;
        }
        else if (*phase == 15)
        {
            auto content = (*fold)->parentWidget()->findChild<QWidget *>("foldContent");
            if (!content || !content->isVisible() || content->height() <= 0 || !(*fold)->isChecked())
            {
                fail("advanced fold did not expand after its transition");
                return;
            }
            settingsDialog->grab().save(reportPath + ".regression-advanced.png");
            (*fold)->click();
            delay = 500;
        }
        else if (*phase == 16)
        {
            if ((*fold)->parentWidget()->findChild<QWidget *>("foldContent")->isVisible())
            {
                fail("advanced fold did not close after its transition");
                return;
            }
            settingsDialog->hide();
            THEMIM->stop();
            showPlayer();
            delay = 600;
        }
        else
        {
            window->grab().save(reportPath + ".regression-empty-player.png");
            openSettings(6);
            for (auto scroll : settingsDialog->findChildren<QScrollArea *>())
                if (scroll->isVisible())
                    scroll->ensureWidgetVisible(settingsDialog->findChild<QWidget *>("equalizerPanel"), 20,
                                                20);
            *next = nullptr;
            report(
                "PASS: playback pause/resume/seek, 13 settings pages, 22 panel/dialog flows, onboarding "
                "layout "
                "and selection, equalizer and subtitle timing; Ogg "
                "library/context-selection/playlist/save/playback, "
                "visible queue rows, volume/fullscreen control geometry, interface and video fullscreen "
                "hide/reveal/Escape/navigation, bottom-right Now Playing, resize handles, anchored dropdown "
                "and advanced fold transitions");
            return;
        }
        ++*phase;
        if (*next)
            QTimer::singleShot(delay, this, *next);
    };
    QTimer::singleShot(0, this, *next);
}
