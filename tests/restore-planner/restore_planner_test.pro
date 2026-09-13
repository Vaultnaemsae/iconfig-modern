QT += core gui widgets network
include(../test_common.pri)
TARGET = restore_planner_test

SOURCES += \
    restore_planner_test.cpp \
    $$ICONFIG_ROOT/DeviceInfo.cpp \
    $$REPO_ROOT/rtmidi-2.1.1/RtMidi.cpp

HEADERS += $$ICONFIG_ROOT/DeviceInfo.h
