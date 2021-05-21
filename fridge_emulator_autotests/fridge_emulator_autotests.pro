QT += testlib
QT -= gui

CONFIG += qt console warn_on depend_includepath testcase
CONFIG -= app_bundle

TEMPLATE = app

DEFINES += POSIT_TEST

SOURCES +=  tst_fridgemulib_tests.cpp \
    ../fridgemulib/fridgemulib.c \
    ../fridgemulib/posit.c

HEADERS += \
    ../fridgemulib/fridgemulib.h \
    ../fridgemulib/posit.h

INCLUDEPATH += \
    $$PWD/../include \
    $$PWD/../fridgemulib

DEPENDPATH += \
    $$PWD/../include \
    $$PWD/../fridgemulib
