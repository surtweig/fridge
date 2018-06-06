#ifndef IPROJECTGROUPITEM_H
#define IPROJECTGROUPITEM_H

#include <QObject>
#include <QMetaType>

class IProjectGroupItem : public QObject
{
    Q_OBJECT
protected:
    QString name;
    IProjectGroupItem* itemParent;
public:
    explicit IProjectGroupItem(QObject *parent, QString name);
    inline QString Name() { return name; }
    virtual QString Path();
    virtual void Select();
    virtual void readJson(const QJsonObject &json);
    virtual void writeJson(QJsonObject &json);

signals:
    void Selected(IProjectGroupItem* item);
    void OnLogMessage(QString sender, QString text);
    void OnLogWarning(QString sender, QString text);
    void OnLogError(QString sender, QString text);
};

Q_DECLARE_METATYPE(IProjectGroupItem*)

#endif // IPROJECTGROUPITEM_H
