#ifndef REQIFPARSER_H
#define REQIFPARSER_H

#include <QObject>
#include <QMap>
#include <QList>
#include <QTreeWidget>
#include <QXmlStreamReader>
#include <QString>
#include <QSet>

// 精简的需求数据结构
struct ReqData {
    QString id;                  // 需求ID
    QString name;                // 需求名称
    QString description;         // 需求描述
    int sortNum = 0;             // 排序号
    int level = 1;               // 层级
    QString parentId;            // 父需求ID
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
    int getValidReqCount() const;                        // 获取有效需求数量（过滤后）

private:
    bool parseXml(const QString &xmlPath);               // 解析XML文件
    void parseHierarchy(QXmlStreamReader &xml, const QString &parentId); // 解析层次结构
    void parseIntegerAttribute(QXmlStreamReader &xml, ReqData &currentReq);
    void parseXhtmlAttribute(QXmlStreamReader &xml, ReqData &currentReq);
    void inferHierarchyFromSortNumbers();                // 从排序号推断层次结构
    void updateTopLevelReqs();                           // 更新顶层需求列表
    int calculateLevel(const QString &reqId);            // 计算需求层级
    bool isValidReq(const ReqData &req) const;           // 检查是否为有效需求
    QString cleanHtml(const QString &htmlText);          // HTML清理
    QString readXhtmlContent(QXmlStreamReader &xml);     // 读取XHTML内容
    bool isReqifElement(const QXmlStreamReader &xml, const QString &localName);

private:
    QMap<QString, ReqData> m_reqMap;       // 存储所有需求
    QMap<QString, QString> m_parentMap;    // 存储需求父子关系
    QList<QString> m_topReqIds;            // 顶层需求ID列表
    QString m_reqifNamespace;              // ReqIF命名空间URI
};

#endif // REQIFPARSER_H
