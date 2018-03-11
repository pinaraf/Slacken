#include "mainwindow.h"
#include <QApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    qDebug() << "Coucou !";
    MainWindow w;
    w.show();

    return a.exec();
}
