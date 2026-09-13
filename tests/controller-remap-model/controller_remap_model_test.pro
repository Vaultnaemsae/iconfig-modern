QT += core gui widgets
include(../test_common.pri)
TARGET = controller_remap_model_test

SOURCES += \
    controller_remap_model_test.cpp \
    $$ICONFIG_ROOT/MIDIRelated/MIDIControllerRemapForm.cpp \
    $$ICONFIG_ROOT/MIDIRelated/MIDIPortSelectionForm.cpp \
    $$ICONFIG_ROOT/CCList.cpp \
    $$ICONFIG_ROOT/DeviceInfo.cpp \
    $$ICONFIG_ROOT/RefreshObject.cpp \
    $$ICONFIG_ROOT/TableListener.cpp \
    $$ICONFIG_ROOT/TreeUtils.cpp \
    $$ICONFIG_ROOT/MyCheckBox.cpp \
    $$ICONFIG_ROOT/MyComboBox.cpp \
    $$ICONFIG_ROOT/MyLabel.cpp \
    $$ICONFIG_ROOT/MyLineEdit.cpp \
    $$ICONFIG_ROOT/MySpinBox.cpp \
    $$REPO_ROOT/rtmidi-2.1.1/RtMidi.cpp

HEADERS += \
    $$ICONFIG_ROOT/MIDIRelated/MIDIControllerRemapForm.h \
    $$ICONFIG_ROOT/MIDIRelated/MIDIPortSelectionForm.h \
    $$ICONFIG_ROOT/DeviceInfo.h \
    $$ICONFIG_ROOT/RefreshObject.h \
    $$ICONFIG_ROOT/TableListener.h \
    $$ICONFIG_ROOT/MyCheckBox.h \
    $$ICONFIG_ROOT/MyComboBox.h \
    $$ICONFIG_ROOT/MyLabel.h \
    $$ICONFIG_ROOT/MyLineEdit.h \
    $$ICONFIG_ROOT/MySpinBox.h

FORMS += \
    $$ICONFIG_ROOT/MIDIRelated/MIDIControllerRemapForm.ui \
    $$ICONFIG_ROOT/MIDIRelated/MIDIPortSelectionForm.ui

RESOURCES += $$ICONFIG_ROOT/Resources.qrc
