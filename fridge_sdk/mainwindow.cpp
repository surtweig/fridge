#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "XCM2AssemblyLanguageCompiler.h"
#include "NewProjectGroupDialog.h"
#include "NewItemDialog.h"
#include "IProjectGroupItem.h"
#include "XCMX2ALSource.h"
#include <QFileDialog>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    projectGroup = NULL;
    selectedProject = NULL;
    emulator = NULL;
    initConsoleTable();   

    LogMessage("MainWindow", "Welcome to XCM2 Fridge IDE");

    connect(this, &MainWindow::ProjectGroupChanged, this, &MainWindow::on_ProjectGroupChanged);
}

MainWindow::~MainWindow()
{
    delete console;
    delete ui;    
}

void MainWindow::initConsoleTable()
{
    console = new M_ConsoleTable(this);
    ui->ConsoleTableView->setModel(console);
    ui->ConsoleTableView->show();
    ui->ConsoleTableView->setColumnWidth(0, 60);
    ui->ConsoleTableView->setColumnWidth(1, 100);
    ui->ConsoleTableView->setColumnWidth(2, 300);
    ui->ConsoleTableView->setFocusPolicy(Qt::NoFocus);
}

void MainWindow::refreshOutputTab()
{
    if (projectGroup)
    {
        ui->TextEditorTabs->setTabText(ui->TextEditorTabs->indexOf(ui->OutputTab), projectGroup->Name() + " - Output");

        XCMROMImage* img = projectGroup->ROMImage();

        ui->BuildAvailableROMSectionsList->setModel(img->AvailableSectionsList());
        ui->BuildROMSectionsList->setModel(img->ROMSectionsList());

        ui->BuildROMSectionsList->setColumnWidth(0, 50);
        ui->BuildROMSectionsList->setColumnWidth(1, 150);
        ui->BuildROMSectionsList->setFocusPolicy(Qt::ClickFocus);
        qApp->setStyleSheet ( " QTableWidget::item:focus { border: 0px }" );

        ui->BuildOutputFolderEdit->setText(projectGroup->exportDirectory);

        if (ui->BuildOutputFolderEdit->text().size() == 0)
        {
            QDir emulDir("../emulator/x64/Release");
            ui->BuildOutputFolderEdit->setText(emulDir.absolutePath());
        }

        QList<XCMBINFile*> binoutputs = projectGroup->GetBINFiles();
        QString loadedBinBootLoader = projectGroup->binBootLoader;
        ui->BuildBootLoaderProjectComboBox->clear();
        ui->BuildBootLoaderProjectComboBox->addItem("<None>");
        int bootLoaderIndex = 0;
        for (int i = 0; i < binoutputs.size(); i++)
        {
            ui->BuildBootLoaderProjectComboBox->addItem(binoutputs[i]->Name());
            if (binoutputs[i]->Name() == loadedBinBootLoader)
                bootLoaderIndex = i+1;
        }
        ui->BuildBootLoaderProjectComboBox->setCurrentIndex(bootLoaderIndex);
    }
}

void MainWindow::ClearLog()
{
    console->Clear();
    ui->ConsoleTableView->setRowHeight(console->rowCount()-1, 20);
    ui->ConsoleTableView->viewport()->repaint();
}

void MainWindow::LogMessage(QString sender, QString text)
{
    console->AddMessage(0, sender, text);
    ui->ConsoleTableView->setRowHeight(console->rowCount()-1, 20);
    ui->ConsoleTableView->viewport()->repaint();
}

void MainWindow::LogWarning(QString sender, QString text)
{
    console->AddMessage(1, sender, text);
    ui->ConsoleTableView->setRowHeight(console->rowCount()-1, 20);
    ui->ConsoleTableView->viewport()->repaint();
}

void MainWindow::LogError(QString sender, QString text)
{
    console->AddMessage(2, sender, text);
    ui->ConsoleTableView->setRowHeight(console->rowCount()-1, 20);
    ui->ConsoleTableView->viewport()->repaint();
}

void MainWindow::ParseLogStream(QString sender, std::ostringstream* stream)
{
    std::string s = stream->str();
    int pos = 0;
    int cnt = 0;
    std::string errstr("[ERROR]");
    std::string wrnstr("[WARNING]");
    for (int i = 0; i < s.size(); i++)
    {
        cnt++;
        if (s[i] == '\n')
        {
            std::string msg = s.substr(pos, cnt);
            std::size_t errfound = msg.find(errstr);
            std::size_t wrnfound = msg.find(wrnstr);

            if (errfound != std::string::npos)
                LogError(sender, QString::fromLocal8Bit(msg.c_str()));
            else if (wrnfound != std::string::npos)
                LogWarning(sender, QString::fromLocal8Bit(msg.c_str()));
            else
                LogMessage(sender, QString::fromLocal8Bit(msg.c_str()));

            pos += cnt;
            cnt = 0;
        }
    }
}

void MainWindow::on_actionNew_triggered()
{
    NewItemDialog dlg(this, selectedProject ? selectedProject->Name() : "");
    connect(&dlg, &NewItemDialog::NewProjectAdded, this, &MainWindow::AddProject);
    connect(&dlg, &NewItemDialog::NewX2ALSourceAdded, this, &MainWindow::AddX2ALSource);
    connect(&dlg, &NewItemDialog::NewIncludeFolderAdded, this, &MainWindow::AddIncludeFolder);
    dlg.exec();
}

void MainWindow::on_actionExit_triggered()
{
    QApplication::exit();
}

void MainWindow::CreateNewProjectGroup(QString name, QString path)
{
    LogMessage("MainWindow", "Open project group " + name + " at " + path);
    if (projectGroup)
        delete projectGroup;
    projectGroup = new XCMProjectGroup(this, name, path);
    connect(projectGroup, &IProjectGroupItem::OnLogMessage, this, &MainWindow::LogMessage);
    connect(projectGroup, &IProjectGroupItem::OnLogWarning, this, &MainWindow::LogWarning);
    connect(projectGroup, &IProjectGroupItem::OnLogError, this, &MainWindow::LogError);
    connect(projectGroup->ROMImage(), &XCMROMImage::BuilderLog, this, &MainWindow::ParseLogStream);
    ProjectGroupChanged(projectGroup);
    ui->actionNew->setEnabled(true);
}

void MainWindow::on_actionOpenProjectGroup_triggered()
{
    QDir dir(QFileDialog::getExistingDirectory(this, "Select project group folder"));
    QString pgName = dir.dirName();
    dir.cdUp();
    CreateNewProjectGroup(pgName, dir.path());
}

void MainWindow::on_ProjectGroupChanged()
{
    ui->ProjectView->setModel(projectGroup->BuildUIModel());
    QItemSelectionModel* selm = ui->ProjectView->selectionModel();
    connect(selm, SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(on_ProjectTreeViewSelection(QItemSelection,QItemSelection)));
    ui->ProjectView->expandAll();
    refreshOutputTab();
}

void MainWindow::on_actionNewProjectGroup_triggered()
{
    NewProjectGroupDialog dlg(this);
    connect(&dlg, &NewProjectGroupDialog::NewProjectGroupCreated, this, &MainWindow::CreateNewProjectGroup);
    dlg.exec();
}

void MainWindow::AddProject(QString name)
{
    projectGroup->AddProject(name);
    ProjectGroupChanged(projectGroup);
}

void MainWindow::AddX2ALSource(QString name)
{
    XCMSourceFile* src = selectedProject->AddSourceFile(name);

    ProjectGroupChanged(projectGroup);
}

void MainWindow::AddIncludeFolder(QString path)
{
    QDir dir(path);
    projectGroup->AddIncludeFolder(dir.dirName(), projectGroup->Directory().relativeFilePath(path));
    ProjectGroupChanged(projectGroup);
}

void MainWindow::SourceSelected(IProjectGroupItem* source)
{
    QMap<IProjectGroupItem*, ScintillaEdit*>::iterator iedit = sourceEdits.find(source);
    if (iedit == sourceEdits.end())
    {
        ScintillaEdit* sciEdit = createSciEditX2AL();
        sciEdit->setText(((XCMSourceFile*)source)->LoadText().toStdString().c_str());
        ui->TextEditorTabs->addTab(sciEdit, source->Name());
        sourceEdits[source] = sciEdit;
    }

    ui->TextEditorTabs->setCurrentWidget(sourceEdits[source]);
}

const char x2alReservedWords[] = "subroutine entry alias end static offset main endsub mem_origin include unsafe_flow";
const char x2alRegisters[] = "a b c d e h l bc de hl sp af m";
const char x2alMemoryIrs[] = "mov mvi lxi lda sta lhld shld ldax stax xcng push pop xthl sphl";
const char x2alArithmeticIrs[] = "add adi adc aci sub sui sbb sbi inr dcr inx dcx dad daa ana ani ora ori xra xri cmp cpi rlc rrc ral rar cma cmc stc rtc";
const char x2alFlowControlIrs[] = "jmp jnz jz jnc jc jpo jpe jp jm call cnz cz cnc cc cpo cpe cp cm ret rnz rz rnc rc rpo rpe rp rm pchl in out ei di hlt nop";

ScintillaEdit* MainWindow::createSciEditX2AL()
{
    ScintillaEdit* sciEdit = new ScintillaEdit(this);
    sciEdit->setMargins(1);
    sciEdit->setEOLMode(SC_EOL_CRLF);
    sciEdit->setMarginTypeN(0, SC_MARGIN_NUMBER);
    sciEdit->setMarginWidthN(0, 40);
    sciEdit->setMarginBackN(0, SCI_COLOR_RGB(0xA0,0xD0,0xFF));

    sciEdit->setLexer(SCLEX_ASM);
    sciEdit->styleSetSize(STYLE_DEFAULT, 10);
    sciEdit->styleSetFont(STYLE_DEFAULT, "Consolas");
//    sciEdit->styleSetBold(STYLE_DEFAULT, false);
    sciEdit->styleClearAll();
    sciEdit->styleSetFore(SCE_ASM_COMMENT, SCI_COLOR_RGB(0x00,0x80,0x40));
    sciEdit->styleSetFore(SCE_ASM_STRING, SCI_COLOR_RGB(0xFF,0x00,0x00));
    sciEdit->styleSetItalic(SCE_ASM_COMMENT, true);
    //sciEdit->setStyleBits(7);
    sciEdit->setKeyWords(3, x2alReservedWords);
    sciEdit->styleSetBold(SCE_ASM_DIRECTIVE, true);
    sciEdit->styleSetFore(SCE_ASM_DIRECTIVE, SCI_COLOR_RGB(0x00,0x80,0xFF));
    //sciEdit->styleSetBold(0, true);
    //sciEdit->styleSetBack(0, 1000000);//SCI_COLOR_RGB(0xA0,0xFF,0xFF));
    sciEdit->setKeyWords(1, x2alArithmeticIrs);
    sciEdit->styleSetBold(SCE_ASM_MATHINSTRUCTION, true);
    sciEdit->styleSetFore(SCE_ASM_MATHINSTRUCTION, SCI_COLOR_RGB(0xA0,0x00,0x00));
    sciEdit->setKeyWords(2, x2alRegisters);
    sciEdit->styleSetBold(SCE_ASM_REGISTER, true);
    sciEdit->styleSetFore(SCE_ASM_REGISTER, SCI_COLOR_RGB(0x80,0x00,0x80));
    sciEdit->setKeyWords(0, x2alMemoryIrs);
    sciEdit->styleSetBold(SCE_ASM_CPUINSTRUCTION, true);
    sciEdit->setKeyWords(5, x2alFlowControlIrs);
    sciEdit->styleSetBold(SCE_ASM_EXTINSTRUCTION, true);
    sciEdit->styleSetFore(SCE_ASM_EXTINSTRUCTION, SCI_COLOR_RGB(0x00,0x00,0xFF));
    sciEdit->setTabWidth(4);

    sciEdit->styleSetSize(STYLE_LINENUMBER, 10);
    sciEdit->styleSetFont(STYLE_LINENUMBER, "Consolas");
    sciEdit->styleSetFore(STYLE_LINENUMBER, SCI_COLOR_RGB(0x50,0x90,0xA0));
    sciEdit->styleSetBack(STYLE_LINENUMBER, SCI_COLOR_RGB(0xA0,0xD0,0xFF));

    //sciEdit->setKeyWords(4, x2alFlowControlIrs);
    //sciEdit->setKeyWords(5, x2alSpecialIrs);
    return sciEdit;
}

void MainWindow::on_ProjectTreeViewSelection(const QItemSelection &newSelection, const QItemSelection &oldSelection)
{
    const QModelIndex index = ui->ProjectView->selectionModel()->currentIndex();
    QVariant d = index.data(Qt::UserRole+1);
    if (d.isValid() && !d.isNull())
        d.value<IProjectGroupItem*>()->Select();
    else if (ui->ProjectView->model()->data(index, Qt::DisplayRole) == "Output")
        ui->TextEditorTabs->setCurrentWidget(ui->OutputTab);

/*    //find out the hierarchy level of the selected item
    int hierarchyLevel=1;
    QModelIndex seekRoot = index;
    while(seekRoot.parent() != QModelIndex())
    {
        seekRoot = seekRoot.parent();
        hierarchyLevel++;
    }
    setWindowTitle("row = " + QString::number(index.row()) + " hierarchyLevel = " + QString::number(hierarchyLevel));*/
}

void MainWindow::ProjectSelected(IProjectGroupItem* project)
{
    selectedProject = reinterpret_cast<XCMProject*>(project);
}


void MainWindow::on_actionSave_all_triggered()
{
    projectGroup->SaveAll();
    for (QMap<IProjectGroupItem*, ScintillaEdit*>::iterator iedit = sourceEdits.begin(); iedit != sourceEdits.end(); ++iedit)
    {
        XCMSourceFile* srcf = dynamic_cast<XCMSourceFile*>(iedit.key());
        if (srcf)
        {
            iedit.value()->convertEOLs(SC_EOL_CRLF);
            sptr_t textlength = iedit.value()->textLength();
            QByteArray textbytes = iedit.value()->getText(textlength+1);
            textbytes.truncate(textlength);
            srcf->SaveText(textbytes);
        }
    }
}

void MainWindow::on_BuildAddROMSectionBtn_clicked()
{
    if (!projectGroup) return;

    int index = ui->BuildAvailableROMSectionsList->currentIndex().row();
    if (index >= 0)
    {

        projectGroup->ROMImage()->AddSectionToROMList(index);
        ui->BuildAvailableROMSectionsList->viewport()->repaint();
        ui->BuildROMSectionsList->viewport()->repaint();
    }
}

void MainWindow::on_BuildRemoveROMSectionBtn_clicked()
{
    if (!projectGroup) return;

    int index = ui->BuildROMSectionsList->currentIndex().row();
    if (index >= 0)
    {
        projectGroup->ROMImage()->RemoveSectionFromROMList(index);
        ui->BuildAvailableROMSectionsList->viewport()->repaint();
        ui->BuildROMSectionsList->viewport()->repaint();
    }
}

void MainWindow::on_BuildMoveUpROMSectionBtn_clicked()
{
    if (!projectGroup) return;

    int index = ui->BuildROMSectionsList->currentIndex().row();
    if (index >= 0)
    {
        projectGroup->ROMImage()->MoveSectionInROMList(index, true);
        ui->BuildROMSectionsList->viewport()->repaint();
    }
}

void MainWindow::on_BuildMoveDownROMSectionBtn_clicked()
{
    if (!projectGroup) return;

    int index = ui->BuildROMSectionsList->currentIndex().row();
    if (index >= 0)
    {
        projectGroup->ROMImage()->MoveSectionInROMList(index, false);
        ui->BuildROMSectionsList->viewport()->repaint();
    }
}

void MainWindow::on_BuildEmulatorFolderBrowseBtn_clicked()
{
    ui->BuildOutputFolderEdit->setText(QFileDialog::getExistingDirectory(this, "Select output destination"));
}

void MainWindow::on_actionBuild_project_triggered()
{
    if (projectGroup)
        projectGroup->Build();
}

void MainWindow::on_ConsoleTableView_clicked(const QModelIndex &index)
{
    ui->SelectedLogMessageText->setPlainText(ui->ConsoleTableView->model()->data(index).toString());
}

void MainWindow::on_BuildOutputFolderEdit_textChanged(const QString &arg1)
{
    if (projectGroup)
        projectGroup->exportDirectory = arg1;
}

void MainWindow::on_BuildBootLoaderProjectComboBox_currentIndexChanged(const QString &arg1)
{
    if (projectGroup)
        projectGroup->binBootLoader = arg1;
}

void MainWindow::on_actionBuild_and_run_emulator_triggered()
{
    if (projectGroup && emulator == NULL)
    {
        ClearLog();
        projectGroup->Build();
        QStringList args(projectGroup->exportDirectory);
        emulator = new QProcess(this);
        emulator->setWorkingDirectory(projectGroup->exportDirectory);
        connect(emulator, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(on_emulatorFinished(int, QProcess::ExitStatus)));
        connect(emulator, &QProcess::readyReadStandardOutput, this, &MainWindow::on_emulatorStdOut);
        emulator->start(projectGroup->exportDirectory + "/" + EMULATOR_EXE, args);
        //QString cmd("cd " + projectGroup->exportDirectory);
        //system(cmd.toStdString().c_str());
        //QDir emudir(projectGroup->exportDirectory);
        //QString cmd = ("start " + QDir::toNativeSeparators(emudir.absolutePath()) + "\\" + EMULATOR_EXE);
        //system(cmd.toStdString().c_str());
    }

}

void MainWindow::on_emulatorFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    delete emulator;
    emulator = NULL;
}

void MainWindow::on_emulatorStdOut()
{
    if (emulator)
    {
        LogMessage("emulator", (QString)emulator->readAllStandardOutput());
    }
}
