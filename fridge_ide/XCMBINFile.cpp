#include "XCMBINFile.h"
#include "XCMProjectGroup.h"
#include <QFile>

XCMBINFile::XCMBINFile(QObject* parent, QString name, XCMProject* ownerProject) :
    IProjectGroupItem(parent, name)
{
    this->ownerProject = ownerProject;
}

QString XCMBINFile::Path()
{
    return itemParent->Path() + "/" + BIN_FOLDER + "/" + Name();
}

void XCMBINFile::Export(QString fileName)
{
    QFile f(Path());
    QFile dest(fileName);
    if (dest.exists())
        dest.remove();
    f.copy(fileName);
}
