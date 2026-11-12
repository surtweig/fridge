#include "XCMIncludeFolder.h"
#include <QDir>
#include <QStringList>

XCMIncludeFolder::XCMIncludeFolder(QObject* parent, QString name, QString relPath) :
    XCMProject(parent, name)
{
    this->relPath = relPath;
    if (relPath.size() > 0)
        scanSourceFiles();
}

void XCMIncludeFolder::scanSourceFiles()
{
    QDir dir(Path());
    QStringList srcFilters({"*.inc", "*.x2al"});
    QStringList lst = dir.entryList(srcFilters, QDir::Files);
    for (int i = 0; i < lst.size(); i++)
    {
        AddSourceFile(lst[i]);
    }
}

QString XCMIncludeFolder::AbsolutePath()
{
    QDir dir(Path());
    return dir.absolutePath();
}

QString XCMIncludeFolder::Path()
{
     return itemParent->Path() + "/" + relPath;
}

void XCMIncludeFolder::readJson(const QJsonObject &json)
{
    relPath = json["relPath"].toString();
    scanSourceFiles();
    XCMProject::readJson(json);
}

void XCMIncludeFolder::writeJson(QJsonObject &json)
{
    XCMProject::writeJson(json);
    json["relPath"] = relPath;
}

