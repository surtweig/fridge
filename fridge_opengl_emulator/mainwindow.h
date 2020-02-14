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

    const int RAMViewRowsCount = 16;
    const int RAMViewColumnsCount = 16;
    const QColor ErrorBackgroundColor = QColor::fromRgb(255, 128, 128);

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void updateRegistersView();
    void updateRAMView();

private slots:
    void on_sysTickBtn_clicked();
    void on_sysResetBtn_clicked();
    void on_ramScrollRightBtn_clicked();
    void on_ramScrollLeftBtn_clicked();
    void on_ramAddrEdit_returnPressed();
    void on_ramAddrEdit_textChanged(const QString &arg1);

    void on_ramScrollToPC_clicked();

    void on_ramScrollToSP_clicked();

    void on_asmCompileBtn_clicked();

private:
    Ui::MainWindow *ui;
    FRIDGE_SYSTEM *sys;

    int ramViewPosition;
    int ramSelectedAddress;

    void initFridge();
    void destroyFridge();

protected:
    void resizeEvent(QResizeEvent *event);
};
#endif // MAINWINDOW_H
