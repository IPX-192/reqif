#include "ReqifParser.h"
#include <QFile>
#include <QXmlStreamReader>
#include <QDebug>
#include <QMessageBox>
#include <QRegExp>
#include <algorithm>

ReqifParser::ReqifParser(QObject *parent) : QObject(parent)
{
}

bool ReqifParser::load(const QString &filePath) {
    m_reqMap.clear();
    m_parentMap.clear();
    m_topReqIds.clear();
    return parseXml(filePath);
}

bool ReqifParser::parseXml(const QString &xmlPath) {
    QFile xmlFile(xmlPath);
    if (!xmlFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(nullptr, u8"错误", u8"无法打开文件：" + xmlFile.errorString());
        return false;
    }

    QXmlStreamReader xml(&xmlFile);
    xml.setNamespaceProcessing(true);

    QString currentReqId;
    ReqData currentReq;
    bool inSpecifications = false;

    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::StartElement) {
            QString elementName = xml.name().toString();

            if (elementName.compare("REQ-IF", Qt::CaseInsensitive) == 0) {
                m_reqifNamespace = xml.namespaceUri().toString();
                if (m_reqifNamespace.isEmpty()) {
                    m_reqifNamespace = "http://www.omg.org/spec/ReqIF/20110401/reqif.xsd";
                }
            }
            else if (isReqifElement(xml, "SPEC-OBJECT")) {
                currentReqId = xml.attributes().value("IDENTIFIER").toString();
                currentReq = ReqData();
                currentReq.id = currentReqId;
            }
            else if (isReqifElement(xml, "SPECIFICATIONS")) {
                inSpecifications = true;
            }
            else if (inSpecifications && isReqifElement(xml, "SPEC-HIERARCHY")) {
                parseHierarchy(xml, "");
            }
            else if (isReqifElement(xml, "ATTRIBUTE-VALUE-INTEGER")) {
                parseIntegerAttribute(xml, currentReq);
            }
            else if (isReqifElement(xml, "ATTRIBUTE-VALUE-XHTML")) {
                parseXhtmlAttribute(xml, currentReq);
            }
        }
        else if (token == QXmlStreamReader::EndElement) {
            QString elementName = xml.name().toString();

            if (isReqifElement(xml, "SPEC-OBJECT") && !currentReqId.isEmpty()) {
                m_reqMap[currentReqId] = currentReq;
                currentReqId.clear();
            }
            else if (isReqifElement(xml, "SPECIFICATIONS")) {
                inSpecifications = false;
            }
        }
    }

    if (xml.hasError()) {
        QMessageBox::critical(nullptr, u8"解析失败", u8"XML解析错误: " + xml.errorString());
        return false;
    }

    // 处理层次结构
    if (m_parentMap.isEmpty()) {
        inferHierarchyFromSortNumbers();
    }

    // 确保所有需求都有正确的层级
    for (auto &req : m_reqMap) {
        if (req.level <= 1) {
            req.level = calculateLevel(req.id);
        }
    }

    // 更新顶层需求列表
    updateTopLevelReqs();

    qDebug() << "解析完成 - 总需求:" << m_reqMap.size()
             << ", 有效需求:" << getValidReqCount();

    return getValidReqCount() > 0;
}

void ReqifParser::parseHierarchy(QXmlStreamReader &xml, const QString &parentId) {
    QString currentChildId;

    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::StartElement) {
            if (isReqifElement(xml, "SPEC-OBJECT-REF")) {
                currentChildId = xml.readElementText().trimmed();
                if (!currentChildId.isEmpty()) {
                    if (!parentId.isEmpty()) {
                        m_parentMap[currentChildId] = parentId;
                        if (m_reqMap.contains(currentChildId)) {
                            m_reqMap[currentChildId].parentId = parentId;
                        }
                    } else {
                        if (!m_topReqIds.contains(currentChildId)) {
                            m_topReqIds.append(currentChildId);
                        }
                    }
                }
            }
            else if (isReqifElement(xml, "SPEC-HIERARCHY")) {
                parseHierarchy(xml, currentChildId);
            }
        }
        else if (token == QXmlStreamReader::EndElement &&
                 isReqifElement(xml, "SPEC-HIERARCHY")) {
            break;
        }
    }
}

void ReqifParser::inferHierarchyFromSortNumbers() {
    QList<ReqData*> validReqs;

    for (auto &req : m_reqMap) {
        if (req.sortNum > 0 && isValidReq(req)) {
            validReqs.append(&req);
        }
    }

    if (validReqs.isEmpty()) return;

    std::sort(validReqs.begin(), validReqs.end(),
              [](ReqData* a, ReqData* b) { return a->sortNum < b->sortNum; });

    QString lastLevel1, lastLevel2;

    for (ReqData* req : validReqs) {
        int num = req->sortNum;

        if (num < 10) {
            req->level = 1;
            req->parentId = "";
            lastLevel1 = req->id;
            lastLevel2 = "";
            if (!m_topReqIds.contains(req->id)) {
                m_topReqIds.append(req->id);
            }
        } else if (num < 100) {
            req->level = 2;
            req->parentId = lastLevel1;
            lastLevel2 = req->id;
            m_parentMap[req->id] = lastLevel1;
        } else {
            req->level = 3;
            req->parentId = lastLevel2;
            m_parentMap[req->id] = lastLevel2;
        }
    }
}

void ReqifParser::updateTopLevelReqs() {
    m_topReqIds.clear();
    for (const auto &req : m_reqMap) {
        if (!m_parentMap.contains(req.id) && isValidReq(req)) {
            m_topReqIds.append(req.id);
        }
    }
}

int ReqifParser::calculateLevel(const QString &reqId) {
    if (reqId.isEmpty() || !m_parentMap.contains(reqId)) {
        return 1;
    }

    int level = 1;
    QString currentId = reqId;
    QSet<QString> visited;

    while (m_parentMap.contains(currentId) && !visited.contains(currentId)) {
        visited.insert(currentId);
        level++;
        currentId = m_parentMap[currentId];

        if (level > 10) {
            break;
        }
    }

    return level;
}

bool ReqifParser::isValidReq(const ReqData &req) const {
    return !req.name.isEmpty() && !req.name.contains(u8"未命名需求");
}

void ReqifParser::parseIntegerAttribute(QXmlStreamReader &xml, ReqData &currentReq) {
    QString theValue = xml.attributes().value("THE-VALUE").toString();
    QString defRef;

    while (!xml.atEnd()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::StartElement) {
            if (isReqifElement(xml, "ATTRIBUTE-DEFINITION-INTEGER-REF")) {
                defRef = xml.readElementText();
                break;
            }
        }
        else if (token == QXmlStreamReader::EndElement &&
                 isReqifElement(xml, "ATTRIBUTE-VALUE-INTEGER")) {
            break;
        }
    }

    if (!defRef.isEmpty() && defRef.contains("ABSOLUTENUMBER", Qt::CaseInsensitive)) {
        currentReq.sortNum = theValue.toInt();
    }
}

void ReqifParser::parseXhtmlAttribute(QXmlStreamReader &xml, ReqData &currentReq) {
    QString defRef;
    QString theValue;

    while (!xml.atEnd()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::StartElement) {
            if (isReqifElement(xml, "ATTRIBUTE-DEFINITION-XHTML-REF")) {
                defRef = xml.readElementText();
                break;
            }
        }
        else if (token == QXmlStreamReader::EndElement &&
                 isReqifElement(xml, "ATTRIBUTE-VALUE-XHTML")) {
            return;
        }
    }

    while (!xml.atEnd()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::StartElement) {
            if (isReqifElement(xml, "THE-VALUE")) {
                theValue = readXhtmlContent(xml);
                break;
            }
        }
        else if (token == QXmlStreamReader::EndElement &&
                 isReqifElement(xml, "ATTRIBUTE-VALUE-XHTML")) {
            break;
        }
    }

    if (defRef.isEmpty()) return;

    if (defRef.contains("_valm_Name", Qt::CaseInsensitive)) {
        currentReq.name = cleanHtml(theValue);
    }
    else if (defRef.contains("_valm_Description", Qt::CaseInsensitive)) {
        currentReq.description = cleanHtml(theValue);
    }
}

QString ReqifParser::readXhtmlContent(QXmlStreamReader &xml) {
    QString content;
    int depth = 1;

    while (!xml.atEnd() && depth > 0) {
        QXmlStreamReader::TokenType token = xml.readNext();

        switch (token) {
        case QXmlStreamReader::StartElement:
            depth++;
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

QString ReqifParser::cleanHtml(const QString &htmlText) {
    if (htmlText.isEmpty()) {
        return u8"[无内容]";
    }

    QString result = htmlText;

    result.replace(QRegExp("<br\\s*/?>"), "\n");
    result.replace(QRegExp("<div[^>]*>"), "\n\n");
    result.replace(QRegExp("</div>"), "");
    result.replace(QRegExp("<li>"), "• ");
    result.replace(QRegExp("</li>"), "\n");
    result.replace(QRegExp("<[^>]*>"), "");

    result.replace("&amp;", "&");
    result.replace("&lt;", "<");
    result.replace("&gt;", ">");
    result.replace("&quot;", "\"");
    result.replace("&nbsp;", " ");

    return result.trimmed();
}

void ReqifParser::fillTree(QTreeWidget *treeWidget) {
    if (!treeWidget) return;

    treeWidget->clear();
    treeWidget->setHeaderLabels(QStringList() << u8"序号" << u8"需求名称");
    treeWidget->setSortingEnabled(false);

    QMap<QString, QTreeWidgetItem*> itemMap;

    // 先创建所有有效需求的节点
    for (const auto &req : m_reqMap) {
        if (!isValidReq(req)) continue;

        QTreeWidgetItem *item = new QTreeWidgetItem();
        item->setText(0, req.sortNum > 0 ? QString::number(req.sortNum) : "");
        item->setText(1, req.name);
        item->setData(0, Qt::UserRole, req.id);
        itemMap[req.id] = item;
    }

    // 构建层次结构
    for (const auto &req : m_reqMap) {
        if (!isValidReq(req)) continue;

        QTreeWidgetItem *item = itemMap[req.id];

        if (req.parentId.isEmpty() || !itemMap.contains(req.parentId)) {
            treeWidget->addTopLevelItem(item);
        } else {
            itemMap[req.parentId]->addChild(item);
        }
    }

    treeWidget->expandAll();
    treeWidget->resizeColumnToContents(0);
    treeWidget->resizeColumnToContents(1);
}

QString ReqifParser::getReqDescription(const QString &reqId) {
    if (m_reqMap.contains(reqId)) {
        QString desc = m_reqMap[reqId].description;
        return desc.isEmpty() ? u8"[暂无详细描述信息]" : desc;
    }
    return u8"[未找到该需求]";
}

int ReqifParser::getAllReqCount() const {
    return m_reqMap.size();
}

int ReqifParser::getValidReqCount() const {
    int count = 0;
    for (const auto &req : m_reqMap) {
        if (isValidReq(req)) count++;
    }
    return count;
}

bool ReqifParser::isReqifElement(const QXmlStreamReader &xml, const QString &localName) {
    return xml.namespaceUri() == m_reqifNamespace &&
           xml.name().toString().compare(localName, Qt::CaseInsensitive) == 0;
}
