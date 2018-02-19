TEMPLATE = app
TARGET = test_inplace_solve_basic

!include (configuration.pri)

DEFINES += \
    $$UBLAS_TESTSET

HEADERS += \
    ../../../test/utils.hpp

SOURCES += \
    ../../../test/test_inplace_solve.cpp
