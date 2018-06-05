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
    SlackClient *client;
    QSystemTrayIcon *tray;
    QList<QTreeWidgetItem*> clients;
};

#endif // MAINWINDOW_H
