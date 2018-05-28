#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class SlackClient;
class SlackMessage;

class QListWidgetItem;
class QSystemTrayIcon;

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

    void on_channelListWidget_itemClicked(QListWidgetItem *item);

    void channelHistoryAvailable(const QList<SlackMessage> &messages);

    void on_newMessage_returnPressed();

    void newMessageArrived(const QString &channel, const SlackMessage &msg);
    void on_actionQuit_triggered();

    void on_actionLogin_triggered();

private:
    Ui::MainWindow *ui;
    SlackClient *client;
    QString currentChannel;
    QSystemTrayIcon *tray;
};

#endif // MAINWINDOW_H
