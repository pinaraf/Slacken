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

void MainWindow::channelHistoryAvailable(const QList<SlackMessage> &messages)
{
    qDebug() << "Received !";
    ui->historyView->clear();
    auto cursor = ui->historyView->textCursor();
    cursor.movePosition(QTextCursor::End);
    for (const SlackMessage &message: messages) {
        QString userDisplay = "  <?>  ";
        if (!message.user.isEmpty()) {
            auto user = client->user(message.user);
            userDisplay = QString("  <%1>  ").arg(user.name);
        }
        cursor.insertText(message.when.toLocalTime().toString());
        cursor.insertText(userDisplay);
        cursor.insertText(message.text);
        cursor.insertBlock();
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
        QString userDisplay = "  <?>  ";
        if (!message.user.isEmpty()) {
            auto user = client->user(message.user);
            userDisplay = QString("  <%1>  ").arg(user.name);
        }
        cursor.insertText(message.when.toLocalTime().toString());
        cursor.insertText(userDisplay);
        cursor.insertText(message.text);
        cursor.insertBlock();
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
