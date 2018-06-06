#include "XCMX2ALSource.h"
#include "mainwindow.h"
#include <QFile>
#include <QByteArray>

XCMSourceFile::XCMSourceFile(QObject* parent, QString name) :
    IProjectGroupItem(parent, name)
{
    connect(this, &IProjectGroupItem::Selected, reinterpret_cast<MainWindow*>(parent->parent()->parent()), &MainWindow::SourceSelected);

    QFile srcFile(Path());
    if (!srcFile.open(QIODevice::ReadOnly))
    {
        srcFile.open(QIODevice::WriteOnly);
        srcFile.close();
    }
}

QString XCMSourceFile::LoadText()
{
    QFile srcFile(Path());
    if (srcFile.open(QIODevice::ReadOnly))
    {
        QString txt = QString::fromUtf8(srcFile.readAll());
        srcFile.close();
        return txt;
    }
    return "Error loading '" + Path() + "'.";
}

void XCMSourceFile::SaveText(QString txt)
{
    SaveText(txt.toUtf8());
}

void XCMSourceFile::SaveText(QByteArray txt)
{
    QFile srcFile(Path());
    if (srcFile.open(QIODevice::WriteOnly))
    {
        srcFile.write(txt);
        srcFile.close();
    }
}
