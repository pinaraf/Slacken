#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "slackclient.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    client = new SlackClient(this);
    connect(client, &SlackClient::authenticated, [this]() {
        ui->pushButton_2->setEnabled(true);
        ui->pushButton_3->setEnabled(true);
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_pushButton_clicked()
{
    client->login();
}

void MainWindow::on_pushButton_2_clicked()
{
    client->fire();
}

void MainWindow::on_pushButton_3_clicked()
{
    for (SlackConversation &conv: client->conversations()) {
        qDebug() << conv.id << "\t" << conv.name;
    }
}
