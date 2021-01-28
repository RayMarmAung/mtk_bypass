QT       += core
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11
DEFINES += QT_DEPRECATED_WARNINGS

INCLUDEPATH += $$PWD/include

LIBS += -lsetupapi
LIBS += -L$$PWD/lib -llibusb

SOURCES += \
    About.cpp \
    Serial/impl/list_ports/list_ports_win.cc \
    Serial/impl/win.cc \
    Serial/serial.cc \
    main.cpp \
    mainwindow.cpp \
    sponsor.cpp

HEADERS += \
    About.h \
    UsbDevice.h \
    deivceIo.h \
    mainwindow.h \
    sponsor.h

FORMS += \
    About.ui \
    mainwindow.ui \
    sponsor.ui

RESOURCES += \
    res.qrc

RC_FILE += \
    mtk_bypass.rc
