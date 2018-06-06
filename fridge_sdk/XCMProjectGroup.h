#ifndef XCMPROJECTGROUP_H
#define XCMPROJECTGROUP_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include "XCMProject.h"
#include "XCMROMImage.h"
#include "XCMBINFile.h"
#include <QMap>
#include <QDir>
#include <QStandardItemModel>
#include "IProjectGroupItem.h"
#include "XCMIncludeFolder.h"
#include <QList>

#define CONFIG_JSON_FILE "projectgroup.json"
#define BIN_FOLDER "bin"
#define ROM_FILE_EXT ".rom"
#define BIN_FILE_EXT ".bin"
#define BIN_BOOT_FILE "boot.bin"

class XCMProjectGroup : public IProjectGroupItem
{
    Q_OBJECT
private:
    QString rootPath;
    QString binPath;
    QJsonObject* config;
    QMap<QString, XCMProject*> projects;
    QMap<QString, XCMIncludeFolder*> incfolders;
    XCMROMImage* romimage;
    QMap<QString, XCMBINFile*> binoutputs;
    QDir directory;
    QStandardItemModel* uimodel;

    void initAsNew();
    void initAsExisting();

    void initDefaultConfig();
    void saveConfig();
    void loadConfig();

public:
    QString binBootLoader;
    QString exportDirectory;


    XCMProjectGroup(QObject *parent, QString name, QString path);

    inline QString Name() { return name; }
    inline QString RootPath() { return rootPath; }
    inline QString BINPath() { return binPath; }
    QString Path() override;
    inline QDir Directory() { return directory; }
    QStandardItemModel* BuildUIModel();
    inline XCMROMImage* ROMImage() { return romimage; }
    QList<XCMBINFile*> GetBINFiles();
    XCMBINFile* GetBINFile(QString name);

    XCMProject* AddProject(QString projname);
    XCMIncludeFolder* AddIncludeFolder(QString name, QString relpath = "");
    void SaveAll();
    void Select() override;

    void readJson(const QJsonObject &json) override;
    void writeJson(QJsonObject &json) override;

    void Build();

    ~XCMProjectGroup();
};

#endif // XCMPROJECTGROUP_H
