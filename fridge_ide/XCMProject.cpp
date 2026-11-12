#include "XCMProject.h"
#include "mainwindow.h"
#include "XCMX2ALSource.h"
#include <QJsonArray>
#include "XCM2AssemblyLanguageCompiler.h"
#include <ostream>

XCMProject::XCMProject(QObject* parent, QString name) : IProjectGroupItem(parent, name)
{
    MainWindow* wnd = reinterpret_cast<MainWindow*>(parent->parent());
    connect(this, &IProjectGroupItem::Selected, wnd, &MainWindow::ProjectSelected);
    connect(this, &XCMProject::BuilderLog, wnd, &MainWindow::ParseLogStream);
}

void XCMProject::Init()
{
    QDir dir(Path());
    if (!dir.exists())
        initAsNew();
}

void XCMProject::initAsNew()
{
    QDir(itemParent->Path()).mkdir(name);

    XCMSourceFile* incsrc = AddSourceFile(name + ".inc");
    incsrc->SaveText(
        "// " + name + ".inc\n"
        "// This is file is automatically generated on each build."
    );

    XCMSourceFile* mainsrc = AddSourceFile(name + ".x2al");
    mainsrc->SaveText(
        "include \"" + name + ".inc\"\n"
        "include \"stdapp.inc\"\n"
        "\n"
        "// Defines the main entry point of " + name + "\n"
        "main " + name + "\n"
    );
}

void XCMProject::Select()
{
    IProjectGroupItem::Select();
}

void XCMProject::readJson(const QJsonObject &json)
{
    QJsonArray srcsjson = json["sources"].toArray();
    for (int isrc = 0; isrc < srcsjson.size(); isrc++)
    {
        AddSourceFile(srcsjson[isrc].toString());
    }
}

void XCMProject::writeJson(QJsonObject &json)
{
    QJsonArray srcsjson;
    for (QMap<QString, XCMSourceFile*>::iterator isrc = sources.begin(); isrc != sources.end(); ++isrc)
    {
        srcsjson.append(isrc.value()->Name());
    }
    json["sources"] = srcsjson;
}

XCMSourceFile* XCMProject::AddSourceFile(QString name)
{
    XCMSourceFile* src = new XCMSourceFile(this, name);
    sources[name] = src;
    return src;
}

void XCMProject::FillUIModel(QStandardItem* projItem)
{
    for (QMap<QString, XCMSourceFile*>::iterator isrc = sources.begin(); isrc != sources.end(); ++isrc)
    {
        QStandardItem* srcitem = new QStandardItem(isrc.value()->Name());
        srcitem->setData(QVariant::fromValue(isrc.value()), Qt::UserRole+1);
        projItem->appendRow(srcitem);
    }
}

void XCMProject::Build(QString outputPath, vector<string>& incfolders)
{
    ostringstream buildout;
    sstreamWrapper builderrstr(&buildout);
    XCM2AssemblyLanguageCompiler x2alcomp(Path().toStdString() + "/", (Name() + ".x2al").toStdString(), (outputPath + "/" + Name() + BIN_FILE_EXT).toStdString(), incfolders, &builderrstr);
    BuilderLog(Name(), &buildout);

    ofstream compout;
    compout.open((outputPath + "/" + Name() + ".txt").toStdString());
    coutStreamWrapper compiled(&compout);
    x2alcomp.printCompiled(&compiled);
}

XCMProject::~XCMProject()
{
    for (QMap<QString, XCMSourceFile*>::iterator isrc = sources.begin(); isrc != sources.end(); ++isrc)
        delete isrc.value();
}
