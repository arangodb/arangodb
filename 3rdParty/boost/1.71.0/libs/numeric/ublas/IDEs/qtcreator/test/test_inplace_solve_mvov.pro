TEMPLATE = app
TARGET = test_inplace_solve_mvov

include (configuration.pri)

DEFINES += \
    USE_MAPPED_VECTOR_OF_MAPPED_VECTOR

HEADERS += \
    ../../../test/utils.hpp

SOURCES += \
    ../../../test/test_inplace_solve.cpp
