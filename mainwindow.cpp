#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "slackclient.h"

#include <QSystemTrayIcon>
#include <QRegularExpression>
#include <QMessageBox>
#include <QDesktopServices>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    tray = new QSystemTrayIcon(this);
    tray->setIcon(this->windowIcon());
    tray->show();
    connect(tray, &QSystemTrayIcon::activated, [this](QSystemTrayIcon::ActivationReason) {
        this->show();
    });
    client = new SlackClient(this);
    connect(client, &SlackClient::authenticated, [this]() {
        QSettings settings;
        settings.beginGroup("auth");
        settings.setValue("token", client->currentToken());
        settings.sync();
        client->fire();
    });
    connect(client, &SlackClient::channelAdded, [this](const SlackChannel &chan) {
        if (chan.is_member) {
            QString display = chan.name;
            if (chan.is_channel)
                display = QString("#%1").arg(chan.name);
            else if (chan.is_group)
                display = QString("&%1").arg(chan.name);
            qDebug() << display << chan.is_im << chan.is_channel << chan.is_group;
            auto item = new QListWidgetItem(display, ui->channelListWidget);
            item->setData(Qt::UserRole + 42, chan.id);
            ui->channelListWidget->addItem(item);
            ui->channelListWidget->sortItems();
        }
    });
    connect(client, &SlackClient::channelHistory, this, &MainWindow::channelHistoryAvailable);
    connect(client, &SlackClient::newMessage, this, &MainWindow::newMessageArrived);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_channelListWidget_itemClicked(QListWidgetItem *item)
{
    currentChannel = item->data(Qt::UserRole + 42).toString();
    qDebug() << "Switching to " << currentChannel;
    client->requestHistory(currentChannel);
    item->setTextColor(QPalette().windowText().color());
    ui->topicViewer->clear();
    auto chanIt = client->channels().find(currentChannel);
    if (chanIt != client->channels().end()) {
        auto cursor = ui->topicViewer->textCursor();
        renderText(cursor, chanIt->second.topic);
    }
    ui->newMessage->setFocus();
}

void MainWindow::renderText(QTextCursor &cursor, const QString &text)
{
    QString output = text;

    QRegularExpression formatDelimiter("(?:\\W|^)(<([^>]+)>)(?:\\W|$)");
    int currentIdx = 0;
    int pos;
    QRegularExpressionMatch match;
    while ((pos = output.indexOf(formatDelimiter, currentIdx, &match)) != -1) {
        pos = match.capturedStart(1);
        cursor.insertText(text.mid(currentIdx, pos - currentIdx));
        auto backupCharFormat = cursor.charFormat();

        QStringRef capturedPart = match.capturedRef(2);

        if (capturedPart.startsWith('@')) {
            QString userId = capturedPart.mid(1).toString();
            auto charFormat = cursor.charFormat();
            charFormat.setFontWeight(QFont::Bold);
            cursor.setCharFormat(charFormat);
            auto &user = client->user(userId);
            cursor.insertText("@");
            cursor.insertText(user.name);
        } else if (capturedPart.startsWith("http")) {
            // This is a link !
            auto charFormat = cursor.charFormat();
            charFormat.setForeground(QBrush(QColor(0, 0, 255)));
            charFormat.setAnchorHref(capturedPart.mid(0, capturedPart.indexOf('|')).toString());
            cursor.setCharFormat(charFormat);

            if (capturedPart.contains('|')) {
                cursor.insertText(capturedPart.mid(capturedPart.indexOf('|')).toString());
            } else {
                cursor.insertText(capturedPart.toString());
            }
        } else {
            // We don't know
            cursor.insertText(capturedPart.toString());
        }

        cursor.setCharFormat(backupCharFormat);

        currentIdx = pos + 1 + match.capturedLength(1) - 1;
    }
    cursor.insertText(text.mid(currentIdx));
}

void MainWindow::renderMessage(QTextCursor &cursor, const SlackMessage &message)
{
    QString userDisplay = "\t<%1>\t";
    if (!message.user.isEmpty()) {
        auto user = client->user(message.user);
        userDisplay = userDisplay.arg(user.name);
    } else if (!message.username.isEmpty()) {
        userDisplay = userDisplay.arg(message.username);
    }
    cursor.insertText(message.when.toLocalTime().toString());
    cursor.insertText(userDisplay);
    renderText(cursor, message.text);
    //cursor.insertText(message.text);
    for (auto &attachment: message.attachments) {
        QTextCharFormat fmt = cursor.charFormat();
        QBrush foreground = fmt.foreground();
        if (attachment.color.isValid()) {
            fmt.setForeground(attachment.color);
            cursor.setCharFormat(fmt);
        }
        if (!attachment.title.isEmpty()) {
            qDebug() << "TITLE";
            cursor.insertText(attachment.title);
            if (!attachment.text.isEmpty()) {
                cursor.insertBlock();
            }
            cursor.insertText("\t\t");
        }
        cursor.insertText(attachment.text);
        if (attachment.color.isValid()) {
            fmt.setForeground(foreground);
            cursor.setCharFormat(fmt);
        }
    }
    cursor.insertBlock();
}

void MainWindow::channelHistoryAvailable(const QList<SlackMessage> &messages)
{
    qDebug() << "Received !";
    ui->historyView->clear();
    QList<QTextOption::Tab> tabs;
    tabs << QTextOption::Tab(300, QTextOption::LeftTab);
    tabs << QTextOption::Tab(500, QTextOption::LeftTab);
    ui->historyView->document()->defaultTextOption().setTabs(tabs);
    //ui->historyView->setTabStopDistance(200);
    auto cursor = ui->historyView->textCursor();
    cursor.movePosition(QTextCursor::End);
    QList<SlackMessage> sorted_messages = messages;
    std::sort(sorted_messages.begin(), sorted_messages.end(), [] (const SlackMessage &msgA, const SlackMessage &msgB) {
        return msgA.when < msgB.when;
    });
    for (const SlackMessage &message: sorted_messages) {
        renderMessage(cursor, message);
        cursor.movePosition(QTextCursor::NextBlock);
    }

    ui->historyView->setTextCursor(cursor);
}

void MainWindow::on_newMessage_returnPressed()
{
    qDebug() << "Sending " << ui->newMessage->text() << " to " << currentChannel;
    QString msg = ui->newMessage->text();

    // Trying to match @XXX to users
    QRegularExpression userRegex("(?:\\W|^)(@\\w+)(?:\\W|$)");
    QRegularExpressionMatch match;
    int offset = 0;
    while ((match = userRegex.match(msg, offset)).hasMatch()) {
        //qDebug() << match.capturedTexts();
        QString id = client->userId(match.captured(1).mid(1));
        if (!id.isNull()) {
            msg = QString("%1<@%2>%3")
                    .arg(msg.mid(0, match.capturedStart(1)))
                    .arg(id)
                    .arg(msg.mid(match.capturedEnd(1)));
            offset = match.capturedStart() + id.length();
        } else {
            offset = match.capturedEnd();
        }
    }
    //qDebug() << msg;
    client->sendMessage(currentChannel, msg);
    ui->newMessage->clear();
}

void MainWindow::newMessageArrived(const QString &channel, const SlackMessage &message)
{
    qDebug() << "Received message on " << channel << " while on " << currentChannel;
    if (channel == currentChannel) {
        auto cursor = ui->historyView->textCursor();
        cursor.movePosition(QTextCursor::End);
        renderMessage(cursor, message);
        cursor.movePosition(QTextCursor::NextBlock);
        ui->historyView->setTextCursor(cursor);
    } else {
        // Find the matching item, and change its color
        for (int i = 0 ; i < ui->channelListWidget->count() ; i++) {
            QListWidgetItem *chanItem = ui->channelListWidget->item(i);
            if (chanItem->data(Qt::UserRole + 42).toString() == channel) {
                chanItem->setTextColor(QColor(255, 0, 0));
                tray->showMessage("New message", QString("Received a new message in %1").arg(chanItem->text()));
            }
        }
    }
}

void MainWindow::on_actionQuit_triggered()
{
    qApp->exit();
}

void MainWindow::on_actionLogin_triggered()
{
    QSettings settings;
    settings.beginGroup("auth");
    if (settings.contains("token")) {
        client->login(settings.value("token").toString());
    } else {
        QMessageBox::information(this, "OAuth authentication", "A new webbrowser window will open for you to login. It will contact Slacken back on the loopback for authentication purpose.\n"
                                                               "Make sure extensions like NoScript won't block that as a XSS attack.");
        client->login(QString());
    }
}

void MainWindow::on_historyView_anchorClicked(const QUrl &url)
{
    QDesktopServices::openUrl(url);
}
