#include "XCMROMImage.h"
#include "XCMProjectGroup.h"
#include "XCM2ROMImageBuilder.h"
#include <vector>
#include <ostream>
#include <QDebug>
#include <QJsonArray>
#include <QList>

XCMROMImage::XCMROMImage(QObject *parent, QString name) :
    IProjectGroupItem(parent, name),
    availableSectionsList(this),
    romSectionsList(this)
{
    projectGroup = qobject_cast<XCMProjectGroup*>(parent);

}

void XCMROMImage::AddSectionToROMList(int availIndex)
{
    romSectionsList.BeginInsert(romSectionsList.sections.size());
    availableSectionsList.BeginRemove(availIndex);

    XCMBINFile* section = availableSectionsList.sections.at(availIndex);
    availableSectionsList.sections.removeAt(availIndex);
    romSectionsList.sections.append(section);

    availableSectionsList.EndRemove();
    romSectionsList.EndInsert();
}

void XCMROMImage::RemoveSectionFromROMList(int romIndex)
{
    romSectionsList.BeginRemove(romIndex);
    availableSectionsList.BeginInsert(availableSectionsList.sections.size());

    XCMBINFile* section = romSectionsList.sections.at(romIndex);
    romSectionsList.sections.removeAt(romIndex);
    availableSectionsList.sections.append(section);

    availableSectionsList.EndInsert();
    romSectionsList.EndRemove();
}

void XCMROMImage::MoveSectionInROMList(int romIndex, bool up)
{
    if (romIndex == 0 && up) return;
    if (romIndex == romSectionsList.sections.size()-1 && !up) return;
    int newIndex = up ? romIndex-1 : romIndex+1;
    //romSectionsList.BeginMove(romIndex, newIndex);
    romSectionsList.sections.move(romIndex, newIndex);
    //romSectionsList.EndMove();
}

void XCMROMImage::Build(QString outputPath)
{
    vector<std::string> binfiles;
    for (int i = 0; i < romSectionsList.sections.size(); i++)
        binfiles.push_back(romSectionsList.sections[i]->Path().toStdString());
    ostringstream buildout;
    sstreamWrapper builderrstr(&buildout);
    if (binfiles.size() > 0)
        XCM2ROMImageBuilder(binfiles, (outputPath + "/" + Name()).toStdString(), &builderrstr);
    else
        builderrstr.Append("[WARNING] ROM sections are not specified. No ROM image file generated.\n");
    BuilderLog("ROM builder", &buildout);
}

void XCMROMImage::readJson(const QJsonObject &json)
{
    QJsonArray jsonROMSections = json["sections"].toArray();
    romSectionsList.BeginInsert(0);
    for (int i = 0; i < jsonROMSections.size(); i++)
    {
        XCMBINFile* binoutput = projectGroup->GetBINFile(jsonROMSections[i].toString());
        if (binoutput)
        {
            romSectionsList.sections.append(binoutput);
            availableSectionsList.sections.removeOne(binoutput);
        }
    }
    romSectionsList.EndInsert();
}

void XCMROMImage::writeJson(QJsonObject &json)
{
    QJsonArray jsonROMSections;
    for (int i = 0; i < romSectionsList.sections.size(); i++)
        jsonROMSections.append(romSectionsList.sections[i]->Name());
    json["sections"] = jsonROMSections;
}


M_AvailableSectionsList::M_AvailableSectionsList(QObject *parent)
{

}

int M_AvailableSectionsList::rowCount(const QModelIndex &parent) const
{
    return sections.size();
}

int M_AvailableSectionsList::columnCount(const QModelIndex &parent) const
{
    return 1;
}

QVariant M_AvailableSectionsList::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if (role == Qt::DisplayRole)
    {
        return sections.at(row)->Name();
    }
    if (role == Qt::TextAlignmentRole)
    {
        return Qt::AlignLeft;
    }
    return QVariant();
}

QVariant M_AvailableSectionsList::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole)
    {
        return "Name";
    }
    if (role == Qt::TextAlignmentRole)
    {
        return Qt::AlignLeft;
    }
    return QVariant();
}

void M_AvailableSectionsList::BeginInsert(int index)
{
    beginInsertRows(QModelIndex(), index, index);
}

void M_AvailableSectionsList::BeginRemove(int index)
{
    beginRemoveRows(QModelIndex(), index, index);
}

void M_AvailableSectionsList::EndInsert()
{
    endInsertRows();
}

void M_AvailableSectionsList::EndRemove()
{
    endRemoveRows();
}

M_ROMSectionsList::M_ROMSectionsList(QObject *parent)
{

}

int M_ROMSectionsList::rowCount(const QModelIndex &parent) const
{
    return sections.size();
}

int M_ROMSectionsList::columnCount(const QModelIndex &parent) const
{
    return 2;
}

QVariant M_ROMSectionsList::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    int col = index.column();
    if (role == Qt::DisplayRole)
    {
        if (col == 0)
        {
            //QString posstr;
            //posstr.setNum(row, base = 16);
            return QString("0x%1").arg(row, 2, 16, QLatin1Char( '0' ));
        }
        else
            return sections.at(row)->Name();
    }
    if (role == Qt::TextAlignmentRole)
    {
        return Qt::AlignLeft;
    }
    return QVariant();
}

QVariant M_ROMSectionsList::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole)
    {
        if (section == 0)
            return "Index";
        else
            return "Name";
    }
    if (role == Qt::TextAlignmentRole)
    {
        return Qt::AlignLeft;
    }
    return QVariant();
}

void M_ROMSectionsList::BeginInsert(int index)
{
    beginInsertRows(QModelIndex(), index, index);
}

void M_ROMSectionsList::BeginRemove(int index)
{
    beginRemoveRows(QModelIndex(), index, index);
}

void M_ROMSectionsList::EndInsert()
{
    endInsertRows();
}

void M_ROMSectionsList::EndRemove()
{
    endRemoveRows();
}

void M_ROMSectionsList::BeginMove(int index, int newIndex)
{
    beginMoveRows(QModelIndex(), index, index, QModelIndex(), newIndex);
}

void M_ROMSectionsList::EndMove()
{
    endMoveRows();
}
