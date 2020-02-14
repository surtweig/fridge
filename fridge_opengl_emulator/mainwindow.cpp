#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "FridgeAssemblyLanguageCompiler.h"
#include "fridgemulib.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    initFridge();

    ui->setupUi(this);
    ramViewPosition = 0;
    ramSelectedAddress = 0;

    updateRegistersView();
    updateRAMView();
}

void MainWindow::initFridge()
{
    sys = new FRIDGE_SYSTEM();
    sys->cpu = new FRIDGE_CPU();
    FRIDGE_cpu_reset(sys->cpu);
}

void MainWindow::destroyFridge()
{
    delete sys->cpu;
    delete sys;
}

MainWindow::~MainWindow()
{
    delete ui;
    destroyFridge();
}

void MainWindow::updateRegistersView()
{
    ui->cpuRegAEdit->setText(QString::number(sys->cpu->rA, 16));
    ui->cpuRegBEdit->setText(QString::number(sys->cpu->rB, 16));
    ui->cpuRegCEdit->setText(QString::number(sys->cpu->rC, 16));
    ui->cpuRegDEdit->setText(QString::number(sys->cpu->rD, 16));
    ui->cpuRegEEdit->setText(QString::number(sys->cpu->rE, 16));
    ui->cpuRegHEdit->setText(QString::number(sys->cpu->rH, 16));
    ui->cpuRegLEdit->setText(QString::number(sys->cpu->rL, 16));
    ui->cpuPairBCEdit->setText(QString::number(FRIDGE_cpu_pair_BC(sys->cpu), 16));
    ui->cpuPairDEEdit->setText(QString::number(FRIDGE_cpu_pair_DE(sys->cpu), 16));
    ui->cpuPairHLEdit->setText(QString::number(FRIDGE_cpu_pair_HL(sys->cpu), 16));
    ui->cpuRegSPEdit->setText(QString::number(sys->cpu->SP, 16));
    ui->cpuRegPCEdit->setText(QString::number(sys->cpu->PC, 16));
    ui->cpuFlagAuxCheckBox->setChecked(FRIDGE_cpu_flag_AUX(sys->cpu));
    ui->cpuFlagPanicCheckBox->setChecked(FRIDGE_cpu_flag_PANIC(sys->cpu));
    ui->cpuFlagCarryCheckBox->setChecked(FRIDGE_cpu_flag_CARRY(sys->cpu));
    ui->cpuFlagParityCheckBox->setChecked(FRIDGE_cpu_flag_PARITY(sys->cpu));
    ui->cpuFlagZeroCheckBox->setChecked(FRIDGE_cpu_flag_ZERO(sys->cpu));
    ui->cpuFlagSignCheckBox->setChecked(FRIDGE_cpu_flag_SIGN(sys->cpu));
}

void MainWindow::updateRAMView()
{
    int rowCount = RAMViewRowsCount;
    int colCount = RAMViewColumnsCount;
    ui->ramTableWidget->setRowCount(rowCount);
    ui->ramTableWidget->setColumnCount(colCount);
    int rowHeight = (ui->ramTableWidget->size().height() - ui->ramTableWidget->horizontalHeader()->height()) / rowCount - 1;
    int colWidth = (ui->ramTableWidget->size().width() - ui->ramTableWidget->verticalHeader()->width()) / colCount - 1;

    for (int i = 0; i < rowCount; ++i)
        ui->ramTableWidget->setRowHeight(i, rowHeight);

    for (int i = 0; i < colCount; ++i)
        ui->ramTableWidget->setColumnWidth(i, colWidth);

    for (int i = 0; i < colCount; ++i)
    {
        if (ui->ramTableWidget->horizontalHeaderItem(i) == nullptr)
            ui->ramTableWidget->setHorizontalHeaderItem(i, new QTableWidgetItem());
        ui->ramTableWidget->horizontalHeaderItem(i)->setText(QString::number(i, 16).toUpper());
    }
    for (int i = 0; i < rowCount; ++i)
    {
        if (ui->ramTableWidget->verticalHeaderItem(i) == nullptr)
            ui->ramTableWidget->setVerticalHeaderItem(i, new QTableWidgetItem());
        ui->ramTableWidget->verticalHeaderItem(i)->setText(QString::number(ramViewPosition + i*RAMViewColumnsCount, 16).toUpper());
    }

    for (int i = 0; i < rowCount; ++i)
        for (int j = 0; j < colCount; ++j)
        {
            if (ui->ramTableWidget->item(i, j) == nullptr)
                ui->ramTableWidget->setItem(i, j, new QTableWidgetItem());
            ui->ramTableWidget->item(i, j)->setText(QString::number(sys->cpu->ram[ramViewPosition + i*colCount + j], 16).toUpper());
        }

    ui->ramTableWidget->clearSelection();
    if (ramSelectedAddress >= ramViewPosition && ramSelectedAddress < ramViewPosition + rowCount*colCount)
    {
        int offset = ramSelectedAddress - ramViewPosition;
        ui->ramTableWidget->item(offset / colCount, offset % colCount)->setSelected(true);
    }

    /*
    int pc = sys->cpu->PC;
    if (pc >= ramViewPosition && pc < ramViewPosition + rowCount*colCount)
    {
        int offset = pc - ramViewPosition;
        int i = offset / colCount;
        int j = offset % colCount;
        QBrush brush = ui->ramTableWidget->item(i, j)->background();
        brush.setColor(Qt::blue);
        ui->ramTableWidget->item(i, j)->setBackground(brush);
    }
    */
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateRAMView();
}

void MainWindow::on_sysTickBtn_clicked()
{
    FRIDGE_sys_tick(sys);
    updateRegistersView();
    updateRAMView();
}

void MainWindow::on_sysResetBtn_clicked()
{
    FRIDGE_cpu_reset(sys->cpu);
    updateRegistersView();
    updateRAMView();
}

void MainWindow::on_ramScrollRightBtn_clicked()
{
    ramViewPosition += RAMViewRowsCount*RAMViewColumnsCount;
    if (ramViewPosition >= FRIDGE_RAM_SIZE)
        ramViewPosition = FRIDGE_RAM_SIZE - RAMViewRowsCount*RAMViewColumnsCount;
    updateRAMView();
}

void MainWindow::on_ramScrollLeftBtn_clicked()
{
    ramViewPosition -= RAMViewRowsCount*RAMViewColumnsCount;
    if (ramViewPosition < 0)
        ramViewPosition = 0;
    updateRAMView();
}

void MainWindow::on_ramAddrEdit_returnPressed()
{
    bool ok = false;
    int addr = ui->ramAddrEdit->text().toInt(&ok, 16);
    if (ok && addr >= 0 && addr < FRIDGE_RAM_SIZE)
    {
        ramSelectedAddress = addr;
        ramViewPosition = (addr / (RAMViewRowsCount*RAMViewColumnsCount)) * RAMViewRowsCount*RAMViewColumnsCount;
        updateRAMView();
    }
}

void MainWindow::on_ramAddrEdit_textChanged(const QString &arg1)
{
    bool ok = false;
    QString addrText = arg1;
    int addr = addrText.toInt(&ok, 16);
    QPalette pal = ui->ramAddrEdit->palette();
    if ((ok && addr >= 0 && addr < FRIDGE_RAM_SIZE) || addrText.length() == 0)
        pal.setColor(QPalette::Base, Qt::white);
    else
        pal.setColor(QPalette::Base, ErrorBackgroundColor);
    ui->ramAddrEdit->setPalette(pal);
}

void MainWindow::on_ramScrollToPC_clicked()
{
    ramSelectedAddress = sys->cpu->PC;
    ramViewPosition = (ramSelectedAddress / (RAMViewRowsCount*RAMViewColumnsCount)) * RAMViewRowsCount*RAMViewColumnsCount;
    updateRAMView();
}

void MainWindow::on_ramScrollToSP_clicked()
{
    ramSelectedAddress = sys->cpu->SP;
    ramViewPosition = (ramSelectedAddress / (RAMViewRowsCount*RAMViewColumnsCount)) * RAMViewRowsCount*RAMViewColumnsCount;
    updateRAMView();
}

void MainWindow::on_asmCompileBtn_clicked()
{
}
