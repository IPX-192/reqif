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
    resize(1000, 600); // 初始窗口大小
    initUI();          // 初始化界面组件
}

MainWindow::~MainWindow() {
    delete ui;
}

// 初始化界面布局和组件
void MainWindow::initUI() {
    // 水平分割器：左侧树 + 右侧描述
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

    // 左侧需求树
    m_treeWidget = new QTreeWidget(splitter);
    m_treeWidget->setMinimumWidth(300);                  // 最小宽度
    m_treeWidget->setHeaderLabels(QStringList() << u8"序号" << u8"需求名称"); // 表头
    QFont treeFont = m_treeWidget->font();
    treeFont.setPointSize(10);                           // 字体大小
    m_treeWidget->setFont(treeFont);
    m_treeWidget->setAlternatingRowColors(true);          // 交替行颜色（提升可读性）
    m_treeWidget->setIndentation(20);                     // 层级缩进（替代原错误的setIndentation）

    // 右侧描述框
    m_descBrowser = new QTextBrowser(splitter);
    m_descBrowser->setMinimumWidth(600);                  // 最小宽度
    QFont descFont = m_descBrowser->font();
    descFont.setPointSize(10);                            // 字体大小
    m_descBrowser->setFont(descFont);
    m_descBrowser->setStyleSheet("background-color: #f8f8f8; padding: 15px;"); // 样式
    m_descBrowser->setPlaceholderText(u8"点击左侧需求节点查看描述"); // 提示文本

    // 设置分割器比例（左侧30%，右侧70%）
    splitter->setSizes({300, 700});
    setCentralWidget(splitter);

    // 菜单栏：文件 -> 加载
    QMenu *fileMenu = menuBar()->addMenu(u8"文件");
    QAction *loadAction = fileMenu->addAction(u8"加载.reqif文件");
    loadAction->setShortcut(QKeySequence::Open); // 快捷键 Ctrl+O
    connect(loadAction, &QAction::triggered, this, &MainWindow::onLoadFile);

    // 连接树节点点击事件：显示对应需求描述
    connect(m_treeWidget, &QTreeWidget::itemClicked, this, &MainWindow::onReqItemClicked);

    // 状态栏初始化
    statusBar()->showMessage(u8"就绪");
}

// 加载ReqIF文件：打开文件选择对话框并解析
void MainWindow::onLoadFile() {
    // 打开文件选择对话框
    QString filePath = QFileDialog::getOpenFileName(
        this, u8"选择ReqIF文件", "",
        u8"ReqIF文件 (*.reqif);;所有文件 (*.*)" // 文件筛选
    );
    if (filePath.isEmpty()) return; // 用户取消选择

    // 避免重复加载同一文件
    if (filePath == m_lastLoadedPath) {
        QMessageBox::information(this, u8"提示", u8"已加载该文件，无需重复加载");
        return;
    }
    m_lastLoadedPath = filePath;

    // 显示解析状态
    statusBar()->showMessage(u8"正在解析文件...");

    // 调用解析器加载文件
    if (m_parser.load(filePath)) {
        // 解析成功：填充树并更新状态
        m_parser.fillTree(m_treeWidget);
        int totalCount = m_parser.getAllReqCount();
        int validCount = m_parser.getValidReqCount();

        QString message = QString(u8"加载完成，共解析 %1 条需求，其中有效需求 %2 条")
                         .arg(totalCount).arg(validCount);
        statusBar()->showMessage(message, 5000); // 5秒后自动消失

        // 无有效需求时提示
        if (validCount == 0) {
            QMessageBox::warning(this, u8"警告",
                                 u8"文件加载成功，但没有找到有效需求。\n"
                                 u8"可能原因：\n"
                                 u8"1. 所有需求都是未命名需求\n"
                                 u8"2. 属性映射不匹配\n"
                                 u8"3. 命名空间配置问题");
        }
    } else {
        // 解析失败：提示错误
        statusBar()->showMessage(u8"文件解析失败", 5000);
        QMessageBox::critical(this, u8"失败", u8"文件解析失败，请检查文件格式");
    }
}

// 树节点点击事件：显示对应需求的描述
void MainWindow::onReqItemClicked(QTreeWidgetItem *item, int column) {
    Q_UNUSED(column); // 忽略列参数
    if (!item) return; // 空节点防护

    // 从节点数据中获取需求ID（存储在第0列的UserRole中）
    QString reqId = item->data(0, Qt::UserRole).toString();

    // 无效ID处理
    if (reqId.isEmpty()) {
        m_descBrowser->setPlainText(u8"[该节点无有效需求信息]");
        return;
    }

    // 获取并显示需求描述
    QString description = m_parser.getReqDescription(reqId);
    m_descBrowser->setPlainText(description);
}
