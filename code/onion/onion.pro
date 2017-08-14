QT += core network
QT -= gui

# uncomment here to build tests
#CONFIG += test

CONFIG += c++11

TARGET = onion
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += \
    controller.cpp \
    peertopeer.cpp \
    settings.cpp \
    onionapi.cpp \
    rpsapi.cpp \
    peertopeermessage.cpp \
    sessionkeystore.cpp \
    tunnelidmapper.cpp \
    oauthapi.cpp \
    peersampler.cpp

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

HEADERS += \
    controller.h \
    peertopeer.h \
    settings.h \
    binding.h \
    onionapi.h \
    messagetypes.h \
    rpsapi.h \
    metatypes.h \
    peertopeermessage.h \
    sessionkeystore.h \
    tunnelidmapper.h \
    oauthapi.h \
    peersampler.h

test{
#    message(Configuring test build...)

    TEMPLATE = app
    TARGET = oniontest

    QT += testlib

    SOURCES += \
        tests/onionapitester.cpp \
        tests/rpsapitester.cpp \
        tests/peertopeermessagetester.cpp \
        tests/oauthapitester.cpp \
        test.cpp

    HEADERS += \
        tests/onionapitester.h \
        tests/rpsapitester.h \
        tests/peertopeermessagetester.h \
        tests/oauthapitester.h#
} else {
    SOURCES += main.cpp
}
