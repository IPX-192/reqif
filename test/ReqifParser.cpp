#include "ReqifParser.h"
#include <QFile>
#include <QXmlStreamReader>
#include <QDebug>
#include <QRegExp>
#include <algorithm>
#include <QXmlStreamNamespaceDeclaration>

// 构造函数
ReqifParser::ReqifParser(QObject *parent) : QObject(parent)
{
}

// 加载ReqIF文件入口
bool ReqifParser::load(const QString &filePath) {
    // 清空历史数据，避免残留
    m_reqMap.clear();
    m_parentMap.clear();
    m_topReqIds.clear();
    m_reqifNamespace.clear();
    return parseXml(filePath);
}

// 核心XML解析逻辑
bool ReqifParser::parseXml(const QString &xmlPath) {
    QFile xmlFile(xmlPath);
    // 1. 打开文件校验
    if (!xmlFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(nullptr, u8"错误", u8"无法打开文件：" + xmlFile.errorString());
        return false;
    }
    // 2. 空文件校验
    if (xmlFile.size() == 0) {
        QMessageBox::critical(nullptr, u8"错误", u8"文件为空，无法解析");
        xmlFile.close();
        return false;
    }

    QXmlStreamReader xml(&xmlFile);
    xml.setNamespaceProcessing(true); // 启用命名空间处理

    QString currentReqId;
    ReqData currentReq;
    bool inSpecifications = false; // 标记是否在规格层次区域

    // 3. 遍历XML节点
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::StartElement) {
            // 3.1 解析根节点：获取ReqIF命名空间（默认值兜底）
            if (xml.name().toString().compare("REQ-IF", Qt::CaseInsensitive) == 0) {
                m_reqifNamespace = xml.namespaceUri().toString();
                if (m_reqifNamespace.isEmpty()) {
                    m_reqifNamespace = "http://www.omg.org/spec/ReqIF/20110401/reqif.xsd";
                }
            }
            // 3.2 解析需求对象：初始化当前需求
            else if (isReqifElement(xml, "SPEC-OBJECT")) {
                currentReqId = xml.attributes().value("IDENTIFIER").toString();
                currentReq = ReqData(); // 重置当前需求
                currentReq.id = currentReqId;
            }
            // 3.3 标记进入规格区域：后续优先解析层次
            else if (isReqifElement(xml, "SPECIFICATIONS")) {
                inSpecifications = true;
            }
            // 3.4 解析层次结构：仅在规格区域内处理
            else if (inSpecifications && isReqifElement(xml, "SPEC-HIERARCHY")) {
                parseHierarchy(xml, ""); // 顶层需求无父ID
            }
            // 3.5 解析整数属性：提取排序号
            else if (isReqifElement(xml, "ATTRIBUTE-VALUE-INTEGER")) {
                parseIntegerAttribute(xml, currentReq);
            }
            // 3.6 解析XHTML属性：提取名称/描述
            else if (isReqifElement(xml, "ATTRIBUTE-VALUE-XHTML")) {
                parseXhtmlAttribute(xml, currentReq);
            }
        }
        // 3.7 处理结束标签：保存需求或退出规格区域
        else if (token == QXmlStreamReader::EndElement) {
            if (isReqifElement(xml, "SPEC-OBJECT") && !currentReqId.isEmpty()) {
                m_reqMap[currentReqId] = currentReq; // 保存当前需求
                currentReqId.clear();
            }
            else if (isReqifElement(xml, "SPECIFICATIONS")) {
                inSpecifications = false;
            }
        }
    }

    // 4. 解析错误处理
    if (xml.hasError()) {
        QString errorMsg = QString(u8"XML解析错误：%1\n行号：%2\n列号：%3")
                           .arg(xml.errorString())
                           .arg(xml.lineNumber())
                           .arg(xml.columnNumber());
        if (errorMsg.contains("Premature end of document", Qt::CaseInsensitive)) {
            errorMsg += u8"\n建议：检查文件是否完整或重新获取";
        }
        QMessageBox::critical(nullptr, u8"解析失败", errorMsg);
        xmlFile.close();
        return false;
    }

    xmlFile.close();

    // 5. 层次结构补充：无显式层次时从排序号推断
    if (m_parentMap.isEmpty()) {
        inferHierarchyFromSortNumbers();
    }
    // 6. 计算所有需求层级
    for (auto &req : m_reqMap) {
        if (req.level <= 1) {
            req.level = calculateLevel(req.id);
        }
    }
    // 7. 更新顶层需求列表
    updateTopLevelReqs();

    // 8. 解析结果日志
    qDebug() << u8"解析完成 | 总需求：" << getAllReqCount() << u8"有效需求：" << getValidReqCount();
    return getValidReqCount() > 0;
}

// 递归解析需求层次结构
void ReqifParser::parseHierarchy(QXmlStreamReader &xml, const QString &parentId) {
    QString currentChildId;

    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::StartElement) {
            // 读取子需求ID
            if (isReqifElement(xml, "SPEC-OBJECT-REF")) {
                currentChildId = xml.readElementText().trimmed();
                if (!currentChildId.isEmpty()) {
                    // 建立父子关系
                    if (!parentId.isEmpty()) {
                        m_parentMap[currentChildId] = parentId;
                        if (m_reqMap.contains(currentChildId)) {
                            m_reqMap[currentChildId].parentId = parentId;
                        }
                    }
                    // 顶层需求加入列表
                    else if (!m_topReqIds.contains(currentChildId)) {
                        m_topReqIds.append(currentChildId);
                    }
                }
            }
            // 递归解析子层次
            else if (isReqifElement(xml, "SPEC-HIERARCHY")) {
                parseHierarchy(xml, currentChildId);
            }
        }
        // 遇到当前层次结束标签，退出递归
        else if (token == QXmlStreamReader::EndElement && isReqifElement(xml, "SPEC-HIERARCHY")) {
            break;
        }
    }
}

// 解析整数属性（排序号）
void ReqifParser::parseIntegerAttribute(QXmlStreamReader &xml, ReqData &currentReq) {
    QString theValue = xml.attributes().value("THE-VALUE").toString();
    QString defRef; // 属性定义引用

    // 仅在当前属性范围内读取
    while (!xml.atEnd()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::StartElement) {
            if (isReqifElement(xml, "ATTRIBUTE-DEFINITION-INTEGER-REF")) {
                defRef = xml.readElementText();
                break;
            }
        }
        // 遇到属性结束标签，强制退出
        else if (token == QXmlStreamReader::EndElement && isReqifElement(xml, "ATTRIBUTE-VALUE-INTEGER")) {
            break;
        }
    }

    // 匹配排序号标识（ABSOLUTENUMBER）
    if (!defRef.isEmpty() && defRef.contains("ABSOLUTENUMBER", Qt::CaseInsensitive)) {
        currentReq.sortNum = theValue.toInt();
    }
}

// 解析XHTML属性（名称/描述）
void ReqifParser::parseXhtmlAttribute(QXmlStreamReader &xml, ReqData &currentReq) {
    QString defRef;   // 属性定义引用
    QString theValue; // XHTML内容

    // 第一步：读取属性定义引用
    while (!xml.atEnd()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::StartElement) {
            if (isReqifElement(xml, "ATTRIBUTE-DEFINITION-XHTML-REF")) {
                defRef = xml.readElementText();
                break;
            }
        }
        // 提前结束，直接返回
        else if (token == QXmlStreamReader::EndElement && isReqifElement(xml, "ATTRIBUTE-VALUE-XHTML")) {
            return;
        }
    }

    // 第二步：读取XHTML内容
    while (!xml.atEnd()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::StartElement && isReqifElement(xml, "THE-VALUE")) {
            theValue = readXhtmlContent(xml);
            break;
        }
        else if (token == QXmlStreamReader::EndElement && isReqifElement(xml, "ATTRIBUTE-VALUE-XHTML")) {
            break;
        }
    }

    // 第三步：映射到需求字段
    if (defRef.isEmpty()) return;
    if (defRef.contains("_valm_Name", Qt::CaseInsensitive)) {
        currentReq.name = cleanHtml(theValue);
    }
    else if (defRef.contains("_valm_Description", Qt::CaseInsensitive)) {
        currentReq.description = cleanHtml(theValue);
    }
}

// 读取嵌套XHTML内容
QString ReqifParser::readXhtmlContent(QXmlStreamReader &xml) {
    QString content;
    int depth = 1; // 初始深度（THE-VALUE节点）

    while (!xml.atEnd() && depth > 0) {
        QXmlStreamReader::TokenType token = xml.readNext();

        switch (token) {
        case QXmlStreamReader::StartElement:
            depth++;
            // 拼接开始标签（含属性）
            content += "<" + xml.name().toString();
            foreach (const QXmlStreamAttribute &attr, xml.attributes()) {
                content += " " + attr.name().toString() + "=\"" + attr.value().toString() + "\"";
            }
            content += ">";
            break;
        case QXmlStreamReader::EndElement:
            depth--;
            if (depth > 0) {
                content += "</" + xml.name().toString() + ">";
            }
            break;
        case QXmlStreamReader::Characters:
            content += xml.text().toString();
            break;
        default:
            break;
        }
    }
    return content;
}

// 清理HTML标签
QString ReqifParser::cleanHtml(const QString &htmlText) {
    if (htmlText.isEmpty()) return u8"[无内容]";

    QString result = htmlText;
    // 保留关键格式（换行、列表）
    result.replace(QRegExp("<br\\s*/?>", Qt::CaseInsensitive), "\n");
    result.replace(QRegExp("<div[^>]*>", Qt::CaseInsensitive), "\n\n");
    result.replace(QRegExp("</div>", Qt::CaseInsensitive), "");
    result.replace(QRegExp("<li>", Qt::CaseInsensitive), "• ");
    result.replace(QRegExp("</li>", Qt::CaseInsensitive), "\n");
    // 移除所有剩余标签
    result.replace(QRegExp("<[^>]*>", Qt::CaseInsensitive), "");
    // 解码HTML实体
    result.replace("&amp;", "&");
    result.replace("&lt;", "<");
    result.replace("&gt;", ">");
    result.replace("&nbsp;", " ");
    // 清理空白
    return result.trimmed();
}

// 从排序号推断层次结构
void ReqifParser::inferHierarchyFromSortNumbers() {
    QList<ReqData*> validReqs;
    // 筛选有排序号的有效需求
    for (auto &req : m_reqMap) {
        if (req.sortNum > 0 && isValidReq(req)) {
            validReqs.append(&req);
        }
    }
    if (validReqs.isEmpty()) return;

    // 按排序号排序
    std::sort(validReqs.begin(), validReqs.end(),
              [](ReqData* a, ReqData* b) { return a->sortNum < b->sortNum; });

    QString lastLevel1, lastLevel2; // 上一级需求ID
    for (ReqData* req : validReqs) {
        int num = req->sortNum;
        // 层级规则：1-9（1级）、10-99（2级）、100+（3级）
        if (num < 10) {
            req->level = 1;
            req->parentId = "";
            lastLevel1 = req->id;
            lastLevel2 = "";
            if (!m_topReqIds.contains(req->id)) {
                m_topReqIds.append(req->id);
            }
        }
        else if (num < 100) {
            req->level = 2;
            req->parentId = lastLevel1;
            lastLevel2 = req->id;
            m_parentMap[req->id] = lastLevel1;
        }
        else {
            req->level = 3;
            req->parentId = lastLevel2;
            m_parentMap[req->id] = lastLevel2;
        }
    }
}

// 更新顶层需求列表
void ReqifParser::updateTopLevelReqs() {
    m_topReqIds.clear();
    for (const auto &req : m_reqMap) {
        if (!m_parentMap.contains(req.id) && isValidReq(req)) {
            m_topReqIds.append(req.id);
        }
    }
}

// 计算需求层级（防循环引用）
int ReqifParser::calculateLevel(const QString &reqId) {
    if (reqId.isEmpty() || !m_parentMap.contains(reqId)) {
        return 1;
    }

    int level = 1;
    QString currentId = reqId;
    QSet<QString> visited; // 避免循环

    while (m_parentMap.contains(currentId) && !visited.contains(currentId)) {
        visited.insert(currentId);
        level++;
        currentId = m_parentMap[currentId];
        if (level > 10) break; // 层级上限保护
    }
    return level;
}

// 判断需求是否有效（非空名称）
bool ReqifParser::isValidReq(const ReqData &req) const {
    return !req.name.isEmpty() && !req.name.contains(u8"未命名需求", Qt::CaseInsensitive);
}

// 填充需求树到UI
void ReqifParser::fillTree(QTreeWidget *treeWidget) {
    if (!treeWidget) return;

    // 初始化树控件
    treeWidget->clear();
    treeWidget->setHeaderLabels(QStringList() << u8"序号" << u8"需求名称");
    treeWidget->setSortingEnabled(false);
    treeWidget->setAlternatingRowColors(true); // 交替行颜色

    // 设置树控件的缩进值（替代原setIndentation方法）
    treeWidget->setIndentation(20); // 统一设置所有层级的基础缩进

    QMap<QString, QTreeWidgetItem*> itemMap; // 需求ID->树节点映射

    // 1. 创建所有有效需求节点
    for (const auto &req : m_reqMap) {
        if (!isValidReq(req)) continue;

        QTreeWidgetItem *item = new QTreeWidgetItem();
        item->setText(0, req.sortNum > 0 ? QString::number(req.sortNum) : "");
        item->setText(1, req.name);
        item->setData(0, Qt::UserRole, req.id); // 存储需求ID
        itemMap[req.id] = item;
    }

    // 2. 构建层次结构
    for (const auto &req : m_reqMap) {
        if (!isValidReq(req)) continue;

        QTreeWidgetItem *item = itemMap[req.id];
        // 无父节点->顶层；有父节点->子节点
        if (req.parentId.isEmpty() || !itemMap.contains(req.parentId)) {
            treeWidget->addTopLevelItem(item);
        }
        else {
            itemMap[req.parentId]->addChild(item);
        }
    }

    // 3. 优化显示
    treeWidget->expandAll();
    treeWidget->resizeColumnToContents(0);
    treeWidget->resizeColumnToContents(1);
}

// 获取需求描述
QString ReqifParser::getReqDescription(const QString &reqId) {
    if (m_reqMap.contains(reqId)) {
        QString desc = m_reqMap[reqId].description;
        return desc.isEmpty() ? u8"[暂无详细描述]" : desc;
    }
    return u8"[未找到该需求]";
}

// 获取总需求数
int ReqifParser::getAllReqCount() const {
    return m_reqMap.size();
}

// 获取有效需求数
int ReqifParser::getValidReqCount() const {
    int count = 0;
    for (const auto &req : m_reqMap) {
        if (isValidReq(req)) count++;
    }
    return count;
}

// 判断是否为ReqIF命名空间元素
bool ReqifParser::isReqifElement(const QXmlStreamReader &xml, const QString &localName) {
    return xml.namespaceUri() == m_reqifNamespace &&
           xml.name().toString().compare(localName, Qt::CaseInsensitive) == 0;
}

void ReqifParser::fillTreeWithFilter(QTreeWidget *treeWidget, const QString &filterText) {
    if (!treeWidget || filterText.isEmpty()) {
        fillTree(treeWidget); // 如果过滤文本为空，显示全部
        return;
    }

    // 初始化树控件
    treeWidget->clear();
    treeWidget->setHeaderLabels(QStringList() << u8"序号" << u8"需求名称");
    treeWidget->setSortingEnabled(false);
    treeWidget->setIndentation(20);

    QMap<QString, QTreeWidgetItem*> itemMap;
    QSet<QString> matchedIds; // 存储匹配的需求ID

    // 1. 查找所有匹配过滤条件的需求
    for (const auto &req : m_reqMap) {
        if (!isValidReq(req)) continue;

        // 检查需求名称是否包含过滤文本
        if (req.name.contains(filterText, Qt::CaseInsensitive)) {
            // 递归添加所有相关节点：父级、自身、所有子级
            addRelatedNodes(req.id, matchedIds);
        }
    }

    // 2. 创建匹配需求的节点
    for (const auto &req : m_reqMap) {
        if (!matchedIds.contains(req.id)) continue;

        QTreeWidgetItem *item = new QTreeWidgetItem();
        item->setText(0, req.sortNum > 0 ? QString::number(req.sortNum) : "");
        item->setText(1, req.name);
        item->setData(0, Qt::UserRole, req.id);
        itemMap[req.id] = item;
    }

    // 3. 构建层次结构（只包含匹配的需求）
    for (const auto &req : m_reqMap) {
        if (!matchedIds.contains(req.id)) continue;

        QTreeWidgetItem *item = itemMap[req.id];
        if (req.parentId.isEmpty() || !itemMap.contains(req.parentId)) {
            treeWidget->addTopLevelItem(item);
        } else {
            itemMap[req.parentId]->addChild(item);
        }
    }

    // 4. 优化显示
    treeWidget->expandAll();
    treeWidget->resizeColumnToContents(0);
    treeWidget->resizeColumnToContents(1);

    // 显示过滤结果统计
    int totalCount = matchedIds.size();
    if (totalCount == 0) {
        QTreeWidgetItem *noResultItem = new QTreeWidgetItem(treeWidget);
        noResultItem->setText(1, QString(u8"未找到包含\"%1\"的需求").arg(filterText));
        noResultItem->setFlags(noResultItem->flags() & ~Qt::ItemIsSelectable);
    }
}

// 递归添加相关节点（父级、自身、所有子级）
void ReqifParser::addRelatedNodes(const QString &reqId, QSet<QString> &matchedIds) {
    if (matchedIds.contains(reqId) || !m_reqMap.contains(reqId)) {
        return;
    }

    // 添加当前节点
    matchedIds.insert(reqId);

    // 递归添加所有父级节点
    QString parentId = m_reqMap[reqId].parentId;
    while (!parentId.isEmpty() && m_reqMap.contains(parentId)) {
        if (!matchedIds.contains(parentId)) {
            matchedIds.insert(parentId);
            parentId = m_reqMap[parentId].parentId;
        } else {
            break; // 避免循环
        }
    }

    // 递归添加所有子级节点
    addAllChildren(reqId, matchedIds); // 这里调用 addAllChildren
}

// 递归添加所有子节点
// 递归添加所有子节点
void ReqifParser::addAllChildren(const QString &parentId, QSet<QString> &matchedIds) {
    // 查找所有直接子节点
    for (auto it = m_reqMap.constBegin(); it != m_reqMap.constEnd(); ++it) {
        const QString &id = it.key();
        const ReqData &req = it.value();

        if (req.parentId == parentId && isValidReq(req)) {
            if (!matchedIds.contains(id)) {
                matchedIds.insert(id);
                addAllChildren(id, matchedIds); // 递归添加子节点的子节点
            }
        }
    }
}
