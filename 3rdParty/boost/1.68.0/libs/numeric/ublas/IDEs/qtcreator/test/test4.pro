TEMPLATE = app
TARGET = test4

!include (configuration.pri)

DEFINES += $$UBLAS_TESTSET

HEADERS += ../../../test/test4.hpp

SOURCES += \
    ../../../test/test43.cpp \
    ../../../test/test42.cpp \
    ../../../test/test4.cpp
