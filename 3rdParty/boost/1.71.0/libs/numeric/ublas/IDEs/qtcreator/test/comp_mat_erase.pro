TEMPLATE = app
TARGET = comp_mat_erase

win*: QMAKE_CXXFLAGS += /EHa

include (configuration.pri)

SOURCES += \
    ../../../test/comp_mat_erase.cpp
