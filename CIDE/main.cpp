#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.setWindowTitle("CIDE - Tiny Editor");



    w.resize(1500, 1000);
    w.show();

    return a.exec();
}
