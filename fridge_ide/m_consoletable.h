#ifndef M_CONSOLETABLE_H
#define M_CONSOLETABLE_H

#include <QAbstractTableModel>
#include <QList>

struct ConsoleTableMessage
{
    int severity;
    QString sender;
    QString text;
};

class M_ConsoleTable : public QAbstractTableModel
{
    Q_OBJECT
private:
    QList<ConsoleTableMessage> messages;
public:
    M_ConsoleTable(QObject *parent);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void AddMessage(int severity, QString sender, QString text);
    void Clear();
};

#endif // M_CONSOLETABLE_H
