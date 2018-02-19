TEMPLATE = app
TARGET = test1

!include (configuration.pri)

DEFINES += $$UBLAS_TESTSET

HEADERS += ../../../test/test1.hpp

SOURCES += \
    ../../../test/test13.cpp \
    ../../../test/test12.cpp \
    ../../../test/test11.cpp \
    ../../../test/test1.cpp
