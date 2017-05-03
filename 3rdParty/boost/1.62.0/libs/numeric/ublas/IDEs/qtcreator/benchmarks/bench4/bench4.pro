TEMPLATE = app
TARGET = bench4

!include (../configuration.pri)

DEFINES += BOOST_UBLAS_USE_INTERVAL

OTHER_FILES += \
    ../../../../benchmarks/bench4/Jamfile.v2

SOURCES += \
    ../../../../benchmarks/bench4/bench43.cpp \
    ../../../../benchmarks/bench4/bench42.cpp \
    ../../../../benchmarks/bench4/bench41.cpp \
    ../../../../benchmarks/bench4/bench4.cpp
