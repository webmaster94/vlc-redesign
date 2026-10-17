/* Copyright 2026 webmaster94. GPL-3.0-or-later.
 * OpenSubtitles search for the Juxtopposed batch-track flow, CC BY 4.0.
 * API contract: https://opensubtitles.stoplight.io/docs/opensubtitles-api */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "redesign.hpp"
#include "redesign_controls.hpp"
#include "main_interface.hpp"
#include <QApplication>
#include <QTableWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QFormLayout>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QSettings>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDesktopServices>
#include <QCryptographicHash>
#include <memory>
#include <QTimer>
#include <QPointer>

namespace
{
using Response = std::function<void(QByteArray, QString)>;
QString quoted(QString text)
{
    return "\"" + text.replace('\\', "\\\\").replace('"', "\\\"").replace('\r', "").replace('\n', "\\n") +
           "\"";
}
bool safeHeader(const QString &text)
{
    if (text.size() > 8192)
        return false;
    for (const auto character : text)
        if (character.unicode() < 32 || character.unicode() == 127)
            return false;
    return true;
}
bool safeDestination(const QUrl &url, bool credentialed)
{
    if (!url.isValid() || url.scheme() != "https" || url.host().isEmpty() ||
        url.authority(QUrl::FullyEncoded).contains('@') || url.hasFragment())
        return false;
    if (!credentialed)
        return true;
    return (url.host() == "api.opensubtitles.com" || url.host() == "vip-api.opensubtitles.com") &&
           (url.port() == -1 || url.port() == 443) && url.path().startsWith("/api/v1/");
}
void request(QObject *owner, const QUrl &url, const QString &key, const QString &token,
             const QJsonObject &body, Response done)
{
    const bool credentialed = !key.isEmpty() || !token.isEmpty() || !body.isEmpty();
    if (!safeDestination(url, credentialed) || !safeHeader(key) || !safeHeader(token))
    {
        done(QByteArray(), "The service returned an unsafe address or credential header.");
        return;
    }
    const QString executable = Reframe::systemCurl();
    if (executable.isEmpty())
    {
        done(QByteArray(), "The trusted Windows curl client is unavailable.");
        return;
    }
    QString config = "url = " + quoted(url.toString(QUrl::FullyEncoded)) + "\n";
    if (!key.isEmpty())
        config += "header = " + quoted("Api-Key: " + key) + "\n";
    if (!token.isEmpty())
        config += "header = " + quoted("Authorization: Bearer " + token) + "\n";
    if (!body.isEmpty())
    {
        config += "request = \"POST\"\nheader = \"Content-Type: application/json\"\n";
        config +=
            "data = " + quoted(QString::fromUtf8(QJsonDocument(body).toJson(QJsonDocument::Compact))) + "\n";
    }
    struct State
    {
        QByteArray output;
        qint64 errors = 0;
        bool complete = false;
        bool oversized = false;
    };
    auto process = new QProcess(owner);
    process->setObjectName("subtitleRequest");
    auto state = std::make_shared<State>();
    // Bound our buffers independently of curl's version and Content-Length handling.
    auto drain = [process, state]
    {
        const qint64 outputLimit = 10485760 + 16; // Body plus the HTTP status trailer.
        const qint64 errorLimit = 65536;
        process->setReadChannel(QProcess::StandardOutput);
        while (process->bytesAvailable() && !state->oversized)
        {
            const QByteArray chunk =
                process->read(qMin<qint64>(16384, outputLimit - state->output.size() + 1));
            if (state->output.size() + chunk.size() > outputLimit)
                state->oversized = true;
            else
                state->output += chunk;
        }
        process->setReadChannel(QProcess::StandardError);
        while (process->bytesAvailable() && !state->oversized)
        {
            const QByteArray chunk = process->read(qMin<qint64>(16384, errorLimit - state->errors + 1));
            state->errors += chunk.size();
            if (state->errors > errorLimit)
                state->oversized = true;
        }
        if (state->oversized)
            process->kill();
    };
    const QPointer<QProcess> guardedProcess(process);
    auto finish = [guardedProcess, state, done](QByteArray bytes, const QString &error)
    {
        if (state->complete)
            return;
        state->complete = true;
        done(bytes, error);
        if (guardedProcess)
            guardedProcess->deleteLater();
    };
    QObject::connect(process, &QProcess::readyReadStandardOutput, owner, drain);
    QObject::connect(process, &QProcess::readyReadStandardError, owner, drain);
    QObject::connect(process, &QProcess::started, owner,
                     [process, config]
                     {
                         process->write(config.toUtf8());
                         process->closeWriteChannel();
                     });
    QObject::connect(process, &QProcess::errorOccurred, owner,
                     [finish](QProcess::ProcessError error)
                     {
                         if (error == QProcess::FailedToStart)
                             finish(QByteArray(), "The trusted Windows curl client could not start.");
                     });
    QObject::connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), owner,
                     [process, state, drain, finish](int exit, QProcess::ExitStatus status)
                     {
                         if (state->complete)
                             return;
                         drain();
                         if (state->oversized)
                         {
                             finish(QByteArray(), "The service response exceeded the size limit.");
                             return;
                         }
                         QByteArray bytes = state->output;
                         const int separator = bytes.lastIndexOf('\n');
                         bool validStatus = false;
                         const int http = bytes.mid(separator + 1).trimmed().toInt(&validStatus);
                         bytes.truncate(qMax(0, separator));
                         QString error;
                         if (exit || status != QProcess::NormalExit)
                             error = "The connection failed or timed out. Try again.";
                         else if (!validStatus || http < 200 || http >= 300)
                         {
                             const auto message =
                                 QJsonDocument::fromJson(bytes).object().value("message").toString();
                             error = message.isEmpty() ? QString("OpenSubtitles returned HTTP %1.").arg(http)
                                                       : message.left(4096);
                         }
                         finish(bytes, error);
                     });
    auto watchdog = new QTimer(process);
    watchdog->setSingleShot(true);
    QObject::connect(watchdog, &QTimer::timeout, owner,
                     [process, finish]
                     {
                         process->kill();
                         finish(QByteArray(), "The connection timed out. Try again.");
                     });
    watchdog->start(35000);
    QObject::connect(owner, &QObject::destroyed, process,
                     [process, state]
                     {
                         state->complete = true;
                         if (process->state() == QProcess::Starting)
                             process->waitForStarted(1000);
                         process->kill();
                         process->waitForFinished(1000);
                     });
    // -q must come first to disable ambient .curlrc settings. Credentials use stdin.
    QStringList arguments = {"-q",
                             "--silent",
                             "--show-error",
                             "--proto",
                             "=https",
                             "--proto-redir",
                             "=https",
                             "--max-time",
                             "30",
                             "--connect-timeout",
                             "10",
                             "--max-filesize",
                             "10485760",
                             "--user-agent",
                             "Reframe v0.6.0",
                             "--write-out",
                             "\n%{http_code}",
                             "--config",
                             "-"};
    if (!credentialed)
        arguments << "--location"
                  << "--max-redirs"
                  << "3";
    process->start(executable, arguments);
}
} // namespace

void RedesignShell::openSubtitles(QWidget *owner, QTableWidget *bindings)
{
    auto dialog = new Reframe::Dialog(owner, "Find Subtitles", QSize(1048, 800));
    auto connection = std::make_shared<QVariantMap>();
    QFile packaged(QCoreApplication::applicationDirPath() + "/opensubtitles-app-key.txt");
    if (packaged.open(QIODevice::ReadOnly) && packaged.size() <= 8192)
        (*connection)["key"] = QString::fromUtf8(packaged.read(8193)).trimmed();
    if (connection->value("key").toString().isEmpty())
        (*connection)["key"] = getSettings()->value("OpenSubtitles/applicationKey");
    (*connection)["base"] = "https://api.opensubtitles.com/api/v1";
    auto query = new QLineEdit;
    query->setPlaceholderText("Movie or series title");
    if (bindings->currentRow() < 0 && bindings->rowCount())
        bindings->setCurrentCell(0, 0);
    if (bindings->currentRow() >= 0)
        query->setText(QFileInfo(bindings->item(bindings->currentRow(), 0)->text()).completeBaseName());
    auto language = new QLineEdit("en");
    language->setMaximumWidth(100);
    language->setToolTip("Language code, for example en, es or ja");
    auto search = new QPushButton("Search");
    auto row = new QHBoxLayout;
    row->addWidget(query, 1);
    row->addWidget(language);
    row->addWidget(search);
    dialog->body->addLayout(row);
    auto account = new QWidget;
    auto login = new QFormLayout(account);
    auto username = new QLineEdit;
    auto password = new QLineEdit;
    password->setEchoMode(QLineEdit::Password);
    login->addRow("Username", username);
    login->addRow("Password", password);
    login->addRow(new QLabel("Your password and session token are not saved."));
    dialog->body->addWidget(Reframe::fold("OpenSubtitles Account", account, dialog));
    auto results = new QListWidget;
    results->setSelectionMode(QAbstractItemView::SingleSelection);
    dialog->body->addWidget(results, 1);
    auto status = new QLabel;
    status->setTextFormat(Qt::PlainText);
    status->setWordWrap(true);
    dialog->body->addWidget(status);
    auto footer = new QHBoxLayout;
    auto site = new QPushButton("Open website"), cancel = new QPushButton("Cancel"),
         download = new QPushButton("Download and Assign");
    footer->addWidget(site);
    footer->addStretch();
    footer->addWidget(cancel);
    footer->addWidget(download);
    dialog->body->addLayout(footer);
    download->setEnabled(false);
    QObject::connect(site, &QPushButton::clicked, dialog,
                     [] { QDesktopServices::openUrl(QUrl("https://www.opensubtitles.com/")); });
    QObject::connect(cancel, &QPushButton::clicked, dialog, &QDialog::close);
    dialog->beforeClose = [dialog]
    {
        for (auto process : dialog->findChildren<QProcess *>("subtitleRequest"))
            if (process->state() != QProcess::NotRunning)
                process->kill();
    };
    QObject::connect(
        search, &QPushButton::clicked, dialog,
        [dialog, connection, query, language, search, results, status, username, password, download]
        {
            const QString key = connection->value("key").toString();
            if (key.isEmpty())
            {
                status->setText("Online subtitle search is unavailable in this build. Use Open website "
                                "to download files manually.");
                return;
            }
            if (!username->text().isEmpty() && password->text().isEmpty() &&
                connection->value("token").toString().isEmpty())
            {
                status->setText("Enter your password to sign in, or clear Username to search anonymously.");
                return;
            }
            search->setEnabled(false);
            download->setEnabled(false);
            results->clear();
            status->setText("Searching…");
            auto find = [dialog, connection, query, language, search, results, status, download]
            {
                QUrl url(connection->value("base").toString() + "/subtitles");
                QUrlQuery parameters;
                parameters.addQueryItem("query", query->text().trimmed());
                parameters.addQueryItem("languages", language->text().trimmed());
                url.setQuery(parameters);
                request(
                    dialog, url, connection->value("key").toString(), connection->value("token").toString(),
                    QJsonObject(),
                    [search, results, status, download](QByteArray bytes, QString error)
                    {
                        search->setEnabled(true);
                        if (!error.isEmpty())
                        {
                            status->setText(error);
                            return;
                        }
                        const auto data = QJsonDocument::fromJson(bytes).object().value("data").toArray();
                        bool truncated = false;
                        for (const auto entry : data)
                        {
                            if (results->count() >= 500)
                            {
                                truncated = true;
                                break;
                            }
                            const auto attributes = entry.toObject().value("attributes").toObject();
                            for (const auto file : attributes.value("files").toArray())
                            {
                                if (results->count() >= 500)
                                {
                                    truncated = true;
                                    break;
                                }
                                const auto value = file.toObject();
                                auto item = new QListWidgetItem(
                                    attributes.value("language").toString().left(32) + "  " +
                                        attributes.value("release").toString().left(1024) + "  " +
                                        value.value("file_name").toString().left(1024),
                                    results);
                                item->setData(Qt::UserRole, value.value("file_id").toInt());
                            }
                        }
                        status->setText(
                            truncated
                                ? "Showing the first 500 files. Refine the search for more specific results."
                                : QString("%1 subtitle files found").arg(results->count()));
                        if (results->count())
                            results->setCurrentRow(0);
                        download->setEnabled(results->count() > 0);
                    });
            };
            if (username->text().isEmpty() || !connection->value("token").toString().isEmpty())
                find();
            else
                request(dialog, QUrl("https://api.opensubtitles.com/api/v1/login"), key, QString(),
                        QJsonObject({{"username", username->text()}, {"password", password->text()}}),
                        [connection, password, search, status, find](QByteArray bytes, QString error)
                        {
                            password->clear();
                            const auto data = QJsonDocument::fromJson(bytes).object();
                            if (!error.isEmpty() || data.value("token").toString().isEmpty())
                            {
                                search->setEnabled(true);
                                status->setText(error.isEmpty() ? "Sign-in did not return a session."
                                                                : error);
                                return;
                            }
                            (*connection)["token"] = data.value("token").toString();
                            const QString host = data.value("base_url").toString();
                            if (host == "api.opensubtitles.com" || host == "vip-api.opensubtitles.com")
                                (*connection)["base"] = QString("https://" + host + "/api/v1");
                            find();
                        });
        });
    QObject::connect(
        download, &QPushButton::clicked, dialog,
        [this, dialog, connection, results, status, download, search, bindings]
        {
            if (!results->currentItem() || bindings->currentRow() < 0)
                return;
            const int target = bindings->currentRow();
            const int fileId = results->currentItem()->data(Qt::UserRole).toInt();
            download->setEnabled(false);
            search->setEnabled(false);
            status->setText("Requesting download…");
            request(dialog, QUrl(connection->value("base").toString() + "/download"),
                    connection->value("key").toString(), connection->value("token").toString(),
                    QJsonObject({{"file_id", fileId}}),
                    [this, dialog, bindings, target, fileId, status, download, search](QByteArray bytes,
                                                                                       QString error)
                    {
                        const auto data = QJsonDocument::fromJson(bytes).object();
                        const QUrl url(data.value("link").toString());
                        if (!error.isEmpty() || !url.isValid() || url.isEmpty())
                        {
                            download->setEnabled(true);
                            search->setEnabled(true);
                            status->setText(error.isEmpty() ? "No download link returned." : error);
                            return;
                        }
                        // Never forward the app key or session to a download host.
                        request(dialog, url, QString(), QString(), QJsonObject(),
                                [this, dialog, bindings, target, fileId, status, download,
                                 search](QByteArray content, QString error)
                                {
                                    download->setEnabled(true);
                                    search->setEnabled(true);
                                    if (!error.isEmpty())
                                    {
                                        status->setText(error);
                                        return;
                                    }
                                    const QString directory = cacheDirectory + "/subtitles";
                                    QDir().mkpath(directory);
                                    const QString path =
                                        directory + "/opensubtitles-" + QString::number(fileId) + ".srt";
                                    QFile file(path);
                                    if (!file.open(QIODevice::WriteOnly) ||
                                        file.write(content) != content.size())
                                    {
                                        status->setText("The subtitle file could not be saved.");
                                        return;
                                    }
                                    file.close();
                                    if (target >= bindings->rowCount())
                                        return;
                                    auto button = static_cast<QPushButton *>(bindings->cellWidget(target, 1));
                                    auto tracks = button->property("tracks").toStringList();
                                    if (!tracks.contains(path))
                                        tracks << path;
                                    button->setProperty("tracks", tracks);
                                    button->setText(QString::number(tracks.size()) + " track(s)");
                                    button->setToolTip(tracks.join('\n'));
                                    dialog->close();
                                });
                    });
        });
    Reframe::reveal(dialog);
}
