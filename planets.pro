#-------------------------------------------------
#
# Project created by QtCreator 2018-07-31T09:19:15
#
#-------------------------------------------------

QT       += core gui widgets opengl

TARGET = planets
TEMPLATE = app

DESTDIR = $$PWD/Bin

CONFIG += c++11

SOURCES += \
        main.cpp \
        mainwindow.cpp \
    glutils.cpp \
    planetgl.cpp

HEADERS += \
        mainwindow.h \
    glutils.h \
    planetgl.h

FORMS += \
        mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    data.qrc
