#-------------------------------------------------
#
# Project created by QtCreator 2024-09-10T19:32:55
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = tapiterminal
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += QTTAPIMODEM_STATICALLY_LINKED QTAPI_DEBUG

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

INCLUDEPATH += ../../

SOURCES += main.cpp\
        mainwindow.cpp \
    console.cpp \
    settingsdialog.cpp \
    ../../qttapimodem.cpp

HEADERS  += mainwindow.h \
    console.h \
    settingsdialog.h \
    ../../qttapimodem_global.h \
    ../../qttapimodem.h


FORMS    += mainwindow.ui \
    settingsdialog.ui

RESOURCES += \
    tapiterminal.qrc

LIBS += -luser32 -ltapi32
