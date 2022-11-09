TARGET = open62541_backend
QT += core core-private opcua opcua-private network
QT -= gui

QMAKE_LIBDIR_MBEDTLS = $$PWD/../../../../ext/mbedtls/lib
QMAKE_LIBS_MBEDTLS = -lmbedtls -lmbedx509 -lmbedcrypto
QMAKE_INCDIR_MBEDTLS = $$PWD/../../../../ext/mbedtls/include
LIBS += -ladvapi32

qtConfig(open62541):!qtConfig(system-open62541) {
    qtConfig(mbedtls):{
        QMAKE_USE_PRIVATE += mbedtls
        DEFINES += UA_ENABLE_ENCRYPTION
		message("ENABLE_ENCRYTION")
    }
    include($$PWD/../../../3rdparty/open62541.pri)
} else {
    QMAKE_USE_PRIVATE += open62541
    win32-msvc: LIBS += open62541.lib
}

HEADERS += \
    qopen62541backend.h \
    qopen62541client.h \
    qopen62541node.h \
    qopen62541plugin.h \
    qopen62541subscription.h \
    qopen62541valueconverter.h \
    qopen62541.h \
    qopen62541utils.h

SOURCES += \
    qopen62541backend.cpp \
    qopen62541client.cpp \
    qopen62541node.cpp \
    qopen62541plugin.cpp \
    qopen62541subscription.cpp \
    qopen62541valueconverter.cpp \
    qopen62541utils.cpp

OTHER_FILES = open62541-metadata.json

PLUGIN_TYPE = opcua
PLUGIN_CLASS_NAME = QOpen62541Plugin
load(qt_plugin)
