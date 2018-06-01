#include "mainwindow.h"
#include <QApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setOrganizationName("slacken");
    a.setApplicationName("slacken");
    a.setApplicationVersion("0.1-alpha");
    a.setQuitOnLastWindowClosed(false);
    MainWindow w;
    w.show();

    return a.exec();
}
