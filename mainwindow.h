#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class SlackClient;
class SlackMessage;

class QListWidgetItem;
class QSystemTrayIcon;
class QTextCursor;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void channelHistoryAvailable(const QList<SlackMessage> &messages);
    void newMessageArrived(const QString &channel, const SlackMessage &msg);
    void desktopNotificationArrived(const QString &title, const QString &subtitle, const QString &msg);

    void on_channelListWidget_itemClicked(QListWidgetItem *item);


    void on_newMessage_returnPressed();

    void on_actionQuit_triggered();

    void on_actionLogin_triggered();

    void on_historyView_anchorClicked(const QUrl &arg1);

private:
    void renderText(QTextCursor &cursor, const QString &text);
    void renderMessage(QTextCursor &cursor, const SlackMessage &message);

    Ui::MainWindow *ui;
    SlackClient *client;
    QString currentChannel;
    QSystemTrayIcon *tray;
};

#endif // MAINWINDOW_H
