#ifndef XCMPROJECT_H
#define XCMPROJECT_H
#include <QString>
#include "IProjectGroupItem.h"
#include <QMap>
#include <QStandardItem>
#include <vector>

using namespace std;

class XCMSourceFile;

class XCMProject : public IProjectGroupItem
{
    Q_OBJECT
protected:
    QMap<QString, XCMSourceFile*> sources;
    void initAsNew();
signals:
    void BuilderLog(QString sender, std::ostringstream* stream);
public:
    XCMProject(QObject* parent, QString name);
    void Init();
    void Select() override;

    XCMSourceFile* AddSourceFile(QString name);

    void readJson(const QJsonObject &json) override;
    void writeJson(QJsonObject &json) override;
    void FillUIModel(QStandardItem* projItem);

    void Build(QString outputPath, vector<string>& incfolders);

    ~XCMProject();
};

#endif // XCMPROJECT_H
