/* Copyright 2026 webmaster94. GPL-3.0-or-later. */
#ifndef VLC_REDESIGN_SETTINGS_HPP
#define VLC_REDESIGN_SETTINGS_HPP
#include "qt.hpp"
#include "redesign_controls.hpp"
#include <QVariant>
#include <QSet>
#include <QTimer>
#include <functional>
class MainInterface;
class QFormLayout;
class QStackedWidget;
class QListWidget;
class RedesignPreferences : public Reframe::Dialog
{
    intf_thread_t *p_intf;
    MainInterface *window;
    bool local;
    QTimer saveTimer;
    std::function<void()> themeChanged;
    QHash<QString, QVariant> localValues;
    QSet<QString> edited;
    QHash<QString, QString> videoEffectForOption;
    QListWidget *navigation;
    QStackedWidget *pages;
    QVariant read(const QString &) const;
    void write(const QString &, const QVariant &);
    QWidget *option(const QString &, QWidget *);
    void row(QVBoxLayout *, const QString &, QWidget *, const QString &key = QString());
    void heading(QVBoxLayout *, const QString &);
    void options(QVBoxLayout *, const char *);
    QWidget *buildPage(int);
    QWidget *advanced(int);
    QWidget *equalizer(bool);
    QWidget *filter(const QString &, const QString &, const char *);
    QWidget *videoEffects();
    void filterEnabled(const QString &, bool, bool video = false);
    void shortcuts();
    void profiles();
    void preset(QVBoxLayout *);

  public:
    RedesignPreferences(intf_thread_t *, MainInterface *, int, std::function<void()>, bool media = false);
    ~RedesignPreferences();
    void selectPage(int);
    void restore(const QVariantMap &);
    void editShortcuts()
    {
        shortcuts();
    }
    void manageProfiles()
    {
        profiles();
    }
    void saveAppProfile(const QString &);
    int currentPage() const;
    void openAdvanced(int);
    static QStringList categories();
};
#endif
