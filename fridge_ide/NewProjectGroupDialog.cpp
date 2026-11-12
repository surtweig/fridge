#include "NewProjectGroupDialog.h"
#include "ui_NewProjectGroupDialog.h"
#include <QStyle>
#include <QDesktopWidget>
#include <QFileDialog>
#include <QDir>

NewProjectGroupDialog::NewProjectGroupDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::NewProjectGroupDialog)
{
    ui->setupUi(this);
    this->setGeometry(
        QStyle::alignedRect(
            Qt::LeftToRight,
            Qt::AlignCenter,
            this->size(),
            qApp->desktop()->availableGeometry()
        )
    );
    rootLocation = "";
}

NewProjectGroupDialog::~NewProjectGroupDialog()
{
    delete ui;
}

void NewProjectGroupDialog::on_SelectRootDirectoryBtn_clicked()
{
    /*QFileDialog dlg(this);
    dlg.setFileMode(QFileDialog::DirectoryOnly);
    dlg.setOption(QFileDialog::ShowDirsOnly, true);
    if (dlg.exec())
    {

    }*/
    rootLocation = QFileDialog::getExistingDirectory(this, "Select root directory location");
    on_NameEdit_textChanged(ui->NameEdit->text());
}

void NewProjectGroupDialog::on_NameEdit_textChanged(const QString &text)
{
    ui->PathEdit->setText(rootLocation + "/" + text);
    QDir path(ui->PathEdit->text());
    ui->ProjectGroupCreateBtn->setEnabled(text.size() > 0 && rootLocation.size() > 0 && !path.exists());
}

void NewProjectGroupDialog::on_ProjectGroupCreateCancelBtn_clicked()
{
    setResult(QDialog::DialogCode::Rejected);
    close();
}

void NewProjectGroupDialog::on_ProjectGroupCreateBtn_clicked()
{
    setResult(QDialog::DialogCode::Accepted);
    NewProjectGroupCreated(ui->NameEdit->text(), rootLocation);
    close();
}
