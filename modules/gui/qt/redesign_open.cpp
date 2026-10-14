/* Copyright 2026 webmaster94. GPL-3.0-or-later.
 * Source and pin dialogs adapted from Juxtopposed's VLC Redesign, CC BY 4.0. */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "redesign.hpp"
#include "redesign_controls.hpp"
#include "main_interface.hpp"
#include "components/open_panels.hpp"
#include "recents.hpp"
#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QListWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QScrollArea>
#include <QFormLayout>
#include <QSettings>
#include <QUrl>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMessageBox>
#include <QTimer>
#include <QUuid>
#include <memory>

namespace
{
class SourceFiles : public QListWidget
{
  public:
    std::function<void(QStringList)> addPaths;
    SourceFiles()
    {
        setAcceptDrops(true);
        setSelectionMode(QAbstractItemView::ExtendedSelection);
    }

  protected:
    void dragEnterEvent(QDragEnterEvent *event) override
    {
        if (event->mimeData()->hasUrls())
            event->acceptProposedAction();
        else
            event->ignore();
    }
    void dragMoveEvent(QDragMoveEvent *event) override
    {
        if (event->mimeData()->hasUrls())
            event->acceptProposedAction();
        else
            event->ignore();
    }
    void dropEvent(QDropEvent *event) override
    {
        QStringList paths;
        for (const auto &url : event->mimeData()->urls())
            if (url.isLocalFile() && url.host().isEmpty())
            {
                const QString path = QDir::fromNativeSeparators(url.toLocalFile());
                if (QDir::isAbsolutePath(path) && !path.startsWith("//") && !path.contains(QChar(0)))
                    paths << path;
            }
        if (addPaths)
            addPaths(paths);
        event->acceptProposedAction();
    }
};
} // namespace

void RedesignShell::openSource(int kind)
{
    leaveFullscreen();
    const QString title = kind == 1   ? "Open Folder"
                          : kind == 2 ? "Open Disc"
                          : kind == 3 ? "Open Capture Device"
                                      : "Open Files";
    auto dialog = new Reframe::Dialog(window, title, QSize(880, 630));
    dialog->setObjectName("sourceDialog");
    dialog->setProperty("sourceKind", kind);
    auto sources = std::make_shared<QStringList>();
    auto sourceOptions = std::make_shared<QString>();
    OpenPanel *panel = nullptr;
    SourceFiles *files = nullptr;
    if (kind < 2)
    {
        dialog->body->addWidget(
            new QLabel(kind == 1 ? "Media folder" : "Select files or drop them into the list."));
        files = new SourceFiles;
        files->setObjectName("sourceFiles");
        dialog->body->addWidget(files, 1);
        auto addPaths = [files, sources, kind](const QStringList &paths)
        {
            for (const auto &path : paths)
            {
                if (sources->size() >= 512 || sources->contains(path))
                    continue;
                const QFileInfo info(path);
                if (kind == 1 ? !info.isDir() : !info.isFile())
                    continue;
                if (kind == 1)
                {
                    sources->clear();
                    files->clear();
                }
                *sources << path;
                auto item = new QListWidgetItem(Reframe::icon(kind == 1 ? "folder" : "videos"),
                                                info.fileName(), files);
                item->setData(Qt::UserRole, path);
                item->setToolTip(path);
            }
        };
        files->addPaths = addPaths;
        auto buttons = new QHBoxLayout;
        auto browse = new QPushButton(kind == 1 ? "Choose Folder" : "Add Files");
        auto remove = new QPushButton("Remove Selected");
        buttons->addWidget(browse);
        buttons->addWidget(remove);
        buttons->addStretch();
        dialog->body->addLayout(buttons);
        connect(browse, &QPushButton::clicked, dialog,
                [dialog, addPaths, kind]
                {
                    if (kind == 1)
                        addPaths(QStringList()
                                 << QFileDialog::getExistingDirectory(dialog, "Choose Media Folder"));
                    else
                        addPaths(QFileDialog::getOpenFileNames(dialog, "Select Media Files"));
                });
        connect(remove, &QPushButton::clicked, dialog,
                [files, sources]
                {
                    for (auto item : files->selectedItems())
                    {
                        sources->removeAll(item->data(Qt::UserRole).toString());
                        delete item;
                    }
                });
    }
    else
    {
        panel = kind == 2 ? static_cast<OpenPanel *>(new DiscOpenPanel(dialog, p_intf))
                          : static_cast<OpenPanel *>(new CaptureOpenPanel(dialog, p_intf));
        auto scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidget(panel);
        dialog->body->addWidget(scroll, 1);
        connect(panel, &OpenPanel::mrlUpdated, dialog,
                [sources, sourceOptions](const QStringList &items, const QString &options)
                {
                    *sources = items;
                    *sourceOptions = options;
                });
        panel->show();
        if (kind == 3)
            static_cast<CaptureOpenPanel *>(panel)->initialize();
        panel->onFocus();
        QTimer::singleShot(0, panel, [panel] { panel->updateMRL(); });
    }
    auto name = new QLineEdit(dialog);
    name->setMaxLength(256);
    name->setObjectName("sourcePlaylistName");
    name->setPlaceholderText("Playlist name (optional)");
    if (kind < 2)
        dialog->body->addWidget(name);
    else
        name->hide();
    auto advanced = new QWidget;
    auto form = new QFormLayout(advanced);
    auto caching = new QDoubleSpinBox;
    caching->setDecimals(0);
    caching->setRange(0, 60000);
    caching->setValue(kind == 3 ? 300 : 1000);
    caching->setSuffix(" ms");
    auto start = new QDoubleSpinBox;
    start->setRange(0, 604800);
    start->setDecimals(1);
    start->setSuffix(" s");
    auto stop = new QDoubleSpinBox;
    stop->setRange(0, 604800);
    stop->setDecimals(1);
    stop->setSuffix(" s");
    stop->setSpecialValueText("End of media");
    form->addRow("Caching", caching);
    form->addRow("Start time", start);
    form->addRow("Stop time", stop);
    auto subtitle = new QLineEdit;
    subtitle->setMaxLength(4096);
    subtitle->setPlaceholderText("Optional local subtitle file");
    auto subtitleRow = new QWidget;
    auto subtitleLayout = new QHBoxLayout(subtitleRow);
    subtitleLayout->setContentsMargins(0, 0, 0, 0);
    auto subtitleBrowse = new QPushButton("Browse");
    subtitleLayout->addWidget(subtitle, 1);
    subtitleLayout->addWidget(subtitleBrowse);
    form->addRow("Subtitle file", subtitleRow);
    connect(subtitleBrowse, &QPushButton::clicked, dialog,
            [dialog, subtitle]
            {
                const QString file =
                    QFileDialog::getOpenFileName(dialog, "Choose Subtitle", QString(),
                                                 "Subtitles (*.srt *.vtt *.ass *.ssa *.sub);;All files (*)");
                if (!file.isEmpty())
                    subtitle->setText(file);
            });
    dialog->body->addWidget(Reframe::fold("Advanced Options", advanced, dialog));
    auto footer = new QHBoxLayout;
    footer->addStretch();
    auto cancel = new QPushButton("Cancel"), enqueue = new QPushButton("Add to Queue"),
         play = new QPushButton("Play");
    play->setObjectName("sourcePlay");
    enqueue->setObjectName("sourceEnqueue");
    footer->addWidget(cancel);
    footer->addWidget(enqueue);
    footer->addWidget(play);
    dialog->body->addLayout(footer);
    connect(cancel, &QPushButton::clicked, dialog, &QDialog::close);
    auto open = [this, dialog, sources, sourceOptions, panel, kind, caching, start, stop, subtitle,
                 name](bool playing)
    {
        if (panel)
        {
            panel->onAccept();
            panel->updateMRL();
        }
        if (sources->isEmpty())
        {
            QMessageBox::information(dialog, "Select media", "Select a media source first.");
            return;
        }
        if (stop->value() > 0 && stop->value() <= start->value())
        {
            QMessageBox::warning(dialog, "Playback range", "Stop time must be later than start time.");
            return;
        }
        QStringList options = sourceOptions->split(" :", QString::SkipEmptyParts);
        for (auto &option : options)
            if (option.startsWith(':'))
                option.remove(0, 1);
        options << QString("%1-caching=%2")
                       .arg(kind == 2   ? "disc"
                            : kind == 3 ? "live"
                                        : "file")
                       .arg(qRound(caching->value()));
        if (start->value() > 0)
            options << "start-time=" + QString::number(start->value());
        if (stop->value() > 0)
            options << "stop-time=" + QString::number(stop->value());
        if (!subtitle->text().isEmpty())
        {
            if (!QFileInfo(subtitle->text()).isFile())
            {
                QMessageBox::warning(dialog, "Subtitle file", "Choose an existing local subtitle file.");
                return;
            }
            options << "input-slave=" + QUrl::fromLocalFile(subtitle->text()).toString(QUrl::FullyEncoded);
        }
        if (kind < 2 && !name->text().trimmed().isEmpty())
        {
            const QString id = QUuid::createUuid().toString();
            QVariantMap data;
            data["name"] = name->text().trimmed();
            data["paths"] = *sources;
            data["type"] = "playlist";
            getSettings()->setValue("Redesign/collection/" + id, data);
            auto ids = getSettings()->value("Redesign/collections").toStringList();
            ids << id;
            getSettings()->setValue("Redesign/collections", ids);
        }
        bool first = true;
        for (const auto &path : *sources)
        {
            const QString uri = kind < 2 ? QUrl::fromLocalFile(path).toString() : path;
            Open::openMRLwithOptions(p_intf, uri, &options, playing && first, true);
            first = false;
        }
        dialog->close();
        if (playing)
            showPlayer();
    };
    connect(play, &QPushButton::clicked, dialog, [open] { open(true); });
    connect(enqueue, &QPushButton::clicked, dialog, [open] { open(false); });
    Reframe::reveal(dialog);
    Reframe::centerOnParent(dialog);
}

void RedesignShell::pinDestination()
{
    auto dialog = new Reframe::Dialog(window, "Pin to Sidebar", QSize(650, 440));
    dialog->setObjectName("pinDialog");
    auto type = new Reframe::Dropdown;
    type->addItems(QStringList({"File(s)", "Folder", "Network Stream", "Saved Playlist"}));
    dialog->body->addWidget(new QLabel("Choose what to pin. Pinned items appear in the navigation sidebar."));
    dialog->body->addWidget(type);
    auto paths = new SourceFiles;
    paths->setObjectName("pinFiles");
    dialog->body->addWidget(paths, 1);
    auto address = new QLineEdit;
    address->setMaxLength(4096);
    address->setPlaceholderText("https://example.com/stream");
    address->hide();
    dialog->body->addWidget(address);
    auto collections = new Reframe::Dropdown;
    for (const auto &id : getSettings()->value("Redesign/collections").toStringList())
        collections->addItem(
            getSettings()->value("Redesign/collection/" + id).toMap().value("name").toString(),
            QString("collection:" + id));
    collections->hide();
    dialog->body->addWidget(collections);
    auto browse = new QPushButton("Choose Files");
    dialog->body->addWidget(browse);
    auto addPaths = [paths](const QStringList &items)
    {
        for (const auto &path : items)
        {
            if (!QFileInfo::exists(path) || paths->count() >= 256)
                continue;
            bool exists = false;
            for (int i = 0; i < paths->count(); ++i)
                exists |= paths->item(i)->data(Qt::UserRole).toString() == path;
            if (exists)
                continue;
            auto item = new QListWidgetItem(QFileInfo(path).fileName(), paths);
            item->setData(Qt::UserRole, path);
            item->setToolTip(path);
        }
    };
    paths->addPaths = addPaths;
    connect(browse, &QPushButton::clicked, dialog,
            [dialog, type, addPaths]
            {
                if (type->currentIndex() == 1)
                    addPaths(QStringList() << QFileDialog::getExistingDirectory(dialog, "Choose Folder"));
                else
                    addPaths(QFileDialog::getOpenFileNames(dialog, "Choose Files to Pin"));
            });
    connect(type, QOverload<int>::of(&QComboBox::currentIndexChanged), dialog,
            [paths, address, collections, browse](int index)
            {
                paths->clear();
                paths->setVisible(index < 2);
                browse->setVisible(index < 2);
                browse->setText(index == 1 ? "Choose Folder" : "Choose Files");
                address->setVisible(index == 2);
                collections->setVisible(index == 3);
            });
    auto footer = new QHBoxLayout;
    footer->addStretch();
    auto cancel = new QPushButton("Cancel"), pin = new QPushButton("Pin");
    pin->setObjectName("pinSave");
    footer->addWidget(cancel);
    footer->addWidget(pin);
    dialog->body->addLayout(footer);
    connect(cancel, &QPushButton::clicked, dialog, &QDialog::close);
    connect(pin, &QPushButton::clicked, dialog,
            [this, dialog, paths, type, address, collections]
            {
                QStringList items;
                if (type->currentIndex() < 2)
                    for (int i = 0; i < paths->count(); ++i)
                        items << paths->item(i)->data(Qt::UserRole).toString();
                else if (type->currentIndex() == 2)
                {
                    const QUrl url(address->text().trimmed());
                    if (!url.isValid() || url.scheme().isEmpty() || url.isLocalFile())
                    {
                        QMessageBox::warning(dialog, "Network stream",
                                             "Enter a complete network URL with a protocol.");
                        return;
                    }
                    items << url.toString();
                }
                else if (collections->currentIndex() >= 0)
                    items << collections->currentData().toString();
                if (items.isEmpty())
                    return;
                auto favorites = getSettings()->value("Redesign/favorites").toStringList();
                for (const auto &item : items)
                    if (!favorites.contains(item) && favorites.size() < 256)
                        favorites << item;
                getSettings()->setValue("Redesign/favorites", favorites);
                refreshPins();
                if (section == "Home")
                    renderHome();
                dialog->close();
            });
    Reframe::reveal(dialog);
    Reframe::centerOnParent(dialog);
}
