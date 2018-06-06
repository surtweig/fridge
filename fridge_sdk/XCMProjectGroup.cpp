#include "XCMProjectGroup.h"
#include <QDir>
#include <QFile>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <vector>
#include <string>

XCMProjectGroup::XCMProjectGroup(QObject *parent, QString name, QString path) :
    IProjectGroupItem(parent, name),
    directory(path + "/" + name)
{
    this->rootPath = path;
    uimodel = NULL;
    exportDirectory = "";
    binBootLoader = "<None>";

    if (directory.exists())
        initAsExisting();
    else
        initAsNew();

    binPath = Path() + "/" + BIN_FOLDER;
}

QString XCMProjectGroup::Path()
{
    return directory.absolutePath();
}

void XCMProjectGroup::initAsNew()
{
    QDir rootdir(rootPath);
    rootdir.mkdir(name);
    rootdir.cd(name);
    rootdir.mkdir(BIN_FOLDER);

    //directory = QDir(rootPath + "/" + name);
    initDefaultConfig();
    saveConfig();

    romimage = new XCMROMImage(this, name + ROM_FILE_EXT);
}

void XCMProjectGroup::initAsExisting()
{
    loadConfig();
}

void XCMProjectGroup::initDefaultConfig()
{
}

void XCMProjectGroup::saveConfig()
{
    QFile jsonFile(directory.absolutePath() + "/" + QStringLiteral(CONFIG_JSON_FILE));
    if (!jsonFile.open(QIODevice::WriteOnly))
    {
        OnLogError(name, "Cannot save project group config json.");
        jsonFile.close();
        return;
    }
    QJsonObject config;
    writeJson(config);
    QJsonDocument saveDoc(config);
    jsonFile.write(saveDoc.toJson());
    jsonFile.close();
}

void XCMProjectGroup::loadConfig()
{
    QFile jsonFile(directory.absolutePath() + "/" + QStringLiteral(CONFIG_JSON_FILE));

    if (!jsonFile.open(QIODevice::ReadOnly))
    {
        OnLogError(name, "Cannot load project group config json.");
        jsonFile.close();
        return;
    }

    QByteArray saveData = jsonFile.readAll();
    QJsonDocument loadDoc(QJsonDocument::fromJson(saveData));

    readJson(loadDoc.object());
    jsonFile.close();
}

void XCMProjectGroup::readJson(const QJsonObject &json)
{
    name = json["name"].toString();
    QJsonObject projectsNode = json["projects"].toObject();
    for (QJsonObject::iterator iproj = projectsNode.begin(); iproj != projectsNode.end(); ++iproj)
    {
        XCMProject* proj = AddProject(iproj.key());
        proj->readJson(iproj.value().toObject());
    }
    QJsonObject incfoldersNode = json["includeFolders"].toObject();
    for (QJsonObject::iterator iinc = incfoldersNode.begin(); iinc != incfoldersNode.end(); ++iinc)
    {
        XCMIncludeFolder* inc = AddIncludeFolder(iinc.key());
        inc->readJson(iinc.value().toObject());
    }
    romimage = new XCMROMImage(this, name + ROM_FILE_EXT);
    romimage->readJson(json["romimage"].toObject());
    binBootLoader = json["binBootLoader"].toString();
    exportDirectory = json["exportDirectory"].toString();
}

void XCMProjectGroup::writeJson(QJsonObject &json)
{
    json["name"] = name;
    QJsonObject projectsNode;
    for (QMap<QString, XCMProject*>::iterator iproj = projects.begin(); iproj != projects.end(); ++iproj)
    {
        QJsonObject proj;
        iproj.value()->writeJson(proj);
        projectsNode[iproj.key()] = proj;
    }
    json["projects"] = projectsNode;

    QJsonObject incfoldersNode;
    for (QMap<QString, XCMIncludeFolder*>::iterator iinc = incfolders.begin(); iinc != incfolders.end(); ++iinc)
    {
        QJsonObject inc;
        iinc.value()->writeJson(inc);
        incfoldersNode[iinc.key()] = inc;
    }
    json["includeFolders"] = incfoldersNode;

    QJsonObject romImageNode;
    romimage->writeJson(romImageNode);
    json["romimage"] = romImageNode;

    json["exportDirectory"] = exportDirectory;
    json["binBootLoader"] = binBootLoader;
}

XCMProject* XCMProjectGroup::AddProject(QString projname)
{
    QMap<QString, XCMProject*>::iterator iproj = projects.find(projname);
    if (iproj == projects.end())
    {
        XCMProject* proj = new XCMProject(this, projname);
        proj->Init();
        projects[projname] = proj;
        XCMBINFile* projbinout = new XCMBINFile(this, projname + ".bin", proj);
        binoutputs[projbinout->Name()] = projbinout;
        return proj;
    }
    else
        OnLogError(name, "Project '" + projname + "' already exists in this project group.");
    return NULL;
}

XCMIncludeFolder* XCMProjectGroup::AddIncludeFolder(QString name, QString relpath)
{
    QMap<QString, XCMIncludeFolder*>::iterator iinc = incfolders.find(name);
    if (iinc == incfolders.end())
    {
        XCMIncludeFolder* inc = new XCMIncludeFolder(this, name, relpath);
        incfolders[name] = inc;
        return inc;
    }
    else
        OnLogError(name, "Include folder '" + name + "' already exists in this project group.");
    return NULL;
}

void XCMProjectGroup::SaveAll()
{
    saveConfig();
}

QStandardItemModel* XCMProjectGroup::BuildUIModel()
{
    if (uimodel != NULL)
        delete uimodel;
    uimodel = new QStandardItemModel();

    QStandardItem* rootitem = uimodel->invisibleRootItem();
    QStandardItem* pgitem = new QStandardItem(name);
    //pgitem->setData(ProjectGroupTreeItem::Root);

    rootitem->appendRow(pgitem);

    QStandardItem* pjsitem = new QStandardItem("Projects");
    QStandardItem* incnode = new QStandardItem("Include");
    QStandardItem* outitem = new QStandardItem("Output");

    pgitem->appendRow(outitem);
    pgitem->appendRow(pjsitem);
    pgitem->appendRow(incnode);

    for (QMap<QString, XCMProject*>::iterator iproj = projects.begin(); iproj != projects.end(); ++iproj)
    {
        QStandardItem* projitem = new QStandardItem(iproj.key());
        projitem->setData(QVariant::fromValue(iproj.value()), Qt::UserRole+1);
        pjsitem->appendRow(projitem);
        iproj.value()->FillUIModel(projitem);
    }

    for (QMap<QString, XCMIncludeFolder*>::iterator iinc = incfolders.begin(); iinc != incfolders.end(); ++iinc)
    {
        QStandardItem* incitem = new QStandardItem(iinc.key());
        incitem->setData(QVariant::fromValue(iinc.value()), Qt::UserRole+1);
        incnode->appendRow(incitem);
        iinc.value()->FillUIModel(incitem);
    }

    romimage->AvailableSectionsList()->sections.clear();

    for (QMap<QString, XCMBINFile*>::iterator ibin = binoutputs.begin(); ibin != binoutputs.end(); ++ibin)
    {
        QStandardItem* binitem = new QStandardItem(ibin.value()->Name());
        if (romimage->ROMSectionsList()->sections.indexOf(ibin.value()) < 0)
            romimage->AvailableSectionsList()->sections.append(ibin.value());
        outitem->appendRow(binitem);
    }
    return uimodel;
}

QList<XCMBINFile*> XCMProjectGroup::GetBINFiles()
{
    return binoutputs.values();
}

XCMBINFile* XCMProjectGroup::GetBINFile(QString name)
{
    QMap<QString, XCMBINFile*>::iterator ibin = binoutputs.find(name);
    if (ibin == binoutputs.end())
        return nullptr;
    return ibin.value();
}

void XCMProjectGroup::Select()
{
    IProjectGroupItem::Select();
}

void XCMProjectGroup::Build()
{
    std::vector<std::string> incfolderslist;
    for (QMap<QString, XCMIncludeFolder*>::iterator iinc = incfolders.begin(); iinc != incfolders.end(); ++iinc)
        incfolderslist.push_back((iinc.value()->AbsolutePath() + "/").toStdString());

    for (QMap<QString, XCMProject*>::iterator iproj = projects.begin(); iproj != projects.end(); ++iproj)
    {
        iproj.value()->Build(BINPath(), incfolderslist);
    }
    romimage->Build(exportDirectory);

    QMap<QString, XCMBINFile*>::iterator ibootbin = binoutputs.find(binBootLoader);
    if (ibootbin != binoutputs.end())
        ibootbin.value()->Export(exportDirectory + "/" + BIN_BOOT_FILE);
}

XCMProjectGroup::~XCMProjectGroup()
{
    delete uimodel;

    for (QMap<QString, XCMProject*>::iterator iproj = projects.begin(); iproj != projects.end(); ++iproj)
        delete iproj.value();
    for (QMap<QString, XCMIncludeFolder*>::iterator iinc = incfolders.begin(); iinc != incfolders.end(); ++iinc)
        delete iinc.value();
    for (QMap<QString, XCMBINFile*>::iterator ibin = binoutputs.begin(); ibin != binoutputs.end(); ++ibin)
        delete ibin.value();
    delete romimage;
}

