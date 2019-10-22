TEMPLATE = app
TARGET = test_lu

include (configuration.pri)

HEADERS += \
    ../../../test/common/testhelper.hpp

SOURCES += \
    ../../../test/test_lu.cpp
