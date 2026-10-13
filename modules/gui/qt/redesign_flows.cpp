/* Copyright 2026 webmaster94. GPL-2.0-or-later.
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
#include "actions_manager.hpp"
#include "dialogs_provider.hpp"
#include "recents.hpp"
#include <vlc_configuration.h>
#include <vlc_input.h>
#include <vlc_input_item.h>
#include <vlc_aout.h>
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
#include <QMimeData>
#include <QDropEvent>
#include <QKeyEvent>
#include <QTimer>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <functional>
#include <algorithm>

namespace {
QString timeText(qint64 seconds){return QString("%1:%2").arg(seconds/60).arg(seconds%60,2,10,QChar('0'));}
QStringList selectedPaths(QListWidget *list){QStringList result;for(auto item:list->selectedItems())result<<item->data(Qt::UserRole).toString();return result;}
QStringList queuePaths(playlist_t *pl){QStringList paths;vlc_playlist_locker lock(pl);std::function<void(playlist_item_t*)> visit=[&](playlist_item_t *node){if(node->i_children<0){char *uri=input_item_GetURI(node->p_input);paths<<qfu(uri);free(uri);}else for(int i=0;i<node->i_children;++i)visit(node->pp_children[i]);};visit(pl->p_playing);return paths;}
class FileList : public QListWidget {
public:
    explicit FileList(QWidget *p=nullptr):QListWidget(p){setAcceptDrops(true);setDragDropMode(QAbstractItemView::InternalMove);setDefaultDropAction(Qt::MoveAction);setSelectionMode(QAbstractItemView::ExtendedSelection);}
    void dropEvent(QDropEvent *e) override{if(e->mimeData()->hasUrls()){for(const auto &url:e->mimeData()->urls()){if(!url.isLocalFile())continue;auto path=url.toLocalFile();auto item=new QListWidgetItem(QFileInfo(path).fileName(),this);item->setData(Qt::UserRole,path);}e->acceptProposedAction();}else QListWidget::dropEvent(e);}
};
void fileItems(QListWidget *list,const QStringList &paths){for(const auto &path:paths){auto item=new QListWidgetItem(QFileInfo(path).fileName().isEmpty()?path:QFileInfo(path).fileName(),list);item->setData(Qt::UserRole,path);item->setToolTip(path);}}
QString saveCollection(QSettings *settings,QVariantMap data,const QString &previous=QString()){
    QString id=previous.isEmpty()?QUuid::createUuid().toString():previous;
    auto ids=settings->value("Redesign/collections").toStringList();if(!ids.contains(id)){ids<<id;settings->setValue("Redesign/collections",ids);}settings->setValue("Redesign/collection/"+id,data);return id;
}
}

void RedesignShell::openMediaSettings(int page){
    showPlayer();
    if(sidePanel){auto preferences=dynamic_cast<RedesignPreferences *>(sidePanel);if(preferences){preferences->selectPage(page);sidePanel->show();return;}bodyLayout->removeWidget(sidePanel);delete sidePanel;sidePanel=nullptr;}
    auto prefs=new RedesignPreferences(p_intf,window,page,[this]{applyTheme();},true);
    prefs->setWindowFlags(Qt::Widget);prefs->setParent(this);prefs->setFixedWidth(400);sidePanel=prefs;bodyLayout->addWidget(sidePanel);
    connect(prefs,&QObject::destroyed,this,[this,prefs]{if(sidePanel==prefs)sidePanel=nullptr;});prefs->show();
    Reframe::animateHeight(prefs,true,104);
}
void RedesignShell::applyMediaBindings(){
    auto input=THEMIM->getInput();auto item=THEMIM->currentInputItem();if(!input||!item){bindingInput.clear();return;}
    char *raw=input_item_GetURI(item);const QString uri=qfu(raw?raw:"");free(raw);const QString id=uri+QString::number(quintptr(input));if(bindingInput==id)return;bindingInput=id;
    const QString path=QUrl(uri).isLocalFile()?QUrl(uri).toLocalFile():uri;
    for(int kind=0;kind<2;++kind){const QString prefix=kind?"Redesign/audioBindings/":"Redesign/bindings/";auto tracks=getSettings()->value(prefix+path).toStringList();if(tracks.isEmpty())tracks=getSettings()->value(prefix+uri).toStringList();for(const auto &track:tracks){const QString source=QUrl::fromUserInput(track,QString(),QUrl::AssumeLocalFile).toString();input_AddSlave(input,kind?SLAVE_TYPE_AUDIO:SLAVE_TYPE_SPU,qtu(source),false,true,false);}}
    const QString folder=QFileInfo(path).absolutePath();const auto defaults=getSettings()->value("Redesign/mediaDefaults/"+folder).toMap();
    if(!defaults.isEmpty()){auto preferences=new RedesignPreferences(p_intf,window,4,[this]{applyTheme();},true);preferences->restore(defaults);preferences->deleteLater();}
}
void RedesignShell::toggleLibraryDock(){
    if(libraryDock){bodyLayout->removeWidget(libraryDock);delete libraryDock;libraryDock=nullptr;return;}
    showPlayer();libraryDock=new QWidget(this);libraryDock->setObjectName("libraryDock");libraryDock->setFixedWidth(320);auto layout=new QVBoxLayout(libraryDock);layout->setContentsMargins(8,8,8,8);
    auto header=new QHBoxLayout;auto close=button("close");auto name=new QLabel("Library");header->addWidget(name,1);header->addWidget(close);layout->addLayout(header);
    auto search=new QLineEdit;search->setPlaceholderText("Search Library");layout->addWidget(search);
    auto list=new QListWidget;list->setObjectName("compactLibrary");layout->addWidget(list,1);
    for(int i=0;i<files->count();++i)list->addItem(files->item(i)->clone());
    connect(list,&QListWidget::itemDoubleClicked,this,[this,list,name](QListWidgetItem *item){auto path=item->data(Qt::UserRole).toString();if(QFileInfo(path).isDir()){browse(path);showPlayer();list->clear();for(int i=0;i<files->count();++i)list->addItem(files->item(i)->clone());name->setText(QFileInfo(path).fileName());}else openMedia(path);});
    connect(search,&QLineEdit::textChanged,list,[list](const QString &value){for(int i=0;i<list->count();++i)list->item(i)->setHidden(!list->item(i)->text().contains(value,Qt::CaseInsensitive));});
    connect(close,&QToolButton::clicked,this,[this]{toggleLibraryDock();});bodyLayout->insertWidget(1,libraryDock);Reframe::animateHeight(libraryDock,true,208);
}
void RedesignShell::showQueue(int selected){
    showPlayer();
    if(sidePanel&&sidePanel->objectName()=="queuePanel"){auto tabs=sidePanel->findChild<QStackedWidget *>("queueTabs");tabs->setCurrentIndex(selected);sidePanel->show();return;}
    if(sidePanel){bodyLayout->removeWidget(sidePanel);delete sidePanel;sidePanel=nullptr;}
    sidePanel=new QWidget(this);sidePanel->setObjectName("queuePanel");sidePanel->setFixedWidth(360);auto layout=new QVBoxLayout(sidePanel);layout->setContentsMargins(12,8,12,12);layout->setSpacing(16);
    auto close=button("close");auto tabs=Reframe::segmented(QStringList()<<"Queue"<<"Chapters"<<"Bookmarks",QStringList({"Queue","Chapters","Bookmarks"})[selected],sidePanel,[this](const QString &value){auto stack=sidePanel->findChild<QStackedWidget *>("queueTabs");stack->setCurrentIndex(QStringList({"Queue","Chapters","Bookmarks"}).indexOf(value));updateQueue();});
    auto top=new QHBoxLayout;top->addWidget(tabs,1);top->addWidget(close);layout->addLayout(top);connect(close,&QToolButton::clicked,this,[this]{sidePanel->hide();});
    auto stack=new QStackedWidget;stack->setObjectName("queueTabs");layout->addWidget(stack,1);
    for(int i=0;i<3;++i){auto page=new QWidget;auto body=new QVBoxLayout(page);body->setContentsMargins(0,0,0,0);auto list=new QListWidget;list->setObjectName(i==0?"queueItems":i==1?"chapterItems":"bookmarkItems");list->setSpacing(6);body->addWidget(list,1);
        if(i==0){list->setDragDropMode(QAbstractItemView::InternalMove);list->setDefaultDropAction(Qt::MoveAction);
            connect(list->model(),&QAbstractItemModel::rowsMoved,list,[this,list](const QModelIndex &,int,int,const QModelIndex &,int){vlc_playlist_locker lock(THEPL);for(int row=0;row<list->count();++row){auto item=playlist_ItemGetById(THEPL,list->item(row)->data(Qt::UserRole).toInt());if(item)playlist_TreeMove(THEPL,item,THEPL->p_playing,row);}});
            connect(list,&QListWidget::itemDoubleClicked,this,[this](QListWidgetItem *row){vlc_playlist_locker lock(THEPL);auto item=playlist_ItemGetById(THEPL,row->data(Qt::UserRole).toInt());if(item)playlist_Control(THEPL,PLAYLIST_VIEWPLAY,pl_Locked,THEPL->p_playing,item);});
            auto buttons=new QHBoxLayout;auto add=new QPushButton("Add Files");auto remove=new QPushButton("Remove");auto save=new QPushButton("Save Playlist");buttons->addWidget(add);buttons->addWidget(remove);buttons->addWidget(save);body->addLayout(buttons);
            connect(add,&QPushButton::clicked,this,[this]{auto paths=QFileDialog::getOpenFileNames(this,"Add files to Queue");for(const auto &path:paths)Open::openMRL(p_intf,QUrl::fromLocalFile(path).toString(),false);updateQueue();});
            connect(remove,&QPushButton::clicked,this,[this,list]{vlc_playlist_locker lock(THEPL);for(auto row:list->selectedItems()){auto item=playlist_ItemGetById(THEPL,row->data(Qt::UserRole).toInt());if(item)playlist_NodeDelete(THEPL,item);}});
            connect(save,&QPushButton::clicked,this,[this]{newPlaylist();});
        }else if(i==1){connect(list,&QListWidget::itemDoubleClicked,this,[this](QListWidgetItem *row){auto input=THEMIM->getInput();if(input)var_SetInteger(input,"chapter",row->data(Qt::UserRole).toInt());});}
        else {auto add=new QPushButton("Add Bookmark");body->addWidget(add);connect(add,&QPushButton::clicked,this,[this]{auto input=THEMIM->getInput();if(!input)return;seekpoint_t bookmark;if(!input_Control(input,INPUT_GET_BOOKMARK,&bookmark)){QByteArray name=QString("Bookmark %1").arg(timeText(bookmark.i_time_offset/CLOCK_FREQ)).toUtf8();bookmark.psz_name=name.data();input_Control(input,INPUT_ADD_BOOKMARK,&bookmark);}updateQueue();});
            connect(list,&QListWidget::itemDoubleClicked,this,[this](QListWidgetItem *row){auto input=THEMIM->getInput();if(input)var_SetInteger(input,"time",row->data(Qt::UserRole).toLongLong());});
            auto remove=new QPushButton("Remove Bookmark");body->addWidget(remove);connect(remove,&QPushButton::clicked,this,[this,list]{auto input=THEMIM->getInput();if(input&&list->currentRow()>=0){input_Control(input,INPUT_DEL_BOOKMARK,list->currentRow());updateQueue();}});
        }stack->addWidget(page);
    }
    auto timer=new QTimer(sidePanel);timer->setInterval(750);connect(timer,&QTimer::timeout,this,[this]{updateQueue();});timer->start();stack->setCurrentIndex(selected);bodyLayout->addWidget(sidePanel);updateQueue();sidePanel->show();Reframe::animateHeight(sidePanel,true,208);
}
void RedesignShell::updateQueue(){
    if(!sidePanel||sidePanel->objectName()!="queuePanel"||!sidePanel->isVisible())return;
    auto list=sidePanel->findChild<QListWidget *>("queueItems");
    QStringList names;QList<int> ids;QList<qint64> durations;
    {vlc_playlist_locker lock(THEPL);std::function<void(playlist_item_t*)> visit=[&](playlist_item_t *node){if(node->i_children<0){char *name=input_item_GetName(node->p_input);names<<qfu(name);free(name);ids<<node->i_id;durations<<qMax<qint64>(0,input_item_GetDuration(node->p_input)/CLOCK_FREQ);}else for(int i=0;i<node->i_children;++i)visit(node->pp_children[i]);};visit(THEPL->p_playing);}
    QString signature;for(int i=0;i<ids.size();++i)signature+=QString::number(ids[i])+names[i]+QString::number(durations[i]);
    if(list->property("signature").toString()!=signature){QSignalBlocker block(list->model());list->clear();for(int i=0;i<names.size();++i){auto row=new QListWidgetItem(names[i]+"   "+timeText(durations[i]),list);row->setData(Qt::UserRole,ids[i]);row->setSizeHint(QSize(300,48));}list->setProperty("signature",signature);}
    auto chapters=sidePanel->findChild<QListWidget *>("chapterItems");chapters->clear();auto bookmarks=sidePanel->findChild<QListWidget *>("bookmarkItems");bookmarks->clear();auto input=THEMIM->getInput();if(!input)return;
    vlc_value_t values,texts;if(!var_Change(input,"chapter",VLC_VAR_GETCHOICES,&values,&texts)){for(int i=0;i<values.p_list->i_count;++i){auto row=new QListWidgetItem(qfu(texts.p_list->p_values[i].psz_string),chapters);row->setData(Qt::UserRole,int(values.p_list->p_values[i].i_int));}var_FreeList(&values,&texts);}
    seekpoint_t **points;int count;if(!input_Control(input,INPUT_GET_BOOKMARKS,&points,&count)){for(int i=0;i<count;++i){auto row=new QListWidgetItem(qfu(points[i]->psz_name)+"   "+timeText(points[i]->i_time_offset/CLOCK_FREQ),bookmarks);row->setData(Qt::UserRole,qlonglong(points[i]->i_time_offset));vlc_seekpoint_Delete(points[i]);}free(points);}
}
void RedesignShell::openNetwork(){
    auto dialog=new Reframe::Dialog(window,"Open Network Stream",QSize(1048,631));dialog->body->addWidget(new QLabel("Network URL"));auto urls=new QPlainTextEdit;urls->setPlaceholderText("e.g., https://www.example.com/stream.avi");dialog->body->addWidget(urls,1);dialog->body->addWidget(new QLabel("You can enter multiple links to load."));
    auto name=new QLineEdit;name->setPlaceholderText("e.g., Favorite TV Channels");dialog->body->addWidget(new QLabel("Playlist Name (optional)"));dialog->body->addWidget(name);dialog->body->addWidget(new QLabel("The playlist will be saved and displayed with this title."));
    auto row=new QHBoxLayout;row->addStretch();auto cancel=new QPushButton("Cancel"),save=new QPushButton("Save");row->addWidget(cancel);row->addWidget(save);dialog->body->addLayout(row);connect(cancel,&QPushButton::clicked,dialog,&QDialog::close);
    connect(save,&QPushButton::clicked,this,[this,dialog,urls,name]{QStringList paths;for(const auto &line:urls->toPlainText().split(QRegularExpression("[\\r\\n]+"),QString::SkipEmptyParts)){QUrl url(line.trimmed());if(!url.isValid()||url.scheme().isEmpty()){QMessageBox::warning(dialog,"Invalid network URL","Enter a complete URL with a protocol.");return;}paths<<url.toString();}if(paths.isEmpty())return;
        if(!name->text().trimmed().isEmpty()){QVariantMap data;data["name"]=name->text().trimmed();data["paths"]=paths;data["network"]=true;saveCollection(getSettings(),data);}
        bool first=true;for(const auto &path:paths){Open::openMRL(p_intf,path,first);first=false;}dialog->close();showPlayer();showQueue();});Reframe::reveal(dialog);
}
void RedesignShell::newPlaylist(const QString &id){
    auto dialog=new Reframe::Dialog(window,id.isEmpty()?"New Playlist":"Edit Playlist",QSize(713,589));auto data=getSettings()->value("Redesign/collection/"+id).toMap();auto name=new QLineEdit(data.value("name").toString());name->setPlaceholderText("e.g., Favorite Videos");dialog->body->addWidget(new QLabel("Playlist Details"));dialog->body->addWidget(name);
    auto thumbnail=new QPushButton("Choose Thumbnail");thumbnail->setProperty("path",data.value("thumbnail"));dialog->body->addWidget(thumbnail);connect(thumbnail,&QPushButton::clicked,dialog,[dialog,thumbnail]{auto path=QFileDialog::getOpenFileName(dialog,"Playlist thumbnail",QString(),"Images (*.png *.jpg *.jpeg *.webp)");if(!path.isEmpty()){thumbnail->setProperty("path",path);thumbnail->setText(QFileInfo(path).fileName());}});
    dialog->body->addWidget(new QLabel("Select Files. Drag files into the box, or choose files below."));auto list=new FileList;fileItems(list,id.isEmpty()?queuePaths(THEPL):data.value("paths").toStringList());dialog->body->addWidget(list,1);
    auto add=new QPushButton("Select Files");dialog->body->addWidget(add);connect(add,&QPushButton::clicked,dialog,[dialog,list]{fileItems(list,QFileDialog::getOpenFileNames(dialog,"Select playlist files"));});
    auto row=new QHBoxLayout;auto remove=new QPushButton("Remove Selected");row->addWidget(remove);connect(remove,&QPushButton::clicked,list,[list]{qDeleteAll(list->selectedItems());});row->addStretch();auto cancel=new QPushButton("Cancel"),save=new QPushButton("Save");row->addWidget(cancel);row->addWidget(save);dialog->body->addLayout(row);connect(cancel,&QPushButton::clicked,dialog,&QDialog::close);
    connect(save,&QPushButton::clicked,this,[this,dialog,list,name,thumbnail,id]{if(name->text().trimmed().isEmpty()){name->setFocus();return;}QStringList paths;for(int i=0;i<list->count();++i)paths<<list->item(i)->data(Qt::UserRole).toString();QVariantMap data;data["name"]=name->text().trimmed();data["paths"]=paths;data["thumbnail"]=thumbnail->property("path");data["network"]=false;saveCollection(getSettings(),data,id);dialog->close();showLibrary("Playlists");});Reframe::reveal(dialog);
}
void RedesignShell::batchTracks(const QStringList &supplied){
    QStringList paths=supplied.isEmpty()?selectedPaths(files):supplied;if(paths.isEmpty())paths=queuePaths(THEPL);
    QStringList expanded;for(const auto &path:paths){if(path.startsWith("collection:"))expanded+=getSettings()->value("Redesign/collection/"+path.mid(11)).toMap().value("paths").toStringList();else if(QFileInfo(path).isDir()){QDir folder(path);for(const auto &file:folder.entryInfoList(QStringList()<<"*.mp4"<<"*.mkv"<<"*.avi"<<"*.webm",QDir::Files))expanded<<file.absoluteFilePath();}else expanded<<path;}paths=expanded;
    auto dialog=new Reframe::Dialog(window,"Add Tracks to Playlist / Folder",QSize(1048,800));auto top=new QHBoxLayout;auto kind=new QComboBox;kind->addItems(QStringList()<<"Add Subtitles"<<"Add Audio Tracks");auto origin=new QComboBox;origin->addItems(QStringList()<<"From Computer"<<"From OpenSubtitles"<<"From Link");top->addWidget(kind);top->addStretch();top->addWidget(origin);dialog->body->addLayout(top);
    auto table=new QTableWidget(paths.size(),2);table->setHorizontalHeaderLabels(QStringList()<<"Media"<<"Assigned tracks");table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);dialog->body->addWidget(table,1);
    for(int i=0;i<paths.size();++i){table->setItem(i,0,new QTableWidgetItem(QFileInfo(QUrl(paths[i]).isLocalFile()?QUrl(paths[i]).toLocalFile():paths[i]).fileName()));auto choose=new QPushButton("Choose tracks");choose->setProperty("tracks",getSettings()->value("Redesign/bindings/"+paths[i]));table->setCellWidget(i,1,choose);connect(choose,&QPushButton::clicked,dialog,[choose,dialog,kind]{auto tracks=QFileDialog::getOpenFileNames(dialog,kind->currentIndex()?"Choose audio tracks":"Choose subtitle tracks",QString(),kind->currentIndex()?"Audio (*.aac *.ac3 *.flac *.m4a *.mp3 *.ogg *.wav)":"Subtitles (*.srt *.ass *.ssa *.vtt *.sub *.idx)");if(!tracks.isEmpty()){choose->setProperty("tracks",tracks);choose->setText(QString::number(tracks.size())+" track(s)");choose->setToolTip(tracks.join('\n'));}});}
    auto match=new QPushButton("Choose tracks and match filenames");dialog->body->addWidget(match);
    connect(match,&QPushButton::clicked,dialog,[dialog,paths,table,kind]{auto tracks=QFileDialog::getOpenFileNames(dialog,"Choose tracks",QString(),kind->currentIndex()?"Audio (*.aac *.ac3 *.flac *.m4a *.mp3 *.ogg *.wav)":"Subtitles (*.srt *.ass *.ssa *.vtt *.sub *.idx)");for(int row=0;row<paths.size();++row){QStringList matched;const auto base=QFileInfo(paths[row]).completeBaseName().toLower();const auto episode=QRegularExpression("s\\d+e\\d+",QRegularExpression::CaseInsensitiveOption).match(base).captured();for(const auto &track:tracks){const auto name=QFileInfo(track).completeBaseName().toLower();if(name.startsWith(base)||(!episode.isEmpty()&&name.contains(episode)))matched<<track;}if(!matched.isEmpty()){auto button=static_cast<QPushButton *>(table->cellWidget(row,1));button->setProperty("tracks",matched);button->setText(QString::number(matched.size())+" track(s)");button->setToolTip(matched.join('\n'));}}});
    connect(origin,QOverload<int>::of(&QComboBox::activated),dialog,[dialog,origin,table](int selected){if(selected==1){QDesktopServices::openUrl(QUrl("https://www.opensubtitles.com/"));origin->setCurrentIndex(0);}else if(selected==2){auto input=new Reframe::Dialog(dialog,"Track Links",QSize(519,239));auto links=new QPlainTextEdit;links->setPlaceholderText("One track URL per line");input->body->addWidget(links);auto add=new QPushButton("Assign to selected media");input->body->addWidget(add);connect(add,&QPushButton::clicked,input,[input,links,table]{int row=table->currentRow();if(row<0)return;QStringList tracks=links->toPlainText().split(QRegularExpression("[\\r\\n]+"),QString::SkipEmptyParts);auto b=static_cast<QPushButton *>(table->cellWidget(row,1));b->setProperty("tracks",tracks);b->setText(QString::number(tracks.size())+" track(s)");input->close();});Reframe::reveal(input);}});
    auto row=new QHBoxLayout;row->addStretch();auto cancel=new QPushButton("Cancel"),save=new QPushButton("Save");row->addWidget(cancel);row->addWidget(save);dialog->body->addLayout(row);connect(cancel,&QPushButton::clicked,dialog,&QDialog::close);
    connect(save,&QPushButton::clicked,this,[this,dialog,paths,table,kind]{for(int i=0;i<paths.size();++i){auto tracks=table->cellWidget(i,1)->property("tracks").toStringList();getSettings()->setValue("Redesign/"+(kind->currentIndex()?QString("audioBindings/"):QString("bindings/"))+paths[i],tracks);}dialog->close();});Reframe::reveal(dialog);
}
void RedesignShell::onboarding(){
    auto dialog=new Reframe::Dialog(window,"Welcome to Reframe!",QSize(1090,759));auto stack=new QStackedWidget;dialog->body->addWidget(stack,1);
    auto folderPage=new QWidget;auto folderLayout=new QVBoxLayout(folderPage);folderLayout->addStretch();auto folderHeading=new QLabel("Choose a Media Folder to get started with");folderHeading->setObjectName("sectionHeading");folderLayout->addWidget(folderHeading);auto path=new QLineEdit(getSettings()->value("Redesign/VideosDirectory",QString(QDir::homePath()+"/Videos")).toString());folderLayout->addWidget(path);auto browseButton=new QPushButton("Browse");folderLayout->addWidget(browseButton);folderLayout->addStretch();connect(browseButton,&QPushButton::clicked,dialog,[dialog,path]{auto folder=QFileDialog::getExistingDirectory(dialog,"Choose media folder",path->text());if(!folder.isEmpty())path->setText(folder);});stack->addWidget(folderPage);
    auto appearance=new QWidget;auto themeLayout=new QVBoxLayout(appearance);themeLayout->addWidget(new QLabel("Select your preferred theme and color."));
    auto preview=new QLabel;preview->setAlignment(Qt::AlignCenter);preview->setPixmap(window->grab().scaled(900,500,Qt::KeepAspectRatio,Qt::SmoothTransformation));themeLayout->addWidget(preview,1);
    themeLayout->addWidget(Reframe::segmented(QStringList()<<"Light"<<"Dark"<<"System",getSettings()->value("Redesign/theme","Dark").toString(),appearance,[this,preview](QString theme){getSettings()->setValue("Redesign/theme",theme);applyTheme();QTimer::singleShot(50,preview,[this,preview]{preview->setPixmap(window->grab().scaled(900,500,Qt::KeepAspectRatio,Qt::SmoothTransformation));});}));
    auto colors=new QHBoxLayout;for(const auto &color:QStringList({"#d37918","#ad4129","#93751b","#1a882b","#008ba8","#1a56b3","#8035c8","#ba3a88","#8f8f8f"})){auto b=new QPushButton;b->setFixedSize(34,34);b->setStyleSheet("background:"+color+";");colors->addWidget(b);connect(b,&QPushButton::clicked,dialog,[this,color,preview]{getSettings()->setValue("Redesign/accent",color);applyTheme();preview->setPixmap(window->grab().scaled(900,500,Qt::KeepAspectRatio,Qt::SmoothTransformation));});}themeLayout->addLayout(colors);stack->addWidget(appearance);
    auto keys=new QLabel("Learn the basic keyboard shortcuts.\n\nSpace   Play / Pause\nF   Toggle Fullscreen\nV   Cycle Subtitle Track\nB   Cycle Audio Track\nN / P   Next / Previous Item\nArrow keys   Seek and Volume\nShift + S   Snapshot\nShift + R   Record\n\nFind the full list in the menu.");keys->setAlignment(Qt::AlignCenter);keys->setObjectName("onboardingKeys");stack->addWidget(keys);
    auto footer=new QHBoxLayout;auto back=new QPushButton("Back"),next=new QPushButton("Next");footer->addWidget(back);footer->addStretch();footer->addWidget(next);dialog->body->addLayout(footer);back->setEnabled(false);
    connect(back,&QPushButton::clicked,dialog,[stack,back,next]{stack->setCurrentIndex(stack->currentIndex()-1);back->setEnabled(stack->currentIndex()>0);next->setText("Next");});
    connect(next,&QPushButton::clicked,this,[this,dialog,stack,path,back,next]{if(stack->currentIndex()==2){getSettings()->setValue("Redesign/onboarded",true);getSettings()->setValue("Redesign/VideosDirectory",path->text());dialog->close();showLibrary("Videos");}else{stack->setCurrentIndex(stack->currentIndex()+1);back->setEnabled(true);next->setText(stack->currentIndex()==2?"Finish":"Next");}});Reframe::reveal(dialog);
}
void RedesignShell::toggleMiniPlayer(){
    if(miniPlayer){showPlayer();window->showNormal();window->resize(previousSize);return;}
    previousSize=window->size();if(sidePanel)sidePanel->hide();pages->setCurrentWidget(browser);
    miniPlayer=new Reframe::Dialog(window,"Now Playing",QSize(431,300));miniPlayer->setWindowFlags(Qt::Tool|Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint);
    static_cast<Reframe::Dialog *>(miniPlayer)->beforeClose=[this]{videoLayout->addWidget(player);player->show();};
    auto layout=static_cast<Reframe::Dialog *>(miniPlayer)->body;videoLayout->removeWidget(player);layout->addWidget(player,1);auto actions=new QHBoxLayout;auto pause=button("play"),restore=button("maximize"),queue=button("playlist");actions->addWidget(pause);actions->addStretch();actions->addWidget(queue);actions->addWidget(restore);layout->addLayout(actions);connect(pause,&QToolButton::clicked,THEAM,&ActionsManager::play);connect(restore,&QToolButton::clicked,this,[this]{toggleMiniPlayer();});connect(queue,&QToolButton::clicked,this,[this]{showQueue();});
    connect(miniPlayer,&QObject::destroyed,this,[this]{miniPlayer=nullptr;});Reframe::reveal(miniPlayer);
}
void RedesignShell::layoutControls(){
    const QStringList order=getSettings()->value("Redesign/toolbar",QStringList()<<"previous"<<"play"<<"next"<<"stop"<<"elapsed"<<"seek"<<"duration"<<"volume"<<"volumeSlider"<<"audio"<<"subtitles"<<"settings"<<"playlist").toStringList();
    appliedToolbar=order;
    for(auto b:controlButtons)b->hide();
    const QHash<QString,QWidget*> widgets={{"elapsed",elapsed},{"seek",seek},{"duration",duration},{"volumeSlider",volume}};
    for(auto widget:widgets)widget->hide();
    while(auto item=transportLayout->takeAt(0))delete item;
    const QHash<QString,int> actions={{"record",RECORD_ACTION},{"snapshot",SNAPSHOT_ACTION},{"repeat",LOOP_ACTION},{"shuffle",RANDOM_ACTION},{"frame",FRAME_ACTION},{"reverse",REVERSE_ACTION}};
    for(const auto &name:order){
        if(name=="spacer"){transportLayout->addStretch();continue;}if(name=="divider"){auto line=new QLabel("|");transportLayout->addWidget(line);continue;}
        QWidget *widget=widgets.value(name,nullptr);if(!widget){auto b=controlButtons.value(name,nullptr);if(!b){b=button(name);b->setToolTip(name);controlButtons[name]=b;if(actions.contains(name))connect(b,&QToolButton::clicked,this,[this,name,actions]{THEAM->doAction(actions.value(name));});else connect(b,&QToolButton::clicked,this,[this,name]{
            if(name.startsWith("back")||name.startsWith("forward")){auto input=THEMIM->getInput();if(input){const qint64 amount=name.endsWith("15")?15:10;var_SetInteger(input,"time",qMax<qint64>(0,var_GetInteger(input,"time")+(name.startsWith("back")?-amount:amount)*CLOCK_FREQ));}}
            else if(name=="replay"){auto input=THEMIM->getInput();if(input){var_SetInteger(input,"time",0);var_SetInteger(input,"state",PLAYING_S);}}
            else if(name=="bookmark"){auto input=THEMIM->getInput();if(input){seekpoint_t point;if(!input_Control(input,INPUT_GET_BOOKMARK,&point)){QByteArray text=QString("Bookmark %1").arg(timeText(point.i_time_offset/CLOCK_FREQ)).toUtf8();point.psz_name=text.data();input_Control(input,INPUT_ADD_BOOKMARK,&point);}}}
            else if(name=="mini")toggleMiniPlayer();else if(name=="library")toggleLibraryDock();else if(name=="home")showLibrary("Home");else if(name=="playlists")showLibrary("Playlists");else if(name=="quit")THEDP->quit();else if(name=="chapters")showQueue(1);else if(name=="bookmarks")showQueue(2);else if(name=="preferences")openSettings();else if(name=="properties")THEDP->mediaInfoDialog();else if(name=="favorite"){auto item=THEMIM->currentInputItem();if(item){char *uri=input_item_GetURI(item);auto favorites=getSettings()->value("Redesign/favorites").toStringList();const QString path=qfu(uri);free(uri);if(favorites.contains(path))favorites.removeAll(path);else favorites<<path;getSettings()->setValue("Redesign/favorites",favorites);}}else openMediaSettings(name=="hdr"||name=="zoom"?5:4);
        });}widget=b;}
        widget->show();transportLayout->addWidget(widget,name=="seek"?1:0);
    }
    transport->updateGeometry();
}
void RedesignShell::customize(){
    auto dialog=new Reframe::Dialog(window,"Customize Player",QSize(1100,650));dialog->body->addWidget(new QLabel("Drag items around and out of the boxes to reorder or remove them."));
    const QStringList available=QStringList()<<"previous"<<"play"<<"next"<<"stop"<<"elapsed"<<"seek"<<"duration"<<"volume"<<"volumeSlider"<<"audio"<<"subtitles"<<"settings"<<"playlist"<<"record"<<"snapshot"<<"repeat"<<"shuffle"<<"back10"<<"forward10"<<"back15"<<"forward15"<<"frame"<<"reverse"<<"replay"<<"mini"<<"library"<<"hdr"<<"zoom"<<"home"<<"preferences"<<"quit"<<"chapters"<<"bookmark"<<"bookmarks"<<"playlists"<<"favorite"<<"properties"<<"speed"<<"spacer"<<"divider";
    auto selected=new QListWidget;auto inventory=new QListWidget;selected->setObjectName("toolbarItems");inventory->setObjectName("availableControls");
    for(auto list:{selected,inventory}){list->setViewMode(QListView::IconMode);list->setMovement(QListView::Snap);list->setDragDropMode(QAbstractItemView::DragDrop);list->setDefaultDropAction(Qt::MoveAction);list->setAcceptDrops(true);list->setDragEnabled(true);list->setGridSize(QSize(120,60));}
    const auto order=getSettings()->value("Redesign/toolbar",QStringList(available.mid(0,13))).toStringList();selected->addItems(order);for(const auto &name:available)if(!order.contains(name))inventory->addItem(name);
    dialog->body->addWidget(new QLabel("Player toolbar"));dialog->body->addWidget(selected,1);dialog->body->addWidget(new QLabel("Available controls"));dialog->body->addWidget(inventory,1);
    auto row=new QHBoxLayout;auto reset=new QPushButton("Reset"),cancel=new QPushButton("Cancel"),save=new QPushButton("Save");row->addWidget(reset);row->addStretch();row->addWidget(cancel);row->addWidget(save);dialog->body->addLayout(row);connect(reset,&QPushButton::clicked,dialog,[selected,inventory,available]{selected->clear();inventory->clear();selected->addItems(available.mid(0,13));inventory->addItems(available.mid(13));});connect(cancel,&QPushButton::clicked,dialog,&QDialog::close);
    connect(save,&QPushButton::clicked,this,[this,dialog,selected]{QStringList names;for(int i=0;i<selected->count();++i)names<<selected->item(i)->text();if(!names.contains("play"))names.prepend("play");getSettings()->setValue("Redesign/toolbar",names);layoutControls();dialog->close();});Reframe::reveal(dialog);
}
void RedesignShell::mainMenu(const QPoint &position){
    QMenu menu;menu.addAction("Open File(s)\tCtrl+O",THEDP,&DialogsProvider::openFileDialog);menu.addAction("Open Folder\tCtrl+F",this,[this]{auto path=QFileDialog::getExistingDirectory(this,"Open Folder");if(!path.isEmpty())browse(path);});menu.addAction("Open Network Stream\tCtrl+N",this,[this]{openNetwork();});menu.addAction("Open Disc\tCtrl+D",THEDP,&DialogsProvider::openDiscDialog);menu.addAction("Open Capture Device\tCtrl+C",THEDP,&DialogsProvider::openCaptureDialog);menu.addAction("Convert\tCtrl+R",THEDP,&DialogsProvider::openAndTranscodingDialogs);
    auto recent=menu.addMenu("Open Recent");for(const auto &path:RecentsMRL::getInstance(p_intf)->recentList())recent->addAction(QFileInfo(QUrl(path).toLocalFile()).fileName(),this,[this,path]{openMedia(path);});
    menu.addSeparator();menu.addAction("Profiles",this,[this]{openMediaSettings(4);});menu.addAction("Keyboard Shortcuts",this,[this]{openSettings(3);});menu.addAction("Customize Player",this,[this]{customize();});menu.addAction("Settings\tCtrl+P",this,[this]{openSettings();});
    menu.addAction("About Reframe",this,[this]{auto dialog=new Reframe::Dialog(window,"About Reframe",QSize(650,450));auto label=new QLabel("Reframe, an unofficial VLC 3.0.24 fork.\n\nVideoLAN playback engine, GPL v2 or later.\nInterface adapted from VLC Redesign by Juxtopposed, CC BY 4.0.\nImplementation by webmaster94, 2026.\n\nNot endorsed by VideoLAN or Juxtopposed.");label->setWordWrap(true);dialog->body->addWidget(label);auto source=new QLabel("<a href='https://github.com/webmaster94/vlc-redesign'>Source, licenses and releases</a>");source->setOpenExternalLinks(true);dialog->body->addWidget(source);Reframe::reveal(dialog);});menu.exec(position);
}
void RedesignShell::playerMenu(const QPoint &position){
    QMenu menu;menu.addAction("Play / Pause\tSpace",THEAM,&ActionsManager::play);menu.addAction("Stop\tS",THEMIM,&MainInputManager::stop);menu.addAction("Previous Item\tP",THEMIM,&MainInputManager::prev);menu.addAction("Next Item\tN",THEMIM,&MainInputManager::next);menu.addSeparator();
    for(const auto &pair:QList<QPair<QString,int>>({{"Playback",4},{"Video",5},{"Audio",6},{"Subtitle",7}}))menu.addAction(pair.first,this,[this,pair]{openMediaSettings(pair.second);});menu.addAction("Queue",this,[this]{showQueue();});menu.addAction("Record",THEAM,&ActionsManager::record);menu.addSeparator();menu.addAction("Hide UI",this,[this]{setMinimal(!minimal);});menu.addAction("More Settings",this,[this]{openSettings();});menu.addAction("Open Media",THEDP,&DialogsProvider::openFileDialog);menu.addAction("Quit Reframe\tCtrl+Q",THEDP,&DialogsProvider::quit);menu.exec(position);
}
bool RedesignShell::eventFilter(QObject *target,QEvent *event){
    if(target==duration&&event->type()==QEvent::MouseButtonRelease){remainingTime=!remainingTime;return true;}
    if(minimal&&event->type()==QEvent::MouseMove){const auto point=window->mapFromGlobal(QCursor::pos());if(point.y()<80||point.y()>window->height()-100){header->setVisible(point.y()<80);transport->setVisible(point.y()>window->height()-100);chromeTimer->start(qMax(1000,int(config_GetInt(p_intf,"mouse-hide-timeout"))));}}
    return QWidget::eventFilter(target,event);
}
void RedesignShell::checkUpdates(){
    auto process=new QProcess(this);
    connect(process,QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),this,[this,process](int status,QProcess::ExitStatus){if(status==0){auto data=QJsonDocument::fromJson(process->readAllStandardOutput()).object();const auto tag=data.value("tag_name").toString();if(!tag.isEmpty()&&tag!=getSettings()->value("Redesign/lastRelease","v0.1.0").toString()){getSettings()->setValue("Redesign/lastRelease",tag);if(QMessageBox::question(window,"Reframe update",tag+" is available. Open the release page?")==QMessageBox::Yes)QDesktopServices::openUrl(QUrl(data.value("html_url").toString()));}}process->deleteLater();});
    process->start("curl.exe",QStringList()<<"--silent"<<"--fail"<<"--max-time"<<"15"<<"--user-agent"<<"Reframe"<<"https://api.github.com/repos/webmaster94/vlc-redesign/releases/latest");
}
