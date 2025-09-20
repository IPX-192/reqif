#include "MainWindow.h"
#include <QApplication>
#include <QTextCodec>


bool AppstreServer::SaveFile(const QString& strDIr, const& strFileName, const QBayteArray& data)
{
    if(!QFileKit::createDir(strDIr))
    {
        return false;
    }

    //创建文件
    QString strFilePath = QString("%1/%2").arg(StrDir).arg(strFileName);
    QFileKit::createFile(strFilePath);
    QFile file(strFilePath)
    if(!file.open(QIODevice::WriteOnly))
    {
        return false;
    }
    file.write(QByteArray::formBase64(data));
    file.close();

}

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    // 强制UTF-8编码，避免中文乱码
    QTextCodec::setCodecForLocale(QTextCodec::codecForName(u8"UTF-8"));

    MainWindow w;
    w.show();

    return a.exec();
}
