#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "m_consoletable.h"
#include "XCMProjectGroup.h"
#include "XCMX2ALSource.h"
#include <QItemSelectionModel>
#include "Scintilla.h"
#include "ScintillaEdit.h"
#include "SciLexer.h"
#include <QMap>
#include <QProcess>

#define SCI_COLOR_RGB(r,g,b) r | (g << 8) | (b << 16)
#define EMULATOR_EXE "emulator.exe"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

public slots:
    void CreateNewProjectGroup(QString name, QString path);
    void AddProject(QString name);
    void AddX2ALSource(QString name);
    void AddIncludeFolder(QString path);
    void ProjectSelected(IProjectGroupItem* project);
    void SourceSelected(IProjectGroupItem* source);
    void LogMessage(QString sender, QString text);
    void LogWarning(QString sender, QString text);
    void LogError(QString sender, QString text);
    void ParseLogStream(QString sender, std::ostringstream* stream);
    void ClearLog();

signals:
    void ProjectGroupChanged(XCMProjectGroup *projectGroup);

private slots:
    void on_actionNew_triggered();
    void on_actionExit_triggered();
    void on_ProjectGroupChanged();
    void on_actionNewProjectGroup_triggered();
    void on_ProjectTreeViewSelection(const QItemSelection &newSelection, const QItemSelection &oldSelection);
    void on_actionOpenProjectGroup_triggered();
    void on_actionSave_all_triggered();
    void on_BuildAddROMSectionBtn_clicked();
    void on_BuildRemoveROMSectionBtn_clicked();
    void on_BuildMoveUpROMSectionBtn_clicked();
    void on_BuildMoveDownROMSectionBtn_clicked();
    void on_BuildEmulatorFolderBrowseBtn_clicked();
    void on_actionBuild_project_triggered();
    void on_ConsoleTableView_clicked(const QModelIndex &index);
    void on_BuildOutputFolderEdit_textChanged(const QString &arg1);
    void on_BuildBootLoaderProjectComboBox_currentIndexChanged(const QString &arg1);
    void on_actionBuild_and_run_emulator_triggered();
    void on_emulatorStdOut();
    void on_emulatorFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    Ui::MainWindow *ui;
    M_ConsoleTable *console;
    XCMProjectGroup *projectGroup;
    XCMProject* selectedProject;
    QMap<IProjectGroupItem*, ScintillaEdit*> sourceEdits;
    QProcess* emulator;

    void initConsoleTable();
    void refreshOutputTab();
    ScintillaEdit* createSciEditX2AL();
};

#endif // MAINWINDOW_H
