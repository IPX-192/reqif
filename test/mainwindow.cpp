#include "MainWindow.h"
#include "ui_MainWindow.h"
#include <QMenuBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QSplitter>
#include <QFont>
#include <QStatusBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow) {
    ui->setupUi(this);
    setWindowTitle(u8"ReqIF需求查看器");
    resize(1000, 600);
    initUI();
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::initUI() {
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

    // 左侧需求树
    m_treeWidget = new QTreeWidget(splitter);
    m_treeWidget->setMinimumWidth(300);
    m_treeWidget->setHeaderLabel(u8"需求结构");
    QFont treeFont = m_treeWidget->font();
    treeFont.setPointSize(10);
    m_treeWidget->setFont(treeFont);
    m_treeWidget->setAlternatingRowColors(true);

    // 右侧描述框
    m_descBrowser = new QTextBrowser(splitter);
    m_descBrowser->setMinimumWidth(600);
    QFont descFont = m_descBrowser->font();
    descFont.setPointSize(10);
    m_descBrowser->setFont(descFont);
    m_descBrowser->setStyleSheet("background-color: #f8f8f8; padding: 15px;");
    m_descBrowser->setPlaceholderText(u8"点击左侧需求节点查看描述");

    splitter->setSizes({300, 700});
    setCentralWidget(splitter);

    // 菜单
    QMenu *fileMenu = menuBar()->addMenu(u8"文件");
    QAction *loadAction = fileMenu->addAction(u8"加载.reqif文件");
    loadAction->setShortcut(QKeySequence::Open);
    connect(loadAction, &QAction::triggered, this, &MainWindow::onLoadFile);

    connect(m_treeWidget, &QTreeWidget::itemClicked, this, &MainWindow::onReqItemClicked);

    statusBar()->showMessage(u8"就绪");
}

void MainWindow::onLoadFile() {
    QString filePath = QFileDialog::getOpenFileName(
        this, u8"选择ReqIF文件", "",
        u8"ReqIF文件 (*.reqif);;所有文件 (*.*)"
    );
    if (filePath.isEmpty()) return;

    statusBar()->showMessage(u8"正在解析文件...");

    if (m_parser.load(filePath)) {
        m_parser.fillTree(m_treeWidget);
        int count = m_parser.getValidReqCount();
        QString message = QString(u8"加载完成，共找到 %1 条需求").arg(count);
        statusBar()->showMessage(message, 5000);

        if (count == 0) {
            QMessageBox::warning(this, u8"警告",
                                 u8"文件加载成功，但没有解析到任何需求。\n"
                                 u8"请检查属性映射或命名空间是否匹配。");
        } else {
            QMessageBox::information(this, u8"成功", message);
        }
    } else {
        statusBar()->showMessage(u8"文件解析失败", 5000);
        QMessageBox::critical(this, u8"失败", u8"文件解析失败，请检查文件格式");
    }
}


void MainWindow::onReqItemClicked(QTreeWidgetItem *item, int column) {
    Q_UNUSED(column);
    if (!item) return;
    QString reqId = item->data(0, Qt::UserRole).toString();
    QString description = m_parser.getReqDescription(reqId);
    m_descBrowser->setPlainText(description);
}
