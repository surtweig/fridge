#ifndef XCMX2ALSOURCE_H
#define XCMX2ALSOURCE_H

#include "IProjectGroupItem.h"
#include <QObject>
#include <QByteArray>

class XCMProject;

class XCMSourceFile : public IProjectGroupItem
{
    Q_OBJECT
private:
    XCMProject* ownerProject;
public:
    XCMSourceFile(QObject* parent, QString name);
    QString LoadText();
    void SaveText(QByteArray txt);
    void SaveText(QString txt);
};

#endif // XCMX2ALSOURCE_H
