TEMPLATE = app
TARGET = test2

!include (configuration.pri)

DEFINES += $$UBLAS_TESTSET

HEADERS += ../../../test/test2.hpp

SOURCES += \
    ../../../test/test23.cpp \
    ../../../test/test22.cpp \
    ../../../test/test21.cpp \
    ../../../test/test2.cpp
