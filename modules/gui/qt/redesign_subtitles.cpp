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

namespace
{
using Response = std::function<void(QByteArray, QString)>;
QString quoted(QString text)
{
    return "\"" + text.replace('\\', "\\\\").replace('"', "\\\"").replace('\r', "").replace('\n', "\\n") +
           "\"";
}
void request(QWidget *owner, const QUrl &url, const QString &key, const QString &token,
             const QJsonObject &body, Response done)
{
    if (url.scheme() != "https")
    {
        done(QByteArray(), "The service returned a non-HTTPS address.");
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
    auto process = new QProcess(owner);
    process->setObjectName("subtitleRequest");
    auto complete = std::make_shared<bool>(false);
    QObject::connect(process, &QProcess::started, owner,
                     [process, config]
                     {
                         process->write(config.toUtf8());
                         process->closeWriteChannel();
                     });
    QObject::connect(process, &QProcess::errorOccurred, owner,
                     [process, complete, done](QProcess::ProcessError error)
                     {
                         if (error != QProcess::FailedToStart || *complete)
                             return;
                         *complete = true;
                         done(QByteArray(),
                              "Windows curl.exe could not start. Check that curl is installed.");
                         process->deleteLater();
                     });
    QObject::connect(
        process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), owner,
        [process, complete, done](int exit, QProcess::ExitStatus state)
        {
            if (*complete)
                return;
            *complete = true;
            QByteArray bytes = process->readAllStandardOutput();
            const int separator = bytes.lastIndexOf('\n');
            const int http = bytes.mid(separator + 1).trimmed().toInt();
            bytes.truncate(qMax(0, separator));
            QString error;
            if (exit || state != QProcess::NormalExit)
                error = "The connection failed or timed out. Try again.";
            else if (http < 200 || http >= 300)
            {
                const auto message = QJsonDocument::fromJson(bytes).object().value("message").toString();
                error = message.isEmpty() ? QString("OpenSubtitles returned HTTP %1.").arg(http) : message;
            }
            done(bytes, error);
            process->deleteLater();
        });
    // Credentials travel through stdin, never the command line or a shell.
    process->start("curl.exe", QStringList({"--silent", "--show-error", "--location", "--proto", "=https",
                                            "--proto-redir", "=https", "--max-time", "30", "--max-filesize",
                                            "10485760", "--user-agent", "Reframe v0.2.0", "--write-out",
                                            "\n%{http_code}", "--config", "-"}));
}
} // namespace

void RedesignShell::openSubtitles(QWidget *owner, QTableWidget *bindings)
{
    auto dialog = new Reframe::Dialog(owner, "Find Subtitles", QSize(1048, 800));
    auto connection = std::make_shared<QVariantMap>();
    QFile packaged(QCoreApplication::applicationDirPath() + "/opensubtitles-app-key.txt");
    if (packaged.open(QIODevice::ReadOnly))
        (*connection)["key"] = QString::fromUtf8(packaged.readAll()).trimmed();
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
    if (connection->value("key").toString().isEmpty())
    {
        auto setup = new QWidget;
        auto form = new QFormLayout(setup);
        auto note =
            new QLabel("The app publisher must register one key for this app. Users share that app key; "
                       "they do not need to register their own application.");
        note->setWordWrap(true);
        form->addRow(note);
        auto key = new QLineEdit;
        key->setEchoMode(QLineEdit::Password);
        form->addRow("App publisher key", key);
        auto registration = new QPushButton("Register the app");
        form->addRow(registration);
        QObject::connect(registration, &QPushButton::clicked, dialog,
                         [] { QDesktopServices::openUrl(QUrl("https://www.opensubtitles.com/consumers")); });
        QObject::connect(key, &QLineEdit::editingFinished, dialog,
                         [this, key, connection]
                         {
                             (*connection)["key"] = key->text().trimmed();
                             getSettings()->setValue("OpenSubtitles/applicationKey", key->text().trimmed());
                         });
        dialog->body->addWidget(Reframe::fold("App Publisher Setup", setup, dialog, true));
    }
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
                status->setText("The app publisher has not configured OpenSubtitles access. Use Open website "
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
                request(dialog, url, connection->value("key").toString(),
                        connection->value("token").toString(), QJsonObject(),
                        [search, results, status, download](QByteArray bytes, QString error)
                        {
                            search->setEnabled(true);
                            if (!error.isEmpty())
                            {
                                status->setText(error);
                                return;
                            }
                            const auto data = QJsonDocument::fromJson(bytes).object().value("data").toArray();
                            for (const auto entry : data)
                            {
                                const auto attributes = entry.toObject().value("attributes").toObject();
                                for (const auto file : attributes.value("files").toArray())
                                {
                                    const auto value = file.toObject();
                                    auto item =
                                        new QListWidgetItem(attributes.value("language").toString() + "  " +
                                                                attributes.value("release").toString() +
                                                                "  " + value.value("file_name").toString(),
                                                            results);
                                    item->setData(Qt::UserRole, value.value("file_id").toInt());
                                }
                            }
                            status->setText(QString("%1 subtitle files found").arg(results->count()));
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
