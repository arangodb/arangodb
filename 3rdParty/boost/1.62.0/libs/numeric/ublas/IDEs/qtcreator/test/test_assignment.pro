TEMPLATE = app
TARGET = test_assignment

!include (configuration.pri)

DEFINES += \
    BOOST_UBLAS_COO_ALWAYS_DO_FULL_SORT

HEADERS += \
    ../../../test/utils.hpp

SOURCES += \
    ../../../test/test_assignment.cpp
