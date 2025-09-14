#include "MainWindow.h"
#include "ui_MainWindow.h"
#include <QMenuBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QSplitter>
#include <QFont>
#include <QStatusBar>
#include <QToolBar>
#include <QAction>

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
    m_treeWidget->setHeaderLabels(QStringList() << u8"序号" << u8"需求名称");
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

    // 创建工具栏
    QToolBar *toolBar = addToolBar(u8"功能");
    toolBar->setMovable(false);

    // 文件菜单
    QMenu *fileMenu = menuBar()->addMenu(u8"文件");
    QAction *loadAction = fileMenu->addAction(u8"加载.reqif文件");
    loadAction->setShortcut(QKeySequence::Open);
    connect(loadAction, &QAction::triggered, this, &MainWindow::onLoadFile);

    // 过滤菜单
    QMenu *filterMenu = menuBar()->addMenu(u8"过滤");
    QAction *techReqAction = filterMenu->addAction(u8"显示技术要求");
    connect(techReqAction, &QAction::triggered, this, &MainWindow::onShowTechnicalRequirements);

    // 工具栏按钮
    QAction *showAllAction = toolBar->addAction(u8"显示全部");
    QAction *techFilterAction = toolBar->addAction(u8"技术要求");

    connect(showAllAction, &QAction::triggered, [this]() {
        m_parser.fillTree(m_treeWidget);
        statusBar()->showMessage(u8"显示所有需求", 3000);
    });

    connect(techFilterAction, &QAction::triggered, this, &MainWindow::onShowTechnicalRequirements);
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
        int totalCount = m_parser.getAllReqCount();
        int validCount = m_parser.getValidReqCount();

        QString message = QString(u8"加载完成，共解析 %1 条需求，其中有效需求 %2 条")
                         .arg(totalCount).arg(validCount);
        statusBar()->showMessage(message, 5000);

        if (validCount == 0) {
            QMessageBox::warning(this, u8"警告",
                                 u8"文件加载成功，但没有找到有效需求。\n"
                                 u8"可能原因：\n"
                                 u8"1. 所有需求都是未命名需求\n"
                                 u8"2. 属性映射不匹配\n"
                                 u8"3. 命名空间配置问题");
        }
    } else {
        statusBar()->showMessage(u8"文件解析失败", 5000);
        QMessageBox::critical(this, u8"失败", u8"文件解析失败，请检查文件格式");
    }
}

void MainWindow::onShowTechnicalRequirements() {
    if (m_parser.getAllReqCount() == 0) {
        QMessageBox::information(this, u8"提示", u8"请先加载ReqIF文件");
        return;
    }

    // 过滤显示技术要求相关的内容
    m_parser.fillTreeWithFilter(m_treeWidget, u8"技术");

    int visibleCount = m_treeWidget->topLevelItemCount();
    if (visibleCount > 0) {
        statusBar()->showMessage(QString(u8"显示 %1 条技术要求相关需求").arg(visibleCount), 3000);
    } else {
        statusBar()->showMessage(u8"未找到技术要求相关需求", 3000);
    }
}

void MainWindow::onReqItemClicked(QTreeWidgetItem *item, int column) {
    Q_UNUSED(column);
    if (!item) return;
    QString reqId = item->data(0, Qt::UserRole).toString();
    QString description = m_parser.getReqDescription(reqId);
    m_descBrowser->setPlainText(description);
}
