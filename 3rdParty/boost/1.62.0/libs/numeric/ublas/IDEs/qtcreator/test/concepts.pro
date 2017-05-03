TEMPLATE = app
TARGET = concepts

!include (configuration.pri)

DEFINES += \
    EXTERNAL
# INTERAL
# SKIP_BAD

linux: icc: QMAKE_CXXFLAGS += -Xc
macx: QMAKE_CXXFLAGS += -fabi-version=0


SOURCES += \
    ../../../test/concepts.cpp
