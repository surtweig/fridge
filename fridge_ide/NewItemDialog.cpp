#include "NewItemDialog.h"
#include "ui_NewItemDialog.h"
#include <QFileDialog>
#include <QDir>

NewItemDialog::NewItemDialog(QWidget *parent, QString currentProjectName) :
    QDialog(parent),
    ui(new Ui::NewItemDialog)
{
    ui->setupUi(this);
    this->currentProjectName = currentProjectName;

    bool curproj = currentProjectName.size() > 0;
    ui->X2ALSourceRadioButton->setEnabled(curproj);
    //ui->IncludeFolderRadioButton->setEnabled(curproj);
    ui->ResourceStorageRadioButton->setEnabled(curproj);
    if (curproj)
        ui->AddToProjectNoticeLabel->setText("Project items will be added to '" + currentProjectName + "'.");
    else
        ui->AddToProjectNoticeLabel->setText("A project need to be selected in order to add other items.");
}

NewItemDialog::~NewItemDialog()
{
    delete ui;
}

void NewItemDialog::on_NextBtn_clicked()
{
    if (ui->NewItemPages->currentWidget() == ui->ItemTypeSelectPage)
    {
        if (ui->ProjectRadioButton->isChecked())
            ui->NewItemPages->setCurrentWidget(ui->NewProjectPage);
        if (ui->X2ALSourceRadioButton->isChecked())
            ui->NewItemPages->setCurrentWidget(ui->X2ALSourcePage);
        if (ui->IncludeFolderRadioButton->isChecked())
            ui->NewItemPages->setCurrentWidget(ui->IncludePathPage);
        ui->NextBtn->setEnabled(false);
    }
    else
    if (ui->NewItemPages->currentWidget() == ui->NewProjectPage)
    {
        NewProjectAdded(ui->ProjectNameEdit->text());
        setResult(QDialog::DialogCode::Accepted);
        close();
    }
    else
    if (ui->NewItemPages->currentWidget() == ui->X2ALSourcePage)
    {
        NewX2ALSourceAdded(ui->X2ALSourceNameEdit->text() + ui->X2ALSourceExtComboBox->currentText());
        setResult(QDialog::DialogCode::Accepted);
        close();
    }
    else
    if (ui->NewItemPages->currentWidget() == ui->IncludePathPage)
    {
        NewIncludeFolderAdded(ui->IncludePathEdit->text());
        setResult(QDialog::DialogCode::Accepted);
        close();
    }
}

void NewItemDialog::on_ItemTypeRadioButton_toggled(bool toggled)
{
    ui->NextBtn->setEnabled(true);
}

void NewItemDialog::on_ProjectNameEdit_textChanged(const QString &text)
{
    ui->NextBtn->setEnabled(text.size() > 0);
}

void NewItemDialog::on_CancelBtn_clicked()
{
    setResult(QDialog::DialogCode::Rejected);
    close();
}

void NewItemDialog::on_X2ALSourceNameEdit_textChanged(const QString &text)
{
    ui->NextBtn->setEnabled(text.size() > 0);
}

void NewItemDialog::on_BrowseIncludePathBtn_clicked()
{
    ui->IncludePathEdit->setText(QFileDialog::getExistingDirectory(this, "Select include folder"));
}

void NewItemDialog::on_IncludePathEdit_textChanged(const QString &text)
{
    ui->NextBtn->setEnabled(text.size() > 0 && QDir(text).exists());
}
