TARGET = QtOpcUa
QT += core-private network network-private
QT -= gui
QT_FOR_CONFIG += core-private

include(core/core.pri)
include(client/client.pri)

INCLUDEPATH += C:/Qt/Tools/OpenSSL/Win_x64/include
QMAKE_LIBDIR_MBEDTLS = C:/dev/mbedtls/install/lib
QMAKE_LIBS_MBEDTLS = -lmbedtls -lmbedx509 -lmbedcrypto
QMAKE_INCDIR_MBEDTLS = C:/dev/mbedtls/install/include
DEFINES += UA_ENABLE_ENCRYPTION
QMAKE_LIBDIR += -LC:/dev/mbedtls/install/lib

qtConfig(gds) {
    qtConfig(ssl):!darwin:!winrt: include(x509/x509.pri)
}

MODULE_PLUGIN_TYPES = opcua
QMAKE_DOCS = $$PWD/doc/qtopcua.qdocconf

load(qt_module)

PUBLIC_HEADERS += qopcuaglobal.h

DEFINES += QT_NO_FOREACH

HEADERS += $$PUBLIC_HEADERS $$PRIVATE_HEADERS


!qtConfig(ns0idnames): {
    DEFINES += QT_OPCUA_NO_NS0IDNAMES
}
