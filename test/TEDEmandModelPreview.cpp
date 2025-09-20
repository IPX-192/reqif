#include "TEDEmandModelPreview.h"
#include <QSplitter>
#include <QFileDialog>
#include <QMessageBox>
#include <QFont>

TEDEmandModelPreview::TEDEmandModelPreview(QWidget *parent)
    : QWidget(parent),
      m_treeWidget(nullptr),
      m_descBrowser(nullptr),
      m_toolBar(nullptr)
{
    initUI();
}

TEDEmandModelPreview::~TEDEmandModelPreview() {}

void TEDEmandModelPreview::initUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0,0,0,0);

    // 工具栏
    m_toolBar = new QToolBar(u8"功能", this);
    m_toolBar->setMovable(false);
    mainLayout->addWidget(m_toolBar);

    // 分割器
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

    // 左侧树
    m_treeWidget = new QTreeWidget(splitter);
    m_treeWidget->setMinimumWidth(300);
    m_treeWidget->setHeaderLabels(QStringList() << u8"序号" << u8"需求名称");
    QFont treeFont = m_treeWidget->font();
    treeFont.setPointSize(10);
    m_treeWidget->setFont(treeFont);
    m_treeWidget->setAlternatingRowColors(true);

    // 右侧描述
    m_descBrowser = new QTextBrowser(splitter);
    m_descBrowser->setMinimumWidth(600);
    QFont descFont = m_descBrowser->font();
    descFont.setPointSize(10);
    m_descBrowser->setFont(descFont);
    m_descBrowser->setStyleSheet("background-color: #f8f8f8; padding: 15px;");
    m_descBrowser->setPlaceholderText(u8"点击左侧需求节点查看描述");

    splitter->setSizes({300, 700});
    mainLayout->addWidget(splitter);

    // 工具栏按钮
    QAction *loadAction = m_toolBar->addAction(u8"加载.reqif文件");
    QAction *showAllAction = m_toolBar->addAction(u8"显示全部");
    QAction *techFilterAction = m_toolBar->addAction(u8"技术要求");

    connect(loadAction, &QAction::triggered, this, &TEDEmandModelPreview::onLoadFile);
    connect(showAllAction, &QAction::triggered, this, &TEDEmandModelPreview::onShowAllRequirements);
    connect(techFilterAction, &QAction::triggered, this, &TEDEmandModelPreview::onShowTechnicalRequirements);
    connect(m_treeWidget, &QTreeWidget::itemClicked, this, &TEDEmandModelPreview::onReqItemClicked);
}

void TEDEmandModelPreview::onLoadFile()
{
    QString filePath = QFileDialog::getOpenFileName(
        this, u8"选择ReqIF文件", "",
        u8"ReqIF文件 (*.reqif);;所有文件 (*.*)");
    if (filePath.isEmpty()) return;

    loadReqIfFile(filePath);
}

void TEDEmandModelPreview::loadReqIfFile(const QString &filePath)
{
    if (m_parser.load(filePath)) {
        m_parser.fillTree(m_treeWidget);
        int totalCount = m_parser.getAllReqCount();
        int validCount = m_parser.getValidReqCount();

        QString message = QString(u8"加载完成，共解析 %1 条需求，其中有效需求 %2 条")
                         .arg(totalCount).arg(validCount);
        QMessageBox::information(this, u8"加载成功", message);

        if (validCount == 0) {
            QMessageBox::warning(this, u8"警告",
                                 u8"文件加载成功，但没有找到有效需求。\n"
                                 u8"可能原因：\n"
                                 u8"1. 所有需求都是未命名需求\n"
                                 u8"2. 属性映射不匹配\n"
                                 u8"3. 命名空间配置问题");
        }
    } else {
        QMessageBox::critical(this, u8"失败", u8"文件解析失败，请检查文件格式");
    }
}

void TEDEmandModelPreview::onShowTechnicalRequirements()
{
    if (m_parser.getAllReqCount() == 0) {
        QMessageBox::information(this, u8"提示", u8"请先加载ReqIF文件");
        return;
    }

    m_parser.fillTreeWithFilter(m_treeWidget, u8"技术");

    int visibleCount = m_treeWidget->topLevelItemCount();
    QMessageBox::information(this, u8"过滤",
                             visibleCount > 0 ?
                                 QString(u8"显示 %1 条技术要求相关需求").arg(visibleCount) :
                                 u8"未找到技术要求相关需求");
}

void TEDEmandModelPreview::onShowAllRequirements()
{
    if (m_parser.getAllReqCount() == 0) {
        QMessageBox::information(this, u8"提示", u8"请先加载ReqIF文件");
        return;
    }

    m_parser.fillTree(m_treeWidget);
    QMessageBox::information(this, u8"显示", u8"显示所有需求");
}

void TEDEmandModelPreview::onReqItemClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    if (!item) return;
    QString reqId = item->data(0, Qt::UserRole).toString();
    QString description = m_parser.getReqDescription(reqId);
    m_descBrowser->setPlainText(description);
}
