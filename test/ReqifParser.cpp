#include "ReqifParser.h"
#include <QFile>
#include <QXmlStreamReader>
#include <QDebug>
#include <QMessageBox>
#include <QRegExp>
#include <algorithm>
#include <QStringList>
#include <QTextCodec>

ReqifParser::ReqifParser(QObject *parent) : QObject(parent)
{
}

bool ReqifParser::load(const QString &filePath) {
    m_reqMap.clear();
    m_parentMap.clear();
    m_topReqIds.clear();
    m_reqifNamespace.clear();
    m_xhtmlNamespace.clear();
    return parseXml(filePath);
}

bool ReqifParser::parseXml(const QString &xmlPath) {
    QFile xmlFile(xmlPath);
    if (!xmlFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(nullptr, u8"错误", u8"无法打开文件：" + xmlFile.errorString());
        return false;
    }

    // 预检查文件完整性
    if (xmlFile.size() == 0) {
        QMessageBox::critical(nullptr, u8"错误", u8"文件为空，无法解析");
        xmlFile.close();
        return false;
    }

    // 读取完整文件内容到内存（避免流式读取中断问题）
    QByteArray fileContent = xmlFile.readAll();
    xmlFile.close();

    // 初步验证XML结构完整性
    QString contentStr = QString::fromUtf8(fileContent);
    if (!contentStr.contains("</REQ-IF>", Qt::CaseInsensitive)) {
        QMessageBox::warning(nullptr, u8"警告", u8"文件可能不完整，缺少根元素结束标签</REQ-IF>");
        // 尝试自动修复简单结构问题
        if (contentStr.trimmed().endsWith(">") && !contentStr.trimmed().endsWith("</REQ-IF>")) {
            contentStr += "\n</REQ-IF>";
            fileContent = contentStr.toUtf8();
            qDebug() << "已尝试自动添加根元素结束标签";
        }
    }

    QXmlStreamReader xml(fileContent);
    xml.setNamespaceProcessing(true);

    m_reqMap.clear();
    m_parentMap.clear();
    m_topReqIds.clear();
    m_reqifNamespace.clear();
    m_xhtmlNamespace.clear();

    // 第一阶段：专注解析根节点和命名空间（优化处理）
    bool foundRoot = false;
    while (!xml.atEnd() && !xml.hasError() && !foundRoot) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::StartElement) {
            // 灵活匹配根节点（兼容带前缀和不带前缀的情况）
            if (xml.name().toString().compare("REQ-IF", Qt::CaseInsensitive) == 0) {
                m_reqifNamespace = xml.namespaceUri().toString();
                // 命名空间 fallback：如果未指定则使用标准ReqIF命名空间
                if (m_reqifNamespace.isEmpty()) {
                    m_reqifNamespace = "http://www.omg.org/spec/ReqIF/20110401/reqif.xsd";
                }

                // 解析所有命名空间声明（优化XHTML命名空间检测）
                foreach (const QXmlStreamNamespaceDeclaration &ns, xml.namespaceDeclarations()) {
                    QString nsUri = ns.namespaceUri().toString();
                    if (nsUri == "http://www.w3.org/1999/xhtml") {
                        m_xhtmlNamespace = nsUri;
                        qDebug() << "从命名空间声明中识别XHTML命名空间:" << nsUri;
                    }
                }

                // XHTML命名空间 fallback
                if (m_xhtmlNamespace.isEmpty()) {
                    m_xhtmlNamespace = "http://www.w3.org/1999/xhtml";
                    qDebug() << "未找到XHTML命名空间声明，使用默认值:" << m_xhtmlNamespace;
                }

                foundRoot = true;
                qDebug() << "=== 命名空间识别完成 ===";
                qDebug() << "ReqIF命名空间:" << m_reqifNamespace;
                qDebug() << "XHTML命名空间:" << m_xhtmlNamespace;
            }
        }
    }

    // 检查根节点解析错误
    if (xml.hasError()) {
        QMessageBox::critical(nullptr, u8"解析失败",
            u8"根节点解析错误: " + xml.errorString() +
            u8"\n行号: " + QString::number(xml.lineNumber()) +
            u8"\n可能原因：文件格式损坏或非标准ReqIF文件");
        return false;
    }

    // 处理未找到根节点的情况
    if (!foundRoot) {
        qWarning() << "未找到REQ-IF根节点，尝试强制使用标准命名空间继续解析";
        m_reqifNamespace = "http://www.omg.org/spec/ReqIF/20110401/reqif.xsd";
        m_xhtmlNamespace = "http://www.w3.org/1999/xhtml";
    }

    // 重置解析器，准备完整解析
    xml.clear();
    xml.addData(fileContent);

    QString currentReqId;
    ReqData currentReq;
    bool parsingSuccess = true;
    int elementCount = 0;  // 元素计数，用于监控解析进度

    // 第二阶段：主解析循环（优化控制逻辑）
    while (!xml.atEnd()) {
        // 优先处理解析错误
        if (xml.hasError()) {
            QString errorMsg = xml.errorString();
            qCritical() << "XML解析错误[" << xml.lineNumber() << "行]:" << errorMsg;

            // 特殊处理文档提前结束错误
            if (errorMsg.contains("Premature end of document", Qt::CaseInsensitive)) {
                QMessageBox::critical(nullptr, u8"严重错误",
                    u8"文档提前结束，文件可能损坏或不完整\n"
                    u8"建议操作：\n"
                    u8"1. 重新获取或下载ReqIF文件\n"
                    u8"2. 使用XML验证工具检查文件格式\n"
                    u8"3. 确认文件未被压缩或加密");
                return false;
            }

            // 尝试从错误中恢复
            xml.readNext();
            parsingSuccess = false;
            continue;
        }

        QXmlStreamReader::TokenType token = xml.readNext();

        // 只处理开始元素和结束元素（优化性能）
        if (token != QXmlStreamReader::StartElement && token != QXmlStreamReader::EndElement) {
            continue;
        }

        // 处理开始元素（添加详细调试）
        if (token == QXmlStreamReader::StartElement) {
            elementCount++;
            // 每解析200个元素输出一次进度（避免调试信息过多）
            if (elementCount % 200 == 0) {
                qDebug() << "解析进度：已处理" << elementCount << "个元素（当前位置：" << xml.characterOffset() << "字节）";
            }

            // 调试输出当前元素信息（包含命名空间）
            QString nsUri = xml.namespaceUri().toString();
            QString localName = xml.name().toString();
            qDebug() << "开始元素: {" << nsUri.left(30) << "...}" << localName;

            // 处理需求对象（SPEC-OBJECT）
            if (isReqifElement(xml, "SPEC-OBJECT")) {
                currentReqId = xml.attributes().value("IDENTIFIER").toString();
                currentReq = ReqData();
                currentReq.id = currentReqId;
                qDebug() << "=== 开始解析需求对象 ===";
                qDebug() << "需求ID:" << currentReqId;
                qDebug() << "最后修改时间:" << xml.attributes().value("LAST-CHANGE").toString();
            }
            // 处理规格层次结构（SPECIFICATIONS）
            else if (isReqifElement(xml, "SPECIFICATIONS")) {
                qDebug() << "开始解析需求层次结构";
                parseSpecifications(xml);
            }
            // 处理整数属性（排序号等）
            else if (isReqifElement(xml, "ATTRIBUTE-VALUE-INTEGER")) {
                parseIntegerAttribute(xml, currentReq);
            }
            // 处理XHTML属性（名称、描述等）
            else if (isReqifElement(xml, "ATTRIBUTE-VALUE-XHTML")) {
                parseXhtmlAttribute(xml, currentReq);
            }
            // 处理日期属性
            else if (isReqifElement(xml, "ATTRIBUTE-VALUE-DATE")) {
                parseDateAttribute(xml, currentReq);
            }
            // 处理枚举属性
            else if (isReqifElement(xml, "ATTRIBUTE-VALUE-ENUMERATION")) {
                parseEnumerationAttribute(xml, currentReq);
            }
            // 处理布尔属性
            else if (isReqifElement(xml, "ATTRIBUTE-VALUE-BOOLEAN")) {
                parseBooleanAttribute(xml, currentReq);
            }
        }
        // 处理结束元素（精准控制解析范围）
        else if (token == QXmlStreamReader::EndElement) {
            if (isReqifElement(xml, "SPEC-OBJECT") && !currentReqId.isEmpty()) {
                m_reqMap[currentReqId] = currentReq;
                qDebug() << "=== 完成解析需求对象 ===";
                qDebug() << "需求ID:" << currentReqId;
                qDebug() << "需求名称:" << (currentReq.name.isEmpty() ? u8"[未命名]" : currentReq.name);
                currentReqId.clear();
            }
        }
    }

    // 最终错误检查
    if (xml.hasError()) {
        QMessageBox::critical(nullptr, u8"解析失败",
            u8"最终解析错误: " + xml.errorString() +
            u8"\n行号: " + QString::number(xml.lineNumber()) +
            u8"\n列号: " + QString::number(xml.columnNumber()));
        return false;
    }

    // 处理层次结构（优先级：显式层次 > 排序号推断）
    if (m_parentMap.isEmpty() && !m_reqMap.isEmpty()) {
        qDebug() << "未找到显式层次结构，尝试从排序号推断";
        inferHierarchyFromSortNumbers();
    }
    // 补充顶层需求列表（确保无父节点的需求都为顶层）
    else if (m_topReqIds.isEmpty()) {
        for (const auto &req : m_reqMap) {
            if (!m_parentMap.contains(req.id)) {
                m_topReqIds.append(req.id);
            }
        }
    }

    // 计算所有需求的层级（确保层级准确性）
    for (auto &req : m_reqMap) {
        if (req.level <= 1) {
            req.level = calculateLevel(req.id);
        }
    }

    // 输出解析统计（优化调试体验）
    qDebug() << "\n=== 解析完成 - 统计信息 ===";
    qDebug() << "总需求数量:" << m_reqMap.size();
    qDebug() << "顶层需求数量:" << m_topReqIds.size();
    qDebug() << "父子关系数量:" << m_parentMap.size();
    qDebug() << "解析元素总数:" << elementCount;

    // 输出关键需求信息（便于验证）
    int debugCount = 0;
    for (const auto &req : m_reqMap) {
        if (!req.name.isEmpty() && debugCount < 10) { // 只输出前10个有名称的需求
            qDebug() << "需求[" << debugCount + 1 << "]:"
                     << "ID=" << req.id
                     << "序号=" << req.sortNum
                     << "层级=" << req.level
                     << "名称=" << req.name;
            debugCount++;
        }
    }

    // 检查解析结果有效性
    if (m_reqMap.isEmpty()) {
        QMessageBox::warning(nullptr, u8"警告", u8"解析完成但未获取到任何需求数据");
        return false;
    }

    return parsingSuccess;
}

// 解析规格部分（包含层次结构入口）
void ReqifParser::parseSpecifications(QXmlStreamReader &xml) {
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::StartElement) {
            if (isReqifElement(xml, "SPEC-HIERARCHY")) {
                qDebug() << "进入SPEC-HIERARCHY解析（顶层）";
                parseHierarchy(xml, ""); // 顶层需求无父ID
            }
        }
        // 精准匹配结束标签，避免跳出范围
        else if (token == QXmlStreamReader::EndElement && isReqifElement(xml, "SPECIFICATIONS")) {
            qDebug() << "退出SPECIFICATIONS解析";
            break;
        }
    }
}

// 递归解析需求层次结构（修复递归控制）
void ReqifParser::parseHierarchy(QXmlStreamReader &xml, const QString &parentId) {
    QString currentChildId;
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::StartElement) {
            // 解析需求引用
            if (isReqifElement(xml, "SPEC-OBJECT-REF")) {
                currentChildId = xml.readElementText().trimmed();
                if (!currentChildId.isEmpty()) {
                    qDebug() << "层次结构：父需求ID=" << (parentId.isEmpty() ? u8"[顶层]" : parentId)
                             << "子需求ID=" << currentChildId;

                    // 建立父子关系
                    if (!parentId.isEmpty()) {
                        m_parentMap[currentChildId] = parentId;
                        // 同步更新需求数据中的父ID
                        if (m_reqMap.contains(currentChildId)) {
                            m_reqMap[currentChildId].parentId = parentId;
                        }
                    }
                    // 顶层需求加入顶层列表
                    else if (!m_topReqIds.contains(currentChildId)) {
                        m_topReqIds.append(currentChildId);
                    }
                }
            }
            // 递归解析子层次
            else if (isReqifElement(xml, "SPEC-HIERARCHY")) {
                qDebug() << "进入子SPEC-HIERARCHY解析（父ID=" << currentChildId << "）";
                parseHierarchy(xml, currentChildId);
            }
        }
        // 精准匹配当前层次结束标签，避免递归溢出
        else if (token == QXmlStreamReader::EndElement && isReqifElement(xml, "SPEC-HIERARCHY")) {
            qDebug() << "退出SPEC-HIERARCHY解析（父ID=" << parentId << "）";
            break;
        }
    }
}

// 从排序号推断层次结构（优化逻辑）
void ReqifParser::inferHierarchyFromSortNumbers() {
    QList<ReqData*> sortedReqs;
    // 筛选有排序号的需求
    for (auto &req : m_reqMap) {
        if (req.sortNum > 0) {
            sortedReqs.append(&req);
        }
    }

    if (sortedReqs.isEmpty()) {
        qDebug() << "无有效排序号，无法推断层次结构";
        return;
    }

    // 按排序号升序排列
    std::sort(sortedReqs.begin(), sortedReqs.end(),
              [](ReqData* a, ReqData* b) { return a->sortNum < b->sortNum; });

    qDebug() << "从排序号推断层次结构（共" << sortedReqs.size() << "个需求）";
    QString lastLevel1, lastLevel2; // 记录上一级需求ID（1级和2级）

    for (ReqData* req : sortedReqs) {
        int num = req->sortNum;
        // 层级规则：1-9（1级）、10-99（2级）、100+（3级）
        if (num < 10) {
            req->level = 1;
            req->parentId = "";
            lastLevel1 = req->id;
            lastLevel2 = "";
            // 加入顶层列表
            if (!m_topReqIds.contains(req->id)) {
                m_topReqIds.append(req->id);
            }
            qDebug() << "推断层级：ID=" << req->id << "序号=" << num << "层级=1（顶层）";
        } else if (num < 100) {
            req->level = 2;
            req->parentId = lastLevel1;
            lastLevel2 = req->id;
            m_parentMap[req->id] = lastLevel1;
            qDebug() << "推断层级：ID=" << req->id << "序号=" << num << "层级=2（父ID=" << lastLevel1 << "）";
        } else {
            req->level = 3;
            req->parentId = lastLevel2;
            m_parentMap[req->id] = lastLevel2;
            qDebug() << "推断层级：ID=" << req->id << "序号=" << num << "层级=3（父ID=" << lastLevel2 << "）";
        }
    }
}

// 计算需求层级（修复循环检测）
int ReqifParser::calculateLevel(const QString &reqId) {
    if (reqId.isEmpty() || !m_parentMap.contains(reqId)) {
        return 1; // 无父节点为1级
    }

    int level = 1;
    QString currentId = reqId;
    QSet<QString> visited; // 防止循环引用导致死循环

    while (m_parentMap.contains(currentId) && !visited.contains(currentId)) {
        visited.insert(currentId);
        level++;
        currentId = m_parentMap[currentId];

        // 层级上限保护（防止异常数据导致无限循环）
        if (level > 10) {
            qWarning() << "需求层级超过10级，可能存在循环引用：ID=" << reqId;
            break;
        }
    }

    return level;
}

// 修复后的HTML清理方法（保留关键格式）
QString ReqifParser::cleanHtml(const QString &htmlText) {
    if (htmlText.isEmpty()) {
        return u8"[无内容]";
    }

    QString result = htmlText;

    // 1. 保留关键格式标签（替换为可读性更好的格式）
    // 处理换行相关标签
    result.replace(QRegExp("<reqif-xhtml:br\\s*/>"), "\n");
    result.replace(QRegExp("<br\\s*/>"), "\n");
    result.replace(QRegExp("<br>"), "\n");
    // 处理段落/区块标签
    result.replace(QRegExp("<reqif-xhtml:div[^>]*>"), "\n\n");
    result.replace(QRegExp("</reqif-xhtml:div>"), "");
    result.replace(QRegExp("<div[^>]*>"), "\n\n");
    result.replace(QRegExp("</div>"), "");
    // 处理列表标签
    result.replace(QRegExp("<reqif-xhtml:li>"), "• ");
    result.replace(QRegExp("<li>"), "• ");
    result.replace(QRegExp("</reqif-xhtml:li>"), "\n");
    result.replace(QRegExp("</li>"), "\n");
    result.replace(QRegExp("<reqif-xhtml:ul[^>]*>"), "\n");
    result.replace(QRegExp("<ul[^>]*>"), "\n");
    result.replace(QRegExp("</reqif-xhtml:ul>"), "\n");
    result.replace(QRegExp("</ul>"), "\n");

    // 2. 移除所有剩余HTML标签
    QRegExp tagRegex("<[^>]*>");
    result.replace(tagRegex, "");

    // 3. 解码HTML实体
    result.replace("&amp;", "&");
    result.replace("&lt;", "<");
    result.replace("&gt;", ">");
    result.replace("&quot;", "\"");
    result.replace("&apos;", "'");
    result.replace("&nbsp;", " ");

    // 4. 处理中文编码和空白
    result = result.trimmed();
    result.replace(QRegExp("\\s+"), " "); // 合并多个空格
    result.replace("\n ", "\n"); // 移除行首空格

    return result;
}

// 填充需求树（优化显示效果）
void ReqifParser::fillTree(QTreeWidget *treeWidget) {
    if (!treeWidget) return;
    treeWidget->clear();
    treeWidget->setHeaderLabels(QStringList() << u8"序号" << u8"需求名称");
    treeWidget->setSortingEnabled(false); // 关闭排序，保持原始顺序

    QMap<QString, QTreeWidgetItem*> itemMap;

    // 1. 添加顶层节点
    for (const QString &id : m_topReqIds) {
        if (m_reqMap.contains(id)) {
            const ReqData &req = m_reqMap[id];
            QTreeWidgetItem *item = new QTreeWidgetItem(treeWidget);
            item->setText(0, req.sortNum > 0 ? QString::number(req.sortNum) : "");
            item->setText(1, req.name.isEmpty() ? u8"[未命名需求]" : req.name);
            item->setData(0, Qt::UserRole, id); // 存储需求ID用于后续查询

            // 根据层级设置缩进或样式
            if (req.level > 1) {
                QFont font = item->font(1);
                font.setBold(true);
                item->setFont(1, font);
            }

            itemMap[id] = item;
        }
    }

    // 2. 添加子节点（按层级顺序）
    QList<ReqData> sortedReqs = m_reqMap.values();
    std::sort(sortedReqs.begin(), sortedReqs.end(),
              [](const ReqData &a, const ReqData &b) {
                  return a.sortNum < b.sortNum;
              });

    for (const ReqData &req : sortedReqs) {
        if (req.level > 1 && !req.parentId.isEmpty() && itemMap.contains(req.parentId)) {
            QTreeWidgetItem *item = new QTreeWidgetItem(itemMap[req.parentId]);
            item->setText(0, req.sortNum > 0 ? QString::number(req.sortNum) : "");
            item->setText(1, req.name.isEmpty() ? u8"[未命名需求]" : req.name);
            item->setData(0, Qt::UserRole, req.id);

            // 为不同层级设置不同样式
            if (req.level == 2) {
                QFont font = item->font(1);
                font.setItalic(true);
                item->setFont(1, font);
            }

            itemMap[req.id] = item;
        }
    }

    // 自动调整列宽并展开顶层节点
    treeWidget->expandToDepth(0);
    treeWidget->resizeColumnToContents(0);
    treeWidget->resizeColumnToContents(1);
}

// 获取需求描述
QString ReqifParser::getReqDescription(const QString &reqId) {
    if (m_reqMap.contains(reqId)) {
        QString desc = m_reqMap[reqId].description;
        return desc.isEmpty() ? u8"[暂无详细描述信息]" : desc;
    }
    return u8"[未找到该需求]";
}

// 获取需求总数
int ReqifParser::getAllReqCount() const {
    return m_reqMap.size();
}

// 解析整数属性（优化控制逻辑）
void ReqifParser::parseIntegerAttribute(QXmlStreamReader &xml, ReqData &currentReq) {
    QString theValueAttr = xml.attributes().value("THE-VALUE").toString();
    QString defRef;

    // 只读取当前元素的子元素，不读取到文件末尾
    while (!xml.atEnd()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::StartElement) {
            if (isReqifElement(xml, "ATTRIBUTE-DEFINITION-INTEGER-REF")) {
                defRef = xml.readElementText();
            }
        }
        else if (token == QXmlStreamReader::EndElement &&
                 isReqifElement(xml, "ATTRIBUTE-VALUE-INTEGER")) {
            break; // 遇到结束标签就退出
        }
    }

    if (!defRef.isEmpty() && defRef.contains("ABSOLUTENUMBER", Qt::CaseInsensitive)) {
        currentReq.sortNum = theValueAttr.toInt();
        qDebug() << "解析序号:" << currentReq.sortNum;
    }
}

// 解析XHTML属性（修复嵌套解析）
void ReqifParser::parseXhtmlAttribute(QXmlStreamReader &xml, ReqData &currentReq) {
    QString defRef;
    QString theValue;

    // 先读取定义引用
    while (!xml.atEnd()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::StartElement) {
            if (isReqifElement(xml, "ATTRIBUTE-DEFINITION-XHTML-REF")) {
                defRef = xml.readElementText();
                break; // 找到定义引用后跳出
            }
        }
        else if (token == QXmlStreamReader::EndElement &&
                 isReqifElement(xml, "ATTRIBUTE-VALUE-XHTML")) {
            return; // 提前结束
        }
    }

    // 现在读取 THE-VALUE 内容
    while (!xml.atEnd()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::StartElement) {
            if (isReqifElement(xml, "THE-VALUE")) {
                // 读取整个 THE-VALUE 内容（修复嵌套问题）
                theValue = readXhtmlContent(xml);
                break;
            }
        }
        else if (token == QXmlStreamReader::EndElement &&
                 isReqifElement(xml, "ATTRIBUTE-VALUE-XHTML")) {
            break;
        }
    }

    // 处理属性映射
    if (!defRef.isEmpty()) {
        if (defRef.contains("_valm_Name", Qt::CaseInsensitive)) {
            currentReq.name = cleanHtml(theValue);
            qDebug() << "解析名称:" << currentReq.name;
        }
        else if (defRef.contains("_valm_Description", Qt::CaseInsensitive)) {
            currentReq.description = cleanHtml(theValue);
            qDebug() << "解析描述:" << currentReq.description.left(50) << "...";
        }
    }
}

// 读取XHTML内容（修复嵌套解析逻辑）
QString ReqifParser::readXhtmlContent(QXmlStreamReader &xml) {
    QString content;
    int depth = 1; // 从 THE-VALUE 开始深度为1

    while (!xml.atEnd() && depth > 0) {
        QXmlStreamReader::TokenType token = xml.readNext();

        switch (token) {
        case QXmlStreamReader::StartElement:
            depth++;
            // 添加开始标签
            content += "<" + xml.name().toString();
            // 添加属性
            foreach (const QXmlStreamAttribute &attr, xml.attributes()) {
                content += " " + attr.name().toString() + "=\"" +
                          attr.value().toString() + "\"";
            }
            content += ">";
            break;

        case QXmlStreamReader::EndElement:
            depth--;
            if (depth > 0) { // 不包含 THE-VALUE 的结束标签
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

// 解析日期属性
void ReqifParser::parseDateAttribute(QXmlStreamReader &xml, ReqData &currentReq) {
    QString theValueAttr = xml.attributes().value("THE-VALUE").toString();
    QString defRef;

    while (!xml.atEnd()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::StartElement) {
            if (isReqifElement(xml, "ATTRIBUTE-DEFINITION-DATE-REF")) {
                defRef = xml.readElementText();
            }
        }
        else if (token == QXmlStreamReader::EndElement &&
                 isReqifElement(xml, "ATTRIBUTE-VALUE-DATE")) {
            break;
        }
    }

    // 处理日期属性映射
    if (!defRef.isEmpty()) {
        qDebug() << "解析日期属性 - Ref:" << defRef << "Value:" << theValueAttr;
        if (defRef.contains("CREATION_DATE", Qt::CaseInsensitive) ||
            defRef.contains("创建日期", Qt::CaseInsensitive)) {
            currentReq.creationDate = theValueAttr;
        }
        else if (defRef.contains("MODIFICATION_DATE", Qt::CaseInsensitive) ||
                 defRef.contains("修改日期", Qt::CaseInsensitive)) {
            currentReq.modificationDate = theValueAttr;
        }
    }
}

// 解析枚举属性
void ReqifParser::parseEnumerationAttribute(QXmlStreamReader &xml, ReqData &currentReq) {
    QString defRef;
    QString enumValue;

    while (!xml.atEnd()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::StartElement) {
            if (isReqifElement(xml, "ATTRIBUTE-DEFINITION-ENUMERATION-REF")) {
                defRef = xml.readElementText();
            }
            else if (isReqifElement(xml, "ENUM-VALUE-REF")) {
                enumValue = xml.readElementText();
            }
        }
        else if (token == QXmlStreamReader::EndElement &&
                 isReqifElement(xml, "ATTRIBUTE-VALUE-ENUMERATION")) {
            break;
        }
    }

    if (!defRef.isEmpty()) {
        qDebug() << "解析枚举属性 - Ref:" << defRef << "Value:" << enumValue;
        currentReq.enumAttributes[defRef] = enumValue;
    }
}

// 解析布尔属性
void ReqifParser::parseBooleanAttribute(QXmlStreamReader &xml, ReqData &currentReq) {
    QString theValueAttr = xml.attributes().value("THE-VALUE").toString();
    QString defRef;

    while (!xml.atEnd()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::StartElement) {
            if (isReqifElement(xml, "ATTRIBUTE-DEFINITION-BOOLEAN-REF")) {
                defRef = xml.readElementText();
            }
        }
        else if (token == QXmlStreamReader::EndElement &&
                 isReqifElement(xml, "ATTRIBUTE-VALUE-BOOLEAN")) {
            break;
        }
    }

    if (!defRef.isEmpty()) {
        qDebug() << "解析布尔属性 - Ref:" << defRef << "Value:" << theValueAttr;
        bool value = (theValueAttr.compare("true", Qt::CaseInsensitive) == 0 ||
                     theValueAttr == "1");
        currentReq.booleanAttributes[defRef] = value;
    }
}

// 判断是否为ReqIF命名空间元素（优化）
bool ReqifParser::isReqifElement(const QXmlStreamReader &xml, const QString &localName) {
    return xml.namespaceUri() == m_reqifNamespace &&
           xml.name().toString().compare(localName, Qt::CaseInsensitive) == 0;
}

// 判断是否为XHTML命名空间元素（优化）
bool ReqifParser::isXhtmlElement(const QXmlStreamReader &xml, const QString &localName) {
    return xml.namespaceUri() == m_xhtmlNamespace &&
           xml.name().toString().compare(localName, Qt::CaseInsensitive) == 0;
}
