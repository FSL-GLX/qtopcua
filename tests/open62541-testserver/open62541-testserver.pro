TEMPLATE = app
TARGET = open62541-testserver

QMAKE_LIBDIR_MBEDTLS = $$PWD/../../ext/mbedtls/lib
QMAKE_LIBS_MBEDTLS = -lmbedtls -lmbedx509 -lmbedcrypto
QMAKE_INCDIR_MBEDTLS = $$PWD/../../ext/mbedtls/include

LIBS += -ladvapi32

INCLUDEPATH += \
               $$PWD/../../src/plugins/opcua/open62541

DEPENDPATH += INCLUDEPATH

CONFIG += c++11 console

QT += opcua-private

qtConfig(open62541):!qtConfig(system-open62541) {
    qtConfig(mbedtls):{
        QMAKE_USE_PRIVATE += mbedtls
        DEFINES += UA_ENABLE_ENCRYPTION
		message("SERVER UA_ENABLE_ENCRYPTION")
    }
    include($$PWD/../../src/3rdparty/open62541.pri)
} else {
    QMAKE_USE_PRIVATE += open62541
}

win32: DESTDIR = ./

# Workaround for QTBUG-75020
QMAKE_CFLAGS_RELEASE -= -O2
QMAKE_CFLAGS_RELEASE_WITH_DEBUGINFO -= -O2

SOURCES += \
           main.cpp \
           testserver.cpp \
           $$PWD/../../src/plugins/opcua/open62541/qopen62541utils.cpp \
           $$PWD/../../src/plugins/opcua/open62541/qopen62541valueconverter.cpp


HEADERS += \
           testserver.h

RESOURCES += certs.qrc

