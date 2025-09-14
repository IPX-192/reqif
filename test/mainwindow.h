#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeWidget>
#include <QTextBrowser>
#include "ReqifParser.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onLoadFile();
    void onReqItemClicked(QTreeWidgetItem *item, int column);

private:
    Ui::MainWindow *ui;
    QTreeWidget *m_treeWidget;       // 用于显示需求树
    QTextBrowser *m_descBrowser;     // 用于显示需求详情
    ReqifParser m_parser;            // ReqIF文件解析器

    void initUI();                   // 初始化用户界面
};
#endif // MAINWINDOW_H
