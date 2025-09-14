#ifndef REQIFPARSER_H
#define REQIFPARSER_H

#include <QObject>
#include <QMap>
#include <QList>
#include <QTreeWidget>
#include <QXmlStreamReader>
#include <QString>
#include <QSet>

// 需求数据结构
struct ReqData {
    QString id;                  // 需求ID
    QString name;                // 需求名称
    QString description;         // 需求描述
    int sortNum = 0;             // 排序号
    int level = 1;               // 层级
    QString parentId;            // 父需求ID
    QString creationDate;        // 创建日期
    QString modificationDate;    // 修改日期
    QMap<QString, QString> enumAttributes;  // 枚举属性
    QMap<QString, bool> booleanAttributes;  // 布尔属性
};

class ReqifParser : public QObject
{
    Q_OBJECT
public:
    explicit ReqifParser(QObject *parent = nullptr);
    bool load(const QString &filePath);                  // 加载并解析ReqIF文件
    void fillTree(QTreeWidget *treeWidget);              // 填充需求树
    QString getReqDescription(const QString &reqId);     // 获取需求描述
    int getAllReqCount() const;                          // 获取需求总数
    int getValidReqCount() const;                          // 获取需求总数

private:
    // 核心解析方法
    bool parseXml(const QString &xmlPath);               // 解析XML文件
    void parseSpecifications(QXmlStreamReader &xml);     // 解析规格部分
    void parseHierarchy(QXmlStreamReader &xml, const QString &parentId); // 解析层次结构

    // 属性解析方法（修复后）
    void parseIntegerAttribute(QXmlStreamReader &xml, ReqData &currentReq);
    void parseXhtmlAttribute(QXmlStreamReader &xml, ReqData &currentReq);
    void parseDateAttribute(QXmlStreamReader &xml, ReqData &currentReq);
    void parseEnumerationAttribute(QXmlStreamReader &xml, ReqData &currentReq);
    void parseBooleanAttribute(QXmlStreamReader &xml, ReqData &currentReq);

    // 辅助方法
    void inferHierarchyFromSortNumbers();                // 从排序号推断层次结构
    int calculateLevel(const QString &reqId);            // 计算需求层级
    QString cleanHtml(const QString &htmlText);          // 修复后的HTML清理
    QString readXhtmlContent(QXmlStreamReader &xml);     // 读取XHTML内容（修复嵌套问题）

    // 命名空间判断（优化后）
    bool isReqifElement(const QXmlStreamReader &xml, const QString &localName);
    bool isXhtmlElement(const QXmlStreamReader &xml, const QString &localName);

private:
    QMap<QString, ReqData> m_reqMap;       // 存储所有需求（ID到数据的映射）
    QMap<QString, QString> m_parentMap;    // 存储需求父子关系（子ID到父ID的映射）
    QList<QString> m_topReqIds;            // 顶层需求ID列表
    QString m_reqifNamespace;              // ReqIF命名空间URI（优化处理）
    QString m_xhtmlNamespace;              // XHTML命名空间URI（优化处理）
};

#endif // REQIFPARSER_H
