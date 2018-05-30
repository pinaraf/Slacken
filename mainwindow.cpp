#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "slackclient.h"

#include <QSystemTrayIcon>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    tray = new QSystemTrayIcon(this);
    tray->show();
    client = new SlackClient(this);
    connect(client, &SlackClient::authenticated, [this]() {
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
    ui->newMessage->setFocus();
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
    cursor.insertText(message.text);
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
    auto cursor = ui->historyView->textCursor();
    cursor.movePosition(QTextCursor::End);
    for (const SlackMessage &message: messages) {
        renderMessage(cursor, message);
        cursor.movePosition(QTextCursor::NextBlock);
    }
}

void MainWindow::on_newMessage_returnPressed()
{
    qDebug() << "Sending " << ui->newMessage->text() << " to " << currentChannel;
    client->sendMessage(currentChannel, ui->newMessage->text());
    ui->newMessage->clear();
}

void MainWindow::newMessageArrived(const QString &channel, const SlackMessage &message)
{
    qDebug() << "Received message on " << channel << " while on " << currentChannel;
    if (channel == currentChannel) {
        auto cursor = ui->historyView->textCursor();
        cursor.movePosition(QTextCursor::Start);
        renderMessage(cursor, message);
        cursor.movePosition(QTextCursor::NextBlock);
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
    client->login();
}
