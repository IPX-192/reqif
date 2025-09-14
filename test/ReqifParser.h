#ifndef REQIFPARSER_H
#define REQIFPARSER_H

#include <QObject>
#include <QMap>
#include <QList>
#include <QTreeWidget>
#include <QXmlStreamReader>
#include <QString>
#include <QSet>
#include <QMessageBox>

// 需求数据结构（仅保留核心字段）
struct ReqData {
    QString id;                  // 需求唯一ID
    QString name;                // 需求名称
    QString description;         // 需求描述
    int sortNum = 0;             // 排序号
    int level = 1;               // 需求层级（1为顶层）
    QString parentId;            // 父需求ID（空表示顶层）
};

class ReqifParser : public QObject
{
    Q_OBJECT
public:
    explicit ReqifParser(QObject *parent = nullptr);
    bool load(const QString &filePath);                  // 加载并解析ReqIF文件
    void fillTree(QTreeWidget *treeWidget);              // 填充需求树到UI
    QString getReqDescription(const QString &reqId);     // 根据ID获取需求描述
    int getAllReqCount() const;                          // 获取总需求数
    int getValidReqCount() const;                        // 获取有效需求数（非空名称）

private:
    // 核心解析方法
    bool parseXml(const QString &xmlPath);               // 解析XML文件
    void parseHierarchy(QXmlStreamReader &xml, const QString &parentId); // 递归解析层次结构

    // 属性解析方法
    void parseIntegerAttribute(QXmlStreamReader &xml, ReqData &currentReq); // 解析排序号
    void parseXhtmlAttribute(QXmlStreamReader &xml, ReqData &currentReq);  // 解析名称/描述

    // 层次结构辅助处理
    void inferHierarchyFromSortNumbers();                // 从排序号推断层次
    void updateTopLevelReqs();                           // 更新顶层需求列表
    int calculateLevel(const QString &reqId);            // 计算需求层级（防循环）

    // 工具方法
    bool isValidReq(const ReqData &req) const;           // 判断需求是否有效
    QString cleanHtml(const QString &htmlText);          // 清理HTML标签
    QString readXhtmlContent(QXmlStreamReader &xml);     // 读取嵌套XHTML内容
    bool isReqifElement(const QXmlStreamReader &xml, const QString &localName); // 判断ReqIF元素

private:
    QMap<QString, ReqData> m_reqMap;       // 需求存储（ID -> 需求数据）
    QMap<QString, QString> m_parentMap;    // 父子关系（子ID -> 父ID）
    QList<QString> m_topReqIds;            // 顶层需求ID列表
    QString m_reqifNamespace;              // ReqIF标准命名空间
};

#endif // REQIFPARSER_H
