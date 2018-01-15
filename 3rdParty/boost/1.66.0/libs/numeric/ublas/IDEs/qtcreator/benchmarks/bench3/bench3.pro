TEMPLATE = app
TARGET = bench3

!include (../configuration.pri)

OTHER_FILES += \
    ../../../../benchmarks/bench3/Jamfile.v2

HEADERS += \
    ../../../../benchmarks/bench3/bench3.hpp

SOURCES += \
    ../../../../benchmarks/bench3/bench33.cpp \
    ../../../../benchmarks/bench3/bench32.cpp \
    ../../../../benchmarks/bench3/bench31.cpp \
    ../../../../benchmarks/bench3/bench3.cpp
