#ifndef XCMROMIMAGE_H
#define XCMROMIMAGE_H

#include "IProjectGroupItem.h"
#include <QAbstractTableModel>
#include <QList>

class XCMProjectGroup;
class XCMBINFile;

class M_AvailableSectionsList : public QAbstractTableModel
{
    Q_OBJECT
public:
    QList<XCMBINFile*> sections;

    M_AvailableSectionsList(QObject *parent);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void BeginInsert(int index);
    void BeginRemove(int index);
    void EndInsert();
    void EndRemove();
};

class M_ROMSectionsList : public QAbstractTableModel
{
    Q_OBJECT
public:
    QList<XCMBINFile*> sections;

    M_ROMSectionsList(QObject *parent);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void BeginInsert(int index);
    void BeginRemove(int index);
    void BeginMove(int index, int newIndex);
    void EndInsert();
    void EndRemove();
    void EndMove();
};

class XCMROMImage : public IProjectGroupItem
{
    Q_OBJECT
public:
    XCMROMImage(QObject *parent, QString name);
    void AddSectionToROMList(int availIndex);
    void RemoveSectionFromROMList(int romIndex);
    void MoveSectionInROMList(int romIndex, bool up);
    inline M_AvailableSectionsList* AvailableSectionsList() { return &availableSectionsList; }
    inline M_ROMSectionsList* ROMSectionsList() { return &romSectionsList; }
    void Build(QString outputPath);

    void readJson(const QJsonObject &json) override;
    void writeJson(QJsonObject &json) override;

signals:
    void BuilderLog(QString sender, std::ostringstream* stream);

private:
    XCMProjectGroup* projectGroup;
    M_AvailableSectionsList availableSectionsList;
    M_ROMSectionsList romSectionsList;
};

#endif // XCMROMIMAGE_H
