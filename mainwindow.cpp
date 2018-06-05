#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "slackclient.h"

#include <QSystemTrayIcon>
#include <QRegularExpression>
#include <QMessageBox>
#include <QDesktopServices>
#include <QTreeWidgetItem>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    QSettings settings;
    settings.beginGroup("layout");
    if (settings.contains("splitter"))
        ui->splitter->restoreState(settings.value("splitter").toByteArray());
    tray = new QSystemTrayIcon(this);
    tray->setIcon(this->windowIcon());
    tray->show();
    connect(tray, &QSystemTrayIcon::activated, [this](QSystemTrayIcon::ActivationReason) {
        this->show();
    });
    client = new SlackClient(this);

    clients.append(new QTreeWidgetItem(ui->channelTreeWidget));
    clients[0]->setText(0, "Slack");

    connect(client, &SlackClient::authenticated, [this]() {
        QSettings settings;
        settings.beginGroup("auth");
        settings.setValue("token", client->currentToken());
        settings.sync();
        client->start();
    });
    connect(client, &SlackClient::channelAdded, [this](SlackChannel *chan) {
        if (chan->is_member) {
            this->showChannelInTree(chan);
        }
    });
    connect(client, &SlackClient::channelJoined, [this](SlackChannel *chan) {
        this->showChannelInTree(chan);
    });
    connect(client, &SlackClient::channelHistory, this, &MainWindow::channelHistoryAvailable);
    connect(client, &SlackClient::newMessage, this, &MainWindow::newMessageArrived);
    connect(client, &SlackClient::desktopNotification, this, &MainWindow::desktopNotificationArrived);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::showChannelInTree(SlackChannel *chan)
{
    QString display = chan->name;
    if (chan->is_channel)
        display = QString("#%1").arg(chan->name);
    else if (chan->is_group)
        display = QString("&%1").arg(chan->name);

    auto item = new QTreeWidgetItem(clients[0]);
    item->setText(0, display);
    item->setData(0, Qt::UserRole + 42, QVariant::fromValue(chan));
    if (chan->has_unread) {
        auto font = item->font(0);
        font.setWeight(QFont::Bold);
        item->setFont(0, font);
    }
    connect(chan, &SlackChannel::unreadChanged, [item, chan]() {
        if (chan->has_unread) {
            auto font = item->font(0);
            font.setWeight(QFont::Bold);
            item->setFont(0, font);
        } else {
            auto font = item->font(0);
            font.setWeight(QFont::Normal);
            item->setFont(0, font);
        }
    });
    clients[0]->addChild(item);
    clients[0]->sortChildren(0, Qt::AscendingOrder);
}

void MainWindow::on_channelTreeWidget_itemClicked(QTreeWidgetItem *item)
{
    currentChannel = item->data(0, Qt::UserRole + 42).value<SlackChannel*>();
    qDebug() << "Switching to " << currentChannel->id;
    client->requestHistory(currentChannel->id);
    item->setTextColor(0, QPalette().windowText().color());
    ui->topicViewer->clear();

    auto cursor = ui->topicViewer->textCursor();
    renderText(cursor, currentChannel->topic);

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
            if (client->hasUser(userId)) {
                auto charFormat = cursor.charFormat();
                charFormat.setFontWeight(QFont::Bold);
                cursor.setCharFormat(charFormat);
                auto &user = client->user(userId);
                cursor.insertText("@");
                cursor.insertText(user.name);
            } else {
                cursor.insertText("@");
                cursor.insertText(userId);
            }
        } else if (capturedPart.startsWith("http")) {
            // This is a link !
            auto charFormat = cursor.charFormat();
            charFormat.setForeground(QBrush(QColor(0, 0, 255)));
            charFormat.setAnchorHref(capturedPart.mid(0, capturedPart.indexOf('|')).toString());
            cursor.setCharFormat(charFormat);

            if (capturedPart.contains('|')) {
                cursor.insertText(capturedPart.mid(capturedPart.indexOf('|') + 1).toString());
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
        if (client->hasUser(message.user)) {
            auto user = client->user(message.user);
            userDisplay = userDisplay.arg(user.name);
        }
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
    bool crossedMark = !currentChannel->last_read.isValid();
    QTextBlockFormat baseFmt;
    QString lastReadMarker("last-read-marker");
    QTextCursor lastReadPosition;
    for (const SlackMessage &message: sorted_messages) {
        if (!crossedMark && (message.when > currentChannel->last_read)) {
            qDebug() << "INSERTING HTML " << crossedMark << message.when << currentChannel->last_read;
            baseFmt = cursor.blockFormat();

            cursor.movePosition(QTextCursor::PreviousBlock);
            QTextBlockFormat newFmt;
            newFmt.setBottomMargin(1);
            //newFmt.setTopMargin(1);
            newFmt.setProperty(QTextFormat::BlockTrailingHorizontalRulerWidth, QTextLength(QTextLength::PercentageLength, 100));
            cursor.mergeBlockFormat(newFmt);
            QTextCharFormat newCharFmt;
            newCharFmt.setProperty(QTextFormat::AnchorName, lastReadMarker);
            newCharFmt.setProperty(QTextFormat::IsAnchor, true);
            cursor.mergeCharFormat(newCharFmt);
            lastReadPosition = cursor;
            cursor.movePosition(QTextCursor::NextBlock);
            cursor.setBlockFormat(baseFmt);
        }
        renderMessage(cursor, message);
        cursor.movePosition(QTextCursor::NextBlock);
        if (!crossedMark && (message.when > currentChannel->last_read)) {
            crossedMark = true;
            //cursor.setBlockFormat(baseFmt);
        }
    }

    if (!crossedMark) {
        // Always set the last-read-marker.
        QTextCharFormat newFmt;
        newFmt.setProperty(QTextFormat::AnchorName, lastReadMarker);
        newFmt.setProperty(QTextFormat::IsAnchor, true);
        lastReadPosition = cursor;
        cursor.mergeCharFormat(newFmt);
    }

    currentChannel->markRead(sorted_messages.last());

    //ui->historyView->setTextCursor(cursor);
    /*QTimer::singleShot(100, [this]() {
        ui->historyView->scrollToAnchor("last-read-marker");
    });
    ui->historyView->scrollToAnchor(lastReadMarker);*/
    ui->historyView->setTextCursor(lastReadPosition);
    ui->historyView->ensureCursorVisible();
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
    client->sendMessage(currentChannel->id, msg);
    ui->newMessage->clear();
}

void MainWindow::desktopNotificationArrived(const QString &title, const QString &subtitle, const QString &msg)
{
    tray->showMessage(title + " / " + subtitle, msg);
}

void MainWindow::newMessageArrived(const QString &channel, const SlackMessage &message)
{
    qDebug() << "Received message on " << channel << " while on " << currentChannel->id;
    if (channel == currentChannel->id) {
        auto cursor = ui->historyView->textCursor();
        cursor.movePosition(QTextCursor::End);
        renderMessage(cursor, message);
        cursor.movePosition(QTextCursor::NextBlock);
        ui->historyView->setTextCursor(cursor);
        // TODO : some focus jumble mumble : if window is not focused, don't mark read, wait until it is really seen/acted upon
        currentChannel->markRead(message);
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

void MainWindow::on_splitter_splitterMoved(int, int)
{
    QSettings settings;
    settings.beginGroup("layout");
    if (!settings.contains("splitter") || settings.value("splitter").toByteArray() != ui->splitter->saveState()) {
        settings.setValue("splitter", ui->splitter->saveState());
        // That's ugly
        settings.sync();
    }
}

void MainWindow::on_historyView_highlighted(const QString &arg1)
{
    ui->statusBar->showMessage(arg1, 1000);
}
