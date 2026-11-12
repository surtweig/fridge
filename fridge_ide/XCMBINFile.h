#ifndef XCMBINFILE_H
#define XCMBINFILE_H

#include "IProjectGroupItem.h"
#include "XCMProject.h"
#include <QObject>

class XCMBINFile : public IProjectGroupItem
{
    Q_OBJECT
private:
    XCMProject* ownerProject;

public:
    XCMBINFile(QObject* parent, QString name, XCMProject* ownerProject);
    void Export(QString fileName);
    QString Path() override;
};

#endif // XCMBINFILE_H
