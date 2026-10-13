/* Copyright 2026 webmaster94. GPL-3.0-or-later.
 * Library, metadata, and item flows adapted from Juxtopposed, CC BY 4.0. */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "redesign.hpp"
#include "redesign_controls.hpp"
#include "main_interface.hpp"
#include "dialogs_provider.hpp"
#include "input_manager.hpp"
#include "recents.hpp"
#include <vlc_input_item.h>
#include <vlc_meta.h>
#include <QApplication>
#include <QBoxLayout>
#include <QScrollArea>
#include <QStackedWidget>
#include <QListWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QFormLayout>
#include <QMenu>
#include <QSettings>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QUrl>
#include <QDesktopServices>
#include <QCryptographicHash>
#include <QInputDialog>
#include <QMessageBox>
#include <QTimer>
#include <algorithm>
#include <random>
#include <memory>

namespace
{
QString fingerprint(const QString &path)
{
    QFileInfo f(path);
    return QString::fromLatin1(
        QCryptographicHash::hash((f.absoluteFilePath() + QString::number(f.size()) +
                                  QString::number(f.lastModified().toMSecsSinceEpoch()))
                                     .toUtf8(),
                                 QCryptographicHash::Sha256)
            .toHex());
}
QString meta(input_item_t *item, vlc_meta_type_t type)
{
    char *value = input_item_GetMeta(item, type);
    const QString result = qfu(value ? value : "");
    free(value);
    return result;
}
QStringList selected(QListWidget *list)
{
    QStringList paths;
    for (auto item : list->selectedItems())
        paths << item->data(Qt::UserRole).toString();
    return paths;
}
} // namespace
void RedesignShell::requestMetadata(const QString &path)
{
    if (metadata.contains(path) || metadataRequests.contains(path) || !QFileInfo(path).isFile())
        return;
    const auto cached = getSettings()->value("Redesign/metadata/" + fingerprint(path)).toMap();
    if (!cached.isEmpty())
    {
        metadata[path] = cached;
        return;
    }
    auto item = input_item_New(qtu(QUrl::fromLocalFile(path).toString()), qtu(QFileInfo(path).fileName()));
    if (!item)
        return;
    metadataRequests[path] = item;
    metadataStarted[path] = QDateTime::currentMSecsSinceEpoch();
    if (libvlc_MetadataRequest(p_intf->obj.libvlc, item, META_REQUEST_OPTION_SCOPE_LOCAL, 1000, this) !=
        VLC_SUCCESS)
    {
        metadataRequests.remove(path);
        metadataStarted.remove(path);
        input_item_Release(item);
    }
}
void RedesignShell::pollMetadata()
{
    bool changed = false;
    const auto paths = metadataRequests.keys();
    for (const auto &path : paths)
    {
        auto item = metadataRequests.value(path);
        if (!input_item_IsPreparsed(item) &&
            QDateTime::currentMSecsSinceEpoch() - metadataStarted.value(path) < 2500)
            continue;
        QVariantMap data;
        data["title"] = meta(item, vlc_meta_Title);
        data["artist"] = meta(item, vlc_meta_Artist);
        data["album"] = meta(item, vlc_meta_Album);
        data["genre"] = meta(item, vlc_meta_Genre);
        data["track"] = meta(item, vlc_meta_TrackNumber);
        data["art"] = meta(item, vlc_meta_ArtworkURL);
        data["duration"] = qlonglong(qMax<qint64>(0, input_item_GetDuration(item) / CLOCK_FREQ));
        metadata[path] = data;
        getSettings()->setValue("Redesign/metadata/" + fingerprint(path), data);
        metadataRequests.remove(path);
        metadataStarted.remove(path);
        input_item_Release(item);
        changed = true;
    }
    if (changed && pages->currentWidget() == browser)
        refresh();
}
void RedesignShell::renderHome()
{
    if (homePanel)
    {
        static_cast<QVBoxLayout *>(browser->layout())->removeWidget(homePanel);
        delete homePanel;
        homePanel = nullptr;
    }
    auto scroll = new QScrollArea(browser);
    scroll->setWidgetResizable(true);
    homePanel = scroll;
    auto content = new QWidget;
    auto layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(24);
    const auto recent = RecentsMRL::getInstance(p_intf)->recentList();
    QStringList watching, listening, streams;
    const QStringList audio = QStringList() << "mp3"
                                            << "flac"
                                            << "wav"
                                            << "ogg"
                                            << "opus"
                                            << "m4a"
                                            << "aac"
                                            << "wma"
                                            << "aiff";
    for (const auto &uri : recent)
    {
        const QUrl url(uri);
        const QString path = url.isLocalFile() ? url.toLocalFile() : uri;
        if (url.isLocalFile())
        {
            if (audio.contains(QFileInfo(path).suffix().toLower()))
                listening << path;
            else if (getSettings()->value("Redesign/resume/" + uri).toMap().value("position").toDouble() <
                     .98)
                watching << path;
        }
        else
            streams << uri;
    }
    const auto favorites = getSettings()->value("Redesign/favorites").toStringList();
    const QList<QPair<QString, QStringList>> groups =
        QList<QPair<QString, QStringList>>()
        << qMakePair(QString("Continue Watching"), watching) << qMakePair(QString("Listen Again"), listening)
        << qMakePair(QString("Favorites"), favorites) << qMakePair(QString("Recent Streams"), streams);
    for (const auto &group : groups)
    {
        auto title = new QLabel(group.first);
        title->setObjectName("sectionHeading");
        layout->addWidget(title);
        if (group.second.isEmpty())
        {
            layout->addWidget(new QLabel(
                group.first == "Continue Watching" ? "Your recent videos will appear here."
                : group.first == "Listen Again"    ? "Your recent music will appear here."
                : group.first == "Favorites"       ? "Favorite a file, folder, or playlist to pin it here."
                                                   : "Your recent network streams will appear here."));
            continue;
        }
        auto list = new QListWidget;
        list->setViewMode(QListView::IconMode);
        list->setFlow(QListView::LeftToRight);
        list->setWrapping(false);
        list->setMovement(QListView::Static);
        list->setGridSize(QSize(280, 220));
        list->setIconSize(QSize(260, 146));
        list->setFixedHeight(230);
        list->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        list->setObjectName("homeMedia");
        for (const auto &path : group.second)
        {
            QString title = QFileInfo(path).fileName();
            QIcon icon = Reframe::icon(group.first == "Listen Again"     ? "music"
                                       : group.first == "Recent Streams" ? "network"
                                                                         : "videos");
            if (path.startsWith("collection:"))
            {
                const auto data = getSettings()->value("Redesign/collection/" + path.mid(11)).toMap();
                title = data.value("name").toString();
                if (!data.value("thumbnail").toString().isEmpty())
                    icon = QIcon(data.value("thumbnail").toString());
            }
            else if (QFileInfo(path).isDir())
                icon = Reframe::icon("folder");
            else if (QFileInfo(thumbnails.value(path)).size() > 0)
                icon = QIcon(thumbnails.value(path));
            else if (QUrl(metadata.value(path).value("art").toString()).isLocalFile())
                icon = QIcon(QUrl(metadata.value(path).value("art").toString()).toLocalFile());
            auto item = new QListWidgetItem(icon, title.isEmpty() ? path : title, list);
            item->setData(Qt::UserRole, path);
            item->setToolTip(path);
        }
        connect(list, &QListWidget::itemDoubleClicked, this,
                [this](QListWidgetItem *item)
                {
                    const QString path = item->data(Qt::UserRole).toString();
                    if (QFileInfo(path).isDir())
                    {
                        section = "Videos";
                        browse(path);
                    }
                    else
                        openMedia(path);
                });
        layout->addWidget(list);
    }
    layout->addStretch();
    scroll->setWidget(content);
    static_cast<QVBoxLayout *>(browser->layout())->addWidget(scroll, 1);
    files->hide();
    empty->hide();
    scroll->show();
}
void RedesignShell::refreshPins()
{
    auto layout = static_cast<QVBoxLayout *>(sidebar->layout());
    auto pins = sidebar->findChild<QWidget *>("pinnedDestinations");
    if (!pins)
    {
        pins = new QWidget(sidebar);
        pins->setObjectName("pinnedDestinations");
        auto rows = new QVBoxLayout(pins);
        rows->setContentsMargins(0, 0, 0, 0);
        rows->setSpacing(8);
        layout->insertWidget(7, pins);
    }
    auto rows = static_cast<QVBoxLayout *>(pins->layout());
    while (auto item = rows->takeAt(0))
    {
        delete item->widget();
        delete item;
    }
    const auto favorites = getSettings()->value("Redesign/favorites").toStringList();
    for (const auto &path : favorites)
    {
        const auto url = QUrl(path);
        const auto local = url.isLocalFile() ? url.toLocalFile() : path;
        QString name = QFileInfo(local).fileName();
        if (path.startsWith("collection:"))
            name =
                getSettings()->value("Redesign/collection/" + path.mid(11)).toMap().value("name").toString();
        auto b = button(QFileInfo(local).isDir()         ? "folder"
                        : path.startsWith("collection:") ? "playlist"
                                                         : "favorite",
                        name.isEmpty() ? path : name);
        b->setToolTip(path);
        b->setToolButtonStyle(compact ? Qt::ToolButtonIconOnly : Qt::ToolButtonTextBesideIcon);
        rows->addWidget(b);
        connect(b, &QToolButton::clicked, this,
                [this, path, local]
                {
                    if (QFileInfo(local).isDir())
                    {
                        section = "Videos";
                        browse(local);
                    }
                    else
                        openMedia(path);
                });
    }
    pins->setVisible(!favorites.isEmpty());
}
void RedesignShell::mediaProperties(const QString &path)
{
    auto dialog = new Reframe::Dialog(window, "Properties & Metadata", QSize(720, 640));
    auto layout = new QFormLayout;
    dialog->body->addLayout(layout);
    const QFileInfo file(path);
    const auto data = metadata.value(path);
    layout->addRow("File", new QLabel(file.fileName()));
    auto address = new QLineEdit(path);
    address->setReadOnly(true);
    layout->addRow("Location", address);
    layout->addRow("Size", new QLabel(QString::number(file.size() / 1048576.0, 'f', 2) + " MiB"));
    layout->addRow("Duration", new QLabel(QString::number(data.value("duration").toLongLong()) + " seconds"));
    const QStringList fields = QStringList() << "title"
                                             << "artist"
                                             << "album"
                                             << "genre";
    auto edits = std::make_shared<QHash<QString, QLineEdit *>>();
    for (const auto &name : fields)
    {
        auto edit = new QLineEdit(data.value(name).toString());
        (*edits)[name] = edit;
        layout->addRow(name[0].toUpper() + name.mid(1), edit);
    }
    auto save = new QPushButton("Save Library Metadata");
    dialog->body->addWidget(save);
    connect(save, &QPushButton::clicked, this,
            [this, path, dialog, edits]
            {
                auto data = metadata.value(path);
                for (auto it = edits->begin(); it != edits->end(); ++it)
                    data[it.key()] = it.value()->text();
                metadata[path] = data;
                getSettings()->setValue("Redesign/metadata/" + fingerprint(path), data);
                dialog->close();
                refresh();
            });
    auto original = new QPushButton("Write Metadata to Media File");
    original->setEnabled(file.isFile() && file.isWritable());
    dialog->body->addWidget(original);
    connect(original, &QPushButton::clicked, this,
            [this, path, dialog, edits]
            {
                auto item = input_item_New(qtu(QUrl::fromLocalFile(path).toString()),
                                           qtu(QFileInfo(path).fileName()));
                if (!item)
                    return;
                input_item_SetTitle(item, qtu((*edits)["title"]->text()));
                input_item_SetArtist(item, qtu((*edits)["artist"]->text()));
                input_item_SetAlbum(item, qtu((*edits)["album"]->text()));
                input_item_SetGenre(item, qtu((*edits)["genre"]->text()));
                const int result = input_item_WriteMeta(VLC_OBJECT(THEPL), item);
                input_item_Release(item);
                if (result != VLC_SUCCESS)
                    QMessageBox::warning(
                        dialog, "Metadata",
                        "This format could not be updated. You can save the labels to the library instead.");
                else
                    dialog->close();
            });
    Reframe::reveal(dialog);
}
void RedesignShell::contextMenu(const QPoint &position)
{
    auto item = files->itemAt(position);
    if (!item)
        return;
    if (!item->isSelected())
    {
        files->clearSelection();
        item->setSelected(true);
    }
    const QString path = item->data(Qt::UserRole).toString();
    const auto targets = selected(files);
    const bool collection = path.startsWith("collection:"), folder = QFileInfo(path).isDir();
    QStringList urls;
    for (const auto &target : targets)
    {
        if (target.startsWith("collection:"))
        {
            const auto members = getSettings()
                                     ->value("Redesign/collection/" + target.mid(11))
                                     .toMap()
                                     .value("paths")
                                     .toStringList();
            for (const auto &member : members)
                urls << QUrl::fromUserInput(member, QString(), QUrl::AssumeLocalFile).toString();
        }
        else
            urls << QUrl::fromUserInput(target, QString(), QUrl::AssumeLocalFile).toString();
    }
    QMenu menu;
    menu.setMinimumWidth(265);
    menu.addAction(folder ? "Open" : "Play", this,
                   [this, path, folder]
                   {
                       if (folder)
                           browse(path);
                       else
                           openMedia(path);
                   });
    if (folder)
    {
        menu.addAction("Add File", this,
                       [this, path]
                       {
                           auto files =
                               QFileDialog::getOpenFileNames(this, "Add files to this library folder");
                           auto references =
                               getSettings()->value("Redesign/libraryReferences/" + path).toStringList();
                           references += files;
                           references.removeDuplicates();
                           getSettings()->setValue("Redesign/libraryReferences/" + path, references);
                           refresh();
                       });
        menu.addAction("Add Folder", this,
                       [this]
                       {
                           auto folder = QFileDialog::getExistingDirectory(this, "Add a media folder");
                           if (folder.isEmpty())
                               return;
                           auto favorites = getSettings()->value("Redesign/favorites").toStringList();
                           if (!favorites.contains(folder))
                               favorites << folder;
                           getSettings()->setValue("Redesign/favorites", favorites);
                           refreshPins();
                       });
        menu.addAction("Resync", this,
                       [this]
                       {
                           metadata.clear();
                           refresh();
                       });
    }
    else
    {
        menu.addAction("Stream", this, [this, urls] { THEDP->streamingDialog(window, urls, true); });
        menu.addAction("Convert", this, [this, urls] { THEDP->streamingDialog(window, urls, false); });
    }
    menu.addAction("Add to Queue", this,
                   [this, urls]
                   {
                       for (const auto &url : urls)
                           Open::openMRL(p_intf, url, false);
                   });
    auto playlists = menu.addMenu("Add to Playlist");
    for (const auto &id : getSettings()->value("Redesign/collections").toStringList())
    {
        auto data = getSettings()->value("Redesign/collection/" + id).toMap();
        playlists->addAction(data.value("name").toString(), this,
                             [this, id, urls]
                             {
                                 auto data = getSettings()->value("Redesign/collection/" + id).toMap();
                                 auto paths = data.value("paths").toStringList();
                                 paths += urls;
                                 data["paths"] = paths;
                                 getSettings()->setValue("Redesign/collection/" + id, data);
                             });
    }
    playlists->addAction("New Playlist", this, [this] { newPlaylist(); });
    menu.addAction("Properties & Metadata", this, [this, path] { mediaProperties(path); });
    menu.addAction(folder ? "Edit Subtitle Tracks" : "Add Subtitle Tracks", this,
                   [this, targets] { batchTracks(targets, 0); });
    menu.addAction(folder ? "Edit Audio Tracks" : "Add Audio Tracks", this,
                   [this, targets] { batchTracks(targets, 1); });
    menu.addSeparator();
    auto favorite = menu.addAction("Favorite");
    favorite->setCheckable(true);
    favorite->setChecked(getSettings()->value("Redesign/favorites").toStringList().contains(path));
    connect(favorite, &QAction::triggered, this,
            [this, path](bool on)
            {
                auto favorites = getSettings()->value("Redesign/favorites").toStringList();
                favorites.removeAll(path);
                if (on)
                    favorites << path;
                getSettings()->setValue("Redesign/favorites", favorites);
                refreshPins();
            });
    if (collection)
    {
        menu.addAction("Export Playlist", this,
                       [this, path]
                       {
                           QString file = QFileDialog::getSaveFileName(this, "Export playlist", QString(),
                                                                       "M3U playlist (*.m3u8)");
                           if (file.isEmpty())
                               return;
                           QFile output(file);
                           if (!output.open(QIODevice::WriteOnly | QIODevice::Text))
                               return;
                           QTextStream stream(&output);
                           stream.setCodec("UTF-8");
                           stream << "#EXTM3U\n";
                           for (const auto &uri : getSettings()
                                                      ->value("Redesign/collection/" + path.mid(11))
                                                      .toMap()
                                                      .value("paths")
                                                      .toStringList())
                               stream << QUrl::fromUserInput(uri, QString(), QUrl::AssumeLocalFile).toString()
                                      << '\n';
                       });
        menu.addAction("Edit Playlist", this, [this, path] { newPlaylist(path.mid(11)); });
        menu.addAction(
            "Shuffle", this,
            [this, path]
            {
                auto data = getSettings()->value("Redesign/collection/" + path.mid(11)).toMap();
                auto paths = data.value("paths").toStringList();
                std::mt19937 rng(static_cast<unsigned>(QDateTime::currentMSecsSinceEpoch()));
                std::shuffle(paths.begin(), paths.end(), rng);
                bool first = true;
                for (const auto &uri : paths)
                {
                    Open::openMRL(
                        p_intf, QUrl::fromUserInput(uri, QString(), QUrl::AssumeLocalFile).toString(), first);
                    first = false;
                }
                showQueue();
            });
    }
    else
    {
        menu.addAction("Show in File Explorer", this,
                       [path]
                       { QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath())); });
        menu.addAction("Mark as Watched", this,
                       [this, path]
                       {
                           const QString uri = QUrl::fromLocalFile(path).toString();
                           auto data = getSettings()->value("Redesign/resume/" + uri).toMap();
                           data["position"] = 1.0;
                           getSettings()->setValue("Redesign/resume/" + uri, data);
                           refresh();
                       });
        menu.addAction("Go to Source", this, [this, path] { browse(QFileInfo(path).absolutePath()); });
    }
    menu.exec(files->viewport()->mapToGlobal(position));
}
