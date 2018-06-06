#ifndef XCMINCLUDEFOLDER_H
#define XCMINCLUDEFOLDER_H

#include <QObject>
#include "XCMProject.h"
#include <QJsonObject>

class XCMIncludeFolder : public XCMProject
{
    Q_OBJECT
public:
    XCMIncludeFolder(QObject* parent, QString name, QString relPath = "");
    QString Path() override;
    QString AbsolutePath();
    void readJson(const QJsonObject &json) override;
    void writeJson(QJsonObject &json) override;

private:
    QString relPath;

    void scanSourceFiles();
};

#endif // XCMINCLUDEFOLDER_H
