TEMPLATE = app
TARGET = bench1

!include (../configuration.pri)

OTHER_FILES += \
    ../../../../benchmarks/bench1/Jamfile.v2

HEADERS += \
    ../../../../benchmarks/bench1/bench1.hpp

SOURCES += \
    ../../../../benchmarks/bench1/bench13.cpp \
    ../../../../benchmarks/bench1/bench12.cpp \
    ../../../../benchmarks/bench1/bench11.cpp \
    ../../../../benchmarks/bench1/bench1.cpp
