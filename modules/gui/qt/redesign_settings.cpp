/* Copyright 2026 webmaster94. GPL-3.0-or-later.
 * Native controls for Juxtopposed's complete VLC Redesign, CC BY 4.0.
 * Attribution: doc/redesign/licensing.md. */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "redesign_settings.hpp"
#include "main_interface.hpp"
#include "input_manager.hpp"
#include "actions_manager.hpp"
#include "redesign.hpp"
#include "redesign_profile.hpp"
#include "redesign_seek_data.hpp"
#include "util/customwidgets.hpp"
#include "dialogs_provider.hpp"
#include "recents.hpp"
#include "menus.hpp"
#include <QMenu>
#include <QActionGroup>
#include <vlc_configuration.h>
#include <vlc_modules.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_vout.h>
#include <vlc_actions.h>
#include <QApplication>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QListWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QFileDialog>
#include <QColorDialog>
#include <QFontComboBox>
#include <QKeySequenceEdit>
#include <QSettings>
#include <QSignalBlocker>
#include <QDesktopServices>
#include <QInputDialog>
#include <QMessageBox>
#include <QKeyEvent>
#include <QStyledItemDelegate>
#include <QSaveFile>
#include <QToolTip>
#include <QTextDocument>
#include <QStandardPaths>
#include <QPainter>
#include <QProcess>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QTextStream>
#include <QButtonGroup>
#include <memory>
#include "../../audio_filter/equalizer_presets.h"
#include <climits>
#include <cmath>

namespace
{
QString cfgString(intf_thread_t *intf, const char *name)
{
    char *p = config_GetPsz(intf, name);
    QString value = QString::fromUtf8(p ? p : "");
    free(p);
    return value;
}
QStringList tokens(const char *value)
{
    return QString::fromUtf8(value).split('|', QString::SkipEmptyParts);
}
QString inputName(const QString &key)
{
    if (key == "sub-track")
        return "spu-es";
    if (key == "audio-track")
        return "audio-es";
    if (key == "sub-delay")
        return "spu-delay";
    if (key == "audio-desync")
        return "audio-delay";
    if (key == "rate" || key == "sub-fps" || key == "sub-text-scale")
        return key;
    return QString();
}
bool configVisible(const module_config_t &cfg)
{
    return cfg.psz_name && !cfg.b_internal && !cfg.b_removed && !cfg.b_unsaveable && (cfg.i_type & 0xF0);
}
class SettingsNavigation : public QStyledItemDelegate
{
  public:
    explicit SettingsNavigation(QObject *parent) : QStyledItemDelegate(parent) {}
    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &index) const override
    {
        return QSize(200, index.data(Qt::UserRole + 1).toString().isEmpty() ? 44 : 72);
    }
    void paint(QPainter *p, const QStyleOptionViewItem &o, const QModelIndex &index) const override
    {
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        QRect row = o.rect;
        const QString group = index.data(Qt::UserRole + 1).toString();
        if (!group.isEmpty())
        {
            p->setPen(QColor("#6b6b6b"));
            p->drawText(row.adjusted(10, 0, 0, 0), Qt::AlignTop | Qt::AlignLeft, group);
            row.setTop(row.top() + 28);
        }
        const bool selected = o.state & QStyle::State_Selected;
        if (selected)
        {
            p->setPen(Qt::NoPen);
            p->setBrush(o.palette.button().color().lighter(135));
            p->drawRoundedRect(row.adjusted(0, 2, 0, -2), 3, 3);
        }
        const auto color = selected ? o.palette.highlight().color() : o.palette.text().color();
        Reframe::icon(index.data(Qt::UserRole + 2).toString(), color)
            .paint(p, QRect(row.left() + 10, row.top() + 10, 24, 24));
        p->setPen(color);
        p->drawText(row.adjusted(44, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, index.data().toString());
        p->restore();
    }
};
} // namespace
QStringList RedesignPreferences::categories()
{
    return QStringList() << "General"
                         << "Interface"
                         << "Privacy"
                         << "Controls"
                         << "Playback"
                         << "Video"
                         << "Audio"
                         << "Subtitles"
                         << "Library"
                         << "Network"
                         << "Performance"
                         << "Recording"
                         << "Extensions";
}
RedesignPreferences::RedesignPreferences(intf_thread_t *intf, MainInterface *main, int selected,
                                         std::function<void()> changed, bool media)
    : Reframe::Dialog(main, media ? "Media Settings" : "Global Settings",
                      media ? QSize(440, 760) : QSize(1048, 865)),
      p_intf(intf), window(main), local(media), saveTimer(this), themeChanged(changed)
{
    setObjectName(local ? "mediaSettings" : "settingsDialog");
    saveTimer.setSingleShot(true);
    saveTimer.setInterval(400);
    connect(&saveTimer, &QTimer::timeout, this, [this] { config_SaveConfigFile(p_intf); });
    auto filter = new QLineEdit;
    filter->setPlaceholderText("Search");
    filter->setAccessibleName("Search settings");
    filter->setClearButtonEnabled(true);
    filter->addAction(Reframe::icon("search"), QLineEdit::LeadingPosition);
    auto content = new QHBoxLayout;
    content->setContentsMargins(0, 0, 0, 0);
    content->setSpacing(24);
    navigation = new QListWidget;
    navigation->setObjectName("settingsNav");
    navigation->setFixedWidth(216);
    navigation->setItemDelegate(new SettingsNavigation(navigation));
    pages = new QStackedWidget;
    if (!local)
    {
        auto root = static_cast<QVBoxLayout *>(layout());
        root->setContentsMargins(14, 14, 14, 14);
        auto header = static_cast<QHBoxLayout *>(root->itemAt(0)->layout());
        filter->setFixedWidth(208);
        header->insertWidget(0, filter);
        header->setSpacing(24);
        content->addWidget(navigation);
    }
    content->addWidget(pages, 1);
    body->addLayout(content, 1);
    const auto labels = categories();
    for (int i = 0; i < labels.size(); ++i)
    {
        const QStringList icons = QStringList() << "sliders"
                                                << "pointer"
                                                << "shield"
                                                << "keyboard"
                                                << "play"
                                                << "videos"
                                                << "audio"
                                                << "subtitles"
                                                << "folder"
                                                << "network"
                                                << "sliders"
                                                << "record"
                                                << "puzzle";
        auto item = new QListWidgetItem(Reframe::icon(icons[i]), labels[i], navigation);
        item->setSizeHint(QSize(200, 44));
        item->setData(Qt::UserRole + 2, icons[i]);
        QWidget *page = new QWidget;
        page->setProperty("page", i);
        pages->addWidget(page);
    }
    // Lazy construction keeps the hundreds of advanced plugin options cheap.
    auto select = [this, labels](int i)
    {
        if (i < 0 || i >= pages->count())
            return;
        auto old = pages->widget(i);
        if (!old->property("built").toBool())
        {
            auto next = buildPage(i);
            next->setProperty("built", true);
            pages->removeWidget(old);
            pages->insertWidget(i, next);
            delete old;
        }
        pages->setCurrentIndex(i);
        auto title = findChild<QLabel *>("dialogHeading");
        if (title)
            title->setText(local    ? QString(labels[i] + " Settings")
                           : i == 0 ? QString("Global Settings")
                                    : QString(labels[i] + (i >= 5 && i <= 7 ? " Defaults" : " Settings")));
    };
    connect(navigation, &QListWidget::currentRowChanged, this, select);
    if (local)
    {
        auto root = static_cast<QVBoxLayout *>(layout());
        auto headerItem = root->takeAt(0);
        auto header = headerItem->layout();
        while (auto item = header->takeAt(0))
        {
            if (item->widget())
                item->widget()->hide();
            delete item;
        }
        delete headerItem;
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);
        body->setSpacing(0);
        filter->deleteLater();
        navigation->hide();
        auto tabs = new QWidget;
        tabs->setFixedHeight(50);
        auto tabrow = new QHBoxLayout(tabs);
        tabrow->setContentsMargins(0, 0, 0, 0);
        tabrow->setSpacing(0);
        auto group = new QButtonGroup(tabs);
        group->setExclusive(true);
        for (int i = 4; i <= 7; ++i)
        {
            auto button = new QPushButton(labels[i]);
            button->setCheckable(true);
            button->setChecked(i == selected);
            button->setObjectName("mediaTab");
            button->setIcon(Reframe::icon(i == 4   ? "play"
                                          : i == 5 ? "videos"
                                          : i == 6 ? "audio"
                                                   : "subtitles"));
            button->setIconSize(QSize(18, 18));
            button->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
            group->addButton(button);
            tabrow->addWidget(button);
            connect(button, &QPushButton::clicked, this, [this, i] { selectPage(i); });
        }
        body->insertWidget(0, tabs);
    }
    else
    {
        // Section names correspond to the App / Media / System groups in Figma.
        for (int i : {0, 4, 9})
            navigation->item(i)->setData(Qt::UserRole + 1, i == 0 ? "App" : i == 4 ? "Media" : "System");
        connect(filter, &QLineEdit::textChanged, this,
                [this](const QString &term)
                {
                    if (term.isEmpty())
                    {
                        for (int i = 0; i < navigation->count(); ++i)
                            navigation->item(i)->setHidden(false);
                        return;
                    }
                    const int selected = navigation->currentRow();
                    for (int i = 0; i < pages->count(); ++i)
                        if (!pages->widget(i)->property("built").toBool())
                        {
                            auto old = pages->widget(i);
                            auto next = buildPage(i);
                            next->setProperty("built", true);
                            pages->removeWidget(old);
                            pages->insertWidget(i, next);
                            delete old;
                        }
                    pages->setCurrentIndex(selected);
                    for (int i = 0; i < navigation->count(); ++i)
                    {
                        auto widget = pages->widget(i);
                        bool match = navigation->item(i)->text().contains(term, Qt::CaseInsensitive);
                        if (widget->property("built").toBool())
                            for (auto label : widget->findChildren<QLabel *>())
                                match = match || label->text().contains(term, Qt::CaseInsensitive);
                        navigation->item(i)->setHidden(!match);
                    }
                    for (int i = 0; i < navigation->count(); ++i)
                        if (!navigation->item(i)->isHidden())
                        {
                            navigation->setCurrentRow(i);
                            break;
                        }
                });
    }
    selectPage(selected);
}
RedesignPreferences::~RedesignPreferences()
{
    if (saveTimer.isActive())
        config_SaveConfigFile(p_intf);
}
void RedesignPreferences::selectPage(int page)
{
    navigation->setCurrentRow(qBound(0, page, pages->count() - 1));
    if (local)
        for (auto button : findChildren<QPushButton *>("mediaTab"))
            button->setChecked(button->text() == categories().value(page));
}
int RedesignPreferences::currentPage() const
{
    return navigation->currentRow();
}
void RedesignPreferences::openAdvanced(int page)
{
    selectPage(page);
    for (auto button : pages->currentWidget()->findChildren<QPushButton *>("foldButton"))
        if (button->text().contains("Advanced"))
        {
            button->setChecked(true);
            if (auto scroll = qobject_cast<QScrollArea *>(pages->currentWidget()))
                scroll->ensureWidgetVisible(button);
            break;
        }
}
void RedesignPreferences::saveAppProfile(const QString &name)
{
    QVariantMap settings;
    size_t count;
    auto modules = module_list_get(&count);
    for (size_t m = 0; m < count; ++m)
    {
        unsigned n;
        auto configs = module_config_get(modules[m], &n);
        for (unsigned i = 0; i < n; ++i)
            if (configVisible(configs[i]) && configs[i].i_type != CONFIG_ITEM_PASSWORD)
                settings[qfu(configs[i].psz_name)] = read(qfu(configs[i].psz_name));
        module_config_free(configs);
    }
    module_list_free(modules);
    for (const auto &key :
         QStringList({"theme",          "accent",           "toolbar",           "toolbar2",
                      "leftControls",   "topControls",      "compactOnPlayback", "VideosDirectory",
                      "MusicDirectory", "autoHideControls", "seekThumbnails",    "waveform",
                      "audioWaveform",  "libraryList",      "pinHeader",         "pinControls",
                      "pinSidebar",     "pinPanel",         "subtitleTimeline",  "pauseOtherAudio",
                      "singleClick",    "doubleClick",      "reverseScroll",     "notificationDuration",
                      "startupVolume",  "nightMode",        "rememberWindow",    "checkUpdates",
                      "extraArgs",      "useSkin",          "systemProxy"}))
        settings["ui:" + key] = read("ui:" + key);
    settings["ui:theme"] = getSettings()->value("Redesign/theme", "Dark");
    settings["ui:accent"] = getSettings()->value("Redesign/accent", "#d37918");
    for (auto it = settings.begin(); it != settings.end();)
    {
        if (!it.value().isValid())
            it = settings.erase(it);
        else
            ++it;
    }
    settings = Reframe::portableProfile(settings);
    getSettings()->setValue("Redesign/profiles/" + name, settings);
    auto names = getSettings()->value("Redesign/profileNames").toStringList();
    if (name != "Default" && !names.contains(name))
    {
        names << name;
        getSettings()->setValue("Redesign/profileNames", names);
    }
}
void RedesignPreferences::restore(const QVariantMap &values)
{
    for (auto it = values.begin(); it != values.end(); ++it)
    {
        if (!Reframe::profileValueSafe(it.key(), it.value()))
            continue;
        if (auto cfg = config_FindConfig(qtu(it.key())))
        {
            const int type = cfg->i_type & 0xF0;
            if (type == CONFIG_ITEM_INTEGER &&
                (std::floor(it.value().toDouble()) != it.value().toDouble() ||
                 (cfg->max.i > cfg->min.i &&
                  (it.value().toDouble() < cfg->min.i || it.value().toDouble() > cfg->max.i))))
                continue;
            if (type == CONFIG_ITEM_FLOAT && cfg->max.f > cfg->min.f &&
                (it.value().toDouble() < cfg->min.f || it.value().toDouble() > cfg->max.f))
                continue;
        }
        if (it.key().startsWith("ordinal:"))
        {
            const QString key = it.key().mid(8);
            auto input = THEMIM->getInput();
            if (!local)
            {
                write(key, it.value());
                continue;
            }
            if (!input)
                continue;
            const auto name = inputName(key);
            vlc_value_t values, texts;
            if (var_Change(input, qtu(name), VLC_VAR_GETCHOICES, &values, &texts))
                continue;
            int ordinal = 0;
            for (int i = 0; i < values.p_list->i_count; ++i)
                if (values.p_list->p_values[i].i_int >= 0 && ordinal++ == it.value().toInt())
                {
                    write(key, qlonglong(values.p_list->p_values[i].i_int));
                    break;
                }
            var_FreeList(&values, &texts);
        }
        else
            write(it.key(), it.value());
    }
    const int page = navigation->currentRow();
    auto old = pages->widget(page);
    auto next = buildPage(page);
    next->setProperty("built", true);
    pages->removeWidget(old);
    pages->insertWidget(page, next);
    pages->setCurrentIndex(page);
    old->deleteLater();
}

static bool audioContext(intf_thread_t *p_intf)
{
    auto item = THEMIM->currentInputItem();
    char *uri = item ? input_item_GetURI(item) : nullptr;
    const bool result = ReframeSeekData::audioMediaName(qfu(uri ? uri : "")) ||
                        (THEMIM->getIM()->hasAudio() && !THEMIM->getIM()->hasVideo());
    free(uri);
    return result;
}
QVariant RedesignPreferences::read(const QString &key) const
{
    if (key == "ui:audioWaveform")
        return getSettings()->value("Redesign/audioWaveform", true);
    if (key == "ui:waveform")
        return getSettings()->value(audioContext(p_intf) ? "Redesign/audioWaveform" : "Redesign/waveform",
                                    audioContext(p_intf));
    if (!local && key == "sub-delay")
    {
        const auto precise = getSettings()->value("Redesign/preciseSubtitleDelay");
        if (precise.isValid() && qint64(precise.toDouble()) == config_GetInt(p_intf, "sub-delay"))
            return precise;
    }
    if (key == "ui:toolbar")
        return getSettings()->value("Redesign/toolbar", Reframe::defaultToolbar());
    if (QStringList({"ui:toolbar2", "ui:leftControls", "ui:topControls"}).contains(key))
        return getSettings()->value("Redesign/" + key.mid(3), QStringList());
    if (key == "ui:VideosDirectory" || key == "ui:MusicDirectory")
        return getSettings()->value("Redesign/" + key.mid(3),
                                    QStandardPaths::writableLocation(key == "ui:VideosDirectory"
                                                                         ? QStandardPaths::MoviesLocation
                                                                         : QStandardPaths::MusicLocation));
    if (key == "ui:extraArgs")
    {
        char *directory = config_GetUserDir(VLC_CONFIG_DIR);
        QFile file(qfu(directory) + "/extra-options.txt");
        free(directory);
        return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
    }
    if (key == "ui:useSkin")
        return cfgString(p_intf, "intf").contains("skins2");
    if (key == "ui:systemProxy")
        return cfgString(p_intf, "http-proxy").isEmpty();
    if (key == "ui:visualizer")
    {
        auto output = THEMIM->getAout();
        if (output)
        {
            char *mode = var_GetString(output, "visual");
            const QString name = qfu(mode ? mode : "");
            free(mode);
            vlc_object_release(output);
            return name;
        }
        return QString();
    }
    if (key == "ui:singleClick")
        return getSettings()->value("Redesign/singleClick", "Play / Pause");
    if (key == "ui:doubleClick")
        return getSettings()->value("Redesign/doubleClick", "Toggle fullscreen");
    if (key == "ui:notificationDuration")
        return getSettings()->value("Redesign/notificationDuration", 3);
    if (key == "ui:startupVolume")
        return getSettings()->value("Redesign/startupVolume", 100);
    if (key == "ui:autoHideControls")
        return getSettings()->value("Redesign/autoHideControls", "In fullscreen only");
    if (key.startsWith("ui:"))
        return getSettings()->value(
            "Redesign/" + key.mid(3),
            QStringList({"ui:compactOnPlayback", "ui:rememberWindow", "ui:seekThumbnails"}).contains(key));
    const auto input = THEMIM->getInput();
    const auto name = inputName(key);
    if (local && input && !name.isEmpty() && var_Type(input, qtu(name)))
    {
        if (key == "rate" || key == "sub-fps")
            return var_GetFloat(input, qtu(name));
        qint64 value = var_GetInteger(input, qtu(name));
        return key == "sub-delay"      ? value / double(CLOCK_FREQ) * 10
               : key == "audio-desync" ? value / double(CLOCK_FREQ) * 1000
                                       : double(value);
    }
    if (local)
    {
        auto get = [&](vlc_object_t *obj) -> QVariant
        {
            const int type = var_Type(obj, qtu(key)) & VLC_VAR_CLASS;
            if (type == VLC_VAR_FLOAT)
                return var_GetFloat(obj, qtu(key));
            if (type == VLC_VAR_INTEGER)
                return QVariant::fromValue<qlonglong>(var_GetInteger(obj, qtu(key)));
            if (type == VLC_VAR_BOOL)
                return var_GetBool(obj, qtu(key));
            if (type == VLC_VAR_STRING)
            {
                char *raw = var_GetString(obj, qtu(key));
                const QString text = qfu(raw ? raw : "");
                free(raw);
                return text;
            }
            return QVariant();
        };
        auto aout = THEMIM->getAout();
        if (aout)
        {
            const auto value = get(VLC_OBJECT(aout));
            vlc_object_release(aout);
            if (value.isValid())
                return value;
        }
        auto vout = THEMIM->getVout();
        if (vout)
        {
            const auto value = get(VLC_OBJECT(vout));
            vlc_object_release(vout);
            if (value.isValid())
                return value;
        }
        const auto value = get(VLC_OBJECT(THEPL));
        if (value.isValid())
            return value;
    }
    if (localValues.contains(key))
        return localValues.value(key);
    const auto cfg = config_FindConfig(qtu(key));
    if (!cfg || cfg->b_removed)
        return QVariant();
    switch (cfg->i_type & 0xF0)
    {
    case CONFIG_ITEM_BOOL:
    case CONFIG_ITEM_INTEGER:
        return QVariant::fromValue<qlonglong>(config_GetInt(p_intf, qtu(key)));
    case CONFIG_ITEM_FLOAT:
        return config_GetFloat(p_intf, qtu(key));
    case CONFIG_ITEM_STRING:
        return cfgString(p_intf, qtu(key));
    }
    return QVariant();
}
void RedesignPreferences::write(const QString &key, const QVariant &value)
{
    edited.insert(key);
    if (key.startsWith("ui:"))
    {
        if (key == "ui:waveform" && audioContext(p_intf))
        {
            getSettings()->setValue("Redesign/audioWaveform", value.toBool());
            themeChanged();
            return;
        }
        if (key == "ui:visualizer")
        {
            auto output = THEMIM->getAout();
            if (output)
            {
                var_SetString(output, "visual", qtu(value.toString()));
                vlc_object_release(output);
            }
            if (!local)
            {
                config_PutPsz(p_intf, "audio-visual", value.toString().isEmpty() ? "none" : "visual");
                config_PutPsz(p_intf, "effect-list", qtu(value.toString()));
                config_SaveConfigFile(p_intf);
            }
            else
                localValues[key] = value;
            return;
        }
        getSettings()->setValue("Redesign/" + key.mid(3), value);
        if (key == "ui:extraArgs")
        {
            char *directory = config_GetUserDir(VLC_CONFIG_DIR);
            const QString path = qfu(directory);
            free(directory);
            QDir().mkpath(path);
            QSaveFile file(path + "/extra-options.txt");
            const QByteArray bytes = value.toString().toUtf8();
            if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
                QMessageBox::warning(this, "Additional options", "The startup options could not be saved.");
        }
        if (key == "ui:useSkin")
        {
            config_PutPsz(p_intf, "intf", value.toBool() ? "skins2" : "qt");
            config_SaveConfigFile(p_intf);
        }
        if (key == "ui:systemProxy")
        {
            if (value.toBool())
            {
                getSettings()->setValue("Redesign/manualProxy", cfgString(p_intf, "http-proxy"));
                config_PutPsz(p_intf, "http-proxy", "");
            }
            else
                config_PutPsz(p_intf, "http-proxy",
                              qtu(getSettings()->value("Redesign/manualProxy").toString()));
            config_SaveConfigFile(p_intf);
        }
        if (key == "ui:webRemote")
        {
            if (value.toBool())
                config_AddIntf(p_intf, "http");
            else
                config_RemoveIntf(p_intf, "http");
            config_SaveConfigFile(p_intf);
        }
        if (key == "ui:lastFm")
        {
            if (value.toBool())
                config_AddIntf(p_intf, "audioscrobbler");
            else
                config_RemoveIntf(p_intf, "audioscrobbler");
            config_SaveConfigFile(p_intf);
        }
        if (key == "ui:discoverCast")
        {
            if (value.toBool())
                THEAM->StartRendererScan();
            else
                THEAM->StopRendererScan();
        }
        if (key == "ui:developerMode")
            config_PutInt(p_intf, "verbose", value.toBool() ? 2 : -1);
        if (key == "ui:startupVolume")
            playlist_VolumeSet(THEPL, value.toDouble() / 100);
        if (key == "ui:nightMode")
        {
            if (value.toBool())
            {
                write("compressor-threshold", -20.0);
                write("compressor-ratio", 8.0);
                write("compressor-makeup-gain", 4.0);
            }
            filterEnabled("compressor", value.toBool());
        }
        if (key == "ui:sleepMinutes")
        {
            auto timer = window->findChild<QTimer *>("reframeSleepTimer");
            if (timer)
            {
                timer->stop();
                timer->deleteLater();
            }
            if (value.toDouble() > 0)
            {
                timer = new QTimer(window);
                timer->setObjectName("reframeSleepTimer");
                timer->setSingleShot(true);
                playlist_t *playlist = THEPL;
                connect(timer, &QTimer::timeout, window, [playlist] { playlist_Stop(playlist); });
                timer->start(qRound(value.toDouble() * 60000));
            }
        }
        if (key.startsWith("ui:crop-"))
        {
            auto output = THEMIM->getVout();
            if (output)
            {
                var_SetInteger(output, qtu(key.mid(3)), value.toInt());
                vlc_object_release(output);
            }
        }
        themeChanged();
        return;
    }
    auto cfg = config_FindConfig(qtu(key));
    if (!cfg || cfg->b_removed)
        return;
    const int type = cfg->i_type & 0xF0;
    if (local)
    {
        localValues[key] = value;
        auto keys = window->property("reframeLocalKeys").toStringList();
        if (!keys.contains(key))
        {
            keys << key;
            window->setProperty("reframeLocalKeys", keys);
        }
    }
    else
    {
        if (key == "sub-delay")
            getSettings()->setValue("Redesign/preciseSubtitleDelay", value.toDouble());
        if (type == CONFIG_ITEM_STRING)
            config_PutPsz(p_intf, qtu(key), qtu(value.toString()));
        else if (type == CONFIG_ITEM_FLOAT)
            config_PutFloat(p_intf, qtu(key), value.toDouble());
        else
            config_PutInt(p_intf, qtu(key), value.toLongLong());
        saveTimer.start();
    }
    auto set = [&](vlc_object_t *obj, const QString &name, const QVariant &v)
    {
        const auto t = var_Type(obj, qtu(name)) & VLC_VAR_CLASS;
        if (t == VLC_VAR_STRING)
            var_SetString(obj, qtu(name), qtu(v.toString()));
        else if (t == VLC_VAR_FLOAT)
            var_SetFloat(obj, qtu(name), v.toDouble());
        else if (t == VLC_VAR_BOOL)
            var_SetBool(obj, qtu(name), v.toBool());
        else if (t == VLC_VAR_INTEGER)
            var_SetInteger(obj, qtu(name), v.toLongLong());
    };
    const QString name = inputName(key);
    auto input = THEMIM->getInput();
    if (input && !name.isEmpty())
    {
        QVariant runtime = value;
        if (!local && (key == "sub-track" || key == "audio-track"))
        {
            vlc_value_t values, texts;
            if (!var_Change(input, qtu(name), VLC_VAR_GETCHOICES, &values, &texts))
            {
                int ordinal = 0;
                runtime = QVariant();
                for (int i = 0; i < values.p_list->i_count; ++i)
                    if (values.p_list->p_values[i].i_int >= 0 && ordinal++ == value.toInt())
                    {
                        runtime = qlonglong(values.p_list->p_values[i].i_int);
                        break;
                    }
                var_FreeList(&values, &texts);
            }
            else
                runtime = QVariant();
        }
        if (runtime.isValid())
            set(VLC_OBJECT(input), name,
                key == "sub-delay" ? QVariant(qlonglong(std::llround(value.toDouble() * CLOCK_FREQ / 10)))
                : key == "audio-desync"
                    ? QVariant(qlonglong(std::llround(value.toDouble() * CLOCK_FREQ / 1000)))
                    : runtime);
    }
    if (key == "rate" || key == "loop" || key == "random" || key == "repeat")
        set(VLC_OBJECT(THEPL), key, value);
    const int variableType = type == CONFIG_ITEM_STRING  ? VLC_VAR_STRING
                             : type == CONFIG_ITEM_FLOAT ? VLC_VAR_FLOAT
                             : type == CONFIG_ITEM_BOOL  ? VLC_VAR_BOOL
                                                         : VLC_VAR_INTEGER;
    if (!var_Type(THEPL, qtu(key)))
        var_Create(THEPL, qtu(key), variableType | VLC_VAR_DOINHERIT);
    set(VLC_OBJECT(THEPL), key, value);
    auto aout = THEMIM->getAout();
    if (aout)
    {
        set(VLC_OBJECT(aout), key, value);
        vlc_object_release(aout);
    }
    const auto vouts = THEMIM->getVouts();
    for (auto vout : vouts)
    {
        set(VLC_OBJECT(vout), key, value);
        vlc_object_release(vout);
    }
    set(VLC_OBJECT(p_intf->obj.libvlc), key, value);
    set(VLC_OBJECT(p_intf), key, value);
    if (key == "video-on-top")
        window->setInterfaceAlwaysOnTop(value.toBool());
    if (key == "qt-max-volume")
        if (auto volume = window->findChild<QSlider *>("volumeSlider"))
            volume->setMaximum(qMax(1, value.toInt()));
    if (key.startsWith("qt-") || key == "video-deco")
        window->reloadPrefs();
    if (QStringList({"brightness", "contrast", "hue", "saturation", "gamma"}).contains(key))
        filterEnabled("adjust", true, true);
    const QHash<QString, QString> videoFilters = {{"sharpen-sigma", "sharpen"},
                                                  {"grain-variance", "grain"},
                                                  {"colorthres-color", "colorthres"},
                                                  {"colorthres-similaritythres", "colorthres"},
                                                  {"colorthres-saturationthres", "colorthres"},
                                                  {"rotate-angle", "rotate"},
                                                  {"transform-type", "transform"}};
    if (videoFilters.contains(key))
        filterEnabled(videoFilters.value(key), true, true);
    if (videoEffectForOption.contains(key))
        filterEnabled(videoEffectForOption.value(key), true, true);
    if (key == "subsdelay-factor")
        filterEnabled("subsdelay", value.toDouble() > 0, true);
    if (QStringList({"hue", "brightness", "contrast", "saturation", "gamma"}).contains(key) ||
        videoFilters.contains(key) || videoEffectForOption.contains(key) || key == "subsdelay-factor")
    {
        auto applied = std::make_shared<bool>(false);
        auto input = THEMIM->getInput();
        for (int attempt = 1; attempt <= 5; ++attempt)
            QTimer::singleShot(attempt * 120, this,
                               [this, key, value, set, applied, input]
                               {
                                   if (*applied || THEMIM->getInput() != input)
                                       return;
                                   const auto outputs = THEMIM->getVouts();
                                   for (auto output : outputs)
                                   {
                                       if (var_Type(output, qtu(key)) & VLC_VAR_ISCOMMAND)
                                       {
                                           set(VLC_OBJECT(output), key, value);
                                           *applied = true;
                                       }
                                       vlc_object_release(output);
                                   }
                               });
    }
    char *activeChain = var_GetString(THEPL, "audio-filter");
    const QStringList runningFilters = qfu(activeChain ? activeChain : "").split(':');
    free(activeChain);
    if (key.startsWith("norm-") && runningFilters.contains("normvol"))
    {
        playlist_EnableAudioFilter(THEPL, "normvol", false);
        playlist_EnableAudioFilter(THEPL, "normvol", true);
    }
    if (QStringList({"delay-time", "sweep-depth", "sweep-rate", "feedback-gain", "wet-mix", "dry-mix"})
            .contains(key) &&
        runningFilters.contains("chorus_flanger"))
    {
        auto filter = vlc_object_find_name(VLC_OBJECT(THEPL), "chorus_flanger");
        if (filter && var_Type(filter, qtu(key)))
            set(filter, key, value);
        else
        {
            playlist_EnableAudioFilter(THEPL, "chorus_flanger", false);
            playlist_EnableAudioFilter(THEPL, "chorus_flanger", true);
        }
        if (filter)
            vlc_object_release(filter);
    }
}
void RedesignPreferences::heading(QVBoxLayout *layout, const QString &text)
{
    auto label = new QLabel(text);
    label->setObjectName("sectionHeading");
    layout->addWidget(label);
}
void RedesignPreferences::row(QVBoxLayout *layout, const QString &label, QWidget *control, const QString &key)
{
    QString explanation;
    const auto cfg = config_FindConfig(qtu(key));
    if (cfg)
        explanation = qtr(cfg->psz_longtext && *cfg->psz_longtext ? cfg->psz_longtext : cfg->psz_text);
    static const QHash<QString, QString> descriptions = {
        {"ui:waveform", "Show the audio waveform on the seek bar. Audio tracks enable this by default. Local "
                        "media up to 30 minutes is decoded in a bounded background job."},
        {"ui:subtitleTimeline",
         "Show subtitle cue timing on the seek bar. Drag the cue strip to adjust the subtitle delay."},
        {"ui:visualizer", "Display a visualization driven by the playing audio. Choose Disabled to return to "
                          "album art or the music icon."},
        {"ui:seekThumbnails", "Show a preview frame and playback time when hovering over the seek bar."},
        {"ui:autoHideControls", "Choose when title and playback controls hide. Move to the corresponding "
                                "edge to reveal them, or pin a panel to keep it visible."},
        {"ui:compactOnPlayback", "Show the compact library while media plays in the main window."},
        {"ui:singleClick", "Choose the action performed by one click on the video surface."},
        {"ui:doubleClick", "Choose the action performed by a double click on the video surface."},
        {"ui:pauseOtherAudio", "Pause other controllable Windows audio sessions while Reframe plays. "
                               "Sessions paused by Reframe are resumed afterward."},
        {"ui:reverseScroll", "Reverse the direction of mouse-wheel playback controls."},
        {"ui:rememberWindow", "Restore the saved window size and position on the next launch."},
        {"ui:checkUpdates", "Check the fork's GitHub releases for a newer version. Downloads and "
                            "installation require your action."},
        {"ui:developerMode", "Enable detailed VLC diagnostic logging for troubleshooting."},
        {"ui:extraArgs", "Additional VLC command-line options apply on the next launch. Explicit launch "
                         "arguments take precedence. Use --ignore-config to bypass saved options."},
        {"ui:nightMode", "Reduce the difference between quiet and loud passages using the compressor."},
        {"ui:sleepMinutes",
         "Stop playback when the selected number of minutes has elapsed. Off disables the timer."},
        {"ui:startupVolume", "Volume to use when starting playback."},
        {"ui:notificationDuration", "How long playback notifications remain visible."},
        {"ui:VideosDirectory", "Folder displayed by default in the video library."},
        {"ui:MusicDirectory", "Folder displayed by default in the music library."},
        {"ui:useSkin", "Use VLC's skin interface on the next launch."},
        {"ui:webRemote", "Enable VLC's HTTP remote-control interface. Configure its access and password in "
                         "Network settings before using it."},
        {"ui:lastFm",
         "Submit played music to Last.fm using the account configured in the advanced settings."},
        {"ui:systemProxy",
         "Use the system connection settings instead of the manually configured HTTP proxy."},
        {"ui:discoverCast", "Scan the local network for compatible playback renderers."}};
    if (descriptions.contains(key))
        explanation = descriptions.value(key);
    if (key.startsWith("ui:crop-"))
        explanation = "Crop this many pixels from the corresponding edge of the playing video.";
    auto info = explanation.isEmpty() ? nullptr : new Reframe::ToolButton;
    if (info)
    {
        info->setObjectName("settingInfo");
        info->setFixedSize(20, 20);
        info->setAutoRaise(true);
        info->setIcon(Reframe::icon("info"));
        info->setIconSize(QSize(16, 16));
        info->setAccessibleName("Information about " + label);
        info->setProperty("informationText", explanation);
        info->setProperty("settingKey", key);
        info->setToolTip(Qt::convertFromPlainText(explanation));
        connect(info, &QToolButton::clicked, info,
                [info]
                { QToolTip::showText(info->mapToGlobal(QPoint(0, info->height())), info->toolTip(), info); });
    }
    if (control->property("sliderOption").toBool())
    {
        auto top = static_cast<QHBoxLayout *>(control->layout()->itemAt(0)->layout());
        auto text = new QLabel(label);
        text->setWordWrap(true);
        text->setObjectName("settingLabel");
        auto labelRow = new QHBoxLayout;
        labelRow->setSpacing(6);
        labelRow->addWidget(text);
        if (info)
            labelRow->addWidget(info);
        labelRow->addStretch();
        top->insertLayout(0, labelRow, 1);
        control->setAccessibleName(label);
        layout->addWidget(control);
        return;
    }
    auto widget = new QWidget;
    auto form = new QHBoxLayout(widget);
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(20);
    auto text = new QLabel(label);
    text->setWordWrap(true);
    text->setObjectName("settingLabel");
    auto labelRow = new QHBoxLayout;
    labelRow->setSpacing(6);
    labelRow->addWidget(text);
    if (info)
        labelRow->addWidget(info);
    labelRow->addStretch();
    form->addLayout(labelRow, 1);
    if (!key.isEmpty())
    {
        auto cfg = config_FindConfig(qtu(key));
        if (cfg && cfg->psz_longtext)
            text->setToolTip(qtr(cfg->psz_longtext));
    }
    control->setAccessibleName(label);
    if (control->objectName().isEmpty())
        control->setObjectName(key);
    QWidget *field = control;
    if (dynamic_cast<Reframe::Toggle *>(control))
    {
        field = new QWidget;
        auto contents = new QHBoxLayout(field);
        contents->setContentsMargins(0, 0, 0, 0);
        contents->addStretch();
        contents->addWidget(control, 0, Qt::AlignRight);
    }
    field->setMaximumWidth(local ? 200 : 320);
    if (local)
        field->setSizePolicy(QSizePolicy::Ignored, field->sizePolicy().verticalPolicy());
    form->addWidget(field, 1);
    layout->addWidget(widget);
}
QWidget *RedesignPreferences::option(const QString &key, QWidget *parent)
{
    const auto cfg = config_FindConfig(qtu(key));
    const auto value = read(key);
    const bool app = key.startsWith("ui:");
    const auto type = cfg ? (cfg->i_type & 0xF0) : app ? CONFIG_ITEM_BOOL : 0;
    if (key == "ui:extraArgs")
    {
        auto edit = new QLineEdit(value.toString(), parent);
        edit->setMaxLength(4096);
        edit->setPlaceholderText("e.g. --network-caching=1000");
        edit->setToolTip("Applied on the next launch. Explicit command-line options take precedence. Use "
                         "--ignore-config to bypass these options.");
        connect(edit, &QLineEdit::editingFinished, this, [this, edit, key] { write(key, edit->text()); });
        return edit;
    }
    if (key == "ui:visualizer")
    {
        auto combo = new Reframe::Dropdown(parent);
        combo->addItem("Disabled", "");
        if (module_exists("visual"))
        {
            combo->addItem("Spectrometer", "spectrometer");
            combo->addItem("Scope", "scope");
            combo->addItem("Waveform", "waveform");
            combo->addItem("Spectrum", "spectrum");
            combo->addItem("VU meter", "vuMeter");
        }
        combo->setCurrentIndex(qMax(0, combo->findData(value)));
        connect(combo, QOverload<int>::of(&QComboBox::activated), this,
                [this, combo, key](int index) { write(key, combo->itemData(index)); });
        return combo;
    }
    if (key == "ui:autoHideControls" || key == "ui:singleClick" || key == "ui:doubleClick")
    {
        auto combo = new Reframe::Dropdown(parent);
        combo->addItems(key == "ui:autoHideControls"
                            ? QStringList({"Never", "In fullscreen only", "Always"})
                            : QStringList({"None", "Play / Pause", "Toggle fullscreen"}));
        combo->setCurrentIndex(qMax(0, combo->findText(value.toString())));
        connect(combo, QOverload<int>::of(&QComboBox::activated), this,
                [this, key, combo](int) { write(key, combo->currentText()); });
        return combo;
    }
    if (key.startsWith("ui:crop-") || key == "ui:startupVolume" || key == "ui:sleepMinutes" ||
        key == "ui:notificationDuration")
    {
        auto spin = new QDoubleSpinBox(parent);
        spin->setDecimals(0);
        spin->setRange(0, key == "ui:startupVolume" ? 300 : key == "ui:sleepMinutes" ? 1440 : 10000);
        spin->setValue(value.toDouble());
        if (key == "ui:startupVolume")
            spin->setSuffix(" %");
        if (key == "ui:sleepMinutes")
        {
            spin->setSuffix(" min");
            spin->setSpecialValueText("Off");
        }
        if (key == "ui:notificationDuration")
        {
            spin->setRange(1, 60);
            spin->setSuffix(" s");
        }
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this, key](double value) { write(key, value); });
        return spin;
    }
    if (key == "input-fast-seek")
    {
        auto b = new Reframe::Toggle(parent);
        b->setChecked(!value.toBool());
        connect(b, &QAbstractButton::toggled, this, [this](bool on) { write("input-fast-seek", !on); });
        return b;
    }
    if (type == CONFIG_ITEM_BOOL)
    {
        auto b = new Reframe::Toggle(parent);
        b->setChecked(value.toBool());
        b->setObjectName(key);
        connect(b, &QAbstractButton::toggled, this, [this, key](bool on) { write(key, on); });
        return b;
    }
    if (!cfg)
    {
        auto text = new QLabel("Unavailable in this build", parent);
        text->setEnabled(false);
        return text;
    }
    if (key == "freetype-font" || key == "freetype-monofont")
    {
        auto combo = new Reframe::FontDropdown(parent);
        combo->setCurrentFont(QFont(value.toString()));
        connect(combo, &QFontComboBox::currentFontChanged, this,
                [this, key](const QFont &font) { write(key, font.family()); });
        return combo;
    }
    if (key.endsWith("color") && type == CONFIG_ITEM_INTEGER)
    {
        auto control = new QWidget(parent);
        auto layout = new QHBoxLayout(control);
        layout->setContentsMargins(0, 0, 0, 0);
        auto edit = new QLineEdit(QString("#%1").arg(value.toInt(), 6, 16, QChar('0')));
        auto choose = new QPushButton;
        choose->setFixedSize(28, 28);
        choose->setStyleSheet("background:" + edit->text() + ";");
        layout->addWidget(edit, 1);
        layout->addWidget(choose);
        connect(edit, &QLineEdit::editingFinished, this,
                [this, key, edit, choose]
                {
                    QColor color(edit->text());
                    if (color.isValid())
                    {
                        write(key, color.rgb() & 0xffffff);
                        choose->setStyleSheet("background:" + color.name() + ";");
                    }
                });
        connect(choose, &QPushButton::clicked, this,
                [this, key, choose, edit]
                {
                    QColor color =
                        QColorDialog::getColor(QColor(edit->text()), this, "Choose subtitle color");
                    if (color.isValid())
                    {
                        write(key, color.rgb() & 0xffffff);
                        edit->setText(color.name().toUpper());
                        choose->setStyleSheet("background:" + color.name() + ";");
                    }
                });
        return control;
    }
    if (local && (key == "sub-track" || key == "audio-track"))
    {
        auto combo = new Reframe::Dropdown(parent);
        combo->setObjectName(key);
        auto update = [this, key, combo]
        {
            if (combo->hasFocus())
                return;
            QSignalBlocker block(combo);
            combo->clear();
            auto input = THEMIM->getInput();
            combo->setEnabled(input != nullptr);
            if (!input)
                return;
            vlc_value_t values, texts;
            const auto name = inputName(key);
            if (var_Change(input, qtu(name), VLC_VAR_GETCHOICES, &values, &texts))
                return;
            for (int i = 0; i < values.p_list->i_count; ++i)
                combo->addItem(qfu(texts.p_list->p_values[i].psz_string),
                               qlonglong(values.p_list->p_values[i].i_int));
            combo->setCurrentIndex(combo->findData(qlonglong(var_GetInteger(input, qtu(name)))));
            var_FreeList(&values, &texts);
        };
        update();
        connect(THEMIM, &MainInputManager::inputChanged, combo, [update](bool) { update(); });
        connect(combo, QOverload<int>::of(&QComboBox::activated), this,
                [this, key, combo](int i) { write(key, combo->itemData(i)); });
        return combo;
    }
    if (type == CONFIG_ITEM_INTEGER || type == CONFIG_ITEM_STRING)
    {
        QStringList labels;
        QVariantList values;
        if (type == CONFIG_ITEM_INTEGER)
        {
            int64_t *list = nullptr;
            char **names = nullptr;
            const ssize_t n = config_GetIntChoices(VLC_OBJECT(p_intf), qtu(key), &list, &names);
            for (ssize_t i = 0; i < n; ++i)
            {
                labels << qfu(names[i]);
                values << QVariant::fromValue<qlonglong>(list[i]);
                free(names[i]);
            }
            if (n > 0)
            {
                free(names);
                free(list);
            }
        }
        else
        {
            char **list = nullptr, **names = nullptr;
            const ssize_t n = config_GetPszChoices(VLC_OBJECT(p_intf), qtu(key), &list, &names);
            for (ssize_t i = 0; i < n; ++i)
            {
                labels << qfu(names[i]);
                values << qfu(list[i]);
                free(names[i]);
                free(list[i]);
            }
            if (n > 0)
            {
                free(names);
                free(list);
            }
        }
        if (!labels.isEmpty())
        {
            auto combo = new Reframe::Dropdown(parent);
            for (int i = 0; i < labels.size(); ++i)
                combo->addItem(labels[i], values[i]);
            combo->setCurrentIndex(qMax(0, combo->findData(value)));
            connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                    [this, key, combo](int i)
                    {
                        if (i >= 0)
                            write(key, combo->itemData(i));
                    });
            return combo;
        }
    }
    if (type == CONFIG_ITEM_FLOAT || type == CONFIG_ITEM_INTEGER)
    {
        auto spin = new QDoubleSpinBox(parent);
        spin->setObjectName(key == "sub-delay" ? "subtitleDelay" : key);
        const bool floating = type == CONFIG_ITEM_FLOAT;
        const bool percent = key.endsWith("opacity") || key == "volume-step" || key == "qt-max-volume";
        const bool seconds =
            QStringList({"mouse-hide-timeout", "video-title-timeout", "osd-timeout"}).contains(key);
        const double scale = seconds                   ? .001
                             : key == "volume-step"    ? 100. / AOUT_VOLUME_DEFAULT
                             : key.endsWith("opacity") ? (floating ? 100. : 100. / 255.)
                                                       : 1.;
        double minimum = (floating ? cfg->min.f : double(cfg->min.i)) * scale;
        double maximum = (floating ? cfg->max.f : double(cfg->max.i)) * scale;
        if (seconds && maximum <= minimum)
        {
            minimum = 0;
            maximum = 10;
        }
        spin->setDecimals(floating ? 3 : 0);
        spin->setRange(maximum > minimum ? minimum : -100000000, maximum > minimum ? maximum : 100000000);
        spin->setSingleStep(floating ? .1 : 1);
        spin->setValue(value.toDouble() * scale);
        if (percent)
        {
            spin->setDecimals(key == "volume-step" ? 1 : 0);
            spin->setSuffix(" %");
        }
        if (seconds)
        {
            spin->setDecimals(1);
            spin->setSingleStep(.1);
            spin->setSuffix(" s");
        }
        if (key == "rate")
        {
            spin->setDecimals(2);
            spin->setSuffix("x");
            spin->setSingleStep(.25);
        }
        if (key == "freetype-fontsize")
            spin->setSpecialValueText("Auto");
        if (key == "sub-delay" || key == "audio-desync")
        {
            spin->setDecimals(3);
            spin->setRange(-600, 600);
            spin->setSingleStep(.1);
            spin->setSuffix(" s");
            spin->setValue(value.toDouble() / (key == "sub-delay" ? 10 : 1000));
            auto update = [this, key, spin]
            {
                if (spin->hasFocus())
                    return;
                QSignalBlocker block(spin);
                spin->setEnabled(!local || THEMIM->getInput());
                spin->setValue(read(key).toDouble() / (key == "sub-delay" ? 10 : 1000));
            };
            connect(THEMIM, &MainInputManager::inputChanged, spin, [update](bool) { update(); });
            connect(THEMIM->getIM(), &InputManager::synchroChanged, spin, update);
            update();
        }
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this, key, scale, floating](double number)
                {
                    const double native = key == "sub-delay"      ? number * 10
                                          : key == "audio-desync" ? number * 1000
                                                                  : number / scale;
                    write(key,
                          !floating && key != "sub-delay" && key != "audio-desync" ? qRound(native) : native);
                });
        if ((maximum > minimum && maximum - minimum <= 1000) || key == "sub-delay" || key == "audio-desync" ||
            key == "rate")
        {
            auto widget = new QWidget(parent);
            widget->setProperty("sliderOption", true);
            auto layout = new QVBoxLayout(widget);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->setSpacing(12);
            auto top = new QHBoxLayout;
            top->addWidget(spin);
            layout->addLayout(top);
            spin->setMaximumWidth(140);
            const double low = key == "sub-delay" || key == "audio-desync" ? -10
                               : key == "rate"                             ? .25
                                                                           : minimum,
                         high = key == "sub-delay" || key == "audio-desync" ? 10
                                : key == "rate"                             ? 4
                                                                            : maximum;
            auto slider = new QSlider(Qt::Horizontal);
            slider->setRange(0, 1000);
            slider->setValue(qRound((spin->value() - low) / (high - low) * 1000));
            layout->addWidget(slider);
            connect(slider, &QSlider::valueChanged, this,
                    [spin, low, high](int v) { spin->setValue(low + v / 1000.0 * (high - low)); });
            connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), slider,
                    [slider, low, high](double v)
                    {
                        QSignalBlocker block(slider);
                        slider->setValue(qRound((v - low) / (high - low) * 1000));
                    });
            return widget;
        }
        return spin;
    }
    auto edit = new QLineEdit(value.toString(), parent);
    edit->setObjectName(key);
    if (cfg->i_type == CONFIG_ITEM_PASSWORD)
        edit->setEchoMode(QLineEdit::Password);
    if (key.contains("color") && type == CONFIG_ITEM_INTEGER)
    {
        edit->setText(QString("#%1").arg(value.toInt(), 6, 16, QChar('0')));
    }
    connect(edit, &QLineEdit::editingFinished, this, [this, key, edit] { write(key, edit->text()); });
    if (cfg->i_type == CONFIG_ITEM_DIRECTORY || cfg->i_type == CONFIG_ITEM_LOADFILE ||
        cfg->i_type == CONFIG_ITEM_SAVEFILE)
    {
        auto widget = new QWidget(parent);
        auto row = new QHBoxLayout(widget);
        row->setContentsMargins(0, 0, 0, 0);
        row->addWidget(edit, 1);
        auto browse = new QPushButton("Browse");
        row->addWidget(browse);
        const bool dir = cfg->i_type == CONFIG_ITEM_DIRECTORY;
        const bool save = cfg->i_type == CONFIG_ITEM_SAVEFILE;
        connect(browse, &QPushButton::clicked, this,
                [this, key, edit, dir, save]
                {
                    QString path =
                        dir    ? QFileDialog::getExistingDirectory(this, "Choose folder", edit->text())
                        : save ? QFileDialog::getSaveFileName(this, "Choose file", edit->text())
                               : QFileDialog::getOpenFileName(this, "Choose file", edit->text());
                    if (!path.isEmpty())
                    {
                        edit->setText(path);
                        write(key, path);
                    }
                });
        return widget;
    }
    return edit;
}
void RedesignPreferences::options(QVBoxLayout *layout, const char *spec)
{
    const auto fields = tokens(spec);
    for (const auto &field : fields)
    {
        if (field.startsWith('#'))
        {
            heading(layout, field.mid(1));
            continue;
        }
        const int colon = field.indexOf('=');
        const QString key = colon < 0 ? field : field.mid(colon + 1),
                      label = colon < 0 ? field : field.left(colon);
        // Never expose a dead preference because an optional plugin is absent.
        if (!key.startsWith("ui:") && !config_FindConfig(qtu(key)))
            continue;
        row(layout, label, option(key, this), key);
    }
}
void RedesignPreferences::filterEnabled(const QString &name, bool on, bool video)
{
    const QString key = name == "subsdelay" ? "sub-filter"
                        : name == "marq"    ? "sub-source"
                        : video             ? "video-filter"
                                            : "audio-filter";
    const QString saved = cfgString(p_intf, qtu(key));
    edited.insert(key);
    if (local)
    {
        auto keys = window->property("reframeLocalKeys").toStringList();
        if (!keys.contains(key))
        {
            keys << key;
            window->setProperty("reframeLocalKeys", keys);
        }
    }
    if (video)
    {
        if (!var_Type(THEPL, qtu(key)))
            var_Create(THEPL, qtu(key), VLC_VAR_STRING | VLC_VAR_DOINHERIT);
        char *raw = var_GetString(THEPL, qtu(key));
        QStringList chain = qfu(raw ? raw : "").split(':', QString::SkipEmptyParts);
        free(raw);
        const QString before = chain.join(':');
        chain.removeAll(name);
        if (on)
            chain << name;
        const QString after = chain.join(':');
        if (before == after)
            return;
        var_SetString(THEPL, qtu(key), qtu(after));
        if (!local)
        {
            config_PutPsz(p_intf, qtu(key), qtu(chain.join(':')));
            saveTimer.start();
        }
        const auto outputs = THEMIM->getVouts();
        for (auto output : outputs)
        {
            char *raw = var_GetString(output, qtu(key));
            QStringList live = qfu(raw ? raw : "").split(':', QString::SkipEmptyParts);
            free(raw);
            live.removeAll(name);
            if (on)
                live << name;
            var_SetString(output, qtu(key), qtu(live.join(':')));
            vlc_object_release(output);
        }
    }
    else
    {
        playlist_EnableAudioFilter(THEPL, qtu(name), on);
        if (!local)
        {
            char *chain = var_GetString(THEPL, qtu(key));
            config_PutPsz(p_intf, qtu(key), chain ? chain : "");
            free(chain);
            saveTimer.start();
        }
    }
    localValues["filter:" + name] = on;
}
QWidget *RedesignPreferences::filter(const QString &label, const QString &name, const char *fields)
{
    auto box = new QWidget;
    auto layout = new QVBoxLayout(box);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);
    auto on = new Reframe::Toggle;
    on->setObjectName("filter:" + name);
    char *active = var_GetString(THEPL, "audio-filter");
    on->setChecked(
        (local ? qfu(active ? active : "") : cfgString(p_intf, "audio-filter")).split(':').contains(name));
    free(active);
    row(layout, label, on);
    connect(on, &QAbstractButton::toggled, this,
            [this, name](bool checked) { filterEnabled(name, checked); });
    if (fields && *fields)
    {
        auto content = new QWidget;
        auto controls = new QVBoxLayout(content);
        controls->setContentsMargins(0, 0, 0, 0);
        controls->setSpacing(20);
        options(controls, fields);
        layout->addWidget(Reframe::fold(label + " Settings", content, box));
    }
    return box;
}
QWidget *RedesignPreferences::equalizer(bool horizontal)
{
    auto content = new QWidget;
    content->setObjectName("equalizerPanel");
    auto layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(20);
    layout->addWidget(filter("Equalizer", "equalizer", ""));
    auto presets = static_cast<QComboBox *>(option("equalizer-preset", content));
    row(layout, "Equalizer preset", presets);
    auto bandsWidget = new QWidget;
    auto bandsLayout = horizontal ? static_cast<QBoxLayout *>(new QVBoxLayout(bandsWidget))
                                  : static_cast<QBoxLayout *>(new QHBoxLayout(bandsWidget));
    bandsLayout->setSpacing(horizontal ? 8 : 4);
    bandsLayout->setContentsMargins(8, 8, 8, 8);
    QStringList values = cfgString(p_intf, "equalizer-bands").split(' ', QString::SkipEmptyParts);
    while (values.size() < 10)
        values << "0";
    auto bandValues = std::make_shared<QStringList>(values);
    const QStringList names = QStringList() << "Preamp"
                                            << "60 Hz"
                                            << "170 Hz"
                                            << "310 Hz"
                                            << "600 Hz"
                                            << "1 KHz"
                                            << "3 KHz"
                                            << "6 KHz"
                                            << "12 KHz"
                                            << "14 KHz"
                                            << "16 KHz";
    for (int i = 0; i < 11; ++i)
    {
        auto strip = new QWidget;
        auto col = new QVBoxLayout(strip);
        col->setContentsMargins(0, 0, 0, 0);
        col->setSpacing(10);
        auto title = new QLabel(names[i]);
        title->setAlignment(Qt::AlignCenter);
        auto spin = new QDoubleSpinBox;
        if (!horizontal)
        {
            spin->setFixedWidth(54);
            QFont font = spin->font();
            font.setPixelSize(12);
            spin->setFont(font);
        }
        spin->setDecimals(1);
        spin->setRange(-20, 20);
        spin->setSuffix(" dB");
        spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
        spin->setValue(i ? values[i - 1].toDouble() : read("equalizer-preamp").toDouble());
        auto slider = new QSlider(horizontal ? Qt::Horizontal : Qt::Vertical);
        slider->setRange(-200, 200);
        slider->setValue(qRound(spin->value() * 10));
        slider->setAccessibleName(names[i]);
        slider->setObjectName(i ? QString("equalizerBand" + QString::number(i)) : QString("preampSlider"));
        if (horizontal)
        {
            auto r = new QHBoxLayout;
            r->addWidget(title, 1);
            r->addWidget(spin);
            col->addLayout(r);
            col->addWidget(slider);
        }
        else
        {
            slider->setMinimumHeight(280);
            col->addWidget(slider, 1, Qt::AlignHCenter);
            col->addWidget(title);
            col->addWidget(spin);
        }
        connect(slider, &QSlider::valueChanged, this, [spin](int value) { spin->setValue(value / 10.0); });
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this, i, slider, bandValues](double value)
                {
                    QSignalBlocker block(slider);
                    slider->setValue(qRound(value * 10));
                    if (i)
                    {
                        (*bandValues)[i - 1] = QString::number(value, 'f', 1);
                        write("equalizer-bands", bandValues->join(' '));
                    }
                    else
                        write("equalizer-preamp", value);
                });
        bandsLayout->addWidget(strip, 1);
    }
    connect(presets, QOverload<int>::of(&QComboBox::activated), this,
            [this, presets, bandsWidget, bandValues](int index)
            {
                const QString name = presets->itemData(index).toString();
                for (int p = 0; p < NB_PRESETS; ++p)
                    if (name == preset_list[p])
                    {
                        auto spins = bandsWidget->findChildren<QDoubleSpinBox *>();
                        (*bandValues).clear();
                        for (int i = 0; i < 10; ++i)
                            (*bandValues) << QString::number(eqz_preset_10b[p].f_amp[i], 'f', 1);
                        write("equalizer-bands", bandValues->join(' '));
                        write("equalizer-preamp", eqz_preset_10b[p].f_preamp);
                        for (int i = 0; i < spins.size() && i < 11; ++i)
                            spins[i]->setValue(i ? eqz_preset_10b[p].f_amp[i - 1]
                                                 : eqz_preset_10b[p].f_preamp);
                        break;
                    }
            });
    layout->addWidget(Reframe::fold("Equalizer", bandsWidget, content, !horizontal));
    options(layout, "Two-pass=equalizer-2pass");
    return content;
}
QWidget *RedesignPreferences::videoEffects()
{
    auto widget = new QWidget;
    auto layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(20);
    auto categories = new Reframe::Dropdown;
    categories->addItems(QStringList() << "Essential"
                                       << "Crop"
                                       << "Colors"
                                       << "Geometry"
                                       << "Overlay"
                                       << "Advanced");
    layout->addWidget(categories);
    auto pages = new QStackedWidget;
    layout->addWidget(pages);
    const QStringList names = QStringList()
                              << "adjust,sharpen,grain,antiflicker"
                              << ""
                              << "colorthres,sepia,extract,posterize,invert"
                              << "transform,rotate,magnify,puzzle"
                              << "logo,marq,erase"
                              << "gradient,motionblur,motiondetect,wave,ripple,mirror,anaglyph,oldmovie";
    for (int section = 0; section < names.size(); ++section)
    {
        auto page = new QWidget;
        auto controls = new QVBoxLayout(page);
        controls->setContentsMargins(0, 0, 0, 0);
        controls->setSpacing(20);
        if (section == 1)
            options(controls, "Top=ui:crop-top|Bottom=ui:crop-bottom|Left=ui:crop-left|Right=ui:crop-right");
        for (const auto &name : names[section].split(',', QString::SkipEmptyParts))
        {
            auto module = module_find(qtu(name));
            if (!module)
                continue;
            const QHash<QString, QString> labels = {{"adjust", "Picture"},
                                                    {"sharpen", "Sharpen"},
                                                    {"grain", "Grain"},
                                                    {"antiflicker", "Anti-flicker"},
                                                    {"colorthres", "Color threshold"},
                                                    {"extract", "Color extraction"},
                                                    {"transform", "Transform"},
                                                    {"rotate", "Rotate"},
                                                    {"magnify", "Magnify"},
                                                    {"puzzle", "Puzzle"},
                                                    {"logo", "Logo"},
                                                    {"marq", "Text"},
                                                    {"erase", "Erase"},
                                                    {"motionblur", "Motion blur"},
                                                    {"motiondetect", "Motion detection"},
                                                    {"oldmovie", "Old movie"}};
            const QString title = labels.value(name, qfu(module_get_name(module, true)));
            auto toggle = new Reframe::Toggle;
            toggle->setObjectName("videoFilter:" + name);
            char *active = var_GetString(THEPL, name == "marq" ? "sub-source" : "video-filter");
            toggle->setChecked(qfu(active ? active : "").split(':').contains(name));
            free(active);
            row(controls, title, toggle);
            connect(toggle, &QAbstractButton::toggled, this,
                    [this, name](bool on) { filterEnabled(name, on, true); });
            auto settings = new QWidget;
            auto rows = new QVBoxLayout(settings);
            rows->setContentsMargins(0, 0, 0, 0);
            rows->setSpacing(20);
            unsigned count;
            auto configs = module_config_get(module, &count);
            for (unsigned i = 0; i < count; ++i)
                if (configVisible(configs[i]))
                {
                    const QString key = qfu(configs[i].psz_name);
                    videoEffectForOption[key] = name;
                    row(rows, qtr(configs[i].psz_text), option(key, this), key);
                }
            module_config_free(configs);
            controls->addWidget(Reframe::fold(title + " Settings", settings, page));
        }
        pages->addWidget(page);
    }
    connect(categories, QOverload<int>::of(&QComboBox::currentIndexChanged), pages,
            &QStackedWidget::setCurrentIndex);
    return widget;
}
QWidget *RedesignPreferences::advanced(int page)
{
    auto content = new QWidget;
    auto layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);
    auto search = new QLineEdit;
    search->setPlaceholderText("Search all options");
    layout->addWidget(search);
    auto modules = new Reframe::Dropdown;
    layout->addWidget(modules);
    auto controls = new QWidget;
    auto fields = new QVBoxLayout(controls);
    fields->setContentsMargins(0, 0, 0, 0);
    fields->setSpacing(16);
    layout->addWidget(controls);
    size_t count;
    module_t **list = module_list_get(&count);
    for (size_t i = 0; i < count; ++i)
    {
        unsigned size;
        auto config = module_config_get(list[i], &size);
        bool any = false;
        for (unsigned j = 0; j < size; ++j)
            if (configVisible(config[j]))
                any = true;
        module_config_free(config);
        if (any)
            modules->addItem(qfu(module_get_name(list[i], true)), qfu(module_get_object(list[i])));
    }
    module_list_free(list);
    modules->model()->sort(0);
    auto render = [this, fields, modules, search]
    {
        while (auto item = fields->takeAt(0))
        {
            if (item->widget())
                item->widget()->deleteLater();
            delete item;
        }
        auto module = module_find(qtu(modules->currentData().toString()));
        if (!module)
            return;
        unsigned size;
        auto configs = module_config_get(module, &size);
        for (unsigned i = 0; i < size; ++i)
        {
            auto &cfg = configs[i];
            if (!configVisible(cfg))
                continue;
            const QString name = qfu(cfg.psz_name), text = qtr(cfg.psz_text ? cfg.psz_text : cfg.psz_name);
            if (!search->text().isEmpty() && !name.contains(search->text(), Qt::CaseInsensitive) &&
                !text.contains(search->text(), Qt::CaseInsensitive))
                continue;
            row(fields, text, option(name, this), name);
        }
        module_config_free(configs);
    };
    connect(modules, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [render](int) { render(); });
    connect(search, &QLineEdit::textChanged, this, [render](const QString &) { render(); });
    const QString initial = page == 1    ? "qt"
                            : page == 5  ? "direct3d11"
                            : page == 6  ? "mmdevice"
                            : page == 7  ? "freetype"
                            : page == 9  ? "http"
                            : page == 10 ? "avcodec"
                                         : "main";
    modules->setCurrentIndex(qMax(0, modules->findData(initial)));
    render();
    return content;
}
void RedesignPreferences::preset(QVBoxLayout *layout)
{
    auto preset = new Reframe::Dropdown;
    preset->addItem("Default Preset");
    preset->addItems(getSettings()->value("Redesign/profileNames").toStringList());
    row(layout, "Setting preset", preset);
    connect(
        preset, QOverload<int>::of(&QComboBox::activated), this,
        [this, preset](int i)
        {
            const auto data =
                getSettings()->value("Redesign/profiles/" + (i ? preset->currentText() : "Default")).toMap();
            restore(data);
        });
    auto buttons = new QWidget;
    auto row = new QHBoxLayout(buttons);
    row->setContentsMargins(0, 0, 0, 0);
    auto save = new QPushButton(local ? "Save as default" : "Save Preset");
    auto manage = new QPushButton("Manage Profiles");
    row->addWidget(save);
    row->addWidget(manage);
    layout->addWidget(buttons);
    connect(save, &QPushButton::clicked, this,
            [this]
            {
                QString name =
                    local ? "Default" : QInputDialog::getText(this, "Save setting preset", "Preset name");
                if (name.trimmed().isEmpty())
                    return;
                QVariantMap settings;
                if (local)
                {
                    for (const auto &key : edited)
                        if (!key.startsWith("filter:"))
                            settings[key] = read(key);
                    auto input = THEMIM->getInput();
                    if (input)
                        for (const auto &key : QStringList({"audio-track", "sub-track"}))
                            if (settings.contains(key))
                            {
                                const auto name = inputName(key);
                                const qint64 selected = settings.take(key).toLongLong();
                                vlc_value_t values, texts;
                                if (!var_Change(input, qtu(name), VLC_VAR_GETCHOICES, &values, &texts))
                                {
                                    int ordinal = 0;
                                    for (int i = 0; i < values.p_list->i_count; ++i)
                                        if (values.p_list->p_values[i].i_int >= 0)
                                        {
                                            if (values.p_list->p_values[i].i_int == selected)
                                            {
                                                settings["ordinal:" + key] = ordinal;
                                                break;
                                            }
                                            ++ordinal;
                                        }
                                    var_FreeList(&values, &texts);
                                }
                                if (selected < 0)
                                    settings[key == "sub-track" ? "spu" : "audio"] = false;
                            }
                }
                else
                {
                    saveAppProfile(name);
                    settings = getSettings()->value("Redesign/profiles/" + name).toMap();
                }
                getSettings()->setValue("Redesign/profiles/" + name, settings);
                auto names = getSettings()->value("Redesign/profileNames").toStringList();
                if (name != "Default" && !names.contains(name))
                {
                    names << name;
                    getSettings()->setValue("Redesign/profileNames", names);
                }
                if (local)
                {
                    char *uri =
                        THEMIM->currentInputItem() ? input_item_GetURI(THEMIM->currentInputItem()) : nullptr;
                    const QString folder =
                        uri ? QFileInfo(QUrl(qfu(uri)).toLocalFile()).absolutePath() : QString();
                    free(uri);
                    if (!folder.isEmpty())
                        getSettings()->setValue("Redesign/mediaDefaults/" + folder, settings);
                    const bool old = local;
                    local = false;
                    for (auto it = settings.begin(); it != settings.end(); ++it)
                        write(it.key().startsWith("ordinal:") ? it.key().mid(8) : it.key(), it.value());
                    local = old;
                }
            });
    connect(manage, &QPushButton::clicked, this, [this] { profiles(); });
}
void RedesignPreferences::profiles()
{
    auto dialog = new Reframe::Dialog(this, "Profiles", QSize(651, 633));
    auto list = new QListWidget;
    list->addItems(getSettings()->value("Redesign/profileNames").toStringList());
    dialog->body->addWidget(list, 1);
    auto buttons = new QHBoxLayout;
    for (const QString &action : QStringList({"Rename", "Delete", "Export", "Import"}))
    {
        auto b = new QPushButton(action);
        buttons->addWidget(b);
        connect(
            b, &QPushButton::clicked, dialog,
            [this, list, action]
            {
                auto item = list->currentItem();
                const QString name = item ? item->text() : QString();
                if (action == "Import")
                {
                    QString path =
                        QFileDialog::getOpenFileName(this, "Import profile", QString(), "Profiles (*.json)");
                    QFile file(path);
                    if (!file.open(QIODevice::ReadOnly))
                        return;
                    if (file.size() > 256 * 1024)
                    {
                        QMessageBox::warning(this, "Import profile", "The profile exceeds the size limit.");
                        return;
                    }
                    QVariantMap map;
                    if (!Reframe::parseProfile(file.read(256 * 1024 + 1), &map))
                    {
                        QMessageBox::warning(this, "Import profile",
                                             "The profile contains unsupported or unsafe settings.");
                        return;
                    }
                    const QString label = QFileInfo(path).completeBaseName();
                    getSettings()->setValue("Redesign/profiles/" + label, map);
                    list->addItem(label);
                }
                else if (name.isEmpty())
                    return;
                else if (action == "Export")
                {
                    QString path = QFileDialog::getSaveFileName(this, "Export profile", name + ".json",
                                                                "Profiles (*.json)");
                    QFile file(path);
                    if (file.open(QIODevice::WriteOnly))
                        file.write(
                            QJsonDocument(QJsonObject::fromVariantMap(Reframe::portableProfile(
                                              getSettings()->value("Redesign/profiles/" + name).toMap())))
                                .toJson());
                }
                else if (action == "Delete")
                {
                    getSettings()->remove("Redesign/profiles/" + name);
                    delete item;
                }
                else
                {
                    QString label =
                        QInputDialog::getText(this, "Rename profile", "Name", QLineEdit::Normal, name)
                            .trimmed();
                    if (label.isEmpty() || label == name)
                        return;
                    getSettings()->setValue("Redesign/profiles/" + label,
                                            getSettings()->value("Redesign/profiles/" + name));
                    getSettings()->remove("Redesign/profiles/" + name);
                    item->setText(label);
                }
                QStringList names;
                for (int i = 0; i < list->count(); ++i)
                    names << list->item(i)->text();
                getSettings()->setValue("Redesign/profileNames", names);
            });
    }
    dialog->body->addLayout(buttons);
    Reframe::reveal(dialog);
}
void RedesignPreferences::shortcuts()
{
    auto pending = std::make_shared<QHash<QString, QString>>();
    auto dialog = new Reframe::Dialog(this, "Keyboard Shortcuts", QSize(1048, 800));
    auto filter = new QLineEdit;
    filter->setPlaceholderText("Search shortcuts");
    dialog->body->addWidget(filter);
    dialog->body->addWidget(new QLabel("Shortcut changes apply on the next launch."));
    auto table = new QTableWidget;
    table->setColumnCount(3);
    table->setHorizontalHeaderLabels(QStringList() << "Action"
                                                   << "Shortcut"
                                                   << "Global shortcut");
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    dialog->body->addWidget(table, 1);
    unsigned count;
    auto cfgs = module_config_get(module_get_main(), &count);
    for (unsigned i = 0; i < count; ++i)
    {
        auto &cfg = cfgs[i];
        if (!configVisible(cfg) || !QString::fromUtf8(cfg.psz_name).startsWith("key-"))
            continue;
        const int r = table->rowCount();
        table->insertRow(r);
        table->setItem(r, 0, new QTableWidgetItem(qtr(cfg.psz_text)));
        for (int c = 1; c <= 2; ++c)
        {
            QString name = c == 1 ? qfu(cfg.psz_name) : "global-" + qfu(cfg.psz_name);
            if (!config_FindConfig(qtu(name)))
                continue;
            auto edit = new QKeySequenceEdit(QKeySequence(cfgString(p_intf, qtu(name))), table);
            edit->setProperty("configKey", name);
            table->setCellWidget(r, c, edit);
            connect(edit, &QKeySequenceEdit::editingFinished, this,
                    [pending, name, edit]
                    {
                        const int code = edit->keySequence()[0];
                        QKeyEvent event(QEvent::KeyPress, code & ~Qt::KeyboardModifierMask,
                                        Qt::KeyboardModifiers(code & Qt::KeyboardModifierMask));
                        (*pending)[name] = VLCKeyToString(qtEventToVLCKey(&event), false);
                    });
        }
    }
    module_config_free(cfgs);
    connect(filter, &QLineEdit::textChanged, table,
            [table](const QString &s)
            {
                for (int i = 0; i < table->rowCount(); ++i)
                    table->setRowHidden(i, !table->item(i, 0)->text().contains(s, Qt::CaseInsensitive));
            });
    auto reset = new QPushButton("Reset all shortcuts");
    dialog->body->addWidget(reset);
    connect(reset, &QPushButton::clicked, this,
            [pending, table]
            {
                unsigned n;
                auto cfg = module_config_get(module_get_main(), &n);
                for (unsigned i = 0; i < n; ++i)
                    if (cfg[i].psz_name && (QString::fromUtf8(cfg[i].psz_name).startsWith("key-") ||
                                            QString::fromUtf8(cfg[i].psz_name).startsWith("global-key-")))
                        (*pending)[qfu(cfg[i].psz_name)] = qfu(cfg[i].orig.psz ? cfg[i].orig.psz : "");
                module_config_free(cfg);
                for (auto edit : table->findChildren<QKeySequenceEdit *>())
                {
                    QSignalBlocker block(edit);
                    edit->setKeySequence(
                        QKeySequence(pending->value(edit->property("configKey").toString())));
                }
            });
    auto footer = new QHBoxLayout;
    footer->addStretch();
    auto cancel = new QPushButton("Cancel"), save = new QPushButton("Save");
    footer->addWidget(cancel);
    footer->addWidget(save);
    dialog->body->addLayout(footer);
    connect(cancel, &QPushButton::clicked, dialog, &QDialog::close);
    connect(save, &QPushButton::clicked, this,
            [this, dialog, pending]
            {
                for (auto it = pending->begin(); it != pending->end(); ++it)
                    write(it.key(), it.value());
                dialog->close();
            });
    Reframe::reveal(dialog);
}

QWidget *RedesignPreferences::buildPage(int page)
{
    auto scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto content = new QWidget;
    content->setObjectName("settingsContent");
    auto layout = new QVBoxLayout(content);
    layout->setContentsMargins(local ? 10 : 8, 8, 8, 24);
    layout->setSpacing(20);
    if (page == 0)
    {
        heading(layout, "Appearance");
        options(layout, "Interface language=language");
        row(layout, "Theme",
            Reframe::segmented(QStringList() << "Dark"
                                             << "Light"
                                             << "System",
                               getSettings()->value("Redesign/theme", "Dark").toString(), content,
                               [this](QString value)
                               {
                                   getSettings()->setValue("Redesign/theme", value);
                                   themeChanged();
                               }));
        auto colors = new QWidget;
        auto colorLayout = new QHBoxLayout(colors);
        colorLayout->setContentsMargins(0, 0, 0, 0);
        colorLayout->setSpacing(6);
        const QStringList palette = QStringList() << "#d37918"
                                                  << "#ad4129"
                                                  << "#93751b"
                                                  << "#1a882b"
                                                  << "#008ba8"
                                                  << "#1a56b3"
                                                  << "#8035c8"
                                                  << "#ba3a88"
                                                  << "#8f8f8f";
        for (const auto &color : palette)
        {
            auto b = new QPushButton;
            b->setCheckable(true);
            b->setChecked(getSettings()->value("Redesign/accent", "#d37918").toString() == color);
            b->setProperty("accentColor", color);
            b->setIcon(b->isChecked()
                           ? Reframe::icon("check", QColor(color).lightness() > 150 ? QColor("#111111")
                                                                                    : QColor("#ffffff"))
                           : QIcon());
            b->setAccessibleName("Accent " + color);
            b->setFixedSize(30, 30);
            b->setStyleSheet(
                "QPushButton {background:" + color +
                ";border-radius:2px;padding:0;} QPushButton:checked {border:2px solid #ececec;}");
            colorLayout->addWidget(b);
            connect(b, &QPushButton::clicked, this,
                    [this, color, colors]
                    {
                        getSettings()->setValue("Redesign/accent", color);
                        for (auto child : colors->findChildren<QPushButton *>())
                        {
                            child->setChecked(child->accessibleName() == "Accent " + color);
                            const QColor swatch(child->property("accentColor").toString());
                            child->setIcon(child->isChecked()
                                               ? Reframe::icon("check", swatch.lightness() > 150
                                                                            ? QColor("#111111")
                                                                            : QColor("#ffffff"))
                                               : QIcon());
                        }
                        themeChanged();
                    });
        }
        row(layout, "Accent color", colors);
        options(layout, "#Startup|Show confirmation and error dialogs=qt-error-dialogs|Check for updates "
                        "automatically=ui:checkUpdates|#Diagnostics|Developer mode=ui:developerMode|Save a "
                        "log=file-logging|Log file=logfile|Log details=log-verbose");
        auto more = new QWidget;
        auto controls = new QVBoxLayout(more);
        options(controls,
                "Show minor warnings=verbose|Additional command line options=ui:extraArgs|Collect playback "
                "statistics=stats|Log format=logmode|Write to "
                "the system log=syslog|System log identity=syslog-ident|System log facility=syslog-facility");
        auto folder = new QPushButton("Open configurations folder");
        controls->addWidget(folder);
        connect(folder, &QPushButton::clicked, this,
                [this]
                {
                    char *path = config_GetUserDir(VLC_CONFIG_DIR);
                    QDesktopServices::openUrl(QUrl::fromLocalFile(qfu(path)));
                    free(path);
                });
        auto reset = new QPushButton("Reset all settings");
        controls->addWidget(reset);
        connect(reset, &QPushButton::clicked, this,
                [this]
                {
                    if (QMessageBox::question(this, "Reset settings",
                                              "Reset all preferences to their defaults?") == QMessageBox::Yes)
                    {
                        config_ResetAll(p_intf);
                        config_SaveConfigFile(p_intf);
                        getSettings()->remove("Redesign");
                        char *directory = config_GetUserDir(VLC_CONFIG_DIR);
                        QFile::remove(qfu(directory) + "/extra-options.txt");
                        free(directory);
                        themeChanged();
                        close();
                    }
                });
        layout->addWidget(Reframe::fold("Advanced Options", more, content));
    }
    else if (page == 1)
    {
        options(
            layout,
            "#Window|Play video inside main window=embedded-video|Resize window to fit "
            "video=qt-video-autoresize|Remember window size and position=ui:rememberWindow|Keep window above "
            "other windows=video-on-top|Show window decorations=video-deco|Show the file name in the title "
            "bar=qt-name-in-title|Open fullscreen on=qt-fullscreen-screennumber|#Playback Controls|Start in "
            "compact mode=ui:compactOnPlayback|Auto-hide the controls=ui:autoHideControls|Hide "
            "after=mouse-hide-timeout|Show controls in fullscreen=qt-fs-controller|#Notifications and System "
            "Tray|Show a system tray icon=qt-system-tray|Start minimized to the tray=qt-start-minimized|Show "
            "a popup when the track changes=qt-notification|Popup "
            "duration=ui:notificationDuration|#Playlist|Show the playlist as a "
            "tree=playlist-tree|#Skin|Use a custom skin=ui:useSkin|Skin file=skins2-last");
        auto more = new QWidget;
        auto controls = new QVBoxLayout(more);
        options(controls,
                "Window opacity=qt-opacity|Pause video when window is minimized=qt-pause-minimized|Bring VLC "
                "to the front when playback starts=qt-auto-raise|Show cover art in the window when playing "
                "audio=qt-bgcone|Show the file browser inside the open dialog=qt-embedded-open|Allow the app "
                "icon to change=qt-icon-change|Fullscreen controls opacity=qt-fs-opacity|Fullscreen controls "
                "sensitivity=qt-fs-sensitivity");
        layout->addWidget(Reframe::fold("Advanced Options", more, content));
    }
    else if (page == 2)
    {
        options(layout, "#History|Save recently played items=qt-recentplay|Remember the playback "
                        "position=qt-continue|Never remember items matching=qt-recentplay-filter");
        auto clear = new QPushButton("Clear history now");
        layout->addWidget(clear);
        connect(clear, &QPushButton::clicked, this,
                [this]
                {
                    RecentsMRL::getInstance(p_intf)->clear();
                    getSettings()->remove("Redesign/resume");
                    getSettings()->remove("Redesign/usage");
                });
        options(layout,
                "#Network|Fetch titles and cover art from the internet=metadata-network-access|Submit played "
                "tracks to last.fm=ui:lastFm|Last.fm username=lastfm-username|Last.fm "
                "password=lastfm-password|Last.fm submission URL=scrobbler-url");
    }
    else if (page == 3)
    {
        heading(layout, "Keyboard Shortcuts");
        auto edit = new QPushButton("Edit Keyboard Shortcuts");
        layout->addWidget(edit);
        connect(edit, &QPushButton::clicked, this, [this] { shortcuts(); });
        options(layout,
                "#Skip Amounts|Very short skip=extrashort-jump-size|Short skip=short-jump-size|Medium "
                "skip=medium-jump-size|Long skip=long-jump-size|#Mouse|Scroll "
                "wheel=hotkeys-y-wheel-mode|Horizontal scroll=hotkeys-x-wheel-mode|Reverse scroll "
                "direction=ui:reverseScroll|Single click on the video=ui:singleClick|Double click on the "
                "video=ui:doubleClick|#Remotes|Web remote "
                "control=ui:webRemote|Port=http-port");
    }
    else if (page == 4)
    {
        options(
            layout,
            local ? "#Playback|Playback speed=rate|Repeat playlist=loop|Repeat "
                    "item=repeat|Shuffle=random|Stop after each item=play-and-stop|Sleep "
                    "Timer=ui:sleepMinutes|Show Subtitle Timeline on Seekbar=ui:subtitleTimeline|Show "
                    "Waveform on Seekbar=ui:waveform|Audio Visualizer=ui:visualizer|#Tracks|Audio "
                    "track=audio-track|Subtitle track=sub-track"
                  : "#Startup|Continue where you left off=qt-continue|Start playing as soon as a file "
                    "opens=playlist-autostart|Default playback speed=rate|Pause when another app uses "
                    "audio=ui:pauseOtherAudio|#Tracks|Preferred video "
                    "quality=preferred-resolution|Preferred menu language=menu-language|Default audio "
                    "track=audio-track|Default subtitle track=sub-track|#Order|Repeat playlist=loop|Repeat "
                    "item=repeat|Shuffle=random|Stop after each item=play-and-stop|Exit when playback "
                    "finishes=play-and-exit|#Seeking|Seek precisely=input-fast-seek|Show thumbnail while "
                    "scrubbing=ui:seekThumbnails|#Files & Discs|Default optical drive=dvd|Repair damaged AVI "
                    "files=avi-index|Load matching .mkv files from the same "
                    "folder=mkv-preload-local-dir|Time-shift buffer folder=input-timeshift-path|Time-shift "
                    "buffer size=input-timeshift-granularity");
        if (local)
        {
            auto reverse = new QPushButton("Reverse");
            layout->addWidget(reverse);
            connect(reverse, &QPushButton::clicked, THEAM, [this] { THEAM->doAction(REVERSE_ACTION); });
            auto stats = new QWidget;
            auto values = new QLabel;
            auto column = new QVBoxLayout(stats);
            column->addWidget(values);
            auto timer = new QTimer(stats);
            timer->setInterval(500);
            connect(timer, &QTimer::timeout, stats,
                    [this, values]
                    {
                        auto item = THEMIM->currentInputItem();
                        if (!item || !item->p_stats)
                            return;
                        auto data = item->p_stats;
                        vlc_mutex_lock(&data->lock);
                        const QString text = QString("Read: %1 MiB\nInput bitrate: %2 Mbit/s\nDecoded video: "
                                                     "%3\nDisplayed: %4\nLost frames: %5\nDecoded audio: %6")
                                                 .arg(data->i_read_bytes / 1048576.0, 0, 'f', 2)
                                                 .arg(data->f_input_bitrate * 8, 0, 'f', 2)
                                                 .arg(data->i_decoded_video)
                                                 .arg(data->i_displayed_pictures)
                                                 .arg(data->i_lost_pictures)
                                                 .arg(data->i_decoded_audio);
                        vlc_mutex_unlock(&data->lock);
                        values->setText(text);
                    });
            timer->start();
            layout->addWidget(Reframe::fold("Statistics", stats, content));
        }
    }
    else if (page == 5 && local)
    {
        heading(layout, "Filters");
        layout->addWidget(videoEffects());
        heading(layout, "Defaults");
        options(layout, "Rotate=rotate-angle");
        auto horizontal = new Reframe::Dropdown;
        auto vertical = new Reframe::Dropdown;
        horizontal->addItems(QStringList({"None", "Flip"}));
        vertical->addItems(QStringList({"None", "Flip"}));
        char *raw = var_GetString(THEPL, "video-filter");
        const bool enabled = qfu(raw ? raw : "").split(':').contains("transform");
        free(raw);
        const QString mode = read("transform-type").toString();
        horizontal->setCurrentIndex(enabled && (mode == "hflip" || mode == "180") ? 1 : 0);
        vertical->setCurrentIndex(enabled && (mode == "vflip" || mode == "180") ? 1 : 0);
        auto flip = [this, horizontal, vertical](int)
        {
            const bool h = horizontal->currentIndex(), v = vertical->currentIndex();
            if (!h && !v)
                filterEnabled("transform", false, true);
            else
                write("transform-type", h && v ? "180" : h ? "hflip" : "vflip");
        };
        row(layout, "Horizontal", horizontal);
        row(layout, "Vertical", vertical);
        connect(horizontal, QOverload<int>::of(&QComboBox::activated), this, flip);
        connect(vertical, QOverload<int>::of(&QComboBox::activated), this, flip);
        options(layout,
                "Zoom=zoom|Aspect Ratio=aspect-ratio|HDR=d3d11-hdr-mode|Resolution=preferred-resolution");
        if (VLCMenuBar::rendererMenu && VLCMenuBar::rendererGroup)
        {
            auto renderer = new QPushButton("Local");
            renderer->setMenu(VLCMenuBar::rendererMenu);
            auto update = [renderer](QAction *action)
            {
                if (action)
                {
                    QString text = action->text();
                    text.remove('&');
                    text.remove('<');
                    text.remove('>');
                    renderer->setText(text);
                }
            };
            update(VLCMenuBar::rendererGroup->checkedAction());
            connect(VLCMenuBar::rendererGroup, &QActionGroup::triggered, renderer, update);
            row(layout, "Render", renderer);
        }
    }
    else if (page == 5)
    {
        options(layout, "#Output|Enable video=video|Start videos in fullscreen=fullscreen|Stop the "
                        "screensaver during playback=disable-screensaver|Play video as desktop "
                        "wallpaper=video-wallpaper|#Picture|Hue=hue|Brightness=brightness|Contrast=contrast|"
                        "Saturation=saturation|Gamma=gamma");
        auto reset = new QPushButton("Reset picture");
        layout->addWidget(reset);
        connect(
            reset, &QPushButton::clicked, this,
            [this]
            {
                for (const auto &key : QStringList({"hue", "brightness", "contrast", "saturation", "gamma"}))
                {
                    auto cfg = config_FindConfig(qtu(key));
                    if (cfg)
                        write(key, cfg->orig.f);
                }
                filterEnabled("adjust", false, true);
            });
        options(
            layout,
            "#Deinterlacing|Deinterlacing=deinterlace|Method=deinterlace-mode|#Framing|Aspect "
            "ratio=aspect-ratio|Crop=crop|Scale video to the window=autoscale|Zoom=zoom|Position in the "
            "window=align|#HDR|HDR output=d3d11-hdr-mode|#Snapshots|Save to=snapshot-path|File name "
            "prefix=snapshot-prefix|Format=snapshot-format|Width=snapshot-width|Height=snapshot-height|"
            "Number files sequentially=snapshot-sequential|Show a preview after capture=snapshot-preview");
        if (local)
            options(layout, "Resolution=preferred-resolution");
        if (VLCMenuBar::rendererMenu && VLCMenuBar::rendererGroup)
        {
            auto renderer = new QPushButton("Local");
            renderer->setMenu(VLCMenuBar::rendererMenu);
            auto update = [renderer](QAction *action)
            {
                if (action)
                {
                    QString text = action->text();
                    text.remove('&');
                    text.remove('<');
                    text.remove('>');
                    renderer->setText(text);
                }
            };
            update(VLCMenuBar::rendererGroup->checkedAction());
            connect(VLCMenuBar::rendererGroup, &QActionGroup::triggered, renderer, update);
            row(layout, "Render", renderer);
        }
        layout->addWidget(local ? videoEffects() : Reframe::fold("Video Effects", videoEffects(), content));
    }
    else if (page == 6)
    {
        if (!local)
            options(layout, "#Output|Enable audio=audio");
        auto devices = new Reframe::Dropdown;
        devices->addItem("System default", QString());
        auto aout = THEMIM->getAout();
        if (aout)
        {
            char **ids, **names;
            const int count = aout_DevicesList(aout, &ids, &names);
            char *selected = aout_DeviceGet(aout);
            for (int i = 0; i < count; ++i)
            {
                devices->addItem(qfu(names[i]), qfu(ids[i]));
                free(ids[i]);
                free(names[i]);
            }
            if (count > 0)
            {
                free(ids);
                free(names);
            }
            devices->setCurrentIndex(qMax(0, devices->findData(qfu(selected ? selected : ""))));
            free(selected);
            vlc_object_release(aout);
        }
        row(layout, "Output Device", devices);
        connect(devices, QOverload<int>::of(&QComboBox::activated), this,
                [this, devices](int index)
                {
                    auto output = THEMIM->getAout();
                    if (output)
                    {
                        aout_DeviceSet(output, qtu(devices->itemData(index).toString()));
                        vlc_object_release(output);
                    }
                });
        if (!local)
            options(layout,
                    "Speaker setup=directx-audio-speaker|Stereo mode=stereo-mode|#Volume|Maximum "
                    "volume=qt-max-volume|Remember volume=volume-save|Startup volume=ui:startupVolume|Volume "
                    "step=volume-step|#Loudness|Night mode=ui:nightMode|Target level=norm-max-level|Replay "
                    "gain=audio-replay-gain-mode|Replay gain preamp=audio-replay-gain-preamp");
        heading(layout, "Effects");
        layout->addWidget(equalizer(local));
        layout->addWidget(filter("Normalize loudness", "normvol",
                                 "Target level=norm-max-level|Buffer size=norm-buff-size"));
        layout->addWidget(filter("Spatial audio", "spatializer",
                                 "Size=spatializer-roomsize|Width=spatializer-width|Wet=spatializer-wet|Dry="
                                 "spatializer-dry|Damp=spatializer-damp"));
        options(layout, "Spatial audio mode=spatialaudio-headphones");
        layout->addWidget(filter("Stereo widening", "stereo_widen",
                                 "Delay=stereowiden-delay|Feedback=stereowiden-feedback|Crossfeed="
                                 "stereowiden-crossfeed|Dry mix=stereowiden-dry-mix"));
        layout->addWidget(filter("Compressor", "compressor",
                                 "RMS / "
                                 "peak=compressor-rms-peak|Attack=compressor-attack|Release=compressor-"
                                 "release|Threshold=compressor-threshold|Ratio=compressor-ratio|Knee="
                                 "compressor-knee|Makeup gain=compressor-makeup-gain"));
        layout->addWidget(filter("Pitch shift", "scaletempo_pitch", "Pitch shift=pitch-shift"));
        layout->addWidget(filter("Chorus/flanger", "chorus_flanger",
                                 "Delay=delay-time|Sweep depth=sweep-depth|Sweep "
                                 "rate=sweep-rate|Feedback=feedback-gain|Wet mix=wet-mix|Dry mix=dry-mix"));
        layout->addWidget(filter("Karaoke", "karaoke", ""));
        options(layout, "#Tracks|Preferred audio language=audio-language|Audio "
                        "delay=audio-desync|#Visualization|Visualizer=ui:visualizer");
    }
    else if (page == 7)
    {
        options(
            layout,
            local ? "#Defaults|Subtitle Track=sub-track|Subtitle "
                    "Language=sub-language|#Synchronization|Subtitle Track Delay=sub-delay|Subtitle "
                    "Speed=sub-fps|Subtitle Duration Factor=subsdelay-factor|#Appearance|Embedded Sub "
                    "Format=ass-use-embedded-style|Subtitle Font=freetype-font|Text "
                    "Color=freetype-color|Outline Color=freetype-outline-color|Font "
                    "Size=freetype-rel-fontsize|Exact Font Size=freetype-fontsize|Outline "
                    "Thickness=freetype-outline-thickness|Background "
                    "Opacity=freetype-background-opacity|Subtitle Offset=sub-margin"
                  : "#Tracks & Languages|Enable subtitles=spu|Preferred language=sub-language|Text "
                    "encoding=subsdec-encoding|#Loading Subtitles|Load subtitle files "
                    "automatically=sub-autodetect-file|File name "
                    "matching=sub-autodetect-fuzzy|#Appearance|Font=freetype-font|Font "
                    "size=freetype-rel-fontsize|Bold=freetype-bold|Text color=freetype-color|Text "
                    "opacity=freetype-opacity|Outline thickness=freetype-outline-thickness|Outline "
                    "color=freetype-outline-color|Shadow opacity=freetype-shadow-opacity|Shadow "
                    "color=freetype-shadow-color|Shadow distance=freetype-shadow-distance|Background "
                    "color=freetype-background-color|Background opacity=freetype-background-opacity|Vertical "
                    "position=sub-margin|#Timing|Subtitle delay=sub-delay|#On-Screen Messages|Show feedback "
                    "for shortcuts=osd|Show feedback for=osd-timeout|Show filename when a video "
                    "starts=video-title-show|Show filename for=video-title-timeout");
        if (local)
        {
            auto load = new QPushButton("Load subtitle file");
            layout->addWidget(load);
            connect(load, &QPushButton::clicked, THEAM, [this] { THEAM->doAction(OPEN_SUB_ACTION); });
        }
    }
    else if (page == 8)
    {
        options(layout, "#Opening Files|Only allow one window=one-instance|Apply to files opened from the "
                        "file manager=one-instance-when-started-from-file|Add to the playlist instead of "
                        "replacing it=playlist-enqueue|When opening a folder=recursive|Show hidden "
                        "files=show-hiddenfiles|#Metadata & Artwork|Fetch titles and "
                        "artwork=metadata-network-access|Use the media library=media-library");
        for (const auto &name : QStringList({"Videos", "Music"}))
        {
            auto edit = new QLineEdit(getSettings()->value("Redesign/" + name + "Directory").toString());
            auto widget = new QWidget;
            auto r = new QHBoxLayout(widget);
            r->setContentsMargins(0, 0, 0, 0);
            r->addWidget(edit, 1);
            auto browse = new QPushButton("Browse");
            r->addWidget(browse);
            row(layout, name + " folder", widget);
            connect(browse, &QPushButton::clicked, this,
                    [this, name, edit]
                    {
                        auto path = QFileDialog::getExistingDirectory(this, "Choose " + name + " folder",
                                                                      edit->text());
                        if (!path.isEmpty())
                        {
                            edit->setText(path);
                            getSettings()->setValue("Redesign/" + name + "Directory", path);
                        }
                    });
        }
    }
    else if (page == 9)
    {
        options(layout, "#Buffering|Local files=file-caching|Discs=disc-caching|Network "
                        "streams=network-caching|Live capture=live-caching|#Proxy|System "
                        "proxy=ui:systemProxy|HTTP proxy=http-proxy|HTTP "
                        "proxy password=http-proxy-pwd|#Security|Verify certificates against the system "
                        "trust store=gnutls-system-trust|Cipher preference=gnutls-priorities");
    }
    else if (page == 10)
    {
        options(
            layout,
            "#Decoding|Hardware-accelerated decoding=avcodec-hw|Video post-processing=postproc-q|Skip the "
            "H.264 loop filter=avcodec-skiploopfilter|Decoding threads=avcodec-threads|#Smooth Playback|Drop "
            "frames when the system cannot keep up=drop-late-frames|Give real-time "
            "priority=high-priority|Process "
            "priority=win-priority|#Developer|Encoder=sout-transcode-venc|Decoder=codec|Clock "
            "synchronisation=clock-synchro|Clock jitter=clock-jitter|Clock reference averaging=cr-average");
        auto rescan = new QPushButton("Rescan plugins");
        layout->addWidget(rescan);
        connect(rescan, &QPushButton::clicked, this,
                [this]
                {
                    if (QMessageBox::question(
                            this, "Rescan plugins",
                            "Restart Reframe to rebuild its plugin catalog? Current media resumes.") !=
                        QMessageBox::Yes)
                        return;
                    QStringList args;
                    args << "--no-one-instance"
                         << "--reset-plugins-cache";
                    auto item = THEMIM->currentInputItem();
                    if (item)
                    {
                        auto input = THEMIM->getInput();
                        if (input)
                            args << QString("--start-time=" + QString::number(var_GetInteger(input, "time") /
                                                                              double(CLOCK_FREQ)));
                        char *uri = input_item_GetURI(item);
                        args << qfu(uri);
                        free(uri);
                    }
                    config_SaveConfigFile(p_intf);
                    if (QProcess::startDetached(QCoreApplication::applicationFilePath(), args))
                        THEDP->quit();
                });
    }
    else if (page == 11)
    {
        options(layout, "#Recording|Save recordings to=input-record-path|Record without re-encoding when "
                        "possible=input-record-native|#Casting|Discover cast devices=ui:discoverCast|Cast "
                        "quality=sout-chromecast-conversion-quality|#Streaming|Play locally while "
                        "streaming=sout-display|Include audio=sout-audio|Include video=sout-video|Include "
                        "subtitles=sout-spu|Encoder preset=sout-x264-preset|Tuning=sout-x264-tune");
    }
    else if (page == 12)
    {
        heading(layout, "Add-ons");
        options(layout, "Load add-ons on startup=qt-autoload-extensions");
        auto add = new QPushButton("Find more extensions");
        layout->addWidget(add);
        connect(add, &QPushButton::clicked, this,
                [] { QDesktopServices::openUrl(QUrl("https://addons.videolan.org/")); });
        auto list = new QListWidget;
        layout->addWidget(list);
        char *data = config_GetUserDir(VLC_DATA_DIR);
        QDir dir(qfu(data) + "/lua/extensions");
        free(data);
        for (const auto &name : dir.entryList(QStringList() << "*.lua", QDir::Files))
            list->addItem(name);
        auto install = new QPushButton("Add extension");
        layout->addWidget(install);
        connect(install, &QPushButton::clicked, this,
                [this, list, dir]
                {
                    auto path = QFileDialog::getOpenFileName(this, "Add Lua extension", QString(),
                                                             "Lua extension (*.lua)");
                    if (path.isEmpty())
                        return;
                    QDir().mkpath(dir.absolutePath());
                    if (QFile::copy(path, dir.filePath(QFileInfo(path).fileName())))
                        list->addItem(QFileInfo(path).fileName());
                });
        auto remove = new QPushButton("Remove extension");
        layout->addWidget(remove);
        connect(remove, &QPushButton::clicked, this,
                [this, list, dir]
                {
                    auto item = list->currentItem();
                    if (item &&
                        QMessageBox::question(this, "Remove extension", "Remove " + item->text() + "?") ==
                            QMessageBox::Yes &&
                        QFile::remove(dir.filePath(item->text())))
                        delete item;
                });
    }
    if (page > 0 && !local)
        layout->addWidget(Reframe::fold("Advanced Options", advanced(page), content));
    if (local)
    {
        preset(layout);
        auto advanced = new QPushButton("Open Advanced Settings");
        layout->addWidget(advanced);
        connect(advanced, &QPushButton::clicked, this,
                [this, page] { window->redesign()->openSettings(page); });
    }
    layout->addStretch();
    scroll->setWidget(content);
    return scroll;
}
