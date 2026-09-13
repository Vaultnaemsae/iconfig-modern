QT += core gui widgets
include(../test_common.pri)
TARGET = preset_serialization_test

SOURCES += \
    preset_serialization_test.cpp \
    $$ICONFIG_ROOT/DeviceInfo.cpp \
    $$ICONFIG_ROOT/Presets/ICSaveDialog.cpp \
    $$REPO_ROOT/rtmidi-2.1.1/RtMidi.cpp

HEADERS += \
    $$ICONFIG_ROOT/DeviceInfo.h \
    $$ICONFIG_ROOT/Presets/ICSaveDialog.h

FORMS += $$ICONFIG_ROOT/Presets/ICSaveDialog.ui
