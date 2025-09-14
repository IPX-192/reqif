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
    void onLoadFile();                  // 加载ReqIF文件
    void onReqItemClicked(QTreeWidgetItem *item, int column); // 需求节点点击事件

private:
    void initUI();                      // 初始化界面

private:
    Ui::MainWindow *ui;
    QTreeWidget *m_treeWidget;          // 左侧需求树
    QTextBrowser *m_descBrowser;        // 右侧描述框
    ReqifParser m_parser;               // 需求解析器实例
    QString m_lastLoadedPath;           // 上次加载的文件路径（避免重复加载）
};
#endif // MAINWINDOW_H
