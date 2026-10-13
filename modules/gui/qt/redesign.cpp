/* Reframe interface, adapted from Juxtopposed's VLC Redesign, CC BY 4.0.
 * Implementation copyright 2026 webmaster94. GPL-2.0-or-later.
 * Changes: native player/library/settings implementation, 2026-10-12.
 * Design: https://www.figma.com/community/file/1691465999134370980/vlc-redesign */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include "redesign.hpp"
#include "main_interface.hpp"
#include "input_manager.hpp"
#include "actions_manager.hpp"
#include "dialogs_provider.hpp"
#include "recents.hpp"
#include "components/extended_panels.hpp"
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
#include <cmath>
#include <algorithm>

namespace {
QIcon lineIcon(const QString &name, const QColor &color = QColor("#b8b8b8"))
{
    QPixmap pm(48, 48); pm.fill(Qt::transparent);
    QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
    p.scale(2, 2); p.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    auto line = [&p](int x, int y, int a, int b) { p.drawLine(x,y,a,b); };
    if (name == "home") {
        QPolygonF roof; roof << QPointF(3,10) << QPointF(12,3) << QPointF(21,10);
        p.drawPolyline(roof); p.drawRoundedRect(QRectF(5,9,14,12),2,2); p.drawRect(10,14,4,7);
    } else if (name == "videos" || name == "folder") {
        QPainterPath path; path.moveTo(3,7); path.lineTo(10,7); path.lineTo(12,9);
        path.lineTo(21,9); path.lineTo(21,20); path.lineTo(3,20); path.closeSubpath(); p.drawPath(path);
        if(name == "videos") { p.setBrush(color); QPolygonF tri; tri<<QPointF(10,11)<<QPointF(15,14)<<QPointF(10,17); p.drawPolygon(tri); }
    } else if(name == "play" || name == "brand") {
        if(name == "brand") { p.setPen(Qt::NoPen); p.setBrush(color); p.drawRoundedRect(QRectF(2,2,20,20),5,5); p.setBrush(QColor("#111111")); }
        else p.setBrush(color);
        QPolygonF tri; tri<<QPointF(9,6)<<QPointF(18,12)<<QPointF(9,18); p.drawPolygon(tri);
    } else if(name == "pause") { p.setBrush(color); p.drawRoundedRect(QRectF(7,6,3,12),1,1); p.drawRoundedRect(QRectF(14,6,3,12),1,1);
    } else if(name == "music") { line(10,5,10,17); line(10,5,19,3); line(19,3,19,15); p.drawEllipse(5,15,5,5); p.drawEllipse(14,13,5,5);
    } else if(name == "network") { p.drawEllipse(10,10,4,4); p.drawArc(5,5,14,14,-55*16,110*16); p.drawArc(5,5,14,14,125*16,110*16); p.drawArc(1,1,22,22,-50*16,100*16); p.drawArc(1,1,22,22,130*16,100*16);
    } else if(name == "playlist") { line(3,6,20,6); line(3,11,13,11); line(3,16,11,16); QPolygonF tri;tri<<QPointF(16,11)<<QPointF(21,15)<<QPointF(16,19);p.drawPolygon(tri);
    } else if(name == "recent") { p.drawArc(4,4,16,16,-80*16,290*16); line(12,7,12,12); line(12,12,16,14); line(3,4,3,9); line(3,9,8,9);
    } else if(name == "add") {line(12,4,12,20);line(4,12,20,12);
    } else if(name == "back" || name == "forward") { int d=name=="back"?1:-1;line(12-6*d,12,12+6*d,12);line(12-6*d,12,12,6);line(12-6*d,12,12,18);
    } else if(name == "previous" || name == "next") { bool prev=name=="previous"; int x=prev?16:8; QPolygonF tri;tri<<QPointF(x,6)<<QPointF(prev?7:17,12)<<QPointF(x,18);p.setBrush(color);p.drawPolygon(tri);line(prev?5:19,6,prev?5:19,18);
    } else if(name == "stop") {p.drawRoundedRect(QRectF(6,6,12,12),1,1);
    } else if(name == "audio") {for(int i=0;i<5;i++){int h=i==2?18:(i%2?12:6);line(4+i*4,12-h/2,4+i*4,12+h/2);}
    } else if(name == "volume" || name == "mute") {QPolygonF sp;sp<<QPointF(3,9)<<QPointF(7,9)<<QPointF(12,5)<<QPointF(12,19)<<QPointF(7,15)<<QPointF(3,15);p.drawPolygon(sp);if(name=="volume"){p.drawArc(10,5,10,14,-60*16,120*16);}else{line(17,8,22,16);line(22,8,17,16);}
    } else if(name == "settings") {p.drawEllipse(8,8,8,8);for(int i=0;i<8;i++){p.save();p.translate(12,12);p.rotate(i*45);p.drawRoundedRect(QRectF(-2,-10,4,4),1,1);p.restore();}
    } else if(name == "subtitles") {p.drawRoundedRect(QRectF(2,5,20,14),3,3);line(5,11,10,11);line(13,11,19,11);line(5,15,13,15);line(16,15,19,15);
    } else if(name == "search") {p.drawEllipse(4,3,12,12);line(14,14,21,21);
    } else if(name == "grid") {for(int x=4;x<20;x+=10)for(int y=4;y<20;y+=10)p.drawRoundedRect(QRectF(x,y,6,6),1,1);
    } else if(name == "list") {for(int y=6;y<21;y+=6){line(3,y,5,y);line(9,y,21,y);}
    } else if(name == "close") {line(6,6,18,18);line(6,18,18,6);
    } else if(name == "minimize") {line(5,12,19,12);
    } else if(name == "maximize") {p.drawRect(5,5,14,14);
    } else if(name == "fullscreen") {line(4,9,4,4);line(4,4,9,4);line(15,4,20,4);line(20,4,20,9);line(4,15,4,20);line(4,20,9,20);line(15,20,20,20);line(20,20,20,15);
    } else if(name == "more") {p.setBrush(color);for(int x=5;x<22;x+=7)p.drawEllipse(QPointF(x,12),1,1);}
    return QIcon(pm);
}

class TitleBar : public QWidget
{
    QPoint offset;
public:
    explicit TitleBar(QWidget *p):QWidget(p) {}
    void mousePressEvent(QMouseEvent *e) override {if(e->button()==Qt::LeftButton)offset=e->globalPos()-window()->frameGeometry().topLeft();}
    void mouseMoveEvent(QMouseEvent *e) override {if((e->buttons()&Qt::LeftButton)&&!window()->isMaximized()&&!window()->isFullScreen())window()->move(e->globalPos()-offset);}
    void mouseDoubleClickEvent(QMouseEvent *e) override {if(e->button()==Qt::LeftButton){if(window()->isMaximized())window()->showNormal();else window()->showMaximized();}}
};

class ClickSlider : public QSlider
{
public:
    explicit ClickSlider(QWidget *p):QSlider(Qt::Horizontal,p) {}
    void mousePressEvent(QMouseEvent *e) override {
        if(e->button()==Qt::LeftButton&&isEnabled()){setValue(minimum()+qBound(0,e->pos().x(),width())*(maximum()-minimum())/qMax(1,width()));}
        QSlider::mousePressEvent(e);
    }
};

class MediaDelegate : public QStyledItemDelegate
{
public:
    explicit MediaDelegate(QObject *p):QStyledItemDelegate(p) {}
    QSize sizeHint(const QStyleOptionViewItem &,const QModelIndex &) const override {
        const auto view=qobject_cast<QListWidget *>(parent());
        return QSize(320,view&&view->viewMode()==QListView::ListMode?56:232);
    }
    void paint(QPainter *p,const QStyleOptionViewItem &o,const QModelIndex &i) const override {
        p->save();p->setRenderHint(QPainter::Antialiasing);
        if(o.rect.height()<100){
            QIcon icon=qvariant_cast<QIcon>(i.data(Qt::DecorationRole));icon.paint(p,QRect(o.rect.left()+8,o.rect.top()+4,70,40));
            p->setPen(o.palette.text().color());p->drawText(o.rect.adjusted(92,0,-8,0),Qt::AlignVCenter|Qt::AlignLeft,i.data().toString());p->restore();return;
        }
        QRect r=o.rect.adjusted(8,6,-8,-6), image(r.left(),r.top(),r.width(),r.width()*9/16);
        p->setPen(Qt::NoPen);p->setBrush(o.state&QStyle::State_Selected?o.palette.highlight():o.palette.alternateBase());p->drawRoundedRect(image,8,8);
        QIcon icon=qvariant_cast<QIcon>(i.data(Qt::DecorationRole));
        if(i.data(Qt::UserRole+1).toBool()) {
            QPixmap pm=icon.pixmap(320,180);QPainterPath clip;clip.addRoundedRect(image,8,8);p->setClipPath(clip);
            p->drawPixmap(image,pm);p->setClipping(false);
        } else {icon.paint(p,image.adjusted(65,30,-65,-30));}
        p->setPen(o.palette.text().color());p->setFont(o.font);
        p->drawText(QRect(r.left(),image.bottom()+12,r.width(),30),Qt::AlignTop|Qt::AlignLeft,o.fontMetrics.elidedText(i.data().toString(),Qt::ElideMiddle,r.width()));
        p->restore();
    }
};

QString clockText(qint64 seconds) {
    seconds=qMax<qint64>(0,seconds);
    return QString("%1:%2:%3").arg(seconds/3600,2,10,QChar('0')).arg(seconds/60%60,2,10,QChar('0')).arg(seconds%60,2,10,QChar('0'));
}
}

QToolButton *RedesignShell::button(const QString &name,const QString &text)
{
    QToolButton *b=new QToolButton;
    b->setProperty("glyph",name);b->setIcon(lineIcon(name));b->setIconSize(QSize(22,22));
    b->setText(text);b->setToolTip(text.isEmpty()?name:text);b->setAccessibleName(b->toolTip());
    b->setCursor(Qt::PointingHandCursor);b->setAutoRaise(true);b->setMinimumSize(34,34);
    if(!text.isEmpty())b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    return b;
}

RedesignShell::RedesignShell(intf_thread_t *intf, MainInterface *w, QWidget *panel)
    :QWidget(w),p_intf(intf),window(w),player(panel),settingsDialog(nullptr),historyPosition(-1),
      systemPalette(QApplication::palette()),thumbnailProcess(nullptr),compact(false),minimal(false)
{
    navigationAnimation=nullptr;
    const QDir fonts(QCoreApplication::applicationDirPath()+"/fonts");
    for(const QString &f:fonts.entryList(QStringList()<<"*.ttf",QDir::Files))QFontDatabase::addApplicationFont(fonts.filePath(f));
    setObjectName("redesignShell");
    char *cache=config_GetUserDir(VLC_CACHE_DIR);cacheDirectory=QString::fromUtf8(cache?cache:"");free(cache);
    cacheDirectory+="/thumbnails";QDir().mkpath(cacheDirectory);
    QVBoxLayout *root=new QVBoxLayout(this);root->setContentsMargins(0,0,0,0);root->setSpacing(0);
    header=new TitleBar(this);header->setObjectName("titleBar");header->setFixedHeight(40);
    QHBoxLayout *title=new QHBoxLayout(header);title->setContentsMargins(8,0,6,0);title->setSpacing(4);
    auto logo=button("brand");logo->setToolTip("Reframe, an unofficial VLC fork");title->addWidget(logo);
    connect(logo,&QToolButton::clicked,this,[this]{showLibrary("Home");});
    auto menuButton=button("more");menuButton->setToolTip("Main menu");title->addWidget(menuButton);
    connect(menuButton,&QToolButton::clicked,this,[this,menuButton]{QMenu m;for(QAction *a:window->menuBar()->actions())if(a->menu())m.addMenu(a->menu());m.exec(menuButton->mapToGlobal(QPoint(0,menuButton->height())));});
    auto navToggle=button("list");navToggle->setToolTip("Toggle navigation sidebar");title->addWidget(navToggle);
    connect(navToggle,&QToolButton::clicked,this,[this]{setNavigationWidth(!compact);});
    title->addStretch();auto titleLabel=new QLabel("Reframe");titleLabel->setObjectName("windowTitle");title->addWidget(titleLabel);title->addStretch();
    // operator<< returns a reference. Own the list before range iteration.
    const QStringList windowActions=QStringList()<<"minimize"<<"maximize"<<"fullscreen"<<"close";
    for(const QString &name:windowActions){
        auto b=button(name);title->addWidget(b);
        connect(b,&QToolButton::clicked,this,[this,name]{if(name=="minimize")window->showMinimized();else if(name=="maximize"){if(window->isMaximized())window->showNormal();else window->showMaximized();}else if(name=="fullscreen")window->toggleInterfaceFullScreen();else window->close();});
    }
    root->addWidget(header);
    auto body=new QWidget(this);auto bodyLayout=new QHBoxLayout(body);bodyLayout->setContentsMargins(8,8,8,8);bodyLayout->setSpacing(16);
    sidebar=new QWidget(body);sidebar->setObjectName("navigation");sidebar->setFixedWidth(196);
    auto navigation=new QVBoxLayout(sidebar);navigation->setContentsMargins(8,10,8,10);navigation->setSpacing(8);
    QButtonGroup *navGroup=new QButtonGroup(this);navGroup->setExclusive(true);
    const QStringList names=QStringList()<<"Home"<<"Videos"<<"Music"<<"Network"<<"Playlists"<<"Recent";
    const QStringList icons=QStringList()<<"home"<<"videos"<<"music"<<"network"<<"playlist"<<"recent";
    for(int i=0;i<names.size();++i){auto b=button(icons[i],names[i]);b->setProperty("section",names[i]);b->setCheckable(true);b->setFixedHeight(40);navGroup->addButton(b);navigation->addWidget(b);connect(b,&QToolButton::clicked,this,[this,names,i]{showLibrary(names[i]);});}
    auto add=button("add","Add item");navigation->addWidget(add);
    connect(add,&QToolButton::clicked,THEDP,&DialogsProvider::openFileDialog);
    navigation->addStretch();auto prefs=button("settings","Settings");navigation->addWidget(prefs);connect(prefs,&QToolButton::clicked,this,[this]{openSettings();});
    bodyLayout->addWidget(sidebar);
    pages=new QStackedWidget(body);pages->setObjectName("content");
    browser=new QWidget(pages);browser->setObjectName("browser");auto library=new QVBoxLayout(browser);library->setContentsMargins(8,0,8,0);library->setSpacing(12);
    auto tools=new QHBoxLayout;tools->setContentsMargins(0,0,0,8);tools->setSpacing(12);
    auto back=button("back"),forward=button("forward");tools->addWidget(back);tools->addWidget(forward);
    connect(back,&QToolButton::clicked,this,[this]{if(historyPosition>0)browse(history[--historyPosition],false);});
    connect(forward,&QToolButton::clicked,this,[this]{if(historyPosition+1<history.size())browse(history[++historyPosition],false);});
    location=new QLabel;location->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);tools->addWidget(location,1);count=new QLabel;tools->addWidget(count);
    auto folder=button("folder");folder->setToolTip("Browse a media folder");tools->addWidget(folder);
    connect(folder,&QToolButton::clicked,this,[this]{QString d=QFileDialog::getExistingDirectory(this,"Open media folder",currentDirectory);if(!d.isEmpty()){getSettings()->setValue("Redesign/"+section+"Directory",d);browse(d);}});
    auto grid=button("grid");grid->setToolTip("Switch grid or list view");tools->addWidget(grid);
    connect(grid,&QToolButton::clicked,this,[this,grid]{bool isGrid=files->viewMode()==QListView::IconMode;files->setViewMode(isGrid?QListView::ListMode:QListView::IconMode);files->setGridSize(isGrid?QSize(320,56):QSize(320,232));grid->setIcon(lineIcon(isGrid?"list":"grid"));});
    search=new QLineEdit;search->setPlaceholderText("Search");search->setAccessibleName("Search media");search->setMinimumWidth(220);search->setMaximumWidth(300);search->setClearButtonEnabled(true);search->addAction(lineIcon("search"),QLineEdit::LeadingPosition);tools->addWidget(search);
    connect(search,&QLineEdit::textChanged,this,[this](const QString &term){for(int i=0;i<files->count();++i)files->item(i)->setHidden(!files->item(i)->text().contains(term,Qt::CaseInsensitive));});
    library->addLayout(tools);
    files=new QListWidget(browser);files->setObjectName("mediaGrid");files->setViewMode(QListView::IconMode);files->setResizeMode(QListView::Adjust);files->setMovement(QListView::Static);files->setWrapping(true);files->setGridSize(QSize(320,232));files->setIconSize(QSize(304,171));files->setSelectionMode(QAbstractItemView::ExtendedSelection);files->setItemDelegate(new MediaDelegate(files));files->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(files,&QListWidget::itemDoubleClicked,this,[this](QListWidgetItem *item){QString path=item->data(Qt::UserRole).toString();if(QFileInfo(path).isDir())browse(path);else openMedia(path);});
    connect(files,&QListWidget::customContextMenuRequested,this,&RedesignShell::contextMenu);
    library->addWidget(files,1);empty=new QLabel;empty->setAlignment(Qt::AlignCenter);empty->setWordWrap(true);empty->setObjectName("emptyState");library->addWidget(empty,1);
    pages->addWidget(browser);pages->addWidget(player);bodyLayout->addWidget(pages,1);root->addWidget(body,1);

    transport=new QWidget(this);transport->setObjectName("transport");transport->setFixedHeight(48);
    auto bar=new QHBoxLayout(transport);bar->setContentsMargins(12,4,12,4);bar->setSpacing(8);
    auto previous=button("previous");bar->addWidget(previous);connect(previous,&QToolButton::clicked,THEMIM,&MainInputManager::prev);
    playButton=button("play");playButton->setObjectName("playButton");playButton->setToolTip("Play / Pause");playButton->setFixedSize(54,30);bar->addWidget(playButton);connect(playButton,&QToolButton::clicked,THEAM,&ActionsManager::play);
    auto next=button("next");bar->addWidget(next);connect(next,&QToolButton::clicked,THEMIM,&MainInputManager::next);
    auto stop=button("stop");bar->addWidget(stop);connect(stop,&QToolButton::clicked,THEMIM,&MainInputManager::stop);
    elapsed=new QLabel("00:00:00");elapsed->setObjectName("elapsedTime");elapsed->setMinimumWidth(62);bar->addWidget(elapsed);
    seek=new ClickSlider(transport);seek->setObjectName("seek");seek->setRange(0,100000);seek->setEnabled(false);seek->setAccessibleName("Playback position");bar->addWidget(seek,1);
    connect(seek,&QSlider::valueChanged,this,[this](int v){THEMIM->getIM()->sliderUpdate(v/100000.f);});
    duration=new QLabel("00:00:00");duration->setMinimumWidth(62);bar->addWidget(duration);
    connect(THEMIM->getIM(),&InputManager::positionUpdated,this,[this](float pos,int64_t time,int length){elapsed->setText(clockText(time/CLOCK_FREQ));duration->setText(clockText(length));if(!seek->isSliderDown()){QSignalBlocker blocked(seek);seek->setValue(qMax(0,qRound(pos*100000)));}});
    connect(THEMIM->getIM(),&InputManager::inputCanSeek,seek,&QSlider::setEnabled);
    connect(THEMIM->getIM(),&InputManager::playingStatusChanged,this,[this](int state){playButton->setProperty("glyph",state==PLAYING_S?"pause":"play");playButton->setIcon(lineIcon(state==PLAYING_S?"pause":"play",QColor("#111111")));if(state==END_S)seek->setEnabled(false);});
    auto mute=button("volume");mute->setToolTip("Mute / Unmute");bar->addWidget(mute);connect(mute,&QToolButton::clicked,THEAM,&ActionsManager::toggleMuteAudio);
    connect(THEMIM,&MainInputManager::soundMuteChanged,this,[mute](bool m){mute->setIcon(lineIcon(m?"mute":"volume"));});
    volume=new ClickSlider(transport);volume->setRange(0,125);volume->setFixedWidth(84);volume->setAccessibleName("Volume");volume->setValue(qMax(0,qRound(playlist_VolumeGet(THEPL)*100)));bar->addWidget(volume);connect(volume,&QSlider::valueChanged,this,[this](int v){playlist_VolumeSet(THEPL,v/100.f);volume->setToolTip(QString::number(v)+"%");});
    connect(THEMIM,&MainInputManager::volumeChanged,this,[this](float v){QSignalBlocker blocked(volume);volume->setValue(qRound(v*100));});
    auto eq=button("audio"),sub=button("subtitles"),settings=button("settings"),playlist=button("playlist");bar->addWidget(eq);bar->addWidget(sub);bar->addWidget(settings);bar->addWidget(playlist);
    eq->setToolTip("Audio effects");sub->setToolTip("Subtitles and synchronization");settings->setToolTip("Settings");playlist->setToolTip("Playlist");
    connect(eq,&QToolButton::clicked,this,[this]{openSettings(6);});connect(sub,&QToolButton::clicked,this,[this]{openSettings(7);});connect(settings,&QToolButton::clicked,this,[this]{openSettings();});connect(playlist,&QToolButton::clicked,this,[this]{showLibrary("Playlists");});
    bar->addWidget(new QSizeGrip(window));
    root->addWidget(transport);
    connect(THEMIM,&MainInputManager::inputChanged,this,[this](bool has){if(has)showPlayer();else{seek->setEnabled(false);QSignalBlocker blocked(seek);seek->setValue(0);}});
    connect(THEMIM->getIM(),&InputManager::nameChanged,this,[titleLabel](const QString &name){titleLabel->setText(name.isEmpty()?"Reframe":name);});
    applyTheme();showLibrary("Home");
    if(THEMIM->getIM()->hasInput())showPlayer();
}

RedesignShell::~RedesignShell(){if(thumbnailProcess){thumbnailProcess->kill();thumbnailProcess->waitForFinished(1000);}}

void RedesignShell::applyTheme()
{
    accent=getSettings()->value("Redesign/accent","#d98214").toString();
    QString theme=getSettings()->value("Redesign/theme","Dark").toString();
    bool light=theme=="Light"||(theme=="System"&&systemPalette.color(QPalette::Window).lightness()>128);
    const QString bg=light?"#f5f5f5":"#0e0e0e",panel=light?"#e9e9e9":"#191919",text=light?"#303030":"#b8b8b8",line=light?"#d0d0d0":"#292929";
    QApplication::setStyle("Fusion");QPalette p=systemPalette;
    p.setColor(QPalette::Window,QColor(bg));p.setColor(QPalette::Base,QColor(bg));p.setColor(QPalette::AlternateBase,QColor(panel));p.setColor(QPalette::Button,QColor(panel));p.setColor(QPalette::Text,QColor(text));p.setColor(QPalette::WindowText,QColor(text));p.setColor(QPalette::ButtonText,QColor(text));p.setColor(QPalette::Highlight,QColor(accent));p.setColor(QPalette::HighlightedText,QColor("#101010"));p.setColor(QPalette::Disabled,QPalette::Text,QColor("#727272"));QApplication::setPalette(p);
    QApplication::setFont(QFont(QFontDatabase().families().contains("Inter")?"Inter":"Segoe UI",10));
    QString css=QString(
        "QWidget { color:%1; } QWidget#redesignShell, QWidget#browser, QDialog { background:%2; }"
        "QWidget#titleBar, QWidget#navigation, QWidget#transport { background:%3; } QWidget#navigation { border-radius:7px; }"
        "QToolButton { background:transparent; border:0; border-radius:5px; padding:3px; }"
        "QToolButton:hover,QPushButton:hover { background:%4; } QToolButton:checked { background:%4; color:%5; }"
        "QWidget#navigation QToolButton { text-align:left; padding:7px; }"
        "QToolButton#playButton { background:%5; color:#111; } QToolButton#playButton:hover { background:%5; }"
        "QCheckBox::indicator { width:16px; height:16px; background:%2; border:1px solid %1; border-radius:3px; } QCheckBox::indicator:checked { background:%5; border-color:%5; }"
        "QPushButton { background:%3; border:1px solid %4; border-radius:5px; padding:9px 16px; }"
        "QLineEdit,QComboBox,QSpinBox,QDoubleSpinBox { background:%3; border:1px solid %4; border-radius:5px; padding:7px; min-height:20px; }"
        "QLineEdit:focus,QComboBox:focus,QDoubleSpinBox:focus { border-color:%5; }"
        "QListWidget#mediaGrid { background:%2; border:0; outline:0; } QListWidget#settingsNav { background:%3; border:0; padding:8px; border-radius:7px; }"
        "QListWidget#settingsNav::item { padding:12px; border-radius:4px; } QListWidget#settingsNav::item:selected { background:%4; color:%5; }"
        "QLabel#emptyState { color:%1; font-size:16px; } QLabel#sectionHeading { font-size:17px; padding:14px 0; }"
        "QSlider::groove:horizontal { height:10px; background:%2; border-radius:4px; } QSlider::sub-page:horizontal { background:%5; border-radius:4px; }"
        "QSlider::handle:horizontal { background:%5; width:7px; margin:-3px 0; border-radius:3px; }"
        "QSlider::groove:vertical { width:10px; background:%2; border-radius:4px; } QSlider::add-page:vertical { background:%5; border-radius:4px; }"
        "QSlider::handle:vertical { background:%5; height:7px; margin:0 -4px; border-radius:3px; }"
        "QMenu { background:%3; border:1px solid %4; padding:7px; } QMenu::item { padding:9px 26px; } QMenu::item:selected { background:%4; } QMenu::separator { height:1px; background:%4; margin:5px; }"
        "QScrollArea { border:0; background:%2; } QScrollBar:vertical { background:%2; width:10px; } QScrollBar::handle:vertical { background:%4; min-height:20px; border-radius:4px; }"
        "QTreeView,QTableView,QTabWidget::pane { background:%2; border:1px solid %4; } QHeaderView::section { background:%3; padding:7px; border:0; }"
        "QTabBar::tab { background:%3; padding:9px 14px; } QTabBar::tab:selected { border-bottom:2px solid %5; }"
    ).arg(text,bg,panel,line,accent);
    qApp->setStyleSheet(css);
    for(QToolButton *b:findChildren<QToolButton*>()){
        QString glyph=b->property("glyph").toString();if(!glyph.isEmpty())b->setIcon(lineIcon(glyph,QColor(b==playButton?"#111111":glyph=="brand"?accent:text)));
    }
    window->setWindowIcon(lineIcon("brand",QColor(accent)));
    qApp->setWindowIcon(window->windowIcon());
}

void RedesignShell::setNavigationWidth(bool collapse)
{
    if(compact==collapse&&sidebar->width()==(collapse?48:196))return;
    compact=collapse;
    if(navigationAnimation){navigationAnimation->stop();navigationAnimation->deleteLater();}
    sidebar->setMinimumWidth(48);
    navigationAnimation=new QPropertyAnimation(sidebar,"maximumWidth",this);
    navigationAnimation->setStartValue(sidebar->width());navigationAnimation->setEndValue(collapse?48:196);
    navigationAnimation->setDuration(300);navigationAnimation->setEasingCurve(QEasingCurve::InOutCubic);
    navigationAnimation->start();
    for(QToolButton *b:sidebar->findChildren<QToolButton*>())b->setToolButtonStyle(collapse?Qt::ToolButtonIconOnly:Qt::ToolButtonTextBesideIcon);
}
void RedesignShell::showPlayer(){pages->setCurrentWidget(player);setNavigationWidth(getSettings()->value("Redesign/compactOnPlayback",true).toBool());}
bool RedesignShell::isPlayerVisible() const {return pages->currentWidget()==player;}
void RedesignShell::setMinimal(bool m){minimal=m;header->setVisible(!m);sidebar->setVisible(!m);transport->setVisible(!m);if(m)showPlayer();}

void RedesignShell::showLibrary(const QString &name)
{
    section=name;setNavigationWidth(false);
    for(QToolButton *b:sidebar->findChildren<QToolButton*>())if(b->property("section").isValid())b->setChecked(b->property("section").toString()==name);
    if(name=="Network"){THEDP->openNetDialog();return;}
    if(name=="Playlists"){showPlayer();if(!window->isPlaylistVisible())window->togglePlaylist();return;}
    pages->setCurrentWidget(browser);search->clear();
    if(name=="Recent"||name=="Home"){currentDirectory.clear();refresh();return;}
    QString d=getSettings()->value("Redesign/"+name+"Directory",QStandardPaths::writableLocation(name=="Music"?QStandardPaths::MusicLocation:QStandardPaths::MoviesLocation)).toString();browse(d);
}

void RedesignShell::browse(const QString &directory,bool remember)
{
    currentDirectory=QDir(directory).absolutePath();
    if(remember){while(history.size()>historyPosition+1)history.removeLast();history<<currentDirectory;historyPosition=history.size()-1;}
    pages->setCurrentWidget(browser);refresh();
}

void RedesignShell::refresh()
{
    files->clear();thumbnailQueue.clear();location->setText(currentDirectory.isEmpty()?section:QDir::toNativeSeparators(currentDirectory));
    QStringList paths;
    if(currentDirectory.isEmpty())paths=RecentsMRL::getInstance(p_intf)->recentList();
    else {
        const QStringList filters=QStringList()<<"*.mp4"<<"*.mkv"<<"*.avi"<<"*.mov"<<"*.webm"<<"*.m4v"<<"*.wmv"<<"*.ts"<<"*.mpeg"<<"*.mpg"<<"*.flv"<<"*.mp3"<<"*.flac"<<"*.wav"<<"*.ogg"<<"*.opus"<<"*.m4a"<<"*.aac"<<"*.wma"<<"*.aiff"<<"*.m3u"<<"*.m3u8"<<"*.xspf";
        QDir d(currentDirectory);d.setNameFilters(filters);d.setFilter(QDir::AllDirs|QDir::Files|QDir::NoDotAndDotDot|QDir::Readable);d.setSorting(QDir::DirsFirst|QDir::Name|QDir::IgnoreCase);
        for(const QFileInfo &f:d.entryInfoList())paths<<f.absoluteFilePath();
    }
    for(const QString &path:paths){
        QUrl url(path);QString local=url.isLocalFile()?url.toLocalFile():path;QFileInfo f(local);bool folder=f.isDir();
        if(!folder&&!f.exists()&&url.scheme().isEmpty())continue;
        auto item=new QListWidgetItem(f.fileName().isEmpty()?path:f.fileName(),files);item->setToolTip(local);item->setData(Qt::UserRole,local);
        item->setIcon(lineIcon(folder?"folder":section=="Music"?"music":"videos",QColor(accent)));
        if(!folder&&f.exists()){
            QByteArray fingerprint=(f.absoluteFilePath()+QString::number(f.size())+QString::number(f.lastModified().toMSecsSinceEpoch())).toUtf8();
            QString thumb=cacheDirectory+"/"+QString::fromLatin1(QCryptographicHash::hash(fingerprint,QCryptographicHash::Sha256).toHex())+".png";thumbnails[local]=thumb;
            if(QFileInfo(thumb).size()>0){item->setIcon(QIcon(thumb));item->setData(Qt::UserRole+1,true);}
            if(!QFileInfo::exists(thumb)&&QStringList({"mp4","mkv","avi","mov","webm","m4v","wmv","ts","mpeg","mpg","flv"}).contains(f.suffix().toLower()))thumbnailQueue.enqueue(local);
        }
    }
    count->setText(QString::number(files->count())+" items");
    bool none=files->count()==0;files->setVisible(!none);empty->setVisible(none);empty->setText(currentDirectory.isEmpty()?"Your recent media will appear here.\nOpen a file or browse a media folder to get started.":"No media in this folder.\nUse the folder button to choose your videos or music.");
    nextThumbnail();
}

void RedesignShell::openMedia(const QString &path)
{
    QUrl url=QUrl::fromUserInput(path,QString(),QUrl::AssumeLocalFile);
    showPlayer();if(window->isPlaylistVisible())window->togglePlaylist();Open::openMRL(p_intf,url.toString());
}

void RedesignShell::contextMenu(const QPoint &position)
{
    auto item=files->itemAt(position);if(!item)return;
    QMenu menu;auto play=menu.addAction(lineIcon("play",QColor(accent)),"Play");auto enqueue=menu.addAction(lineIcon("playlist"),"Add to playlist");menu.addSeparator();auto folder=menu.addAction(lineIcon("folder"),"Open containing folder");
    QAction *selected=menu.exec(files->viewport()->mapToGlobal(position));QString path=item->data(Qt::UserRole).toString();
    if(selected==play){if(QFileInfo(path).isDir())browse(path);else openMedia(path);}
    else if(selected==enqueue)for(QListWidgetItem *i:files->selectedItems())Open::openMRL(p_intf,QUrl::fromUserInput(i->data(Qt::UserRole).toString(),QString(),QUrl::AssumeLocalFile).toString(),false);
    else if(selected==folder)QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
}

void RedesignShell::nextThumbnail()
{
    if(thumbnailProcess||thumbnailQueue.isEmpty())return;
    thumbnailPath=thumbnailQueue.dequeue();QString output=thumbnails.value(thumbnailPath);if(output.isEmpty())return;
    thumbnailProcess=new QProcess(this);QProcess *process=thumbnailProcess;QString path=thumbnailPath;
    QStringList args;args<<"--intf=dummy"<<"--ignore-config"<<"--no-one-instance"<<"--no-audio"<<"--no-spu"<<"--no-osd"<<"--no-video-title-show"<<"--no-metadata-network-access"<<"--vout=dummy"<<"--video-filter=scene"<<"--scene-format=png"<<"--scene-width=320"<<"--scene-ratio=1"<<"--scene-replace"<<("--scene-path="+cacheDirectory)<<("--scene-prefix="+QFileInfo(output).completeBaseName())<<"--start-time=1"<<"--stop-time=2"<<"--play-and-exit"<<QUrl::fromLocalFile(path).toString();
    auto done=[this,process,path,output]{if(thumbnailProcess!=process)return;thumbnailProcess=nullptr;process->deleteLater();if(QFileInfo(output).size()>0)for(int i=0;i<files->count();++i){auto item=files->item(i);if(item->data(Qt::UserRole).toString()==path){item->setIcon(QIcon(output));item->setData(Qt::UserRole+1,true);}}QTimer::singleShot(0,this,[this]{nextThumbnail();});};
    connect(process,QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),this,[done](int,QProcess::ExitStatus){done();});
    connect(process,&QProcess::errorOccurred,this,[done](QProcess::ProcessError){done();});
    QTimer::singleShot(10000,process,[process]{if(process->state()!=QProcess::NotRunning)process->kill();});
    process->start(QCoreApplication::applicationFilePath(),args);
}

void RedesignShell::openSettings(int selectedPage)
{
    if(settingsDialog){
        if(auto nav=settingsDialog->findChild<QListWidget *>("settingsNav"))nav->setCurrentRow(selectedPage);
        settingsDialog->raise();settingsDialog->activateWindow();return;
    }
    settingsDialog=new QDialog(window);settingsDialog->setAttribute(Qt::WA_DeleteOnClose);settingsDialog->setWindowFlags(Qt::Dialog|Qt::FramelessWindowHint);settingsDialog->setWindowTitle("Reframe settings");settingsDialog->resize(1048,820);
    connect(settingsDialog,&QObject::destroyed,this,[this]{settingsDialog=nullptr;});
    auto root=new QVBoxLayout(settingsDialog);root->setContentsMargins(16,16,16,16);root->setSpacing(16);
    auto headerWidget=new TitleBar(settingsDialog);auto settingsHeader=new QHBoxLayout(headerWidget);settingsHeader->setContentsMargins(0,0,0,0);
    auto filter=new QLineEdit;filter->setPlaceholderText("Search settings");filter->setFixedWidth(208);settingsHeader->addWidget(filter);
    auto heading=new QLabel("Global Settings");heading->setObjectName("sectionHeading");settingsHeader->addWidget(heading,1);auto dismiss=button("close");settingsHeader->addWidget(dismiss);connect(dismiss,&QToolButton::clicked,settingsDialog,&QDialog::close);root->addWidget(headerWidget);
    auto row=new QHBoxLayout;auto nav=new QListWidget;nav->setObjectName("settingsNav");nav->setFixedWidth(208);
    const QStringList labels=QStringList()<<"General"<<"Interface"<<"Privacy"<<"Controls"<<"Playback"<<"Video"<<"Audio"<<"Subtitles"<<"Library"<<"Network"<<"Advanced"<<"About";
    nav->addItems(labels);row->addWidget(nav);auto stack=new QStackedWidget;row->addWidget(stack,1);root->addLayout(row,1);
    const QStringList glyphs=QStringList()<<"settings"<<"list"<<"home"<<"subtitles"<<"play"<<"videos"<<"audio"<<"subtitles"<<"folder"<<"network"<<"settings"<<"brand";
    for(int i=0;i<nav->count();++i)nav->item(i)->setIcon(lineIcon(glyphs[i]));
    connect(filter,&QLineEdit::textChanged,nav,[nav](const QString &text){for(int i=0;i<nav->count();++i)nav->item(i)->setHidden(!nav->item(i)->text().contains(text,Qt::CaseInsensitive));});
    for(int page=0;page<labels.size();++page){
        auto scroll=new QScrollArea;scroll->setWidgetResizable(true);auto content=new QWidget;auto layout=new QVBoxLayout(content);layout->setContentsMargins(24,4,24,24);layout->setSpacing(18);
        auto pageTitle=new QLabel(labels[page]);pageTitle->setObjectName("sectionHeading");layout->addWidget(pageTitle);
        auto form=new QFormLayout;form->setSpacing(20);form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);layout->addLayout(form);
        if(page==0){
            auto theme=new QComboBox;theme->addItems(QStringList()<<"Dark"<<"Light"<<"System");theme->setCurrentText(getSettings()->value("Redesign/theme","Dark").toString());form->addRow("Theme",theme);
            connect(theme,&QComboBox::currentTextChanged,this,[this](const QString &value){getSettings()->setValue("Redesign/theme",value);applyTheme();});
            auto colors=new QWidget;auto colorRow=new QHBoxLayout(colors);colorRow->setContentsMargins(0,0,0,0);
            const QStringList accentColors=QStringList()<<"#d98214"<<"#c64d3d"<<"#c4a12e"<<"#2caa52"<<"#249dbc"<<"#3b74dd"<<"#9747ff"<<"#e349b4"<<"#b8b8b8";
            for(const QString &color:accentColors){
                auto b=new QPushButton;b->setFixedSize(30,30);b->setToolTip(color);b->setStyleSheet("QPushButton {background:"+color+";border:0;border-radius:3px;}");colorRow->addWidget(b);connect(b,&QPushButton::clicked,this,[this,color]{getSettings()->setValue("Redesign/accent",color);applyTheme();});
            }form->addRow("Accent color",colors);
            auto text=new QLabel("Appearance changes apply immediately. Other VLC preferences remain available in Advanced Options.");text->setWordWrap(true);layout->addWidget(text);
        }else if(page==1){
            auto hide=new QCheckBox("Compact navigation while playing");hide->setChecked(getSettings()->value("Redesign/compactOnPlayback",true).toBool());form->addRow(hide);connect(hide,&QCheckBox::toggled,this,[this](bool on){getSettings()->setValue("Redesign/compactOnPlayback",on);if(pages->currentWidget()==player)setNavigationWidth(on);});
            auto top=new QCheckBox("Always on top");top->setChecked(window->isInterfaceAlwaysOnTop());form->addRow(top);connect(top,&QCheckBox::toggled,window,&MainInterface::setInterfaceAlwaysOnTop);
        }else if(page==2){
            auto network=new QCheckBox("Allow metadata network access");network->setChecked(config_GetInt(p_intf,"metadata-network-access"));form->addRow(network);connect(network,&QCheckBox::toggled,this,[this](bool b){config_PutInt(p_intf,"metadata-network-access",b);var_SetBool(p_intf->obj.libvlc,"metadata-network-access",b);config_SaveConfigFile(p_intf);});
            auto clear=new QPushButton("Clear recent media");form->addRow(clear);connect(clear,&QPushButton::clicked,this,[this]{RecentsMRL::getInstance(p_intf)->clear();if(currentDirectory.isEmpty())refresh();});
        }else if(page==3){
            auto text=new QLabel("Space  Play / pause\nF  Fullscreen\nS  Stop\nN / P  Next / previous\nCtrl+O  Open media\nCtrl+Q  Quit\n\nVLC's existing hotkeys and advanced control customization are preserved.");text->setWordWrap(true);layout->addWidget(text);
        }else if(page==4){
            auto rate=new QDoubleSpinBox;rate->setRange(.25,4);rate->setSingleStep(.25);rate->setSuffix(" x");rate->setValue(var_GetFloat(THEPL,"rate"));form->addRow("Playback speed",rate);connect(rate,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,[this](double v){var_SetFloat(THEPL,"rate",v);config_PutFloat(p_intf,"rate",v);config_SaveConfigFile(p_intf);});
            auto loop=new QCheckBox("Repeat playlist");loop->setChecked(var_GetBool(THEPL,"loop"));form->addRow(loop);connect(loop,&QCheckBox::toggled,this,[this](bool b){var_SetBool(THEPL,"loop",b);config_PutInt(p_intf,"loop",b);config_SaveConfigFile(p_intf);});
            auto shuffle=new QCheckBox("Shuffle");shuffle->setChecked(var_GetBool(THEPL,"random"));form->addRow(shuffle);connect(shuffle,&QCheckBox::toggled,this,[this](bool b){var_SetBool(THEPL,"random",b);config_PutInt(p_intf,"random",b);config_SaveConfigFile(p_intf);});
        }else if(page==6){
            auto equalizer=new Equalizer(p_intf,content);layout->addWidget(equalizer);
            for(QSlider *slider:equalizer->findChildren<QSlider*>())if(slider->orientation()==Qt::Vertical)slider->setMinimumHeight(260);
            for(QLabel *label:equalizer->findChildren<QLabel*>())label->setFont(QApplication::font());
            connect(equalizer,&Equalizer::configChanged,this,[this](const QString &name,const QVariant &value){
                if(value.type()==QVariant::String)config_PutPsz(p_intf,qtu(name),qtu(value.toString()));
                else if(value.userType()==QMetaType::Double||value.userType()==QMetaType::Float)config_PutFloat(p_intf,qtu(name),value.toDouble());
                else config_PutInt(p_intf,qtu(name),value.toInt());
                config_SaveConfigFile(p_intf);
            });
        }else if(page==7){
            auto delay=new QDoubleSpinBox;delay->setObjectName("subtitleDelay");delay->setDecimals(3);delay->setRange(-600,600);delay->setSingleStep(.1);delay->setSuffix(" s");form->addRow("Subtitle track delay",delay);
            auto updateDelay=[this,delay]{QSignalBlocker blocked(delay);auto input=THEMIM->getInput();delay->setEnabled(input!=nullptr);delay->setValue(input?var_GetInteger(input,"spu-delay")/double(CLOCK_FREQ):0);};
            updateDelay();connect(THEMIM,&MainInputManager::inputChanged,delay,[updateDelay](bool){updateDelay();});connect(THEMIM->getIM(),&InputManager::synchroChanged,delay,updateDelay);
            connect(delay,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,[this](double v){if(THEMIM->getInput())var_SetInteger(THEMIM->getInput(),"spu-delay",llround(v*CLOCK_FREQ));});
            auto add=new QPushButton("Load subtitle file");form->addRow(add);connect(add,&QPushButton::clicked,THEAM,[this]{THEAM->doAction(OPEN_SUB_ACTION);});
            auto sync=new QPushButton("Track synchronization and subtitle controls");layout->addWidget(sync);connect(sync,&QPushButton::clicked,THEDP,&DialogsProvider::extendedDialog);
        }else if(page==8){
            const QStringList mediaSections=QStringList()<<"Videos"<<"Music";
            for(const QString &s:mediaSections){
                auto choose=new QPushButton("Choose "+s.toLower()+" folder");form->addRow(choose);connect(choose,&QPushButton::clicked,this,[this,s]{QString d=QFileDialog::getExistingDirectory(settingsDialog,"Choose "+s+" folder");if(!d.isEmpty())getSettings()->setValue("Redesign/"+s+"Directory",d);});
            }
        }else if(page==11){
            auto text=new QLabel("Reframe 3.0.24\nAn unofficial VLC media player fork.\n\nPlayback engine and original application: VideoLAN and VLC authors.\nGPL v2 or later, with the licenses of bundled dependencies.\n\nInterface adapted from VLC Redesign by Juxtopposed.\nDesign licensed under Creative Commons Attribution 4.0.\nNative implementation and interface changes: webmaster94, 2026.\n\nThis application is not endorsed by VideoLAN or Juxtopposed.");text->setWordWrap(true);layout->addWidget(text);
            auto links=new QLabel("<a href='https://github.com/webmaster94/vlc-redesign'>Source and build scripts</a><br><a href='https://www.figma.com/community/file/1691465999134370980/vlc-redesign'>Original design</a><br><a href='https://creativecommons.org/licenses/by/4.0/'>CC BY 4.0</a>");links->setOpenExternalLinks(true);layout->addWidget(links);
        }
        if(page!=11){auto advanced=new QPushButton("Advanced Options");layout->addWidget(advanced);connect(advanced,&QPushButton::clicked,THEDP,&DialogsProvider::prefsDialog);}
        if(page==5||page==9||page==10){auto effects=new QPushButton(page==5?"Video effects and filters":page==9?"Open network stream":"Effects, synchronization and recording controls");layout->insertWidget(2,effects);connect(effects,&QPushButton::clicked,this,[this,page]{if(page==9)THEDP->openNetDialog();else THEDP->extendedDialog();});}
        layout->addStretch();scroll->setWidget(content);stack->addWidget(scroll);
    }
    connect(nav,&QListWidget::currentRowChanged,stack,&QStackedWidget::setCurrentIndex);nav->setCurrentRow(selectedPage);
    auto close=new QPushButton("Close");auto buttons=new QHBoxLayout;buttons->addStretch();buttons->addWidget(close);root->addLayout(buttons);connect(close,&QPushButton::clicked,settingsDialog,&QDialog::close);
    settingsDialog->show();
}

/* An opt-in integration check drives the actual widgets and checks VLC state.
 * It runs only when the private CLI smoke option is supplied. */
void RedesignShell::runSmoke(const QString &reportPath)
{
    auto report=[this,reportPath](const QString &result){
        QFile file(reportPath);if(file.open(QIODevice::WriteOnly))file.write(result.toUtf8());
        window->grab().save(reportPath+".player.png");
        if(settingsDialog)settingsDialog->grab().save(reportPath+".settings.png");
        THEDP->quit();
    };
    if(!THEMIM->getInput()){report("FAIL: input did not start");return;}
    const QStringList parts=elapsed->text().split(':');
    if(parts.size()!=3||parts[0].toInt()!=0||parts[1].toInt()!=0||parts[2].toInt()>30){report("FAIL: elapsed-time units");return;}
    playButton->click();
    QTimer::singleShot(250,this,[this,report]{
        auto input=THEMIM->getInput();
        if(!input||var_GetInteger(input,"state")!=PAUSE_S){report("FAIL: pause control");return;}
        playButton->click();seek->setValue(30000);
        QTimer::singleShot(250,this,[this,report]{
            auto input=THEMIM->getInput();
            if(!input||var_GetInteger(input,"state")!=PLAYING_S){report("FAIL: resume control");return;}
            if(std::abs(var_GetFloat(input,"position")-.3f)>.1f){report("FAIL: seek control");return;}
            showLibrary("Playlists");
            if(!window->isPlaylistVisible()){report("FAIL: playlist control");return;}
            window->togglePlaylist();
            openSettings(0);openSettings(6);
            auto nav=settingsDialog->findChild<QListWidget *>("settingsNav");
            if(!nav||nav->currentRow()!=6){report("FAIL: settings navigation");return;}
            auto preamp=settingsDialog->findChild<QSlider *>("preampSlider");
            if(!preamp){report("FAIL: equalizer widget");return;}
            preamp->setValue(73);
            if(std::abs(config_GetFloat(p_intf,"equalizer-preamp")-7.3f)>.05f){report("FAIL: equalizer config type");return;}
            openSettings(7);auto delay=settingsDialog->findChild<QDoubleSpinBox *>("subtitleDelay");
            if(!delay||!delay->isEnabled()){report("FAIL: subtitle control state");return;}
            delay->setValue(.25);
            if(var_GetInteger(THEMIM->getInput(),"spu-delay")!=CLOCK_FREQ/4){report("FAIL: subtitle delay units");return;}
            openSettings(6);
            QTimer::singleShot(250,this,[report]{report("PASS: elapsed time, pause, resume, seek, playlist, settings navigation, equalizer float persistence, subtitle synchronization");});
        });
    });
}
