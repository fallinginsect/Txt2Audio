#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{

    QApplication a(argc, argv);
    MainWindow w;
    w.setWindowTitle("TXT转MP3工具 v0.1");
    w.show();
    return QCoreApplication::exec();
}
