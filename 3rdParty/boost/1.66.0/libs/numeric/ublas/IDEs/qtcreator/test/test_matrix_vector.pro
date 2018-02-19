TEMPLATE = app
TARGET = test_matrix_vector

!include (configuration.pri)

HEADERS += \
    ../../../test/utils.hpp

SOURCES += \
    ../../../test/test_matrix_vector.cpp

INCLUDEPATH += \
    ../../../include
