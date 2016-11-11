QT += core serialport
QT -= gui

CONFIG += c++11

TARGET = EspQtToolTest
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    mainclass.cpp

HEADERS += \
    mainclass.h

win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../EspQtLib/release/ -lEspQtLib
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../EspQtLib/debug/ -lEspQtLib
else:unix: LIBS += -L$$OUT_PWD/../EspQtLib/ -lEspQtLib

INCLUDEPATH += $$PWD/../EspQtLib
DEPENDPATH += $$PWD/../EspQtLib

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../EspQtLib/release/libEspQtLib.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../EspQtLib/debug/EspQtLib.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../EspQtLib/release/EspQtLib.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../EspQtLib/debug/EspQtLib.lib
else:unix: PRE_TARGETDEPS += $$OUT_PWD/../EspQtLib/libEspQtLib.a
