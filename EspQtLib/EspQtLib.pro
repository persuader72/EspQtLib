#-------------------------------------------------
#
# Project created by QtCreator 2016-09-12T04:38:56
#
#-------------------------------------------------

QT +=  serialport
QT -= gui

TARGET = EspQtLib
TEMPLATE = lib
CONFIG += staticlib

SOURCES += \
    esprom.cpp \
    cesantaflasher.cpp \
    espinterface.cpp

HEADERS += \
    esprom.h \
    cesantaflasher.h \
    espinterface.h
unix {
    target.path = /usr/lib
    INSTALLS += target
}

RESOURCES += \
    resources.qrc
