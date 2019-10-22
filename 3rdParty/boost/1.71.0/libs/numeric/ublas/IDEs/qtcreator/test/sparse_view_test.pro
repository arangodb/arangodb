TEMPLATE = app
TARGET = sparse_view_test

win*:QMAKE_CXXFLAGS += /EHa
# Support asynchronous structured exception handling
# (SEH) with the native C++ catch(...) clause.

include (configuration.pri)

SOURCES += \
    ../../../test/sparse_view_test.cpp
