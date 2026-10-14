/* Copyright 2026 webmaster94. GPL-3.0-or-later.
 * Interface adapted from Juxtopposed, CC BY 4.0; see doc/redesign/licensing.md. */
#ifndef VLC_REDESIGN_CONTROLS_HPP
#define VLC_REDESIGN_CONTROLS_HPP
#include <QAbstractButton>
#include <QDialog>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include <functional>
#include <QIcon>
#include <QToolButton>
#include <QComboBox>
#include <QFontComboBox>

class QAbstractItemDelegate;
class QSizeGrip;
namespace Reframe
{
// Network helpers must not resolve executables through a user-controlled PATH or cwd.
QString systemCurl();
QAbstractItemDelegate *mediaDelegate(QObject *);
QAbstractItemDelegate *choiceDelegate(QObject *);
QStringList defaultToolbar();
QIcon icon(const QString &, const QColor & = QColor("#8f8f8f"));
QEasingCurve transitionCurve();
QEasingCurve springCurve();
void reveal(QWidget *);
void centerOnParent(QDialog *);
void animateHeight(QWidget *, bool, int duration = 300);
void animateWidth(QWidget *, int width, int duration = 208);
QWidget *fold(const QString &, QWidget *, QWidget *, bool expanded = false);
QWidget *segmented(const QStringList &, const QString &, QWidget *, std::function<void(QString)>);

class Toggle : public QAbstractButton
{
    QVariantAnimation motion;
    qreal position;

  protected:
    void paintEvent(QPaintEvent *) override;

  public:
    explicit Toggle(QWidget *parent = nullptr);
    QSize sizeHint() const override
    {
        return QSize(68, 32);
    }
};

class Dialog : public QDialog
{
    QPoint dragOffset;
    QSizeGrip *grip;

  protected:
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void closeEvent(QCloseEvent *) override;
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void showEvent(QShowEvent *) override;

  public:
    explicit Dialog(QWidget *parent, const QString &title, QSize size);
    QVBoxLayout *body;
    std::function<void()> beforeClose;
};
class Dropdown : public QComboBox
{
  public:
    explicit Dropdown(QWidget *parent = nullptr) : QComboBox(parent)
    {
        setMaxVisibleItems(10);
    }
    void showPopup() override;
};
class FontDropdown : public QFontComboBox
{
  public:
    explicit FontDropdown(QWidget *parent = nullptr) : QFontComboBox(parent)
    {
        setMaxVisibleItems(10);
    }
    void showPopup() override;
};
class ToolButton : public QToolButton
{
    QVariantAnimation hover;
    qreal amount;

  protected:
    void enterEvent(QEvent *) override;
    void leaveEvent(QEvent *) override;
    void paintEvent(QPaintEvent *) override;

  public:
    explicit ToolButton(QWidget *parent = nullptr);
};
} // namespace Reframe
#endif
