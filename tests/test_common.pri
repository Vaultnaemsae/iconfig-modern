CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app

DEFINES += BOOST_RESULT_OF_USE_DECLTYPE __MACOSX_CORE__

REPO_ROOT = $$clean_path($$_PRO_FILE_PWD_/../..)
ICONFIG_ROOT = $$REPO_ROOT/iConfig
GENESYSLIB_ROOT = $$REPO_ROOT/GeneSysLib

isEmpty(BOOST_PREFIX): BOOST_PREFIX = $$(BOOST_PREFIX)
isEmpty(BOOST_PREFIX): error("BOOST_PREFIX must name the Boost installation prefix")

isEmpty(GENESYSLIB_BUILD_DIR): GENESYSLIB_BUILD_DIR = $$(GENESYSLIB_BUILD_DIR)
isEmpty(GENESYSLIB_BUILD_DIR): error("GENESYSLIB_BUILD_DIR must name the GeneSysLib build directory")

INCLUDEPATH += \
    $$ICONFIG_ROOT \
    $$ICONFIG_ROOT/Presets \
    $$GENESYSLIB_ROOT \
    $$GENESYSLIB_ROOT/Audio \
    $$GENESYSLIB_ROOT/Audio/Mixer \
    $$GENESYSLIB_ROOT/Audio/AudioV1 \
    $$GENESYSLIB_ROOT/Audio/AudioV2 \
    $$GENESYSLIB_ROOT/Base \
    $$GENESYSLIB_ROOT/Device \
    $$GENESYSLIB_ROOT/MIDI \
    $$REPO_ROOT/rtmidi-2.1.1 \
    $$BOOST_PREFIX/include

LIBS += -L$$GENESYSLIB_BUILD_DIR -lGeneSysLib
LIBS += -framework CoreMIDI -framework CoreFoundation -framework CoreAudio

QMAKE_MACOSX_DEPLOYMENT_TARGET = 14.0
