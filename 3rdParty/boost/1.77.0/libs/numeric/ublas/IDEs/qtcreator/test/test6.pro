TEMPLATE = app
TARGET = test6

include (configuration.pri)

DEFINES += $$UBLAS_TESTSET

HEADERS += ../../../test/test6.hpp

SOURCES += \
    ../../../test/test63.cpp \
    ../../../test/test62.cpp \
    ../../../test/test6.cpp
