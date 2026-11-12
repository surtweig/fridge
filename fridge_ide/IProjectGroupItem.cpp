#include "IProjectGroupItem.h"

IProjectGroupItem::IProjectGroupItem(QObject *parent, QString name) : QObject(parent)
{
    this->name = name;
    itemParent = dynamic_cast<IProjectGroupItem*>(parent);
}

QString IProjectGroupItem::Path()
{
    return itemParent->Path() + "/" + name;
}

void IProjectGroupItem::Select()
{
    emit Selected(this);
}

void IProjectGroupItem::readJson(const QJsonObject &json)
{

}

void IProjectGroupItem::writeJson(QJsonObject &json)
{

}
