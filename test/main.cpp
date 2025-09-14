#include "MainWindow.h"
#include <QApplication>
#include <QTextCodec>

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    // 强制UTF-8编码，避免中文乱码
    QTextCodec::setCodecForLocale(QTextCodec::codecForName(u8"UTF-8"));

    MainWindow w;
    w.show();

    return a.exec();
}
