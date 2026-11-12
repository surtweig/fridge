#-------------------------------------------------
#
# Project created by QtCreator 2017-03-10T01:39:44
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = fridge_sdk
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += main.cpp\
        mainwindow.cpp \
    XCMProjectGroup.cpp \
    XCMProject.cpp \
    XCMROMImage.cpp \
    XCMBINFile.cpp \
    m_consoletable.cpp \
    NewProjectGroupDialog.cpp \
    NewItemDialog.cpp \
    IProjectGroupItem.cpp \
    XCMX2ALSource.cpp \
    XCMIncludeFolder.cpp \
    $$PWD/../emulator/rombuild/XCM2ROMImageBuilder.cpp \
    $$PWD/../emulator/emulator/XCM2.cpp \
    $$PWD/../emulator/x2al/XCM2AssemblyLanguageCompiler.cpp \

HEADERS  += mainwindow.h \
    XCMProjectGroup.h \
    XCMProject.h \
    XCMROMImage.h \
    XCMBINFile.h \
    m_consoletable.h \
    NewProjectGroupDialog.h \
    NewItemDialog.h \
    IProjectGroupItem.h \
    XCMX2ALSource.h \
    XCMIncludeFolder.h \
    $$PWD/../emulator/emulator/XCM2.h \

FORMS    += mainwindow.ui \
    NewProjectGroupDialog.ui \
    NewItemDialog.ui

unix|win32: LIBS += -L$$PWD/../scintilla/bin/ -lScintillaEdit3

INCLUDEPATH += $$PWD/../scintilla/include \
               $$PWD/../scintilla/qt/ScintillaEdit \
               $$PWD/../scintilla/qt/ScintillaEditBase \
               $$PWD/../emulator/x2al/ \
               $$PWD/../emulator/rombuild/

DEPENDPATH += $$PWD/../scintilla/include \
              $$PWD/../scintilla/qt/ScintillaEdit \
              $$PWD/../scintilla/qt/ScintillaEditBase \
              $$PWD/../emulator/x2al/ \
              $$PWD/../emulator/rombuild/
