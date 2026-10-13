/* Reframe interface, adapted from Juxtopposed's VLC Redesign, CC BY 4.0.
 * Implementation copyright 2026 webmaster94. GPL-2.0-or-later.
 * Original design and attribution: doc/redesign/licensing.md. */
#ifndef VLC_QT_REDESIGN_HPP
#define VLC_QT_REDESIGN_HPP

#include "qt.hpp"
#include <QWidget>
#include <QPalette>
#include <QStringList>
#include <QHash>
#include <QQueue>

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

    QToolButton *button(const QString &icon, const QString &text = QString());
    void refresh();
    void browse(const QString &, bool remember = true);
    void openMedia(const QString &);
    void nextThumbnail();
    void contextMenu(const QPoint &);
    void setNavigationWidth(bool);
};
#endif
