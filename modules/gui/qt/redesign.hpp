/* Reframe interface, adapted from Juxtopposed's VLC Redesign, CC BY 4.0.
 * Implementation copyright 2026 webmaster94. GPL-3.0-or-later.
 * Original design and attribution: doc/redesign/licensing.md. */
#ifndef VLC_QT_REDESIGN_HPP
#define VLC_QT_REDESIGN_HPP

#include "qt.hpp"
#include <QWidget>
#include <QPalette>
#include <QStringList>
#include <QHash>
#include <QQueue>
#include <QVariant>
#include <functional>

class MainInterface;
class QListWidget;
class QLabel;
class QLineEdit;
class QStackedWidget;
class QToolButton;
class QSlider;
class QProcess;
class QDialog;
class QPropertyAnimation;
class QHBoxLayout;
class QVBoxLayout;
class QBoxLayout;
class QTimer;
class RedesignPreferences;
class QTableWidget;

class RedesignShell : public QWidget
{
  public:
    RedesignShell(intf_thread_t *, MainInterface *, QWidget *player);
    ~RedesignShell();
    void showPlayer();
    bool isPlayerVisible() const;
    void runSmoke(const QString &reportPath);
    void setMinimal(bool);
    void applyTheme();
    void openSettings(int page = 0);
    void showLibrary(const QString &section);
    void openMediaSettings(int page = 4);
    void openTracks(int page = 7);
    void openNetwork();
    void customize();
    void onboarding();
    void startFirstRun();
    void runFirstRunSmoke(const QString &);
    void showQueue(int tab = 0);
    void toggleQueue();
    bool isQueueVisible() const;
    void showChrome();
    void openProperties();
    void batchTracks(const QStringList &paths = QStringList(), int kind = 0);
    void newPlaylist(const QString &id = QString(), const QStringList &initialPaths = QStringList());
    void leaveFullscreen();
    void toggleFullscreen();
    void toggleMiniPlayer();
    void mainMenu(const QPoint &);
    void playerMenu(const QPoint &);

  private:
    intf_thread_t *p_intf;
    MainInterface *window;
    QWidget *player;
    QWidget *header;
    QWidget *sidebar;
    QWidget *transport;
    QWidget *browser;
    QStackedWidget *pages;
    QListWidget *files;
    QLabel *location;
    QLabel *count;
    QLabel *empty;
    QLabel *elapsed;
    QLabel *duration;
    QLineEdit *search;
    QToolButton *playButton;
    QSlider *seek;
    QSlider *volume;
    QDialog *settingsDialog;
    QString currentDirectory;
    QString section;
    QStringList history;
    int historyPosition;
    QPalette systemPalette;
    QString accent;
    QHash<QString, QString> thumbnails;
    QQueue<QString> thumbnailQueue;
    QProcess *thumbnailProcess;
    QString thumbnailPath;
    QString cacheDirectory;
    bool compact;
    bool minimal;
    QPropertyAnimation *navigationAnimation;
    QWidget *videoPage;
    QWidget *sidePanel;
    QWidget *libraryDock;
    QDialog *miniPlayer;
    QHBoxLayout *bodyLayout;
    QVBoxLayout *videoLayout;
    QBoxLayout *transportLayout;
    QTimer *chromeTimer;
    QTimer *videoClickTimer;
    bool remainingTime;
    bool customizing;
    int sortMode;
    QSize previousSize;
    QString currentCollection;
    QHash<QString, QToolButton *> controlButtons;
    QStringList appliedToolbar;
    QString bindingInput;
    QWidget *homePanel;
    QWidget *customLeft;
    QWidget *customTop;
    QWidget *extraToolbar;
    QWidget *breadcrumbs;
    QHash<QString, QVariantMap> metadata;
    QHash<QString, input_item_t *> metadataRequests;
    QHash<QString, qint64> metadataStarted;
    QString musicMode;
    QString musicGroup;
    qint64 lastResumeSave;

    QToolButton *button(const QString &icon, const QString &text = QString());
    void refresh();
    void browse(const QString &, bool remember = true);
    void openMedia(const QString &);
    void nextThumbnail();
    void contextMenu(const QPoint &);
    void setNavigationWidth(bool);
    void toggleLibraryDock();
    void layoutControls();
    void updateQueue();
    void runFlowSmoke(const QString &, std::function<void(const QString &)>);
    void runInteractionSmoke(const QString &, std::function<void(const QString &)>);
    void checkUpdates();
    void applyMediaBindings();
    void requestMetadata(const QString &);
    void pollMetadata();
    void renderHome();
    void refreshPins();
    void mediaProperties(const QString &);
    void populateAuxiliaryControls();
    void openSubtitles(QWidget *, QTableWidget *);
    void startAudioFocus();
    bool chromeCanHide() const;
    bool eventFilter(QObject *, QEvent *) override;
};
#endif
