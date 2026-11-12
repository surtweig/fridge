#ifndef NEWPROJECTGROUPDIALOG_H
#define NEWPROJECTGROUPDIALOG_H

#include <QDialog>

namespace Ui {
class NewProjectGroupDialog;
}

class NewProjectGroupDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewProjectGroupDialog(QWidget *parent = 0);
    ~NewProjectGroupDialog();

signals:
    void NewProjectGroupCreated(QString name, QString path);

private slots:
    void on_SelectRootDirectoryBtn_clicked();
    void on_NameEdit_textChanged(const QString &text);

    void on_ProjectGroupCreateCancelBtn_clicked();

    void on_ProjectGroupCreateBtn_clicked();

private:
    Ui::NewProjectGroupDialog *ui;
    QString rootLocation;
};

#endif // NEWPROJECTGROUPDIALOG_H
