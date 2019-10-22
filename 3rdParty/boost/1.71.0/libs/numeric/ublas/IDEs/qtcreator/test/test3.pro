TEMPLATE = app
TARGET = test3

include (configuration.pri)

DEFINES += $$UBLAS_TESTSET_SPARSE

HEADERS += ../../../test/test3.hpp

SOURCES += \
    ../../../test/test33.cpp \
    ../../../test/test32.cpp \
    ../../../test/test31.cpp \
    ../../../test/test3.cpp
