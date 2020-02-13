#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
extern "C" {
#include <fridgemulib.h>
}

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void updateRegistersView();

private slots:
    void on_sysTickBtn_clicked();

    void on_sysResetBtn_clicked();

private:
    Ui::MainWindow *ui;
    FRIDGE_SYSTEM *sys;

    void initFridge();
    void destroyFridge();
};
#endif // MAINWINDOW_H
