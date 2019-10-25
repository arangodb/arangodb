TEMPLATE = app
TARGET = test7

include (configuration.pri)

DEFINES += \
    BOOST_UBLAS_USE_INTERVAL \
    $${UBLAS_TESTSET}

HEADERS += ../../../test/test7.hpp

SOURCES += \
    ../../../test/test73.cpp \
    ../../../test/test72.cpp \
    ../../../test/test71.cpp \
    ../../../test/test7.cpp
