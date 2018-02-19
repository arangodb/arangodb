TEMPLATE = app
TARGET = test_coordinate_matrix_always_do_full_sort

!include (configuration.pri)

DEFINES += \
    BOOST_UBLAS_COO_ALWAYS_DO_FULL_SORT

HEADERS += \
    ../../../test/utils.hpp

SOURCES += \
    ../../../test/test_coordinate_matrix_sort.cpp
