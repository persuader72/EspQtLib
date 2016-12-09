#-------------------------------------------------
#
# Project created by QtCreator 2016-12-03T10:09:04
#
#-------------------------------------------------

QT       += core gui serialport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = EspQtFirmwareLoad
TEMPLATE = app


SOURCES += main.cpp\
        firmwarerepository.cpp \
        mainwindow.cpp

HEADERS  += mainwindow.h \
            firmwarerepository.h

FORMS    += mainwindow.ui

win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../quazip/quazip/release/ -lquazip
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../quazip/quazip/debug/ -lquazip
else:unix: LIBS += -L$$OUT_PWD/../quazip/quazip/ -lquazip

INCLUDEPATH += $$PWD/../quazip/quazip
DEPENDPATH += $$PWD/../quazip/quazip

win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../EspQtLib/release/ -lEspQtLib
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../EspQtLib/debug/ -lEspQtLib
else:unix: LIBS += -L$$OUT_PWD/../EspQtLib/ -lEspQtLib

INCLUDEPATH += $$PWD/../EspQtLib
DEPENDPATH += $$PWD/../EspQtLib

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../EspQtLib/release/libEspQtLib.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../EspQtLib/debug/libEspQtLib.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../EspQtLib/release/EspQtLib.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../EspQtLib/debug/EspQtLib.lib
else:unix: PRE_TARGETDEPS += $$OUT_PWD/../EspQtLib/libEspQtLib.a

RC_ICONS = res/esp8266.ico

