#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeWidget>
#include <QTextBrowser>
#include "ReqifParser.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onLoadFile();                       // 加载文件
    void onReqItemClicked(QTreeWidgetItem *item, int column); // 点击需求项
    void onShowTechnicalRequirements();      // 显示技术要求

private:
    void initUI();                           // 初始化界面

private:
    Ui::MainWindow *ui;
    QTreeWidget *m_treeWidget;               // 需求树
    QTextBrowser *m_descBrowser;             // 描述浏览器
    ReqifParser m_parser;                    // 解析器
};

#endif // MAINWINDOW_H
