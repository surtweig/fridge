#include "m_consoletable.h"
#include <QColor>
#include <QBrush>
#include <QTime>

M_ConsoleTable::M_ConsoleTable(QObject *parent) : QAbstractTableModel(parent)
{
}

void M_ConsoleTable::Clear()
{
    beginResetModel();
    messages.clear();
    endResetModel();
}

void M_ConsoleTable::AddMessage(int severity, QString sender, QString text)
{
    //beginResetModel();
    beginInsertRows(QModelIndex(), messages.size(), messages.size());
    messages.append({severity, sender, text});
    //QModelIndex firstCell = createIndex(messages.size()-1, 0);
    //QModelIndex lastCell = createIndex(messages.size()-1, 2);
    //endResetModel();
    //dataChanged(firstCell, lastCell);
    //emit dataChanged(firstCell, lastCell);
    endInsertRows();
}

int M_ConsoleTable::rowCount(const QModelIndex &parent) const
{
    return messages.size();
}

int M_ConsoleTable::columnCount(const QModelIndex &parent) const
{
    return 3;
}

QVariant M_ConsoleTable::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    int col = index.column();

    if (role == Qt::DisplayRole)
    {
        switch (col)
        {
        case 0:

            switch (messages.at(row).severity)
            {
                case 1:
                    return "Warning";
                case 2:
                    return "Error";
            }
            break;

        case 1:
            return messages.at(row).sender;
        case 2:
            return messages.at(row).text;
        }
    }

    if (role == Qt::BackgroundRole)
    {
        int severity = messages.at(row).severity;
        if (severity >= 2)
            return QBrush(QColor(255, 200, 200));
        else if (severity == 1)
            return QBrush(QColor(255, 255, 200));
    }

    if (role == Qt::TextAlignmentRole)
    {
        return Qt::AlignLeft;
    }
    return QVariant();
}

QVariant M_ConsoleTable::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole)
    {
        if (orientation == Qt::Horizontal) {
            switch (section)
            {
            case 0:
                return QString("Severity");
            case 1:
                return QString("Sender");
            case 2:
                return QString("Message");
            }
        }
    }
    if (role == Qt::TextAlignmentRole)
    {
        return Qt::AlignLeft;
    }
    return QVariant();
}
