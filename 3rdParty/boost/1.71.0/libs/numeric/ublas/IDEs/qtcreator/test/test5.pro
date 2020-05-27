TEMPLATE = app
TARGET = test5

include (configuration.pri)

DEFINES += $$UBLAS_TESTSET

HEADERS += ../../../test/test5.hpp

SOURCES += \
    ../../../test/test53.cpp \
    ../../../test/test52.cpp \
    ../../../test/test5.cpp
