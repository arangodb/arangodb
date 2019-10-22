TEMPLATE = app
TARGET = test_inplace_solve_sparse

include (configuration.pri)

DEFINES += \
    $$UBLAS_TESTSET_SPARSE \
    $$UBLAS_TESTSET_SPARSE_COO

HEADERS += \
    ../../../test/utils.hpp

SOURCES += \
    ../../../test/test_inplace_solve.cpp
