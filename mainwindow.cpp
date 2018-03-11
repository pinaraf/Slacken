#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "slackclient.h"

SlackClient *hack;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_pushButton_clicked()
{
    hack = new SlackClient(this);
}

void MainWindow::on_pushButton_2_clicked()
{
    hack->fire();
}
