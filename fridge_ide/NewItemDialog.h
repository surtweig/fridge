#ifndef NEWITEMDIALOG_H
#define NEWITEMDIALOG_H

#include <QDialog>

namespace Ui {
class NewItemDialog;
}

class NewItemDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewItemDialog(QWidget *parent = 0, QString currentProjectName = "");
    ~NewItemDialog();

signals:
    void NewProjectAdded(QString name);
    void NewX2ALSourceAdded(QString name);
    void NewIncludeFolderAdded(QString path);

private slots:
    void on_NextBtn_clicked();
    void on_ItemTypeRadioButton_toggled(bool toggled);
    void on_ProjectNameEdit_textChanged(const QString &text);
    void on_CancelBtn_clicked();
    void on_X2ALSourceNameEdit_textChanged(const QString &text);
    void on_BrowseIncludePathBtn_clicked();
    void on_IncludePathEdit_textChanged(const QString &text);

private:
    Ui::NewItemDialog *ui;
    QString currentProjectName;
};

#endif // NEWITEMDIALOG_H
