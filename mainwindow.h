/**
    This file is part of Slacken.

    Slacken is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Slacken is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Slacken.  If not, see <http://www.gnu.org/licenses/>.
**/

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class SlackClient;
class SlackChannel;
class SlackMessage;

class QSystemTrayIcon;
class QTextCursor;
class QTreeWidgetItem;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void channelHistoryAvailable(const QList<SlackMessage> &messages);
    void newMessageArrived(const QString &channel, const SlackMessage &msg);
    void desktopNotificationArrived(const QString &title, const QString &subtitle, const QString &msg);

    void on_channelTreeWidget_itemClicked(QTreeWidgetItem *item);

    void on_newMessage_returnPressed();

    void on_actionQuit_triggered();

    void on_actionLogin_triggered();

    void on_historyView_anchorClicked(const QUrl &arg1);

    void on_splitter_splitterMoved(int pos, int index);

    void on_historyView_highlighted(const QString &arg1);

private:
    void showChannelInTree(SlackChannel *channel);

    void renderText(QTextCursor &cursor, const QString &text);
    void renderMessage(QTextCursor &cursor, const SlackMessage &message);

    Ui::MainWindow *ui;
    SlackChannel *currentChannel;
    SlackClient *client; // Todo : drop this for multi-team...
    QSystemTrayIcon *tray;
    QList<QTreeWidgetItem*> clients;
};

#endif // MAINWINDOW_H
