TEMPLATE = app
TARGET = test3_mvov

!include (configuration.pri)

DEFINES += \
    USE_FLOAT \
    USE_DOUBLE \
    USE_STD_COMPLEX \
    USE_STD_MAP \
    USE_MAPPED_VECTOR_OF_MAPPED_VECTOR

HEADERS += ../../../test/test3.hpp

SOURCES += \
    ../../../test/test33.cpp \
    ../../../test/test32.cpp \
    ../../../test/test31.cpp \
    ../../../test/test3.cpp
