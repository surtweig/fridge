#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
extern "C" {
#include <fridgemulib.h>
}
#include <iostream>
#include <fstream>
#include <streambuf>
#include <qplaintextedit.h>
#include <PixBufferRenderer.h>
using namespace std;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MessagesLogBuf : public streambuf
{
private:
    QPlainTextEdit* textView;

public:
    MessagesLogBuf(QPlainTextEdit* textView);
    virtual std::streamsize xsputn(const char_type* s, std::streamsize n);
    virtual int_type overflow(int_type c);
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

    const int RAMViewRowsCount = 16;
    const int RAMViewColumnsCount = 16;
    const QColor ErrorBackgroundColor = QColor::fromRgb(255, 128, 128);
    const string FalcTempFile = "TempProgram.falc";
    const string BinTempFile = "TempProgram.bin";

private:
    PixBufferRenderer* pixBufferRenderer;

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
    MessagesLogBuf *logbuf;

    int ramViewPosition;
    int ramSelectedAddress;

    void initFridge();
    void destroyFridge();

protected:
    void resizeEvent(QResizeEvent *event);
};
#endif // MAINWINDOW_H
