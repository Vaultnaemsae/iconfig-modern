QT += core
include(../test_common.pri)
TARGET = namespace_import_test

SOURCES += \
    namespace_import_test.cpp \
    $$ICONFIG_ROOT/LegacyDataImport.cpp

HEADERS += $$ICONFIG_ROOT/LegacyDataImport.h
