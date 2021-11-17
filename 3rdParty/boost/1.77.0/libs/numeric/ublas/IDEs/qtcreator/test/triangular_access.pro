TEMPLATE = app
TARGET = triangular_access

include (configuration.pri)

DEFINES += NOMESSAGES

HEADERS += \
    ../../../test/common/testhelper.hpp

SOURCES += \
    ../../../test/triangular_access.cpp
