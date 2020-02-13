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
    updateRegistersView();
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

void MainWindow::on_sysTickBtn_clicked()
{
    FRIDGE_sys_tick(sys);
    updateRegistersView();
}

void MainWindow::on_sysResetBtn_clicked()
{
    FRIDGE_cpu_reset(sys->cpu);
    updateRegistersView();
}
